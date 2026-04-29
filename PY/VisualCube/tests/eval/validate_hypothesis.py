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
import math
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
        'fcr', 'lcr',
        'net_diff', 'total_power',
        'food_eaten',
        # Phase-split metrics (H3)
        'pre_food_spiral_quality', 'post_food_revisitation_rate',
        'pre_food_area_growth_rate', 'post_food_area_growth_rate',
    ]
    SCALAR_METRICS = ['spiral_detection_rate']

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
                # Scalar metrics (not mean/std dicts)
                for scalar_m in ('n_transitions_mean', 'food_contact_rate',
                                 'spiral_detection_rate'):
                    if scalar_m in agg:
                        config_data[ck][scalar_m].append(agg[scalar_m])

    return config_data, config_std, len(runs), total_trials


def avg(lst: List[float]) -> float:
    return sum(lst) / len(lst) if lst else 0.0


def ci95(mean: float, std: float, n: int) -> Tuple[float, float]:
    """Return 95% confidence interval (lower, upper)."""
    if n <= 0:
        return (mean, mean)
    margin = 1.96 * std / math.sqrt(n)
    return (mean - margin, mean + margin)


def cohens_d(m1: float, s1: float, m2: float, s2: float) -> float:
    """Cohen's d effect size between two groups."""
    pooled_std = math.sqrt((s1**2 + s2**2) / 2)
    return (m1 - m2) / pooled_std if pooled_std > 0 else float('inf')


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
# H3 validation
# ---------------------------------------------------------------------------

def validate_h3(config_data, config_std) -> List[Check]:
    """H3: Exploitation — 1st-degree symmetry: composite spirals in void then finds and tracks a single food source.
    Controls track food but lack the prior exploratory spiral phase."""
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    # --- A. Composites spiral before food (pre-contact spiral quality) ---
    checks.append(check_gt(
        'H3a pre-food spiral > 0.12 (band)',
        m('h3a_composite_dense_band', 'pre_food_spiral_quality'), 0.12))
    checks.append(check_gt(
        'H3b pre-food spiral > 0.10 (small circle)',
        m('h3b_composite_small_circle', 'pre_food_spiral_quality'), 0.10))

    # --- B. Controls don't spiral (single inverter = no spiral phase) ---
    checks.append(check_lt(
        'H3c control spiral < 0.10 (band)',
        m('h3c_inverter_dense_band', 'pre_food_spiral_quality'), 0.10))
    checks.append(check_lt(
        'H3d control spiral < 0.10 (small circle)',
        m('h3d_inverter_small_circle', 'pre_food_spiral_quality'), 0.10))

    # --- C. Composite spiral quality > control spiral quality ---
    checks.append(check_gt(
        'H3a composite spiral > H3c control spiral (band)',
        m('h3a_composite_dense_band', 'pre_food_spiral_quality'),
        m('h3c_inverter_dense_band', 'pre_food_spiral_quality')))
    checks.append(check_gt(
        'H3b composite spiral > H3d control spiral (circle)',
        m('h3b_composite_small_circle', 'pre_food_spiral_quality'),
        m('h3d_inverter_small_circle', 'pre_food_spiral_quality')))

    # --- D. Composite mode transition: high revisitation post-food (circle env) ---
    # Post-food revisitation should be high, confirming agent stays in food region
    # after transitioning from exploration to exploitation.
    checks.append(check_gt(
        'H3b post-food revisitation high (circle)',
        m('h3b_composite_small_circle', 'post_food_revisitation_rate'), 0.90))

    # --- E. Composite eats more food than control ---
    checks.append(check_gt(
        'H3a food_eaten > H3c control (band)',
        m('h3a_composite_dense_band', 'food_eaten'),
        m('h3c_inverter_dense_band', 'food_eaten')))
    checks.append(check_gt(
        'H3b food_eaten > H3d control (circle)',
        m('h3b_composite_small_circle', 'food_eaten'),
        m('h3d_inverter_small_circle', 'food_eaten')))

    return checks


# ---------------------------------------------------------------------------
# H4a validation
# ---------------------------------------------------------------------------

