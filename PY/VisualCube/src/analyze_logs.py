"""
Analyze batch_test log files using trajectory_analyzer metrics.

Scans a test_results directory for .log files, computes per-trial and
aggregate metrics, and outputs JSON + human-readable summary.

Usage:
    python analyze_logs.py <results_dir>  [--run-id <id>]  [--out analysis.json]
    python analyze_logs.py tests/eval/results/
    python analyze_logs.py tests/eval/results/ --run-id 20260320_123052_final_verify

Without --run-id: analyzes all run directories found.
With --run-id: analyzes only that specific run.
"""
import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

from trajectory_analyzer import (
    TrajectoryMetrics,
    analyze_trajectory,
    aggregate_trials,
    format_aggregate,
    load_trajectory_from_log,
    spiral_score,
    revisitation_rate as compute_revisitation_rate,
    area_coverage,
)


def find_log_files(results_dir: Path, run_id: str = None) -> Dict[str, List[Path]]:
    """Find all .log files grouped by run directory.

    Returns {run_id: [log_path, ...]} dict.
    """
    grouped: Dict[str, List[Path]] = defaultdict(list)

    if run_id:
        run_dir = results_dir / run_id / "logs"
        if run_dir.is_dir():
            for log_file in sorted(run_dir.glob("*.log")):
                grouped[run_id].append(log_file)
    else:
        for run_dir in sorted(results_dir.iterdir()):
            if not run_dir.is_dir():
                continue
            logs_dir = run_dir / "logs"
            if logs_dir.is_dir():
                for log_file in sorted(logs_dir.glob("*.log")):
                    grouped[run_dir.name].append(log_file)

    return dict(grouped)


def parse_log_name(log_path: Path) -> dict:
    """Extract config key and trial index from log filename.

    Handles formats:
      {agent}_{env}_trial{N}.log
      {label}_{agent}_{env}_trial{N}.log
      run{I}_{agent}_{env}_trial{N}.log

    The 'config_key' groups trials that share the same configuration.
    """
    stem = log_path.stem
    # Extract trial index from the end
    match = re.match(r'^(.+)_trial(\d+)$', stem)
    if match:
        config_key = match.group(1)
        trial_idx = int(match.group(2))
    else:
        config_key = stem
        trial_idx = 0

    # Try to extract agent_type and environment from the config key
    # Look for known agent types at any position
    agent_type = 'unknown'
    environment = 'unknown'
    for at in ('composite', 'inverter', 'stochastic'):
        if at in config_key:
            agent_type = at
            # Environment is the part after agent_type
            parts = config_key.split(f'{at}_', 1)
            if len(parts) > 1:
                environment = parts[1]
            break

    return {
        'config_key': config_key,
        'agent_type': agent_type,
        'environment': environment,
        'trial_idx': trial_idx,
    }


def compute_phase_split_metrics(data: dict) -> Dict[str, Any]:
    """Compute pre/post food-contact metrics from trajectory data.

    Returns dict with phase-split metrics, or empty dict if no food contact.
    """
    food_freqs = data.get('food_freqs')
    if food_freqs is None or len(food_freqs) == 0:
        return {}

    # Find first frame where food_freq > 0
    contact_indices = np.where(food_freqs > 0)[0]
    if len(contact_indices) == 0:
        return {'first_food_frame': -1}

    first_food_idx = int(contact_indices[0])
    xs, ys = data['xs'], data['ys']

    result: Dict[str, Any] = {'first_food_frame': first_food_idx}

    # Pre-contact spiral quality (need enough points for meaningful analysis)
    if first_food_idx >= 60:  # at least 1 second at 60fps
        pre_spiral = spiral_score(xs[:first_food_idx], ys[:first_food_idx])
        result['pre_food_spiral_quality'] = pre_spiral.quality
        result['pre_food_spiral_growth_rate'] = pre_spiral.growth_rate
    else:
        result['pre_food_spiral_quality'] = 0.0
        result['pre_food_spiral_growth_rate'] = 0.0

    # Post-contact revisitation rate
    post_len = len(xs) - first_food_idx
    if post_len >= 60:
        result['post_food_revisitation_rate'] = compute_revisitation_rate(
            xs[first_food_idx:], ys[first_food_idx:])
        # Post-contact area growth rate
        post_area = area_coverage(xs[first_food_idx:], ys[first_food_idx:])
        result['post_food_area_growth_rate'] = post_area.mean_growth_rate
    else:
        result['post_food_revisitation_rate'] = 0.0
        result['post_food_area_growth_rate'] = 0.0

    # Pre-contact area growth rate for comparison
    if first_food_idx >= 60:
        pre_area = area_coverage(xs[:first_food_idx], ys[:first_food_idx])
        result['pre_food_area_growth_rate'] = pre_area.mean_growth_rate
    else:
        result['pre_food_area_growth_rate'] = 0.0

    return result


