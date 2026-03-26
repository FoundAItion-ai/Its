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

from trajectory_analyzer import (
    TrajectoryMetrics,
    analyze_trajectory,
    aggregate_trials,
    format_aggregate,
    load_trajectory_from_log,
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


def analyze_single_log(log_path: Path) -> Dict[str, Any]:
    """Analyze one log file. Returns dict with parsed info + metrics."""
    info = parse_log_name(log_path)
    data = load_trajectory_from_log(str(log_path))

    if len(data['xs']) < 2:
        return {**info, 'log_file': log_path.name, 'error': 'too few data points',
                'n_frames': len(data['xs'])}

    metrics = analyze_trajectory(data['xs'], data['ys'])

    return {
        **info,
        'log_file': log_path.name,
        'n_frames': len(data['xs']),
        'metrics': {
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
            'n_transitions': len(metrics.transitions),
            'transitions': [
                {'frame': t.frame, 'type': t.transition_type}
                for t in metrics.transitions
            ],
        },
        '_metrics_obj': metrics,  # kept for aggregation, stripped before JSON output
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
            aggregates[config_key] = {
                'n_trials': agg.n_trials,
                'straightness': {'mean': agg.straightness[0], 'std': agg.straightness[1]},
                'circle_fit_score': {'mean': agg.circle_fit_score[0], 'std': agg.circle_fit_score[1]},
                'circle_fit_radius': {'mean': agg.circle_fit_radius[0], 'std': agg.circle_fit_radius[1]},
                'mean_speed': {'mean': agg.mean_speed[0], 'std': agg.mean_speed[1]},
                'spiral_quality': {'mean': agg.spiral_quality[0], 'std': agg.spiral_quality[1]},
                'spiral_growth_rate': {'mean': agg.spiral_growth_rate[0], 'std': agg.spiral_growth_rate[1]},
                'area_cells_visited': {'mean': agg.area_cells_visited[0], 'std': agg.area_cells_visited[1]},
                'revisitation_rate': {'mean': agg.revisitation_rate[0], 'std': agg.revisitation_rate[1]},
                'n_transitions_mean': agg.n_transitions_mean,
            }

    # Strip internal objects before returning
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
        print(f"  {'─'*50}")
        for metric_name in ['straightness', 'circle_fit_score', 'circle_fit_radius',
                            'mean_speed', 'spiral_quality', 'spiral_growth_rate',
                            'area_cells_visited', 'revisitation_rate']:
            m = agg[metric_name]
            print(f"    {metric_name:25s} {m['mean']:10.4f} +/- {m['std']:.4f}")
        print(f"    {'n_transitions_mean':25s} {agg['n_transitions_mean']:10.1f}")

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

    # Write JSON output
    out_path = args.out
    if not out_path:
        out_path = str(results_dir / "analysis.json")

    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(all_results, f, indent=2, default=str)
    print(f"\nJSON output: {out_path}")


if __name__ == "__main__":
    main()