def validate_h4a(config_data, config_std) -> List[Check]:
    """H4a: Parameter characterization — sweep C1 ratio (1:X:2X) with uniform
    internal ratio. Measures FCR/LCR across spiral regime, balance point, and
    reversal to find optimal operating point."""
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    ck1 = 'h4a1_composite_void'   # Net=+1.89 (tight, weak opposition)
    ck2 = 'h4a2_composite_void'   # Net=+0.89 (medium opposition)
    ck3 = 'h4a3_composite_void'   # Net=+0.45 (wide spiral, strong opposition)
    ck4 = 'h4a4_composite_void'   # Net=+0.22 (open spiral)
    ck5 = 'h4a5_composite_void'   # Net=0.0   (balance point)
    ck6 = 'h4a6_composite_void'   # Net=-0.45 (slight reverse)
    ck7 = 'h4a7_composite_void'   # Net=-1.11 (strong reverse)

    # A. Area ordering: stronger opposition = wider exploration (deterministic)
    checks.append(check_order(
        'Area: H4a4 >= H4a3 >= H4a2 >= H4a1 (stronger opposition = wider)',
        [m(ck4, 'area_cells_visited'), m(ck3, 'area_cells_visited'),
         m(ck2, 'area_cells_visited'), m(ck1, 'area_cells_visited')],
        ['H4a4', 'H4a3', 'H4a2', 'H4a1'],
        ascending=False))

    # B. H4a3 covers dramatically more area than H4a1
    checks.append(check_gt(
        'H4a3 area > 5 * H4a1 (expanding spiral vs tight donut)',
        m(ck3, 'area_cells_visited'),
        m(ck1, 'area_cells_visited') * 5))

    # C. Speed gradient: closer to balance = faster (less net curvature)
    checks.append(check_order(
        'Speed: H4a5 >= H4a4 >= H4a3 >= H4a2 >= H4a1',
        [m(ck5, 'mean_speed'), m(ck4, 'mean_speed'), m(ck3, 'mean_speed'),
         m(ck2, 'mean_speed'), m(ck1, 'mean_speed')],
        ['H4a5', 'H4a4', 'H4a3', 'H4a2', 'H4a1'],
        ascending=False))

    # D. Balance point much faster than tight spiral
    checks.append(check_gt(
        'H4a5 speed >> H4a1 (cancellation = fast motion)',
        m(ck5, 'mean_speed'),
        m(ck1, 'mean_speed') * 2.0))

    # E. Balance point covers more area than all forward spirals
    checks.append(check_gt(
        'H4a5 area > H4a4 (balance sweeps widest)',
        m(ck5, 'area_cells_visited'),
        m(ck4, 'area_cells_visited')))

    # F. Forward regime covers more area than reversed (H4a7)
    checks.append(check_gt(
        'H4a3 area > H4a7 (forward spiral > reversed donut)',
        m(ck3, 'area_cells_visited'),
        m(ck7, 'area_cells_visited')))

    # G. Reversed configs are bounded (not expanding)
    checks.append(check_lt(
        'H4a7 area < H4a5 (reversed donut < balance sweep)',
        m(ck7, 'area_cells_visited'),
        m(ck5, 'area_cells_visited')))

    # H. H4a6 speed > H4a1 speed (reversed closer to balance = still fast)
    checks.append(check_gt(
        'H4a6 speed > H4a1 (closer to balance = faster)',
        m(ck6, 'mean_speed'),
        m(ck1, 'mean_speed')))

    # I. Symmetry: reversed H4a6 and forward H4a3 have comparable area
    area3 = m(ck3, 'area_cells_visited')
    area6 = m(ck6, 'area_cells_visited')
    if area3 > 0:
        ratio = area6 / area3
        checks.append(Check(
            'Area symmetry: H4a6/H4a3 ratio between 0.3 and 3.0',
            f"ratio={ratio:.2f} (H4a3={area3:.0f}, H4a6={area6:.0f})",
            0.3 < ratio < 3.0))

    # J. H4a1 is the most bounded (tightest donut)
    checks.append(check_lt(
        'H4a1 area < H4a2 (tightest is most bounded)',
        m(ck1, 'area_cells_visited'),
        m(ck2, 'area_cells_visited')))

    # K. H4a4 area between H4a3 and H4a5 (confirms continuous gradient)
    checks.append(check_gt(
        'H4a4 area > H4a3 (open spiral > wide spiral)',
        m(ck4, 'area_cells_visited'),
        m(ck3, 'area_cells_visited')))

    # L. H4a4 speed between H4a3 and H4a5 (continuous speed gradient)
    checks.append(check_gt(
        'H4a4 speed > H4a3 (closer to balance = faster)',
        m(ck4, 'mean_speed'),
        m(ck3, 'mean_speed')))

    return checks


