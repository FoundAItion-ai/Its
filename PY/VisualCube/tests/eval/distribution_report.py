"""
Distribution report: per-trial metric distributions with optional plots.

Provides distribution-level statistics rather than means alone for major claims.

Reads analysis.json files, extracts per-trial metric values grouped by
config_key, and produces distributional statistics (median, IQR, skewness).
With --plots, generates violin plots via matplotlib.

Usage:
    python distribution_report.py <results_dir>
    python distribution_report.py <results_dir> --hypothesis h6
    python distribution_report.py <results_dir> --plots --output-dir plots/
"""
import argparse
import json
import math
import os
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, List, Optional

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

import numpy as np


KEY_METRICS = ['spiral_quality', 'food_eaten', 'area_cells_visited', 'mean_speed']


def load_per_trial_data(
    results_dir: str,
    hypothesis: Optional[str] = None,
) -> Dict[str, Dict[str, List[float]]]:
    """Load per-trial metric values from analysis.json files.

    Returns: {config_key: {metric_name: [values...]}}
    """
    data: Dict[str, Dict[str, List[float]]] = defaultdict(lambda: defaultdict(list))

    results_path = Path(results_dir)
    for run_dir in sorted(results_path.iterdir()):
        if not run_dir.is_dir():
            continue
        if hypothesis and not run_dir.name.startswith(hypothesis):
            # Also try spec-prefix mapping
            from validate_hypothesis import VALIDATORS
            if hypothesis in VALIDATORS:
                spec_prefix = VALIDATORS[hypothesis][0]
                if not run_dir.name.startswith(spec_prefix):
                    continue
            else:
                continue

        apath = run_dir / 'analysis.json'
        if not apath.exists():
            continue

        with open(apath, 'r', encoding='utf-8') as f:
            run_results = json.load(f)

        # analysis.json is a list (one entry per run_spec call) or a single dict
        if isinstance(run_results, dict):
            run_results = [run_results]

        for result in run_results:
            for trial in result.get('trials', []):
                ck = trial.get('config_key', 'unknown')
                metrics = trial.get('metrics', {})
                if 'error' in trial:
                    continue
                for m in KEY_METRICS:
                    if m in metrics:
                        val = metrics[m]
                        if isinstance(val, (int, float)):
                            data[ck][m].append(float(val))

    return dict(data)


def compute_distribution_stats(values: List[float]) -> Dict[str, Any]:
    if not values:
        return {}
    arr = np.array(values)
    n = len(arr)
    mean = float(np.mean(arr))
    std = float(np.std(arr, ddof=1)) if n > 1 else 0.0
    median = float(np.median(arr))
    q1 = float(np.percentile(arr, 25))
    q3 = float(np.percentile(arr, 75))
    iqr = q3 - q1

    # Skewness (Fisher-Pearson)
    if n > 2 and std > 0:
        skew = float(np.mean(((arr - mean) / std) ** 3) * n / ((n - 1) * (n - 2) / n))
    else:
        skew = 0.0

    return {
        'n': n,
        'mean': round(mean, 4),
        'std': round(std, 4),
        'median': round(median, 4),
        'q1': round(q1, 4),
        'q3': round(q3, 4),
        'iqr': round(iqr, 4),
        'skewness': round(skew, 4),
        'min': round(float(np.min(arr)), 4),
        'max': round(float(np.max(arr)), 4),
    }


def generate_report(data: Dict[str, Dict[str, List[float]]]) -> Dict[str, Any]:
    report = {}
    for ck in sorted(data.keys()):
        metrics_report = {}
        for m in KEY_METRICS:
            values = data[ck].get(m, [])
            if values:
                stats = compute_distribution_stats(values)
                stats['values'] = [round(v, 4) for v in values]
                metrics_report[m] = stats
        report[ck] = metrics_report
    return report


