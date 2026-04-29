"""
Sensitivity analysis: sweep metric thresholds to show conclusion stability.

Shows that hypothesis conclusions survive alternative threshold choices.

Reads existing analysis.json results and re-evaluates the critical
threshold-dependent checks from H0-H9 across a range of threshold values.
No new simulations are run.

Usage:
    python sensitivity_analysis.py <results_dir>
    python sensitivity_analysis.py <results_dir> --hypotheses h1 h2 h7 h8
    python sensitivity_analysis.py <results_dir> --output sensitivity_report.json
"""
import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from validate_hypothesis import load_aggregates, avg


SPIRAL_QUALITY_SWEEP = [0.05, 0.08, 0.10, 0.12, 0.15, 0.18, 0.20, 0.25, 0.30, 0.35]
CIRCLE_FIT_SWEEP = [0.1, 0.2, 0.3, 0.4, 0.5]


def _m(config_data: Dict, ck: str, metric: str) -> float:
    vals = config_data.get(ck, {}).get(metric, [])
    return avg(vals) if vals else 0.0


def _sdr(config_data: Dict, ck: str) -> float:
    vals = config_data.get(ck, {}).get('spiral_detection_rate', [])
    return avg(vals) if vals else 0.0


ThresholdCheck = Tuple[str, str, str, str]  # (name, config_key, metric, direction)


THRESHOLD_CHECKS: Dict[str, Dict[str, List[ThresholdCheck]]] = {
    'spiral_quality': {
        'h1': [
            ('H1a spiral_quality > T', 'h1a_composite_void', 'spiral_quality', '>'),
        ],
        'h2': [
            ('H2a stochastic < T', 'h2a_stochastic_void', 'spiral_quality', '<'),
            ('H2b inverter < T', 'h2b_inverter_void', 'spiral_quality', '<'),
            ('H2c composite (no cross) < T', 'h2c_composite_void', 'spiral_quality', '<'),
            ('H2d composite (no stagger) < T', 'h2d_composite_void', 'spiral_quality', '<'),
            ('H2e opposition only < T', 'h2e_composite_void', 'spiral_quality', '<'),
            ('H2f staggering only < T', 'h2f_composite_void', 'spiral_quality', '<'),
            ('H2g both > T', 'h2g_composite_void', 'spiral_quality', '>'),
        ],
        'h4b': [
            ('H4b1 baseline > T', 'h4b1_composite_void', 'spiral_quality', '>'),
        ],
        'h7': [
            ('H7a1 D_ROT=0 > T', 'h7a1_composite_void', 'spiral_quality', '>'),
            ('H7a4 D_ROT=0.1 > T', 'h7a4_composite_void', 'spiral_quality', '>'),
            ('H7a6 D_ROT=0.5 > T', 'h7a6_composite_void', 'spiral_quality', '>'),
        ],
        'h8': [
            ('H8a1 3-inv > T', 'h8a1_composite_void', 'spiral_quality', '>'),
            ('H8a5 7-inv > T', 'h8a5_composite_void', 'spiral_quality', '>'),
        ],
        'h9': [
            ('NEG-H1 stochastic < T', 'neg_h1_stochastic_void', 'spiral_quality', '<'),
            ('NEG-H2 same-C1 < T', 'neg_h2_composite_void', 'spiral_quality', '<'),
            ('NEG-H4a all-normal < T', 'neg_h4a_composite_void', 'spiral_quality', '<'),
        ],
    },
    'circle_fit_score': {
        'h0': [
            ('H0b circle_fit > T', 'h0b_inverter_void', 'circle_fit_score', '>'),
            ('H0c circle_fit > T', 'h0c_inverter_void', 'circle_fit_score', '>'),
            ('H0f circle_fit > T', 'h0f_inverter_void', 'circle_fit_score', '>'),
            ('H0g circle_fit > T', 'h0g_inverter_void', 'circle_fit_score', '>'),
        ],
    },
}