# ---------------------------------------------------------------------------
# H4b validation
# ---------------------------------------------------------------------------

def validate_h4b(config_data, config_std) -> List[Check]:
    """H4b: C1 parameter jitter causes graceful degradation, not catastrophic
    failure. Spiral quality decreases monotonically with jitter level."""
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    ck0 = 'h4b1_composite_void'   # 0% (baseline)
    ck10 = 'h4b2_composite_void'  # 10%
    ck20 = 'h4b3_composite_void'  # 20%
    ck30 = 'h4b4_composite_void'  # 30%

    # A. Baseline produces spiral
    checks.append(check_gt(
        'H4b1 baseline spiral_quality > 0.15',
        m(ck0, 'spiral_quality'), 0.15))

    # B. Monotonic degradation
    checks.append(check_order(
        'Spiral degrades: 0% >= 10% >= 20% >= 30%',
        [m(ck0, 'spiral_quality'), m(ck10, 'spiral_quality'),
         m(ck20, 'spiral_quality'), m(ck30, 'spiral_quality')],
        ['0%', '10%', '20%', '30%'],
        ascending=False))

    # C. NOT catastrophic: even 30% jitter retains some spiral
    checks.append(check_gt(
        'H4b4 30% jitter spiral_quality > 0.05 (graceful)',
        m(ck30, 'spiral_quality'), 0.05))

    # D. 10% jitter retains > 50% of baseline
    checks.append(check_gt(
        'H4b2 10% retains > 50% of baseline',
        m(ck10, 'spiral_quality') / max(m(ck0, 'spiral_quality'), 0.001),
        0.5, '(retention ratio)'))

    # E. Area coverage also degrades gracefully
    checks.append(check_order(
        'Area degrades: 0% >= 10% >= 20% >= 30%',
        [m(ck0, 'area_cells_visited'), m(ck10, 'area_cells_visited'),
         m(ck20, 'area_cells_visited'), m(ck30, 'area_cells_visited')],
        ['0%', '10%', '20%', '30%'],
        ascending=False))

    return checks


# ---------------------------------------------------------------------------
# H5 validation
# ---------------------------------------------------------------------------

def validate_h5(config_data, config_std) -> List[Check]:
    """H5: 2nd-degree symmetry — open spiral outperforms tighter configurations
    at finding and tracking two parallel food bands."""
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    ck1 = 'h5a1_composite_dense_bands'   # Net=+1.89 (tight spiral)
    ck2 = 'h5a2_composite_dense_bands'   # Net=+0.89 (medium spiral)
    ck3 = 'h5a3_composite_dense_bands'   # Net=+0.45 (wide spiral)
    ck4 = 'h5a4_composite_dense_bands'   # Net=+0.22 (open spiral)

    # A. Monotonic food ordering: more open spiral = more food eaten
    checks.append(check_order(
        'Food: H5a4 >= H5a3 >= H5a2 >= H5a1 (open eats most)',
        [m(ck4, 'food_eaten'), m(ck3, 'food_eaten'),
         m(ck2, 'food_eaten'), m(ck1, 'food_eaten')],
        ['H5a4', 'H5a3', 'H5a2', 'H5a1'],
        ascending=False))

    # B. Open spiral substantially outperforms tight spiral
    checks.append(check_gt(
        'H5a4 food > 1.5 * H5a1 food (open >> tight)',
        m(ck4, 'food_eaten'),
        m(ck1, 'food_eaten') * 1.5))

    # C. Open spiral is specifically the best
    checks.append(check_gt(
        'H5a4 food > H5a3 food (open > wide)',
        m(ck4, 'food_eaten'),
        m(ck3, 'food_eaten')))

    # D. Wide spiral outperforms medium
    checks.append(check_gt(
        'H5a3 food > H5a2 food (wide > medium)',
        m(ck3, 'food_eaten'),
        m(ck2, 'food_eaten')))

    # E. Medium spiral outperforms tight
    checks.append(check_gt(
        'H5a2 food > H5a1 food (medium > tight)',
        m(ck2, 'food_eaten'),
        m(ck1, 'food_eaten')))

    # F. Open spiral eats a meaningful amount of food
    checks.append(check_gt(
        'H5a4 food > 10 (open spiral finds food)',
        m(ck4, 'food_eaten'), 10.0))

    # G. Area coverage ordering matches food ordering
    checks.append(check_order(
        'Area: H5a4 >= H5a3 >= H5a2 >= H5a1 (open explores most)',
        [m(ck4, 'area_cells_visited'), m(ck3, 'area_cells_visited'),
         m(ck2, 'area_cells_visited'), m(ck1, 'area_cells_visited')],
        ['H5a4', 'H5a3', 'H5a2', 'H5a1'],
        ascending=False))

    return checks