def parse_log_footer(log_path: Path) -> Dict[str, float]:
    """Parse FINAL_STATS comment block from log file footer.

    Returns dict with numeric values, e.g. {'food_eaten': 116, 'efficiency': 1.933, ...}.
    """
    stats: Dict[str, float] = {}
    in_footer = False
    with open(log_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line == '# --- FINAL_STATS ---':
                in_footer = True
                continue
            if in_footer and line.startswith('# '):
                m = re.match(r'^# (\w+)=(.+)$', line)
                if m:
                    try:
                        stats[m.group(1)] = float(m.group(2))
                    except ValueError:
                        pass
    return stats


def parse_inverter_params(log_path: Path) -> Dict[str, float]:
    """Extract net_diff and total_power from log header inverter config.

    net_diff = sum of sign_i * (1/C3_i - 1/C1_i) where sign is +1 normal, -1 crossed
    total_power = sum of (1/C1_i + 1/C3_i) for all inverters
    """
    in_json = False
    json_lines: List[str] = []
    brace_depth = 0
    with open(log_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line == '# --- DATA ---':
                break
            if '# agent_params:' in line or '# agent_params :' in line:
                in_json = True
                continue
            if in_json and line.startswith('#'):
                content = line[1:].strip()
                json_lines.append(content)
                brace_depth += content.count('{') + content.count('[')
                brace_depth -= content.count('}') + content.count(']')
                if brace_depth <= 0 and json_lines:
                    break
            elif in_json:
                break
    if not json_lines:
        return {}
    try:
        params = json.loads(' '.join(json_lines))
    except json.JSONDecodeError:
        return {}
    inverters = params.get('inverters', [])
    if not inverters:
        return {}
    net_diff = 0.0
    total_power = 0.0
    for inv in inverters:
        c1, c3 = inv.get('C1', 0), inv.get('C3', 0)
        if c1 <= 0 or c3 <= 0:
            continue
        diff = 1.0 / c3 - 1.0 / c1
        sign = -1.0 if inv.get('crossed', False) else 1.0
        net_diff += sign * diff
        total_power += 1.0 / c1 + 1.0 / c3
    return {'net_diff': round(net_diff, 4), 'total_power': round(total_power, 4)}


def analyze_single_log(log_path: Path) -> Dict[str, Any]:
    """Analyze one log file. Returns dict with parsed info + metrics."""
    info = parse_log_name(log_path)
    data = load_trajectory_from_log(str(log_path))
    footer = parse_log_footer(log_path)
    inv_params = parse_inverter_params(log_path)

    if len(data['xs']) < 2:
        return {**info, 'log_file': log_path.name, 'error': 'too few data points',
                'n_frames': len(data['xs'])}

    metrics = analyze_trajectory(data['xs'], data['ys'])

    # Phase-split metrics for food-contact experiments (H3)
    phase_metrics = compute_phase_split_metrics(data)

    result_metrics = {
        'straightness': metrics.straightness,
        'circle_fit_score': metrics.circle_fit.score,
        'circle_fit_radius': metrics.circle_fit.radius,
        'circle_fit_center': (metrics.circle_fit.center_x, metrics.circle_fit.center_y),
        'mean_speed': metrics.mean_speed,
        'spiral_score': metrics.spiral_fit.score,
        'spiral_quality': metrics.spiral_fit.quality,
        'spiral_growth_rate': metrics.spiral_fit.growth_rate,
        'angular_consistency': metrics.spiral_fit.angular_consistency,
        'area_cells_visited': metrics.area_coverage.total_cells_visited,
        'area_mean_growth_rate': metrics.area_coverage.mean_growth_rate,
        'area_final_growth_rate': metrics.area_coverage.final_growth_rate,
        'revisitation_rate': metrics.revisitation_rate,
        'fcr': metrics.coverage_ratios.fcr,
        'lcr': metrics.coverage_ratios.lcr,
        'n_transitions': len(metrics.transitions),
        'transitions': [
            {'frame': t.frame, 'type': t.transition_type}
            for t in metrics.transitions
        ],
    }
    if phase_metrics:
        result_metrics.update(phase_metrics)

    # Add food_eaten from log footer (authoritative simulation stat)
    result_metrics['food_eaten'] = footer.get('food_eaten', 0.0)

    # Add inverter-derived params (net_diff, total_power) if available
    result_metrics.update(inv_params)

    return {
        **info,
        'log_file': log_path.name,
        'n_frames': len(data['xs']),
        'metrics': result_metrics,
        '_metrics_obj': metrics,  # kept for aggregation, stripped before JSON output
        '_phase_metrics': phase_metrics,  # kept for aggregation
    }


def group_by_config(trial_results: List[Dict]) -> Dict[str, List[Dict]]:
    """Group trial results by config_key (preserves label-based distinctions)."""
    groups: Dict[str, List[Dict]] = defaultdict(list)
    for r in trial_results:
        key = r.get('config_key', f"{r['agent_type']}/{r['environment']}")
        groups[key].append(r)
    return dict(groups)


def analyze_run(run_id: str, log_files: List[Path]) -> Dict[str, Any]:
    """Analyze all logs in one run, return per-trial + aggregate results."""
    trial_results = []
    for log_path in log_files:
        result = analyze_single_log(log_path)
        trial_results.append(result)

    # Group by configuration and compute aggregates
    groups = group_by_config(trial_results)
    aggregates = {}
    for config_key, trials in groups.items():
        valid = [t for t in trials if '_metrics_obj' in t]
        metrics_list = [t['_metrics_obj'] for t in valid]
        if metrics_list:
            agg = aggregate_trials(metrics_list)
            agg_dict = {
                'n_trials': agg.n_trials,
                'straightness': {'mean': agg.straightness[0], 'std': agg.straightness[1]},
                'circle_fit_score': {'mean': agg.circle_fit_score[0], 'std': agg.circle_fit_score[1]},
                'circle_fit_radius': {'mean': agg.circle_fit_radius[0], 'std': agg.circle_fit_radius[1]},
                'mean_speed': {'mean': agg.mean_speed[0], 'std': agg.mean_speed[1]},
                'spiral_quality': {'mean': agg.spiral_quality[0], 'std': agg.spiral_quality[1]},
                'spiral_growth_rate': {'mean': agg.spiral_growth_rate[0], 'std': agg.spiral_growth_rate[1]},
                'area_cells_visited': {'mean': agg.area_cells_visited[0], 'std': agg.area_cells_visited[1]},
                'revisitation_rate': {'mean': agg.revisitation_rate[0], 'std': agg.revisitation_rate[1]},
                'fcr': {'mean': agg.fcr[0], 'std': agg.fcr[1]},
                'lcr': {'mean': agg.lcr[0], 'std': agg.lcr[1]},
                'n_transitions_mean': agg.n_transitions_mean,
            }

            # food_eaten aggregation (from log footer)
            food_vals = [t['metrics'].get('food_eaten', 0.0) for t in valid]
            agg_dict['food_eaten'] = {
                'mean': float(np.mean(food_vals)),
                'std': float(np.std(food_vals, ddof=1)) if len(food_vals) > 1 else 0.0,
            }

            # net_diff and total_power (constant per config, from first valid trial)
            first_inv_metrics = next(
                (t['metrics'] for t in valid if 'net_diff' in t.get('metrics', {})), None)
            if first_inv_metrics:
                agg_dict['net_diff'] = {
                    'mean': first_inv_metrics['net_diff'], 'std': 0.0}
                agg_dict['total_power'] = {
                    'mean': first_inv_metrics['total_power'], 'std': 0.0}

            # Phase-split metrics aggregation (for H3 food-contact experiments)
            phase_keys = ['pre_food_spiral_quality', 'post_food_revisitation_rate',
                          'pre_food_area_growth_rate', 'post_food_area_growth_rate']
            phase_trials = [t.get('_phase_metrics', {}) for t in valid]
            # Only include if at least some trials had food contact
            has_food = [p for p in phase_trials if p.get('first_food_frame', -1) >= 0]
            if has_food:
                agg_dict['food_contact_rate'] = len(has_food) / len(valid)
                for pk in phase_keys:
                    vals = [p[pk] for p in has_food if pk in p]
                    if vals:
                        m_val = float(np.mean(vals))
                        s_val = float(np.std(vals, ddof=1)) if len(vals) > 1 else 0.0
                        agg_dict[pk] = {'mean': m_val, 'std': s_val}

            aggregates[config_key] = agg_dict

    # Strip internal objects (prefixed with _) before returning
    clean_trials = []
    for t in trial_results:
        clean = {k: v for k, v in t.items() if not k.startswith('_')}
        clean_trials.append(clean)

    return {
        'run_id': run_id,
        'n_logs': len(log_files),
        'trials': clean_trials,
        'aggregates': aggregates,
    }


def print_summary(run_result: Dict[str, Any]):
    """Print human-readable summary to stdout."""
    print(f"\n{'='*70}")
    print(f"Run: {run_result['run_id']}  ({run_result['n_logs']} logs)")
    print(f"{'='*70}")

    for config_key, agg in run_result.get('aggregates', {}).items():
        print(f"\n  [{config_key}] ({agg['n_trials']} trials)")
        print(f"  {'-'*50}")
        for metric_name in ['straightness', 'circle_fit_score', 'circle_fit_radius',
                            'mean_speed', 'spiral_quality', 'spiral_growth_rate',
                            'area_cells_visited', 'revisitation_rate',
                            'fcr', 'lcr', 'net_diff', 'total_power']:
            if metric_name not in agg:
                continue
            m = agg[metric_name]
            print(f"    {metric_name:25s} {m['mean']:10.4f} +/- {m['std']:.4f}")
        print(f"    {'n_transitions_mean':25s} {agg['n_transitions_mean']:10.1f}")
        if 'food_eaten' in agg:
            fe = agg['food_eaten']
            print(f"    {'food_eaten':25s} {fe['mean']:10.1f} +/- {fe['std']:.1f}")
        # Phase-split metrics (H3 food-contact experiments)
        if 'food_contact_rate' in agg:
            print(f"    {'food_contact_rate':25s} {agg['food_contact_rate']:10.4f}")
            for pk in ['pre_food_spiral_quality', 'post_food_revisitation_rate',
                        'pre_food_area_growth_rate', 'post_food_area_growth_rate']:
                if pk in agg:
                    m = agg[pk]
                    print(f"    {pk:25s} {m['mean']:10.4f} +/- {m['std']:.4f}")

    # Per-trial details
    print(f"\n  Per-trial details:")
    print(f"  {'File':<45} {'Straight':>8} {'Circle':>8} {'Radius':>8} "
          f"{'Speed':>8} {'Spiral':>8}")
    print(f"  {'-'*45} {'-'*8} {'-'*8} {'-'*8} {'-'*8} {'-'*8}")
    for t in run_result.get('trials', []):
        if 'error' in t:
            print(f"  {t['log_file']:<45} ERROR: {t['error']}")
            continue
        m = t['metrics']
        print(f"  {t['log_file']:<45} {m['straightness']:8.4f} {m['circle_fit_score']:8.4f} "
              f"{m['circle_fit_radius']:8.1f} {m['mean_speed']:8.1f} {m['spiral_quality']:8.4f}")


def print_cross_run_summary(all_results: List[Dict[str, Any]]):
    """Print a summary table averaging aggregates across all runs."""
    METRICS = ['mean_speed', 'straightness', 'circle_fit_score', 'circle_fit_radius',
               'spiral_quality', 'spiral_growth_rate', 'area_cells_visited',
               'fcr', 'lcr', 'net_diff', 'total_power']

    config_data: Dict[str, Dict[str, List[float]]] = defaultdict(
        lambda: defaultdict(list)
    )

    for result in all_results:
        for config_key, agg in result.get('aggregates', {}).items():
            for m in METRICS:
                if m in agg:
                    config_data[config_key][m].append(agg[m]['mean'])

    if not config_data:
        return

    n_runs = len(all_results)
    print(f"\n{'='*90}")
    print(f"Cross-run summary ({n_runs} runs)")
    print(f"{'='*90}")

    # Detect which metrics have non-trivial values to show
    active_metrics = []
    for m in METRICS:
        for ck in config_data:
            vals = config_data[ck].get(m, [])
            if vals and any(v != 0 for v in vals):
                active_metrics.append(m)
                break

    short_names = {
        'mean_speed': 'Speed', 'straightness': 'Straight',
        'circle_fit_score': 'CircFit', 'circle_fit_radius': 'Radius',
        'spiral_quality': 'Spiral', 'spiral_growth_rate': 'Growth',
        'area_cells_visited': 'Area', 'fcr': 'FCR', 'lcr': 'LCR',
        'net_diff': 'NetDiff', 'total_power': 'TotPow',
    }

    header = f"  {'Config':<30}"
    for m in active_metrics:
        header += f" {short_names.get(m, m):>10}"
    print(header)
    print(f"  {'-'*30}" + f" {'-'*10}" * len(active_metrics))

    avg = lambda lst: sum(lst) / len(lst) if lst else 0

    for ck in sorted(config_data.keys()):
        row = f"  {ck:<30}"
        for m in active_metrics:
            vals = config_data[ck].get(m, [])
            row += f" {avg(vals):10.4f}" if vals else f" {'N/A':>10}"
        print(row)

    print()


def main():
    parser = argparse.ArgumentParser(
        description="Analyze batch_test log files with trajectory metrics")
    parser.add_argument("results_dir", help="Path to test_results directory")
    parser.add_argument("--run-id", default=None, help="Analyze specific run only")
    parser.add_argument("--out", default=None, help="Output JSON file path")
    args = parser.parse_args()

    results_dir = Path(args.results_dir)
    if not results_dir.is_dir():
        print(f"Error: {results_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    grouped = find_log_files(results_dir, args.run_id)
    if not grouped:
        print("No log files found.", file=sys.stderr)
        sys.exit(1)

    all_results = []
    for run_id, log_files in grouped.items():
        result = analyze_run(run_id, log_files)
        all_results.append(result)
        print_summary(result)

    # Write JSON output — one analysis.json per run folder
    if args.out:
        # Explicit path: write everything to that single file
        with open(args.out, 'w', encoding='utf-8') as f:
            json.dump(all_results, f, indent=2, default=str)
        print(f"\nJSON output: {args.out}")
    else:
        # Default: write analysis.json inside each run's folder
        for result in all_results:
            run_id = result["run_id"]
            out_path = str(results_dir / run_id / "analysis.json")
            with open(out_path, 'w', encoding='utf-8') as f:
                json.dump([result], f, indent=2, default=str)
            print(f"\nJSON output: {out_path}")

    # Cross-run summary table (averages across all runs)
    if len(all_results) > 1:
        print_cross_run_summary(all_results)


if __name__ == "__main__":
    main()