def sweep_metric(config_data: Dict, metric: str, hypothesis: str,
                 thresholds: List[float]) -> Dict[str, Any]:
    checks = THRESHOLD_CHECKS.get(metric, {}).get(hypothesis, [])
    if not checks:
        return {}

    results = {}
    for name, ck, m, direction in checks:
        value = _m(config_data, ck, m)
        passes = {}
        for t in thresholds:
            if direction == '>':
                passes[str(t)] = value > t
            else:
                passes[str(t)] = value < t

        stable_range = [t for t in thresholds if passes[str(t)]]
        results[name] = {
            'value': round(value, 4),
            'direction': direction,
            'passes': passes,
            'stable_min': min(stable_range) if stable_range else None,
            'stable_max': max(stable_range) if stable_range else None,
            'n_pass': sum(1 for v in passes.values() if v),
            'n_total': len(thresholds),
        }
    return results


def run_sensitivity(results_dir: str, hypotheses: Optional[List[str]] = None) -> Dict[str, Any]:
    all_hypotheses = hypotheses or ['h0', 'h1', 'h2', 'h4b', 'h7', 'h8', 'h9']

    report: Dict[str, Any] = {}

    for hyp in all_hypotheses:
        from validate_hypothesis import VALIDATORS
        if hyp not in VALIDATORS:
            continue
        spec_prefix, _ = VALIDATORS[hyp]
        config_data, config_std, n_runs, n_trials = load_aggregates(
            results_dir, spec_prefix)
        if not config_data:
            print(f"  No data for {hyp}, skipping")
            continue

        hyp_results: Dict[str, Any] = {}

        sq_checks = sweep_metric(config_data, 'spiral_quality', hyp, SPIRAL_QUALITY_SWEEP)
        if sq_checks:
            hyp_results['spiral_quality'] = sq_checks

        cf_checks = sweep_metric(config_data, 'circle_fit_score', hyp, CIRCLE_FIT_SWEEP)
        if cf_checks:
            hyp_results['circle_fit_score'] = cf_checks

        if hyp_results:
            report[hyp] = hyp_results

    return report


def print_report(report: Dict[str, Any]) -> None:
    print(f"\n{'='*90}")
    print("THRESHOLD SENSITIVITY ANALYSIS")
    print(f"{'='*90}")

    for hyp, metrics in sorted(report.items()):
        print(f"\n--- {hyp.upper()} ---")
        for metric_name, checks in metrics.items():
            thresholds = SPIRAL_QUALITY_SWEEP if metric_name == 'spiral_quality' else CIRCLE_FIT_SWEEP
            print(f"\n  {metric_name} thresholds:")

            header = f"  {'Check':<40}"
            header += f" {'Value':>7}"
            for t in thresholds:
                header += f" {t:>5}"
            print(header)
            print(f"  {'-'*40} {'-'*7}" + f" {'-'*5}" * len(thresholds))

            for name, data in checks.items():
                row = f"  {name:<40}"
                row += f" {data['value']:>7.4f}"
                for t in thresholds:
                    passed = data['passes'][str(t)]
                    row += f" {'  +' if passed else '  -':>5}"
                print(row)

                stable = f"stable [{data['stable_min']}, {data['stable_max']}]" if data['stable_min'] is not None else "NONE"
                print(f"  {'':>40} {data['n_pass']}/{data['n_total']} pass, {stable}")

    # Summary
    print(f"\n{'='*90}")
    print("SUMMARY")
    print(f"{'='*90}")
    all_pass = True
    for hyp, metrics in sorted(report.items()):
        for metric_name, checks in metrics.items():
            for name, data in checks.items():
                if data['n_pass'] < data['n_total']:
                    all_pass = False
                    print(f"  PARTIAL: {hyp} / {name}: {data['n_pass']}/{data['n_total']} thresholds pass")

    if all_pass:
        print("  All checks pass across all swept thresholds.")
    print()


def main():
    parser = argparse.ArgumentParser(
        description="Sweep metric thresholds to show conclusion stability")
    parser.add_argument("results_dir", help="Path to results directory")
    parser.add_argument("--hypotheses", nargs="+", default=None,
                        help="Hypotheses to analyze (default: all)")
    parser.add_argument("--output", default=None,
                        help="Output JSON file path")
    args = parser.parse_args()

    report = run_sensitivity(args.results_dir, args.hypotheses)

    if not report:
        print("No data found for any hypothesis.")
        sys.exit(1)

    print_report(report)

    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2)
        print(f"Report saved to {args.output}")


if __name__ == "__main__":
    main()