# ---------------------------------------------------------------------------
# H6 validation
# ---------------------------------------------------------------------------

def validate_h6(config_data, config_std) -> List[Check]:
    """H6: Complexity scaling (inverted) — fewer, stronger activation stages
    outperform many weak ones. 3 inverters beat 7 at same Net=+0.22."""
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    ck1 = 'h6a1_composite_dense_bands'   # 3 inverters
    ck2 = 'h6a2_composite_dense_bands'   # 4 inverters
    ck3 = 'h6a3_composite_dense_bands'   # 5 inverters
    ck4 = 'h6a4_composite_dense_bands'   # 6 inverters
    ck5 = 'h6a5_composite_dense_bands'   # 7 inverters

    # A. Monotonic food ordering: fewer inverters = more food
    checks.append(check_order(
        'Food: H6a1 >= H6a2 >= H6a3 >= H6a4 >= H6a5 (fewer inv eats more)',
        [m(ck1, 'food_eaten'), m(ck2, 'food_eaten'),
         m(ck3, 'food_eaten'), m(ck4, 'food_eaten'),
         m(ck5, 'food_eaten')],
        ['H6a1', 'H6a2', 'H6a3', 'H6a4', 'H6a5'],
        ascending=False))

    # B. 3-inv substantially outperforms 7-inv
    checks.append(check_gt(
        'H6a1 food > 1.3 * H6a5 food (3 inv >> 7 inv)',
        m(ck1, 'food_eaten'),
        m(ck5, 'food_eaten') * 1.3))

    # C. Pairwise: fewer always beats more
    checks.append(check_gt(
        'H6a1 food > H6a2 food (3 inv > 4 inv)',
        m(ck1, 'food_eaten'),
        m(ck2, 'food_eaten')))
    checks.append(check_gt(
        'H6a2 food > H6a3 food (4 inv > 5 inv)',
        m(ck2, 'food_eaten'),
        m(ck3, 'food_eaten')))

    # D. Best config (3 inv) finds food
    checks.append(check_gt(
        'H6a1 food > 10 (3-inv finds food)',
        m(ck1, 'food_eaten'), 10.0))

    # E. Area ordering: fewer inverters = more area explored
    checks.append(check_order(
        'Area: H6a1 >= H6a2 >= H6a3 >= H6a4 >= H6a5 (fewer inv explores more)',
        [m(ck1, 'area_cells_visited'), m(ck2, 'area_cells_visited'),
         m(ck3, 'area_cells_visited'), m(ck4, 'area_cells_visited'),
         m(ck5, 'area_cells_visited')],
        ['H6a1', 'H6a2', 'H6a3', 'H6a4', 'H6a5'],
        ascending=False))

    return checks


# ---------------------------------------------------------------------------
# H7 validation
# ---------------------------------------------------------------------------

