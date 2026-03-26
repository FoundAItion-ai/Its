"""
Batch test runner for NNN agent simulations.

Reads test spec JSON files, runs headless simulations, and writes results
to a JSONL database and optional screenshots/logs.

Usage:
    python tests/eval/batch_test.py tests/eval/specs/<spec.json> [--results-dir tests/eval/results] [--parallel]

Single spec: runs sequentially in current process.
Multiple specs or --parallel: spawns child processes via subprocess.
"""
import sys
import os
import json
import argparse
import time
import traceback
import subprocess
import csv
import statistics
from datetime import datetime
from pathlib import Path
from typing import Dict, Any, List, Optional, TextIO

# Add src/ to path so we can import project modules
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

# Must init pygame before importing main
import pygame
pygame.init()

import config as cfg
from optimize_params import run_single_trial


def write_log_header(f: TextIO, agent_type: str, environment: str,
                     agent_config: Dict[str, Any], run_params: Dict[str, Any],
                     spawn_point: tuple):
    """Write a log header matching the GUI log format."""
    from datetime import datetime
    f.write(f"# Simulation Log - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
    f.write(f"# --- CONFIGURATION ---\n")
    f.write(f"# Agent Type: {agent_type}\n")
    f.write(f"# Food Preset: {environment}\n")
    try:
        f.write(f"# initial_food_count: {len(cfg.FOOD_PRESETS[environment]())}\n")
    except Exception:
        f.write(f"# initial_food_count: 0\n")

    if agent_type == 'composite' and 'inverters' in agent_config:
        import json as _json
        f.write(f"# agent_params: \n")
        for line in _json.dumps(agent_config, indent=4).splitlines():
            f.write(f"# {line}\n")
    else:
        for key, value in agent_config.items():
            if key in ('C1', 'C2', 'C3', 'C4') and isinstance(value, (int, float)) and value > 0:
                freq = 1.0 / value
                f.write(f"# {key}: {freq:.4f} Hz, {value:.4f} sec\n")
            else:
                f.write(f"# {key}: {value}\n")

    f.write(f"# num_agents: {run_params.get('n_agents', 1)}\n")
    f.write(f"# fps: {run_params.get('fps', 60)}\n")
    f.write(f"# global_speed_modifier: {cfg.GLOBAL_SPEED_MODIFIER}\n")
    f.write(f"# agent_speed_scaling_factor: {cfg.AGENT_SPEED_SCALING_FACTOR}\n")
    f.write(f"# angular_proportionality_constant: {cfg.ANGULAR_PROPORTIONALITY_CONSTANT}\n")
    f.write(f"# spawn_point: ({spawn_point[0]}, {spawn_point[1]})\n")
    f.write(f"# initial_direction_deg: -1.0\n")
    f.write(f"# --- DATA ---\n")

    base_cols = "frame,agent_id,pos_x,pos_y,angle_rad,delta_dist,delta_theta_rad,food_freq,mode,speed"
    if agent_type == 'inverter':
        sig_cols = ",inv0_L,inv0_R"
    elif agent_type == 'composite':
        n_inv = len(agent_config.get('inverters', []))
        sig_cols = "," + ",".join(f"inv{i}_L,inv{i}_R" for i in range(n_inv)) if n_inv > 0 else ""
    else:
        sig_cols = ""
    f.write(f"# {base_cols}{sig_cols}\n")


def resolve_spawn_point(environment: str) -> tuple:
    """Look up spawn hint for an environment, return (x, y) in pixels."""
    hint = cfg.FOOD_SPAWN_HINTS.get(environment)
    if hint:
        return (hint[0], hint[1] * cfg.WINDOW_H)
    return (120.0, cfg.WINDOW_H / 2)


def build_agent_params(agent_type: str, agent_config: Dict[str, Any]) -> Dict[str, Any]:
    """Convert spec agent_config to the params dict expected by agent constructors."""
    if agent_type == "inverter":
        params = {}
        for key in ("C1", "C2", "C3", "C4", "turn_decision_interval_sec"):
            if key in agent_config:
                params[key] = agent_config[key]
        params.setdefault("turn_decision_interval_sec", 0.0167)
        return params
    elif agent_type == "composite":
        return dict(agent_config)
    elif agent_type == "stochastic":
        return dict(agent_config)
    else:
        return dict(agent_config)


def run_spec(spec: Dict[str, Any], results_dir: Path, run_id: str) -> List[Dict[str, Any]]:
    """Execute all runs in a spec, return list of result records."""
    defaults = spec.get("defaults", {})
    spec_name = spec.get("name", "unnamed")
    runs_list = spec.get("runs", [])

    exec_dir = results_dir / run_id
    screenshots_dir = exec_dir / "screenshots"
    logs_dir = exec_dir / "logs"

    records = []

    for run_idx, run_entry in enumerate(runs_list):
        agent_type = run_entry["agent_type"]
        environment = run_entry["environment"]
        agent_config = run_entry.get("agent_config", {})
        run_label = run_entry.get("label", "")
        n_trials = run_entry.get("runs", defaults.get("runs", 5))
        seconds = run_entry.get("seconds", defaults.get("seconds", 30))
        n_agents = run_entry.get("n_agents", defaults.get("n_agents", 1))
        fps = run_entry.get("fps", defaults.get("fps", 60))
        do_screenshots = run_entry.get("screenshots", defaults.get("screenshots", False))
        do_logging = run_entry.get("logging", defaults.get("logging", True))

        agent_params = build_agent_params(agent_type, agent_config)
        spawn_point = resolve_spawn_point(environment)

        # Build a unique run prefix for log/screenshot filenames
        run_prefix = f"{agent_type}_{environment}"
        if run_label:
            # Sanitize label for filesystem: take tag before ':'/' ', lowercase
            tag = run_label.split(":")[0].strip().replace(" ", "_").lower()
            run_prefix = f"{tag}_{agent_type}_{environment}"
        elif sum(1 for r in runs_list
                 if r["agent_type"] == agent_type and r["environment"] == environment) > 1:
            # Disambiguate duplicate agent_type/environment combos by index
            run_prefix = f"run{run_idx}_{agent_type}_{environment}"

        print(f"  [{run_idx+1}/{len(runs_list)}] {agent_type} / {environment} "
              f"({n_trials} trials x {seconds}s)"
              + (f"  [{run_label}]" if run_label else ""), flush=True)

        per_trial = []
        error_count = 0
        trial_stats: List[Dict[str, Any]] = []

        for trial_idx in range(n_trials):
            trial_name = f"{run_prefix}_trial{trial_idx}"
            log_handle: Optional[TextIO] = None

            try:
                if do_logging:
                    logs_dir.mkdir(parents=True, exist_ok=True)
                    log_path = logs_dir / f"{trial_name}.log"
                    log_handle = open(log_path, "w")
                    write_log_header(log_handle, agent_type, environment,
                                     agent_config,
                                     {"n_agents": n_agents, "fps": fps},
                                     spawn_point)

                start_cb = None
                if do_screenshots:
                    start_cb = lambda: _save_screenshot(screenshots_dir, trial_name, "start")

                result = run_single_trial(
                    agent_type=agent_type,
                    food_preset=environment,
                    agent_params=agent_params,
                    seconds=seconds,
                    n_agents=n_agents,
                    fps=fps,
                    spawn_point=spawn_point,
                    log_file_handle=log_handle,
                    on_configured=start_cb,
                )

                if do_logging and log_handle:
                    log_handle.write(f"# --- FINAL_STATS ---\n")
                    log_handle.write(f"# food_eaten={result['food_eaten']}\n")
                    log_handle.write(f"# food_remaining={result['food_remaining']}\n")
                    log_handle.write(f"# sim_time={result['sim_time']:.3f}\n")
                    log_handle.write(f"# efficiency={result['efficiency']:.3f}\n")
                    log_handle.write(f"# excursion_dist={result['excursion_dist']:.1f}\n")
                    log_handle.write(f"# input_freq={result['input_freq']:.2f}\n")

                if do_screenshots:
                    _save_screenshot(screenshots_dir, trial_name, "end")

                per_trial.append(result["efficiency"])
                trial_stats.append(result)
                print(f"    trial {trial_idx}: eff={result['efficiency']:.2f}, "
                      f"eaten={result['food_eaten']}", flush=True)

            except Exception as e:
                tb = traceback.format_exc()
                print(f"    trial {trial_idx}: ERROR: {e}", file=sys.stderr, flush=True)
                print(tb, file=sys.stderr)
                per_trial.append(f"ERROR: {e}")
                error_count += 1
            finally:
                if log_handle:
                    log_handle.close()

        # Compute aggregate stats from successful trials
        successful_effs = [v for v in per_trial if isinstance(v, (int, float))]
        successful_count = len(successful_effs)

        if successful_count > 0:
            mean_eff = statistics.mean(successful_effs)
            median_eff = statistics.median(successful_effs)
            std_eff = statistics.stdev(successful_effs) if successful_count > 1 else 0.0
            mean_food = statistics.mean([s["food_eaten"] for s in trial_stats])
            food_remaining = trial_stats[-1].get("food_remaining", 0)
            mean_excursion = statistics.mean([s["excursion_dist"] for s in trial_stats])
            mean_input_freq = statistics.mean([s["input_freq"] for s in trial_stats])
        else:
            mean_eff = median_eff = std_eff = None
            mean_food = food_remaining = mean_excursion = mean_input_freq = None

        record = {
            "timestamp": datetime.now().isoformat(timespec="seconds"),
            "run_id": run_id,
            "spec_name": spec_name,
            "label": run_label,
            "agent_type": agent_type,
            "environment": environment,
            "agent_config": agent_config,
            "run_params": {
                "seconds": seconds,
                "runs": n_trials,
                "n_agents": n_agents,
                "fps": fps,
            },
            "results": {
                "mean_efficiency": mean_eff,
                "median_efficiency": median_eff,
                "std_efficiency": std_eff,
                "mean_food_eaten": mean_food,
                "food_remaining": food_remaining,
                "mean_excursion_dist": mean_excursion,
                "mean_input_freq": mean_input_freq,
                "per_trial": per_trial,
                "errors": error_count,
                "successful_trials": successful_count,
            },
        }
        records.append(record)

    return records


def _save_screenshot(screenshots_dir: Path, trial_name: str, tag: str):
    """Render the current WORLD state to an offscreen surface and save as PNG."""
    from main import WORLD
    screenshots_dir.mkdir(parents=True, exist_ok=True)
    surf = pygame.Surface((cfg.WINDOW_W, cfg.WINDOW_H))
    WORLD.draw(surf)
    path = screenshots_dir / f"{trial_name}_{tag}.png"
    pygame.image.save(surf, str(path))


def write_jsonl(records: List[Dict[str, Any]], jsonl_path: Path):
    """Append records to a JSONL file."""
    jsonl_path.parent.mkdir(parents=True, exist_ok=True)
    with open(jsonl_path, "a", encoding="utf-8") as f:
        for rec in records:
            f.write(json.dumps(rec, default=str) + "\n")


def merge_results(results_dir: Path):
    """Merge all per-execution results.jsonl files, write merged outputs, then clean up."""
    all_records = []
    per_exec_files = []
    for jsonl_file in results_dir.rglob("results.jsonl"):
        # Skip the shared top-level results.jsonl — only collect per-execution ones
        if jsonl_file.parent != results_dir:
            per_exec_files.append(jsonl_file)
        with open(jsonl_file, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    all_records.append(json.loads(line))

    if not all_records:
        print("No results to merge.")
        return

    # Write merged JSONL
    merged_jsonl = results_dir / "results_merged.jsonl"
    with open(merged_jsonl, "w", encoding="utf-8") as f:
        for rec in all_records:
            f.write(json.dumps(rec, default=str) + "\n")

    # Clean up per-execution results.jsonl files (artifacts like screenshots/logs remain)
    for f in per_exec_files:
        f.unlink()
        print(f"  Cleaned up {f}")

    # Write merged CSV
    merged_csv = results_dir / "results_merged.csv"
    csv_cols = [
        "timestamp", "run_id", "spec_name", "agent_type", "environment",
        "mean_efficiency", "median_efficiency", "std_efficiency",
        "mean_food_eaten", "food_remaining", "mean_excursion_dist",
        "mean_input_freq", "errors", "successful_trials", "agent_config",
    ]
    with open(merged_csv, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=csv_cols)
        writer.writeheader()
        for rec in all_records:
            row = {
                "timestamp": rec.get("timestamp"),
                "run_id": rec.get("run_id"),
                "spec_name": rec.get("spec_name"),
                "agent_type": rec.get("agent_type"),
                "environment": rec.get("environment"),
                "agent_config": json.dumps(rec.get("agent_config", {})),
            }
            results = rec.get("results", {})
            for key in ["mean_efficiency", "median_efficiency", "std_efficiency",
                        "mean_food_eaten", "food_remaining", "mean_excursion_dist",
                        "mean_input_freq", "errors", "successful_trials"]:
                row[key] = results.get(key)
            writer.writerow(row)

    print(f"Merged {len(all_records)} records -> {merged_jsonl}")
    print(f"CSV export -> {merged_csv}")


def print_summary(records: List[Dict[str, Any]]):
    """Print a summary table of results."""
    if not records:
        return
    print()
    print(f"{'Agent':>12} {'Environment':>16} {'Eff(mean)':>10} {'Eff(med)':>10} "
          f"{'Food':>6} {'Exc':>6} {'Err':>4}")
    print("-" * 72)
    for rec in records:
        r = rec["results"]
        m_eff = f"{r['mean_efficiency']:.2f}" if r["mean_efficiency"] is not None else "N/A"
        md_eff = f"{r['median_efficiency']:.2f}" if r["median_efficiency"] is not None else "N/A"
        food = f"{r['mean_food_eaten']:.0f}" if r["mean_food_eaten"] is not None else "N/A"
        exc = f"{r['mean_excursion_dist']:.1f}" if r["mean_excursion_dist"] is not None else "N/A"
        print(f"{rec['agent_type']:>12} {rec['environment']:>16} {m_eff:>10} {md_eff:>10} "
              f"{food:>6} {exc:>6} {r['errors']:>4}")


def run_worker(spec_path: str, results_dir: str, is_parallel_child: bool = False):
    """Worker mode: run a single spec file and write results."""
    with open(spec_path, "r", encoding="utf-8") as f:
        spec = json.load(f)

    spec_name = spec.get("name", Path(spec_path).stem)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_id = f"{timestamp}_{spec_name}"
    results_dir = Path(results_dir)

    print(f"Running spec: {spec_name} (run_id: {run_id})")
    records = run_spec(spec, results_dir, run_id)

    if is_parallel_child:
        # Parallel child: write per-execution JSONL only (parent merges later)
        exec_jsonl = results_dir / run_id / "results.jsonl"
        write_jsonl(records, exec_jsonl)
    else:
        # Sequential: append directly to shared results file
        shared_jsonl = results_dir / "results.jsonl"
        write_jsonl(records, shared_jsonl)
        print(f"\nResults written to {shared_jsonl}")

    print_summary(records)
    return records


def main():
    parser = argparse.ArgumentParser(description="Batch test runner for NNN agent simulations")
    parser.add_argument("spec_files", nargs="+", help="Test spec JSON file(s)")
    parser.add_argument("--results-dir", default="tests/eval/results", help="Output directory (default: tests/eval/results)")
    parser.add_argument("--parallel", action="store_true", help="Run multiple specs in parallel via subprocesses")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()

    # Validate spec files exist before doing anything
    for spec_path in args.spec_files:
        if not os.path.isfile(spec_path):
            print(f"Error: spec file not found: {spec_path}", file=sys.stderr)
            sys.exit(1)
        try:
            with open(spec_path, "r", encoding="utf-8") as f:
                spec = json.load(f)
            if "runs" not in spec:
                print(f"Error: spec file missing 'runs' key: {spec_path}", file=sys.stderr)
                sys.exit(1)
        except json.JSONDecodeError as e:
            print(f"Error: invalid JSON in {spec_path}: {e}", file=sys.stderr)
            sys.exit(1)

    if args.worker:
        # Child process mode: run single spec
        run_worker(args.spec_files[0], args.results_dir, is_parallel_child=True)
        return

    if args.parallel and len(args.spec_files) > 1:
        # Spawn child processes for each spec
        procs = []
        for spec_path in args.spec_files:
            cmd = [sys.executable, __file__, spec_path,
                   "--results-dir", args.results_dir, "--worker"]
            print(f"Spawning worker for {spec_path}")
            # cwd = project root (two levels up from tests/eval/)
            project_root = str(Path(__file__).parent.parent.parent)
            proc = subprocess.Popen(cmd, cwd=project_root)
            procs.append((spec_path, proc))

        # Wait for all children
        for spec_path, proc in procs:
            ret = proc.wait()
            if ret != 0:
                print(f"Warning: worker for {spec_path} exited with code {ret}", file=sys.stderr)

        # Merge results
        merge_results(Path(args.results_dir))
    else:
        # Sequential mode: run specs one by one in this process
        all_records = []
        for spec_path in args.spec_files:
            records = run_worker(spec_path, args.results_dir)
            all_records.extend(records)

        if len(args.spec_files) > 1:
            print("\n=== Combined Summary ===")
            print_summary(all_records)


if __name__ == "__main__":
    main()