def print_report(report: Dict[str, Any]) -> None:
    print(f"\n{'='*100}")
    print("DISTRIBUTION REPORT - Per-Trial Metric Statistics")
    print(f"{'='*100}")

    for ck in sorted(report.keys()):
        print(f"\n  [{ck}]")
        metrics = report[ck]
        header = f"  {'Metric':<22} {'N':>5} {'Mean':>9} {'Median':>9} {'Std':>9} {'IQR':>9} {'Skew':>7} {'Min':>9} {'Max':>9}"
        print(header)
        print(f"  {'-'*22} {'-'*5} {'-'*9} {'-'*9} {'-'*9} {'-'*9} {'-'*7} {'-'*9} {'-'*9}")

        for m in KEY_METRICS:
            if m not in metrics:
                continue
            s = metrics[m]
            print(f"  {m:<22} {s['n']:>5} {s['mean']:>9.4f} {s['median']:>9.4f} "
                  f"{s['std']:>9.4f} {s['iqr']:>9.4f} {s['skewness']:>7.2f} "
                  f"{s['min']:>9.4f} {s['max']:>9.4f}")
    print()


def generate_plots(report: Dict[str, Any], output_dir: str) -> None:
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not available, skipping plots")
        return

    os.makedirs(output_dir, exist_ok=True)

    for metric in KEY_METRICS:
        config_keys = []
        all_values = []
        for ck in sorted(report.keys()):
            if metric in report[ck] and report[ck][metric].get('values'):
                config_keys.append(ck)
                all_values.append(report[ck][metric]['values'])

        if not config_keys:
            continue

        fig, ax = plt.subplots(figsize=(max(8, len(config_keys) * 1.2), 6))

        parts = ax.violinplot(all_values, positions=range(len(config_keys)),
                              showmeans=True, showmedians=True, showextrema=False)

        for pc in parts['bodies']:
            pc.set_facecolor('#4C72B0')
            pc.set_alpha(0.7)
        parts['cmeans'].set_color('red')
        parts['cmedians'].set_color('orange')

        bp = ax.boxplot(all_values, positions=range(len(config_keys)),
                        widths=0.15, patch_artist=True,
                        boxprops=dict(facecolor='white', alpha=0.8),
                        medianprops=dict(color='orange', linewidth=2),
                        showfliers=True, flierprops=dict(marker='o', markersize=3))

        ax.set_xticks(range(len(config_keys)))
        ax.set_xticklabels(config_keys, rotation=45, ha='right', fontsize=8)
        ax.set_ylabel(metric)
        ax.set_title(f'Distribution: {metric}')
        ax.grid(axis='y', alpha=0.3)

        plt.tight_layout()
        path = os.path.join(output_dir, f'dist_{metric}.png')
        plt.savefig(path, dpi=150)
        plt.close()
        print(f"  Saved {path}")


def main():
    parser = argparse.ArgumentParser(
        description="Per-trial distribution statistics and plots")
    parser.add_argument("results_dir", help="Path to results directory")
    parser.add_argument("--hypothesis", default=None,
                        help="Filter to a specific hypothesis (e.g., h6)")
    parser.add_argument("--plots", action="store_true",
                        help="Generate violin/box plots via matplotlib")
    parser.add_argument("--output-dir", default="plots",
                        help="Directory for plot PNG files (default: plots/)")
    parser.add_argument("--output", default=None,
                        help="Output JSON file path")
    args = parser.parse_args()

    data = load_per_trial_data(args.results_dir, args.hypothesis)

    if not data:
        print("No per-trial data found.")
        sys.exit(1)

    report = generate_report(data)
    print_report(report)

    if args.plots:
        generate_plots(report, args.output_dir)

    if args.output:
        # Strip raw values from JSON output to keep file size manageable
        json_report = {}
        for ck, metrics in report.items():
            json_report[ck] = {}
            for m, stats in metrics.items():
                json_report[ck][m] = {k: v for k, v in stats.items() if k != 'values'}
        with open(args.output, 'w', encoding='utf-8') as f:
            json.dump(json_report, f, indent=2)
        print(f"Report saved to {args.output}")


if __name__ == "__main__":
    main()