def validate_h7(config_data, config_std) -> List[Check]:
    """H7: D_ROT sensitivity — spiral is structural (exists at D_ROT=0) and
    robust across all noise levels. Noise doesn't destroy the spiral; it adds
    path variation that increases area coverage while preserving the expanding
    pattern."""
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    def sdr(ck):
        vals = config_data[ck].get('spiral_detection_rate', [])
        return avg(vals) if vals else 0.0

    ck1 = 'h7a1_composite_void'   # D_ROT=0.00
    ck2 = 'h7a2_composite_void'   # D_ROT=0.01
    ck3 = 'h7a3_composite_void'   # D_ROT=0.05
    ck4 = 'h7a4_composite_void'   # D_ROT=0.10
    ck5 = 'h7a5_composite_void'   # D_ROT=0.20
    ck6 = 'h7a6_composite_void'   # D_ROT=0.50

    # A. Deterministic (D_ROT=0) produces clear spiral — proves structural
    checks.append(check_gt(
        'H7a1 D_ROT=0 spiral_quality > 0.20 (structural proof)',
        m(ck1, 'spiral_quality'), 0.20))

    # B. D_ROT=0 has highest detection rate (most consistent spiral)
    checks.append(check_gt(
        'H7a1 D_ROT=0 detection_rate > 0.90 (deterministic = reliable)',
        sdr(ck1), 0.90))

    # C. All noise levels still produce detectable spiral (universal robustness)
    checks.append(check_gt(
        'H7a4 D_ROT=0.1 spiral_quality > 0.10',
        m(ck4, 'spiral_quality'), 0.10))
    checks.append(check_gt(
        'H7a6 D_ROT=0.5 spiral_quality > 0.05 (heavy noise, still present)',
        m(ck6, 'spiral_quality'), 0.05))

    # D. Noise increases area coverage (heading perturbation aids exploration)
    checks.append(check_gt(
        'H7a6 area > H7a1 area (noise aids spatial coverage)',
        m(ck6, 'area_cells_visited'),
        m(ck1, 'area_cells_visited')))

    return checks


# ---------------------------------------------------------------------------
# H8 validation
# ---------------------------------------------------------------------------

def validate_h8(config_data, config_std) -> List[Check]:
    """H8: Reconciliation [EXPLORATORY / POST-HOC]

    This analysis is exploratory, motivated by H6 observations during data
    analysis. It was not pre-registered as a confirmatory hypothesis.

    H6 configs in void show more inverters = better spiral quality and higher
    detection rate, the REVERSE of H6 food ordering. More inverters produce a
    tighter, more structured spiral (higher quality) but fewer inverters wander
    more (higher area). The key reconciliation: spiral quality goes UP with
    inverter count while food efficiency goes DOWN."""
    checks = []

    def m(ck, metric):
        return avg(config_data[ck][metric])

    def sdr(ck):
        vals = config_data[ck].get('spiral_detection_rate', [])
        return avg(vals) if vals else 0.0

    ck1 = 'h8a1_composite_void'   # 3 inverters
    ck2 = 'h8a2_composite_void'   # 4 inverters
    ck3 = 'h8a3_composite_void'   # 5 inverters
    ck4 = 'h8a4_composite_void'   # 6 inverters
    ck5 = 'h8a5_composite_void'   # 7 inverters

    # A. 7-inv clearly better spiral than 3-inv (reverse of H6 food ordering)
    checks.append(check_gt(
        'H8a5 spiral > H8a1 (7 inv > 3 inv in void)',
        m(ck5, 'spiral_quality'),
        m(ck1, 'spiral_quality')))

    # B. 7-inv substantially better (at least 1.3x)
    checks.append(check_gt(
        'H8a5 spiral > 1.3 * H8a1 (clear separation)',
        m(ck5, 'spiral_quality'),
        m(ck1, 'spiral_quality') * 1.3))

    # C. 7-inv has highest detection rate
    checks.append(check_gt(
        'H8a5 detection_rate > 0.80 (7 inv most reliable spiral)',
        sdr(ck5), 0.80))

    # D. Detection rate improves with inverter count (endpoints)
    checks.append(check_gt(
        'H8a5 detection > H8a1 detection (more inv = more reliable)',
        sdr(ck5), sdr(ck1)))

    # E. Even weakest config produces some spiral (above noise floor)
    checks.append(check_gt(
        'H8a1 (3 inv, weakest) spiral > 0.05',
        m(ck1, 'spiral_quality'), 0.05))

    return checks


# ---------------------------------------------------------------------------
# H9 validation
# ---------------------------------------------------------------------------

