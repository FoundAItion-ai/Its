"""
Universal hypothesis validation for batch test results.

Reads analysis.json files from run folders, averages aggregates across runs,
and validates against hypothesis-specific pass criteria.

Usage:
    python validate_hypothesis.py <results_dir> <hypothesis>
    python validate_hypothesis.py tests/eval/results h0
    python validate_hypothesis.py tests/eval/results h1
    python validate_hypothesis.py tests/eval/results h1 --runs "h1_emergence_20260401_15*"

Options:
    --runs PATTERN   Glob pattern to filter run folders (default: all matching hypothesis)
    --verbose        Show per-run details
"""
import argparse
import json
import os
import sys
import fnmatch
from collections import defaultdict
from typing import Any, Callable, Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def load_aggregates(
    results_dir: str,
    spec_prefix: str,
    run_pattern: Optional[str] = None,
) -> Tuple[Dict[str, Dict[str, List[float]]], Dict[str, Dict[str, List[float]]], int, int]:
    """Load and average aggregates from analysis.json across matching runs.

    Returns:
        (config_data, config_std, n_runs, n_trials) where
        config_data[config_key][metric_name] = [values...]
        n_trials = total trials per config across all runs
    """
    all_dirs = sorted(os.listdir(results_dir))
    runs = []
    for d in all_dirs:
        if not os.path.isdir(os.path.join(results_dir, d)):
            continue
        if run_pattern:
            if fnmatch.fnmatch(d, run_pattern):
                runs.append(d)
        elif d.startswith(spec_prefix):
            runs.append(d)

    if not runs:
        print(f"No run folders matching '{run_pattern or spec_prefix}*' in {results_dir}")
        sys.exit(1)

    ALL_METRICS = [
        'straightness', 'circle_fit_score', 'circle_fit_radius',
        'mean_speed', 'spiral_quality', 'spiral_growth_rate',
        'area_cells_visited', 'revisitation_rate',
    ]

    config_data: Dict[str, Dict[str, List[float]]] = defaultdict(
        lambda: defaultdict(list)
    )
    config_std: Dict[str, Dict[str, List[float]]] = defaultdict(
        lambda: defaultdict(list)
    )

    total_trials = 0
    for run in runs:
        apath = os.path.join(results_dir, run, 'analysis.json')
        if not os.path.exists(apath):
            continue
        with open(apath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        for result in data:
            for ck, agg in result.get('aggregates', {}).items():
                if 'n_trials' in agg:
                    total_trials = max(total_trials, agg['n_trials'])
                for m in ALL_METRICS:
                    if m in agg:
                        config_data[ck][m].append(agg[m]['mean'])
                        config_std[ck][m].append(agg[m]['std'])

    return config_data, config_std, len(runs), total_trials


def avg(lst: List[float]) -> float:
    return sum(lst) / len(lst) if lst else 0.0


# ---------------------------------------------------------------------------
# Check definitions
# ---------------------------------------------------------------------------

class Check:
    """A single validation check."""

    def __init__(self, name: str, detail: str, passed: bool):
        self.name = name
        self.detail = detail
        self.passed = passed

    def __str__(self):
        status = 'PASS' if self.passed else 'FAIL'
        return f'  [{status}] {self.name}: {self.detail}'


def check_gt(name: str, value: float, threshold: float, label: str = '') -> Check:
    """value > threshold"""
    return Check(name, f'{value:.4f} > {threshold:.4f} {label}'.strip(), value > threshold)


def check_lt(name: str, value: float, threshold: float, label: str = '') -> Check:
    """value < threshold"""
    return Check(name, f'{value:.4f} < {threshold:.4f} {label}'.strip(), value < threshold)


def check_approx(name: str, a: float, b: float, a_std: float, b_std: float) -> Check:
    """a ~= b within combined std"""
    tol = a_std + b_std
    diff = abs(a - b)
    passed = diff <= max(tol, abs(b) * 0.25)  # within 1 std or 25% of b
    return Check(name, f'{a:.1f} ~= {b:.1f} (diff={diff:.1f}, tol={tol:.1f})', passed)


def check_ratio(name: str, a: float, b: float, expected: float, tolerance: float = 0.3) -> Check:
    """a/b ~= expected ratio within tolerance"""
    actual = a / b if b != 0 else float('inf')
    passed = abs(actual - expected) <= tolerance
    return Check(name, f'{a:.1f}/{b:.1f} = {actual:.2f}x (expected {expected:.1f}x)', passed)


def check_order(name: str, values: List[float], labels: List[str], ascending: bool = True) -> Check:
    """Values are monotonically ordered."""
    if ascending:
        passed = all(values[i] <= values[i+1] for i in range(len(values)-1))
        detail = ' <= '.join(f'{v:.4f}({l})' for v, l in zip(values, labels))
    else:
        passed = all(values[i] >= values[i+1] for i in range(len(values)-1))
        detail = ' >= '.join(f'{v:.4f}({l})' for v, l in zip(values, labels))
    return Check(name, detail, passed)


def check_ge_with_std(name: str, a: float, a_std: float, b: float) -> Check:
    """a + std >= b (within tolerance)"""
    passed = a + a_std >= b
    return Check(name, f'{a:.4f} + {a_std:.4f} >= {b:.4f}', passed)


# ---------------------------------------------------------------------------
# H0 validation
# ---------------------------------------------------------------------------

def validate_h0(config_data, config_std) -> List[Check]:
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    def s(ck, metric):
        return avg(config_std[ck][metric])

    # H0a: straight line — circle_fit_score is not useful here (straight line = arc of huge
    # circle, always scores high). Instead check radius is much larger than real circles.
    checks.append(check_gt(
        'H0a straightness >> H0b',
        m('h0a_inverter_void', 'straightness') / max(m('h0b_inverter_void', 'straightness'), 0.001),
        2.0, '(ratio)'))
    checks.append(check_gt(
        'H0a radius >> H0b (straight = huge fitted circle)',
        m('h0a_inverter_void', 'circle_fit_radius') / max(m('h0b_inverter_void', 'circle_fit_radius'), 0.001),
        3.0, '(ratio)'))

    # H0b vs H0c: symmetric circles
    checks.append(check_gt(
        'H0b circle_fit_score > 0.3',
        m('h0b_inverter_void', 'circle_fit_score'), 0.3))
    checks.append(check_gt(
        'H0c circle_fit_score > 0.3',
        m('h0c_inverter_void', 'circle_fit_score'), 0.3))
    checks.append(check_approx(
        'H0b radius ~= H0c radius',
        m('h0b_inverter_void', 'circle_fit_radius'),
        m('h0c_inverter_void', 'circle_fit_radius'),
        s('h0b_inverter_void', 'circle_fit_radius'),
        s('h0c_inverter_void', 'circle_fit_radius')))

    # H0d vs H0b: 2x speed, same radius
    checks.append(check_ratio(
        'H0d speed ~= 2x H0b',
        m('h0d_inverter_void', 'mean_speed'),
        m('h0b_inverter_void', 'mean_speed'), 2.0))
    checks.append(check_approx(
        'H0d radius ~= H0b radius',
        m('h0d_inverter_void', 'circle_fit_radius'),
        m('h0b_inverter_void', 'circle_fit_radius'),
        s('h0d_inverter_void', 'circle_fit_radius'),
        s('h0b_inverter_void', 'circle_fit_radius')))

    # H0e vs H0b: 4x speed, same radius (wider tolerance for discretization at high freq)
    checks.append(check_ratio(
        'H0e speed ~= 4x H0b',
        m('h0e_inverter_void', 'mean_speed'),
        m('h0b_inverter_void', 'mean_speed'), 4.0, tolerance=0.5))
    checks.append(check_approx(
        'H0e radius ~= H0b radius',
        m('h0e_inverter_void', 'circle_fit_radius'),
        m('h0b_inverter_void', 'circle_fit_radius'),
        s('h0e_inverter_void', 'circle_fit_radius'),
        s('h0b_inverter_void', 'circle_fit_radius')))

    # H0d vs H0e: same radius
    checks.append(check_approx(
        'H0d radius ~= H0e radius',
        m('h0d_inverter_void', 'circle_fit_radius'),
        m('h0e_inverter_void', 'circle_fit_radius'),
        s('h0d_inverter_void', 'circle_fit_radius'),
        s('h0e_inverter_void', 'circle_fit_radius')))

    # H0f vs H0b: tighter circle
    checks.append(check_lt(
        'H0f radius < H0b radius',
        m('h0f_inverter_void', 'circle_fit_radius'),
        m('h0b_inverter_void', 'circle_fit_radius')))
    checks.append(check_gt(
        'H0f circle_fit_score > 0.3',
        m('h0f_inverter_void', 'circle_fit_score'), 0.3))

    # H0g: tightest, monotonic
    checks.append(check_order(
        'Radius: H0b > H0f > H0g',
        [m('h0b_inverter_void', 'circle_fit_radius'),
         m('h0f_inverter_void', 'circle_fit_radius'),
         m('h0g_inverter_void', 'circle_fit_radius')],
        ['H0b', 'H0f', 'H0g'],
        ascending=False))
    checks.append(check_gt(
        'H0g circle_fit_score > 0.3',
        m('h0g_inverter_void', 'circle_fit_score'), 0.3))

    return checks


# ---------------------------------------------------------------------------
# H1 validation
# ---------------------------------------------------------------------------

def validate_h1(config_data, config_std) -> List[Check]:
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    def s(ck, metric):
        return avg(config_std[ck][metric])

    # H1a: spiral detected
    checks.append(check_gt(
        'H1a spiral_quality > 0.15',
        m('h1a_composite_void', 'spiral_quality'), 0.15))
    checks.append(check_gt(
        'H1a growth_rate > 0',
        m('h1a_composite_void', 'spiral_growth_rate'), 0.0))
    checks.append(check_lt(
        'H1a straightness < 0.3',
        m('h1a_composite_void', 'straightness'), 0.3))

    # H1b >= H1a
    checks.append(check_ge_with_std(
        'H1b spiral >= H1a',
        m('h1b_composite_void', 'spiral_quality'),
        s('h1b_composite_void', 'spiral_quality'),
        m('h1a_composite_void', 'spiral_quality')))
    checks.append(check_gt(
        'H1b area >= H1a',
        m('h1b_composite_void', 'area_cells_visited'),
        m('h1a_composite_void', 'area_cells_visited') - 1))  # -1 for float tolerance

    # H1c >= H1b
    checks.append(check_ge_with_std(
        'H1c spiral >= H1b',
        m('h1c_composite_void', 'spiral_quality'),
        s('h1c_composite_void', 'spiral_quality'),
        m('h1b_composite_void', 'spiral_quality')))
    checks.append(check_gt(
        'H1c area >= H1b',
        m('h1c_composite_void', 'area_cells_visited'),
        m('h1b_composite_void', 'area_cells_visited') - 1))

    # Monotonicity: more inverters -> more area covered
    checks.append(check_order(
        'Area monotonicity: H1a <= H1b <= H1c',
        [m('h1a_composite_void', 'area_cells_visited'),
         m('h1b_composite_void', 'area_cells_visited'),
         m('h1c_composite_void', 'area_cells_visited')],
        ['H1a', 'H1b', 'H1c'],
        ascending=True))

    return checks


# ---------------------------------------------------------------------------
# H2 validation
# ---------------------------------------------------------------------------

def validate_h2(config_data, config_std) -> List[Check]:
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    def s(ck, metric):
        return avg(config_std[ck][metric])

    # Tier 1: Simpler agents don't spiral
    for label, ck in [('H2a', 'h2a_stochastic_void'),
                       ('H2b', 'h2b_inverter_void'),
                       ('H2c', 'h2c_composite_void'),
                       ('H2d', 'h2d_composite_void')]:
        checks.append(check_lt(
            f'{label} spiral_quality < 0.20',
            m(ck, 'spiral_quality'), 0.20))

    # Tier 2: Neither ingredient alone is sufficient
    checks.append(check_lt(
        'H2e spiral_quality < 0.20 (opposition without staggering)',
        m('h2e_composite_void', 'spiral_quality'), 0.20))
    checks.append(check_lt(
        'H2f spiral_quality < 0.20 (staggering without opposition)',
        m('h2f_composite_void', 'spiral_quality'), 0.20))

    # Tier 3: Both ingredients together produce spiral
    checks.append(check_gt(
        'H2g spiral_quality > 0.20 (positive control)',
        m('h2g_composite_void', 'spiral_quality'), 0.20))

    # Clear separation: H2g > 2x the best non-spiral config
    non_spiral_keys = ['h2a_stochastic_void', 'h2b_inverter_void',
                       'h2c_composite_void', 'h2d_composite_void',
                       'h2e_composite_void', 'h2f_composite_void']
    max_non_spiral = max(m(ck, 'spiral_quality') for ck in non_spiral_keys)
    checks.append(check_gt(
        'H2g spiral > 2x max(H2a..H2f)',
        m('h2g_composite_void', 'spiral_quality') / max(max_non_spiral, 0.001),
        2.0, '(ratio)'))

    # Tier 4: Phase transition evidence
    checks.append(check_gt(
        'H2g spiral >> H2e (staggering matters)',
        m('h2g_composite_void', 'spiral_quality'),
        m('h2e_composite_void', 'spiral_quality') + 0.10))
    checks.append(check_gt(
        'H2g spiral >> H2f (opposition matters)',
        m('h2g_composite_void', 'spiral_quality'),
        m('h2f_composite_void', 'spiral_quality') + 0.10))
    checks.append(check_gt(
        'H2g area > H2b (spiral covers more area than circle)',
        m('h2g_composite_void', 'area_cells_visited'),
        m('h2b_inverter_void', 'area_cells_visited')))

    return checks


# ---------------------------------------------------------------------------
# Summary table
# ---------------------------------------------------------------------------

def print_metrics_table(config_data, config_std, n_runs, n_trials, hypothesis):
    """Print cross-run averaged metrics."""
    ALL_METRICS = [
        'mean_speed', 'straightness', 'circle_fit_score', 'circle_fit_radius',
        'spiral_quality', 'spiral_growth_rate', 'area_cells_visited',
    ]

    # Pick relevant metrics per hypothesis
    if hypothesis == 'h0':
        display_metrics = ['mean_speed', 'straightness', 'circle_fit_score', 'circle_fit_radius']
    elif hypothesis == 'h1':
        display_metrics = ['mean_speed', 'spiral_quality', 'spiral_growth_rate',
                           'circle_fit_score', 'straightness', 'area_cells_visited']
    elif hypothesis == 'h2':
        display_metrics = ['spiral_quality', 'spiral_growth_rate', 'circle_fit_score',
                           'straightness', 'mean_speed', 'area_cells_visited']
    else:
        display_metrics = ALL_METRICS

    total = n_trials * n_runs if n_trials else n_runs
    print(f"\n{'='*80}")
    print(f"Metrics ({total} trials across {n_runs} {'run' if n_runs == 1 else 'runs'})")
    print(f"{'='*80}")

    # Header
    header = f"  {'Config':<30}"
    for m in display_metrics:
        short = m.replace('circle_fit_', 'cf_').replace('spiral_', 'sp_').replace('area_cells_', 'area_')
        header += f" {short:>10}"
    print(header)
    print(f"  {'-'*30}" + f" {'-'*10}" * len(display_metrics))

    for ck in sorted(config_data.keys()):
        row = f"  {ck:<30}"
        for m in display_metrics:
            vals = config_data[ck].get(m, [])
            if vals:
                row += f" {avg(vals):10.4f}"
            else:
                row += f" {'N/A':>10}"
        print(row)
    print()


# ---------------------------------------------------------------------------
# Registry & main
# ---------------------------------------------------------------------------

VALIDATORS = {
    'h0': ('h0_baseline', validate_h0),
    'h1': ('h1_emergence', validate_h1),
    'h2': ('h2_specificity', validate_h2),
}


def main():
    parser = argparse.ArgumentParser(
        description="Validate hypothesis test results against pass criteria")
    parser.add_argument("results_dir", help="Path to results directory")
    parser.add_argument("hypothesis", choices=list(VALIDATORS.keys()),
                        help="Hypothesis to validate")
    parser.add_argument("--runs", default=None,
                        help="Glob pattern to filter run folders")
    parser.add_argument("--verbose", action="store_true",
                        help="Show per-run details")
    args = parser.parse_args()

    spec_prefix, validator_fn = VALIDATORS[args.hypothesis]
    config_data, config_std, n_runs, n_trials = load_aggregates(
        args.results_dir, spec_prefix, args.runs)

    if not config_data:
        print("No data loaded.")
        sys.exit(1)

    # Print metrics table
    print_metrics_table(config_data, config_std, n_runs, n_trials, args.hypothesis)

    # Run checks
    checks = validator_fn(config_data, config_std)
    n_pass = sum(1 for c in checks if c.passed)

    total = n_trials * n_runs if n_trials else n_runs
    print(f"{'='*80}")
    print(f"Validation: {args.hypothesis.upper()} ({total} trials across {n_runs} {'run' if n_runs == 1 else 'runs'})")
    print(f"{'='*80}")
    for c in checks:
        print(str(c))
    print(f"\n  {n_pass}/{len(checks)} checks passed")

    if n_pass == len(checks):
        print(f"\n  >>> {args.hypothesis.upper()} PASSED <<<")
    else:
        print(f"\n  >>> {args.hypothesis.upper()} FAILED ({len(checks) - n_pass} checks) <<<")

    sys.exit(0 if n_pass == len(checks) else 1)


if __name__ == "__main__":
    main()