def validate_h9(config_data, config_std) -> List[Check]:
    """H9: Negative controls — adversarial configs that must produce bad results,
    proving H0-H8 validators have discriminative power."""
    checks = []

    def m(ck, metric):
        vals = config_data.get(ck, {}).get(metric, [])
        return avg(vals) if vals else 0.0

    def sdr(ck):
        vals = config_data.get(ck, {}).get('spiral_detection_rate', [])
        return avg(vals) if vals else 0.0

    # --- NEG-H0: Symmetric inverter (C1=C3, D_ROT=0) -> straight line = huge radius ---
    # H0 checks circle_fit_radius ordering; straight line has enormous fitted radius.
    checks.append(check_gt(
        'NEG-H0: symmetric inverter radius > 200 (straight line)',
        m('neg_h0_inverter_void', 'circle_fit_radius'), 200.0))

    # --- NEG-H1: Stochastic in void -> no spiral ---
    # H1 checks spiral_quality > 0.15 for composites.
    checks.append(check_lt(
        'NEG-H1: stochastic spiral < 0.10',
        m('neg_h1_stochastic_void', 'spiral_quality'), 0.10))

    # --- NEG-H2: Same-C1 crossed -> opposition without staggering, no spiral ---
    checks.append(check_lt(
        'NEG-H2: same-C1 crossed spiral < 0.20',
        m('neg_h2_composite_void', 'spiral_quality'), 0.20))

    # --- NEG-H3: Stochastic agent -> no spiral, poor food exploitation ---
    checks.append(check_lt(
        'NEG-H3: stochastic pre_food_spiral < 0.10',
        m('neg_h3_stochastic_dense_band', 'pre_food_spiral_quality'), 0.10))
    checks.append(check_lt(
        'NEG-H3: stochastic food < 100 (random walk, poor exploiter)',
        m('neg_h3_stochastic_dense_band', 'food_eaten'), 100.0))

    # --- NEG-H4a: All-normal -> no opposition, spiral_quality below baseline ---
    checks.append(check_lt(
        'NEG-H4a: all-normal composite spiral < 0.20',
        m('neg_h4a_composite_void', 'spiral_quality'), 0.20))

    # --- NEG-H4b: Stochastic -> no spiral to be robust ---
    # H4b checks spiral survives jitter; stochastic has no spiral at all.
    checks.append(check_lt(
        'NEG-H4b: stochastic spiral < 0.10',
        m('neg_h4b_stochastic_void', 'spiral_quality'), 0.10))

    # --- NEG-H5: Composite in void -> zero food (no food to exploit) ---
    checks.append(check_lt(
        'NEG-H5: composite in void food = 0',
        m('neg_h5_composite_void', 'food_eaten'), 1.0))

    # --- NEG-H6: 5-inv composite in void -> zero food ---
    checks.append(check_lt(
        'NEG-H6: 5-inv composite in void food = 0',
        m('neg_h6_composite_void', 'food_eaten'), 1.0))

    # --- NEG-H7: Single inverter at D_ROT=0 -> tight circle, not expanding spiral ---
    # H7 checks spiral_quality > 0.20 at D_ROT=0 for composites.
    # Single inverter makes a perfect circle: tiny area, high circle_fit, no expansion.
    checks.append(check_lt(
        'NEG-H7: single inverter area < 15 at D_ROT=0 (tight circle)',
        m('neg_h7_inverter_void', 'area_cells_visited'), 15.0))
    checks.append(check_gt(
        'NEG-H7: single inverter circle_fit > 0.95 (circle, not spiral)',
        m('neg_h7_inverter_void', 'circle_fit_score'), 0.95))

    # --- NEG-H8: Two stochastic agents -> no spiral quality ordering ---
    # H8 checks 7-inv spiral > 1.3x 3-inv spiral; stochastic agents can't spiral.
    sq_a = m('neg_h8a_stochastic_void', 'spiral_quality')
    sq_b = m('neg_h8b_stochastic_void', 'spiral_quality')
    checks.append(check_lt(
        'NEG-H8: stochastic-a spiral < 0.10',
        sq_a, 0.10))
    checks.append(check_lt(
        'NEG-H8: stochastic-b spiral < 0.10',
        sq_b, 0.10))

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
                           'circle_fit_score', 'straightness', 'area_cells_visited',
                           'spiral_detection_rate']
    elif hypothesis == 'h2':
        display_metrics = ['spiral_quality', 'spiral_growth_rate', 'circle_fit_score',
                           'straightness', 'mean_speed', 'area_cells_visited',
                           'spiral_detection_rate']
    elif hypothesis == 'h3':
        display_metrics = ['pre_food_spiral_quality', 'post_food_revisitation_rate',
                           'revisitation_rate', 'pre_food_area_growth_rate',
                           'post_food_area_growth_rate', 'food_eaten',
                           'area_cells_visited']
    elif hypothesis == 'h4a':
        display_metrics = ['net_diff', 'total_power', 'spiral_quality', 'fcr', 'lcr',
                           'spiral_growth_rate', 'area_cells_visited', 'straightness',
                           'mean_speed']
    elif hypothesis == 'h4b':
        display_metrics = ['spiral_quality', 'fcr', 'lcr', 'spiral_growth_rate',
                           'area_cells_visited', 'circle_fit_score', 'mean_speed',
                           'spiral_detection_rate']
    elif hypothesis == 'h5':
        display_metrics = ['food_eaten', 'area_cells_visited', 'mean_speed',
                           'net_diff', 'revisitation_rate']
    elif hypothesis == 'h6':
        display_metrics = ['food_eaten', 'area_cells_visited', 'mean_speed',
                           'net_diff', 'revisitation_rate']
    elif hypothesis == 'h7':
        display_metrics = ['spiral_quality', 'spiral_growth_rate', 'area_cells_visited',
                           'mean_speed', 'spiral_detection_rate']
    elif hypothesis == 'h8':
        display_metrics = ['spiral_quality', 'spiral_growth_rate', 'area_cells_visited',
                           'mean_speed', 'spiral_detection_rate']
    elif hypothesis == 'h9':
        display_metrics = ['spiral_quality', 'circle_fit_score', 'pre_food_spiral_quality',
                           'food_eaten', 'area_cells_visited', 'mean_speed',
                           'spiral_detection_rate']
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

    # Print 95% CIs for key metrics
    total = n_trials * n_runs if n_trials else n_runs
    if total > 1:
        ci_metrics = [m for m in display_metrics
                      if m in ('spiral_quality', 'food_eaten', 'area_cells_visited', 'mean_speed')]
        if ci_metrics:
            print(f"  95% Confidence Intervals (n={total}):")
            for ck in sorted(config_data.keys()):
                for m in ci_metrics:
                    vals = config_data[ck].get(m, [])
                    stds = config_std[ck].get(m, [])
                    if vals and stds:
                        mean_val = avg(vals)
                        std_val = avg(stds)
                        lo, hi = ci95(mean_val, std_val, total)
                        short = m.replace('area_cells_', 'area_')
                        print(f"    {ck:<28} {short:<16} [{lo:.4f}, {hi:.4f}]")
            print()


# ---------------------------------------------------------------------------
# Registry & main
# ---------------------------------------------------------------------------

VALIDATORS = {
    'h0': ('h0_baseline', validate_h0),
    'h1': ('h1_emergence', validate_h1),
    'h2': ('h2_specificity', validate_h2),
    'h3': ('h3_exploitation', validate_h3),
    'h4a': ('h4a_resonance', validate_h4a),
    'h4b': ('h4b_robustness', validate_h4b),
    'h5': ('h5_symmetry', validate_h5),
    'h6': ('h6_complexity', validate_h6),
    'h7': ('h7_sensitivity', validate_h7),
    'h8': ('h8_reconciliation', validate_h8),
    'h9': ('h9_negative', validate_h9),
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

    if args.hypothesis == 'h8':
        print(f"\n  NOTE: H8 is exploratory/post-hoc, motivated by H6 observations.")
        print(f"        It was not pre-registered as a confirmatory hypothesis.")

    if n_pass == len(checks):
        print(f"\n  >>> {args.hypothesis.upper()} PASSED <<<")
    else:
        print(f"\n  >>> {args.hypothesis.upper()} FAILED ({len(checks) - n_pass} checks) <<<")

    sys.exit(0 if n_pass == len(checks) else 1)


if __name__ == "__main__":
    main()
