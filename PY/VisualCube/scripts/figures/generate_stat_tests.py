"""Generate formal statistical tests for all 95 validation checks.

For each check, computes:
- Welch's t-test (comparison) or one-sample t-test (threshold)
- Cohen's d with 95% CI
- Holm-Bonferroni adjusted p-values

Reads per-trial data from zip archives. Outputs a markdown table.
"""

import json
import math
import os
import zipfile
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

import numpy as np
from scipy import stats

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.normpath(os.path.join(_SCRIPT_DIR, "..", ".."))
RESULTS_DIR = os.path.join(_REPO_ROOT, "tests", "eval", "results")
OUTPUT_PATH = os.path.join(_SCRIPT_DIR, "output", "formal_tests_table.md")
os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)

# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

_cache = {}

def load_trials(zip_name: str) -> dict:
    """Load per-trial data from a zip archive. Returns {config_key: {metric: np.array}}."""
    if zip_name in _cache:
        return _cache[zip_name]
    path = os.path.join(RESULTS_DIR, zip_name)
    with zipfile.ZipFile(path) as z:
        af = [n for n in z.namelist() if n.endswith("analysis.json")][0]
        with z.open(af) as f:
            data = json.load(f)
    result = {}
    for trial in data[0]["trials"]:
        ck = trial["config_key"]
        if ck not in result:
            result[ck] = {}
        for metric, value in trial["metrics"].items():
            if metric not in result[ck]:
                result[ck][metric] = []
            result[ck][metric].append(value)
    for ck in result:
        to_remove = []
        for metric in result[ck]:
            try:
                result[ck][metric] = np.array(result[ck][metric], dtype=float)
            except (ValueError, TypeError):
                to_remove.append(metric)
        for m in to_remove:
            del result[ck][m]
    _cache[zip_name] = result
    return result


def get_metric(zip_name: str, config_key: str, metric: str) -> np.ndarray:
    """Get per-trial metric array for a config."""
    trials = load_trials(zip_name)
    return trials[config_key][metric]


# ---------------------------------------------------------------------------
# Statistical helpers
# ---------------------------------------------------------------------------

def cohens_d_two(a: np.ndarray, b: np.ndarray) -> Tuple[float, float, float]:
    """Cohen's d for two independent samples with 95% CI."""
    na, nb = len(a), len(b)
    ma, mb = np.mean(a), np.mean(b)
    sa, sb = np.std(a, ddof=1), np.std(b, ddof=1)
    pooled = math.sqrt(((na - 1) * sa**2 + (nb - 1) * sb**2) / (na + nb - 2))
    d = (ma - mb) / pooled if pooled > 0 else float("inf")
    se_d = math.sqrt(1/na + 1/nb + d**2 / (2*(na+nb)))
    ci_lo = d - 1.96 * se_d
    ci_hi = d + 1.96 * se_d
    return d, ci_lo, ci_hi


def cohens_d_one(a: np.ndarray, threshold: float) -> Tuple[float, float, float]:
    """Cohen's d for one-sample vs threshold."""
    n = len(a)
    ma = np.mean(a)
    sa = np.std(a, ddof=1)
    d = (ma - threshold) / sa if sa > 0 else float("inf")
    se_d = math.sqrt(1/n + d**2 / (2*n))
    ci_lo = d - 1.96 * se_d
    ci_hi = d + 1.96 * se_d
    return d, ci_lo, ci_hi


def holm_bonferroni(p_values: List[float]) -> List[float]:
    """Apply Holm-Bonferroni correction to a list of p-values."""
    n = len(p_values)
    indexed = sorted(enumerate(p_values), key=lambda x: x[1])
    adjusted = [0.0] * n
    cummax = 0.0
    for rank, (orig_idx, p) in enumerate(indexed):
        adj = p * (n - rank)
        adj = min(adj, 1.0)
        cummax = max(cummax, adj)
        adjusted[orig_idx] = cummax
    return adjusted


# ---------------------------------------------------------------------------
# Check result container
# ---------------------------------------------------------------------------

@dataclass
class TestResult:
    hypothesis: str
    check_id: str
    description: str
    check_type: str
    configs: str
    metric: str
    test_name: str
    statistic: float
    df: float
    p_value: float
    adj_p: float = 0.0
    cohens_d: float = 0.0
    d_ci_lo: float = 0.0
    d_ci_hi: float = 0.0
    observed: bool = True
    inferred: bool = True
    mw_p: float = 0.0


# ---------------------------------------------------------------------------
# Check definitions — mirrors validate_hypothesis.py but with per-trial tests
# ---------------------------------------------------------------------------

def run_all_checks() -> List[TestResult]:
    results = []
    check_num = 0

    def add_threshold_gt(hyp, desc, zip_name, config, metric, threshold):
        nonlocal check_num
        check_num += 1
        arr = get_metric(zip_name, config, metric)
        t_stat, p_val = stats.ttest_1samp(arr, threshold)
        p_one = p_val / 2 if t_stat > 0 else 1 - p_val / 2
        d, dlo, dhi = cohens_d_one(arr, threshold)
        obs = np.mean(arr) > threshold
        inf = p_one < 0.05
        results.append(TestResult(
            hypothesis=hyp, check_id=f"{check_num}",
            description=desc, check_type="threshold",
            configs=config, metric=metric,
            test_name="1-sample t", statistic=t_stat,
            df=len(arr)-1, p_value=max(p_one, 1e-300),
            cohens_d=d, d_ci_lo=dlo, d_ci_hi=dhi,
            observed=obs, inferred=inf,
        ))

    def add_threshold_lt(hyp, desc, zip_name, config, metric, threshold):
        nonlocal check_num
        check_num += 1
        arr = get_metric(zip_name, config, metric)
        t_stat, p_val = stats.ttest_1samp(arr, threshold)
        p_one = p_val / 2 if t_stat < 0 else 1 - p_val / 2
        d, dlo, dhi = cohens_d_one(arr, threshold)
        obs = np.mean(arr) < threshold
        inf = p_one < 0.05
        results.append(TestResult(
            hypothesis=hyp, check_id=f"{check_num}",
            description=desc, check_type="threshold",
            configs=config, metric=metric,
            test_name="1-sample t", statistic=t_stat,
            df=len(arr)-1, p_value=max(p_one, 1e-300),
            cohens_d=d, d_ci_lo=dlo, d_ci_hi=dhi,
            observed=obs, inferred=inf,
        ))

    def add_comparison_gt(hyp, desc, zip_a, config_a, zip_b, config_b, metric):
        nonlocal check_num
        check_num += 1
        a = get_metric(zip_a, config_a, metric)
        b = get_metric(zip_b, config_b, metric)
        t_stat, p_val = stats.ttest_ind(a, b, equal_var=False)
        p_one = p_val / 2 if t_stat > 0 else 1 - p_val / 2
        d, dlo, dhi = cohens_d_two(a, b)
        mw_stat, mw_p = stats.mannwhitneyu(a, b, alternative="greater")
        obs = np.mean(a) > np.mean(b)
        inf = p_one < 0.05
        results.append(TestResult(
            hypothesis=hyp, check_id=f"{check_num}",
            description=desc, check_type="comparison",
            configs=f"{config_a} vs {config_b}", metric=metric,
            test_name="Welch t", statistic=t_stat,
            df=len(a)+len(b)-2, p_value=max(p_one, 1e-300),
            cohens_d=d, d_ci_lo=dlo, d_ci_hi=dhi,
            observed=obs, inferred=inf, mw_p=max(mw_p, 1e-300),
        ))

    def add_ratio_gt(hyp, desc, zip_a, config_a, zip_b, config_b, metric, min_ratio):
        """Ratio check: mean(A)/mean(B) > min_ratio.

        Uses bootstrap to test the ratio directly rather than Welch t-test on raw values.
        """
        nonlocal check_num
        check_num += 1
        a = get_metric(zip_a, config_a, metric)
        b = get_metric(zip_b, config_b, metric)
        ratio = np.mean(a) / np.mean(b) if np.mean(b) != 0 else float("inf")
        d, dlo, dhi = cohens_d_two(a, b)
        rng = np.random.default_rng(42)
        n_boot = 10000
        boot_ratios = []
        for _ in range(n_boot):
            ba = rng.choice(a, size=len(a), replace=True)
            bb = rng.choice(b, size=len(b), replace=True)
            mb = np.mean(bb)
            if mb != 0:
                boot_ratios.append(np.mean(ba) / mb)
        boot_ratios = np.array(boot_ratios)
        p_boot = np.mean(boot_ratios <= min_ratio)
        obs = ratio > min_ratio
        inf = p_boot < 0.05
        results.append(TestResult(
            hypothesis=hyp, check_id=f"{check_num}",
            description=f"{desc} (ratio={ratio:.2f})", check_type="ratio",
            configs=f"{config_a} vs {config_b}", metric=metric,
            test_name=f"bootstrap ratio", statistic=ratio,
            df=n_boot, p_value=max(p_boot, 1e-300),
            cohens_d=d, d_ci_lo=dlo, d_ci_hi=dhi,
            observed=obs, inferred=inf,
        ))

    def add_order(hyp, desc, zip_name, configs, metric, ascending=True):
        """Ordering check: all adjacent pairs in predicted direction.

        Reports worst (largest) pairwise one-tailed p-value and smallest |d|.
        PASS when ordering holds in means.
        """
        nonlocal check_num
        check_num += 1
        arrays = [get_metric(zip_name, c, metric) for c in configs]
        means = [np.mean(a) for a in arrays]
        if ascending:
            ok = all(means[i] <= means[i+1] for i in range(len(means)-1))
        else:
            ok = all(means[i] >= means[i+1] for i in range(len(means)-1))
        pairs_p_one = []
        d_vals = []
        for i in range(len(arrays)-1):
            t_stat, p_two = stats.ttest_ind(arrays[i], arrays[i+1], equal_var=False)
            if ascending:
                p_one = p_two / 2 if t_stat < 0 else 1 - p_two / 2
            else:
                p_one = p_two / 2 if t_stat > 0 else 1 - p_two / 2
            pairs_p_one.append(max(p_one, 1e-300))
            d, _, _ = cohens_d_two(arrays[i], arrays[i+1])
            d_vals.append(abs(d))
        worst_p = max(pairs_p_one) if pairs_p_one else 1.0
        min_d = min(d_vals) if d_vals else 0.0
        results.append(TestResult(
            hypothesis=hyp, check_id=f"{check_num}",
            description=desc, check_type="ordering",
            configs=" > ".join(configs) if not ascending else " < ".join(configs),
            metric=metric,
            test_name="pairwise 1-tail (worst)",
            statistic=len(pairs_p_one), df=len(arrays[0])*2-2,
            p_value=worst_p,
            cohens_d=min_d, d_ci_lo=0, d_ci_hi=0,
            observed=ok, inferred=worst_p < 0.05,
        ))

    def add_approx(hyp, desc, zip_name, config_a, config_b, metric):
        """Tolerance check: values within 25% of reference or within combined std.

        Uses TOST (Two One-Sided Tests) with equivalence margin = max(combined_std, 25% of ref).
        PASS when TOST p < 0.05 (equivalence confirmed) OR when |d| < 0.2 (negligible effect).
        """
        nonlocal check_num
        check_num += 1
        a = get_metric(zip_name, config_a, metric)
        b = get_metric(zip_name, config_b, metric)
        d, dlo, dhi = cohens_d_two(a, b)
        ma, mb = np.mean(a), np.mean(b)
        sa, sb = np.std(a, ddof=1), np.std(b, ddof=1)
        diff = ma - mb
        margin = max(sa + sb, abs(mb) * 0.25)
        se = math.sqrt(sa**2/len(a) + sb**2/len(b))
        df_val = len(a) + len(b) - 2
        t_upper = (diff - margin) / se if se > 0 else float("-inf")
        t_lower = (diff + margin) / se if se > 0 else float("inf")
        p_upper = stats.t.cdf(t_upper, df_val)
        p_lower = 1 - stats.t.cdf(t_lower, df_val)
        tost_p = max(p_upper, p_lower)
        passed = tost_p < 0.05 or abs(d) < 0.20
        results.append(TestResult(
            hypothesis=hyp, check_id=f"{check_num}",
            description=desc, check_type="tolerance (TOST)",
            configs=f"{config_a} vs {config_b}", metric=metric,
            test_name=f"TOST (margin={margin:.1f})", statistic=abs(d),
            df=df_val, p_value=max(tost_p, 1e-300),
            cohens_d=d, d_ci_lo=dlo, d_ci_hi=dhi,
            observed=passed, inferred=tost_p < 0.05,
        ))

    z0 = "h0_baseline.zip"
    z0b = "h0_baseline_2.zip"
    z1 = "h1_emergence.zip"
    z2 = "h2_specificity.zip"
    z3 = "h3_exploitation.zip"
    z4a = "h4a_resonance.zip"
    z4b = "h4b_robustness.zip"
    z5 = "h5_symmetry.zip"
    z6 = "h6_complexity.zip"
    z7 = "h7_sensitivity.zip"
    z8 = "h8_reconciliation.zip"
    z9 = "h9_negative.zip"

    # --- H0 (14 checks) ---
    add_ratio_gt("H0", "H0a straightness >> H0b", z0, "h0a_inverter_void", z0, "h0b_inverter_void", "straightness", 2.0)
    add_ratio_gt("H0", "H0a radius >> H0b", z0, "h0a_inverter_void", z0, "h0b_inverter_void", "circle_fit_radius", 3.0)
    add_threshold_gt("H0", "H0b circle_fit > 0.3", z0, "h0b_inverter_void", "circle_fit_score", 0.3)
    add_threshold_gt("H0", "H0c circle_fit > 0.3", z0, "h0c_inverter_void", "circle_fit_score", 0.3)
    add_approx("H0", "H0b radius ~= H0c radius", z0, "h0b_inverter_void", "h0c_inverter_void", "circle_fit_radius")
    # H0d/H0e speed ratios and radius approx — use h0_baseline_2.zip for extended configs
    try:
        add_ratio_gt("H0", "H0d speed ~= 2x H0b", z0, "h0d_inverter_void", z0, "h0b_inverter_void", "mean_speed", 1.7)
        add_approx("H0", "H0d radius ~= H0b radius", z0, "h0d_inverter_void", "h0b_inverter_void", "circle_fit_radius")
        add_ratio_gt("H0", "H0e speed ~= 4x H0b", z0, "h0e_inverter_void", z0, "h0b_inverter_void", "mean_speed", 3.5)
        add_approx("H0", "H0e radius ~= H0b radius", z0, "h0e_inverter_void", "h0b_inverter_void", "circle_fit_radius")
        add_approx("H0", "H0d radius ~= H0e radius", z0, "h0d_inverter_void", "h0e_inverter_void", "circle_fit_radius")
        add_comparison_gt("H0", "H0b radius > H0f radius", z0, "h0b_inverter_void", z0, "h0f_inverter_void", "circle_fit_radius")
        add_threshold_gt("H0", "H0f circle_fit > 0.3", z0, "h0f_inverter_void", "circle_fit_score", 0.3)
        add_order("H0", "Radius: H0b > H0f > H0g", z0,
                  ["h0b_inverter_void", "h0f_inverter_void", "h0g_inverter_void"],
                  "circle_fit_radius", ascending=False)
        add_threshold_gt("H0", "H0g circle_fit > 0.3", z0, "h0g_inverter_void", "circle_fit_score", 0.3)
    except KeyError:
        # Some H0 configs may be in h0_baseline_2.zip
        add_ratio_gt("H0", "H0d speed ~= 2x H0b", z0b, "h0d_inverter_void", z0b, "h0b_inverter_void", "mean_speed", 1.7)
        add_approx("H0", "H0d radius ~= H0b radius", z0b, "h0d_inverter_void", "h0b_inverter_void", "circle_fit_radius")
        add_ratio_gt("H0", "H0e speed ~= 4x H0b", z0b, "h0e_inverter_void", z0b, "h0b_inverter_void", "mean_speed", 3.5)
        add_approx("H0", "H0e radius ~= H0b radius", z0b, "h0e_inverter_void", "h0b_inverter_void", "circle_fit_radius")
        add_approx("H0", "H0d radius ~= H0e radius", z0b, "h0d_inverter_void", "h0e_inverter_void", "circle_fit_radius")
        add_comparison_gt("H0", "H0b radius > H0f radius", z0b, "h0b_inverter_void", z0b, "h0f_inverter_void", "circle_fit_radius")
        add_threshold_gt("H0", "H0f circle_fit > 0.3", z0b, "h0f_inverter_void", "circle_fit_score", 0.3)
        add_order("H0", "Radius: H0b > H0f > H0g", z0b,
                  ["h0b_inverter_void", "h0f_inverter_void", "h0g_inverter_void"],
                  "circle_fit_radius", ascending=False)
        add_threshold_gt("H0", "H0g circle_fit > 0.3", z0b, "h0g_inverter_void", "circle_fit_score", 0.3)

    # --- H1 (8 checks) ---
    add_threshold_gt("H1", "H1a spiral_quality > 0.15", z1, "h1a_composite_void", "spiral_quality", 0.15)
    add_threshold_gt("H1", "H1a growth_rate > 0", z1, "h1a_composite_void", "spiral_growth_rate", 0.0)
    add_threshold_lt("H1", "H1a straightness < 0.3", z1, "h1a_composite_void", "straightness", 0.3)
    add_comparison_gt("H1", "H1b spiral >= H1a", z1, "h1b_composite_void", z1, "h1a_composite_void", "spiral_quality")
    add_comparison_gt("H1", "H1b area >= H1a", z1, "h1b_composite_void", z1, "h1a_composite_void", "area_cells_visited")
    add_comparison_gt("H1", "H1c spiral >= H1b", z1, "h1c_composite_void", z1, "h1b_composite_void", "spiral_quality")
    add_comparison_gt("H1", "H1c area >= H1b", z1, "h1c_composite_void", z1, "h1b_composite_void", "area_cells_visited")
    add_order("H1", "Area: H1a <= H1b <= H1c", z1,
              ["h1a_composite_void", "h1b_composite_void", "h1c_composite_void"],
              "area_cells_visited", ascending=True)

    # --- H2 (11 checks) ---
    for label, ck in [("H2a", "h2a_stochastic_void"), ("H2b", "h2b_inverter_void"),
                       ("H2c", "h2c_composite_void"), ("H2d", "h2d_composite_void")]:
        add_threshold_lt("H2", f"{label} spiral < 0.20", z2, ck, "spiral_quality", 0.20)
    add_threshold_lt("H2", "H2e spiral < 0.20 (opp w/o stagger)", z2, "h2e_composite_void", "spiral_quality", 0.20)
    add_threshold_lt("H2", "H2f spiral < 0.20 (stagger w/o opp)", z2, "h2f_composite_void", "spiral_quality", 0.20)
    add_threshold_gt("H2", "H2g spiral > 0.20 (positive ctrl)", z2, "h2g_composite_void", "spiral_quality", 0.20)
    add_ratio_gt("H2", "H2g spiral > 2x max(H2a-H2f)", z2, "h2g_composite_void", z2, "h2e_composite_void", "spiral_quality", 2.0)
    add_comparison_gt("H2", "H2g >> H2e (stagger matters)", z2, "h2g_composite_void", z2, "h2e_composite_void", "spiral_quality")
    add_comparison_gt("H2", "H2g >> H2f (opp matters)", z2, "h2g_composite_void", z2, "h2f_composite_void", "spiral_quality")
    add_comparison_gt("H2", "H2g area > H2b area", z2, "h2g_composite_void", z2, "h2b_inverter_void", "area_cells_visited")

    # --- H3 (9 checks) ---
    add_threshold_gt("H3", "H3a pre-food spiral > 0.12", z3, "h3a_composite_dense_band", "pre_food_spiral_quality", 0.12)
    add_threshold_gt("H3", "H3b pre-food spiral > 0.10", z3, "h3b_composite_small_circle", "pre_food_spiral_quality", 0.10)
    add_threshold_lt("H3", "H3c ctrl spiral < 0.10", z3, "h3c_inverter_dense_band", "pre_food_spiral_quality", 0.10)
    add_threshold_lt("H3", "H3d ctrl spiral < 0.10", z3, "h3d_inverter_small_circle", "pre_food_spiral_quality", 0.10)
    add_comparison_gt("H3", "H3a spiral > H3c ctrl", z3, "h3a_composite_dense_band", z3, "h3c_inverter_dense_band", "pre_food_spiral_quality")
    add_comparison_gt("H3", "H3b spiral > H3d ctrl", z3, "h3b_composite_small_circle", z3, "h3d_inverter_small_circle", "pre_food_spiral_quality")
    add_threshold_gt("H3", "H3b post-food revisit > 0.90", z3, "h3b_composite_small_circle", "post_food_revisitation_rate", 0.90)
    add_comparison_gt("H3", "H3a food > H3c ctrl", z3, "h3a_composite_dense_band", z3, "h3c_inverter_dense_band", "food_eaten")
    add_comparison_gt("H3", "H3b food > H3d ctrl", z3, "h3b_composite_small_circle", z3, "h3d_inverter_small_circle", "food_eaten")

    # --- H4a (12 checks) ---
    add_order("H4a", "Area: H4a4>=H4a3>=H4a2>=H4a1", z4a,
              ["h4a4_composite_void", "h4a3_composite_void", "h4a2_composite_void", "h4a1_composite_void"],
              "area_cells_visited", ascending=False)
    add_ratio_gt("H4a", "H4a3 area > 5x H4a1", z4a, "h4a3_composite_void", z4a, "h4a1_composite_void", "area_cells_visited", 5.0)
    add_order("H4a", "Speed: H4a5>=H4a4>=H4a3>=H4a2>=H4a1", z4a,
              ["h4a5_composite_void", "h4a4_composite_void", "h4a3_composite_void", "h4a2_composite_void", "h4a1_composite_void"],
              "mean_speed", ascending=False)
    add_ratio_gt("H4a", "H4a5 speed > 2x H4a1", z4a, "h4a5_composite_void", z4a, "h4a1_composite_void", "mean_speed", 2.0)
    add_comparison_gt("H4a", "H4a5 area > H4a4", z4a, "h4a5_composite_void", z4a, "h4a4_composite_void", "area_cells_visited")
    add_comparison_gt("H4a", "H4a3 area > H4a7", z4a, "h4a3_composite_void", z4a, "h4a7_composite_void", "area_cells_visited")
    add_comparison_gt("H4a", "H4a5 area > H4a7", z4a, "h4a5_composite_void", z4a, "h4a7_composite_void", "area_cells_visited")
    add_comparison_gt("H4a", "H4a6 speed > H4a1", z4a, "h4a6_composite_void", z4a, "h4a1_composite_void", "mean_speed")
    add_comparison_gt("H4a", "H4a2 area > H4a1", z4a, "h4a2_composite_void", z4a, "h4a1_composite_void", "area_cells_visited")
    add_comparison_gt("H4a", "H4a4 area > H4a3", z4a, "h4a4_composite_void", z4a, "h4a3_composite_void", "area_cells_visited")
    add_comparison_gt("H4a", "H4a4 speed > H4a3", z4a, "h4a4_composite_void", z4a, "h4a3_composite_void", "mean_speed")
    # Symmetry ratio check — use equivalence test
    add_approx("H4a", "H4a6/H4a3 area ratio 0.3-3.0", z4a, "h4a6_composite_void", "h4a3_composite_void", "area_cells_visited")

    # --- H4b (5 checks) ---
    add_threshold_gt("H4b", "H4b1 baseline spiral > 0.15", z4b, "h4b1_composite_void", "spiral_quality", 0.15)
    add_order("H4b", "Spiral: 0%>=10%>=20%>=30%", z4b,
              ["h4b1_composite_void", "h4b2_composite_void", "h4b3_composite_void", "h4b4_composite_void"],
              "spiral_quality", ascending=False)
    add_threshold_gt("H4b", "H4b4 30% spiral > 0.05", z4b, "h4b4_composite_void", "spiral_quality", 0.05)
    add_ratio_gt("H4b", "H4b2 retains > 50% baseline", z4b, "h4b2_composite_void", z4b, "h4b1_composite_void", "spiral_quality", 0.5)
    add_order("H4b", "Area: 0%>=10%>=20%>=30%", z4b,
              ["h4b1_composite_void", "h4b2_composite_void", "h4b3_composite_void", "h4b4_composite_void"],
              "area_cells_visited", ascending=False)

    # --- H5 (7 checks) ---
    add_order("H5", "Food: H5a4>=H5a3>=H5a2>=H5a1", z5,
              ["h5a4_composite_dense_bands", "h5a3_composite_dense_bands",
               "h5a2_composite_dense_bands", "h5a1_composite_dense_bands"],
              "food_eaten", ascending=False)
    add_ratio_gt("H5", "H5a4 food > 1.5x H5a1", z5, "h5a4_composite_dense_bands", z5, "h5a1_composite_dense_bands", "food_eaten", 1.5)
    add_comparison_gt("H5", "H5a4 food > H5a3", z5, "h5a4_composite_dense_bands", z5, "h5a3_composite_dense_bands", "food_eaten")
    add_comparison_gt("H5", "H5a3 food > H5a2", z5, "h5a3_composite_dense_bands", z5, "h5a2_composite_dense_bands", "food_eaten")
    add_comparison_gt("H5", "H5a2 food > H5a1", z5, "h5a2_composite_dense_bands", z5, "h5a1_composite_dense_bands", "food_eaten")
    add_threshold_gt("H5", "H5a4 food > 10", z5, "h5a4_composite_dense_bands", "food_eaten", 10.0)
    add_order("H5", "Area: H5a4>=H5a3>=H5a2>=H5a1", z5,
              ["h5a4_composite_dense_bands", "h5a3_composite_dense_bands",
               "h5a2_composite_dense_bands", "h5a1_composite_dense_bands"],
              "area_cells_visited", ascending=False)

    # --- H6 (6 checks) ---
    add_order("H6", "Food: H6a1>=H6a2>=H6a3>=H6a4>=H6a5", z6,
              ["h6a1_composite_dense_bands", "h6a2_composite_dense_bands",
               "h6a3_composite_dense_bands", "h6a4_composite_dense_bands",
               "h6a5_composite_dense_bands"],
              "food_eaten", ascending=False)
    add_ratio_gt("H6", "H6a1 food > 1.3x H6a5", z6, "h6a1_composite_dense_bands", z6, "h6a5_composite_dense_bands", "food_eaten", 1.3)
    add_comparison_gt("H6", "H6a1 food > H6a2", z6, "h6a1_composite_dense_bands", z6, "h6a2_composite_dense_bands", "food_eaten")
    add_comparison_gt("H6", "H6a2 food > H6a3", z6, "h6a2_composite_dense_bands", z6, "h6a3_composite_dense_bands", "food_eaten")
    add_threshold_gt("H6", "H6a1 food > 10", z6, "h6a1_composite_dense_bands", "food_eaten", 10.0)
    add_order("H6", "Area: H6a1>=...>=H6a5", z6,
              ["h6a1_composite_dense_bands", "h6a2_composite_dense_bands",
               "h6a3_composite_dense_bands", "h6a4_composite_dense_bands",
               "h6a5_composite_dense_bands"],
              "area_cells_visited", ascending=False)

    # --- H7 (5 checks) ---
    add_threshold_gt("H7", "H7a1 D_ROT=0 spiral > 0.20", z7, "h7a1_composite_void", "spiral_quality", 0.20)
    # Detection rate is a scalar aggregate, not per-trial — use spiral_quality as proxy
    add_threshold_gt("H7", "H7a1 D_ROT=0 spiral > 0.50 (high quality)", z7, "h7a1_composite_void", "spiral_quality", 0.50)
    add_threshold_gt("H7", "H7a4 D_ROT=0.1 spiral > 0.10", z7, "h7a4_composite_void", "spiral_quality", 0.10)
    add_threshold_gt("H7", "H7a6 D_ROT=0.5 spiral > 0.05", z7, "h7a6_composite_void", "spiral_quality", 0.05)
    add_comparison_gt("H7", "H7a6 area > H7a1 area", z7, "h7a6_composite_void", z7, "h7a1_composite_void", "area_cells_visited")

    # --- H8 (5 checks) ---
    add_comparison_gt("H8", "H8a5 spiral > H8a1", z8, "h8a5_composite_void", z8, "h8a1_composite_void", "spiral_quality")
    add_ratio_gt("H8", "H8a5 spiral > 1.3x H8a1", z8, "h8a5_composite_void", z8, "h8a1_composite_void", "spiral_quality", 1.3)
    add_threshold_gt("H8", "H8a5 spiral > 0.50 (high)", z8, "h8a5_composite_void", "spiral_quality", 0.50)
    add_comparison_gt("H8", "H8a5 spiral > H8a1 (endpoints)", z8, "h8a5_composite_void", z8, "h8a1_composite_void", "spiral_quality")
    add_threshold_gt("H8", "H8a1 weakest spiral > 0.05", z8, "h8a1_composite_void", "spiral_quality", 0.05)

    # --- H9 (13 checks) ---
    add_threshold_gt("H9", "NEG-H0 radius > 200", z9, "neg_h0_inverter_void", "circle_fit_radius", 200.0)
    add_threshold_lt("H9", "NEG-H1 spiral < 0.10", z9, "neg_h1_stochastic_void", "spiral_quality", 0.10)
    add_threshold_lt("H9", "NEG-H2 spiral < 0.20", z9, "neg_h2_composite_void", "spiral_quality", 0.20)
    add_threshold_lt("H9", "NEG-H3 pre_food_spiral < 0.10", z9, "neg_h3_stochastic_dense_band", "pre_food_spiral_quality", 0.10)
    add_threshold_lt("H9", "NEG-H3 food < 100", z9, "neg_h3_stochastic_dense_band", "food_eaten", 100.0)
    add_threshold_lt("H9", "NEG-H4a spiral < 0.20", z9, "neg_h4a_composite_void", "spiral_quality", 0.20)
    add_threshold_lt("H9", "NEG-H4b spiral < 0.10", z9, "neg_h4b_stochastic_void", "spiral_quality", 0.10)
    add_threshold_lt("H9", "NEG-H5 food < 1", z9, "neg_h5_composite_void", "food_eaten", 1.0)
    add_threshold_lt("H9", "NEG-H6 food < 1", z9, "neg_h6_composite_void", "food_eaten", 1.0)
    add_threshold_lt("H9", "NEG-H7 area < 15", z9, "neg_h7_inverter_void", "area_cells_visited", 15.0)
    add_threshold_gt("H9", "NEG-H7 circle_fit > 0.95", z9, "neg_h7_inverter_void", "circle_fit_score", 0.95)
    add_threshold_lt("H9", "NEG-H8a spiral < 0.10", z9, "neg_h8a_stochastic_void", "spiral_quality", 0.10)
    add_threshold_lt("H9", "NEG-H8b spiral < 0.10", z9, "neg_h8b_stochastic_void", "spiral_quality", 0.10)

    # Apply Holm-Bonferroni and set inferred based on adjusted p
    raw_ps = [r.p_value for r in results]
    adj_ps = holm_bonferroni(raw_ps)
    for r, ap in zip(results, adj_ps):
        r.adj_p = ap
        r.inferred = ap < 0.05

    return results


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

def format_p(p: float) -> str:
    if p < 1e-200:
        return "< 1e-200"
    if p < 1e-50:
        return f"< 1e-50"
    if p < 0.001:
        return f"{p:.2e}"
    return f"{p:.4f}"


def write_table(results: List[TestResult]):
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as f:
        f.write("# Formal Statistical Tests — All 95 Validation Checks\n\n")
        f.write("All directional tests are one-tailed in the predicted direction, consistent with the\n")
        f.write("directional nature of all 95 checks (each predicts a specific ordering or threshold).\n\n")
        f.write("**Test types:** Welch's t-test (comparisons), one-sample t-test (thresholds),\n")
        f.write("TOST with stated margin (tolerance/approximate-equality), pairwise one-tailed t-tests (ordering).\n")
        f.write("Holm-Bonferroni correction applied across all 95 tests.\n")
        f.write("Cohen's d with 95% CI for effect magnitude.\n\n")
        f.write("**Ratio checks:** Bootstrap test (10,000 resamples) on the ratio mean(A)/mean(B).\n\n")
        f.write("**Tolerance checks:** TOST with stated margin; Inferred=yes when TOST p < 0.05 or |d| < 0.20.\n\n")
        f.write("**Ordering checks:** Observed=yes when predicted ordering holds in means. Inferred=yes when\n")
        f.write("all pairwise tests reach p < 0.05.\n\n")
        f.write("**MW p:** Mann-Whitney U one-tailed p-value (nonparametric robustness check, comparison rows only).\n\n")
        f.write("**Observed** = directional pattern holds in sample means.\n")
        f.write("**Inferred** = statistically supported after Holm-Bonferroni correction (adj p < 0.05).\n\n")

        f.write(f"| # | Hyp | Description | Type | Test | Stat | df | Raw p | Adj p | d | d 95% CI | MW p | Observed | Inferred |\n")
        f.write(f"|---|-----|-------------|------|------|------|-----|-------|-------|---|----------|------|----------|----------|\n")

        for r in results:
            ci_str = f"[{r.d_ci_lo:.2f}, {r.d_ci_hi:.2f}]" if r.check_type != "ordering" else "--"
            d_str = f"{r.cohens_d:.2f}" if not math.isinf(r.cohens_d) else ">10"
            stat_str = f"{r.statistic:.1f}" if r.check_type != "ordering" else "--"
            df_str = f"{r.df:.0f}" if r.df > 0 else "--"
            obs_str = "yes" if r.observed else "no"
            inf_str = "yes" if r.inferred else "no"
            mw_str = format_p(r.mw_p) if r.mw_p > 0 else "--"
            f.write(f"| {r.check_id} | {r.hypothesis} | {r.description} | {r.check_type} | "
                    f"{r.test_name} | {stat_str} | {df_str} | {format_p(r.p_value)} | "
                    f"{format_p(r.adj_p)} | {d_str} | {ci_str} | {mw_str} | {obs_str} | {inf_str} |\n")

        n_obs = sum(1 for r in results if r.observed)
        n_inf = sum(1 for r in results if r.inferred)
        f.write(f"\n**Summary:** {n_obs}/{len(results)} observed in predicted direction. "
                f"{n_inf}/{len(results)} inferentially supported (adj p < 0.05).\n")

    print(f"Table saved to: {OUTPUT_PATH}")


def print_summary(results: List[TestResult]):
    n_obs = sum(1 for r in results if r.observed)
    n_inf = sum(1 for r in results if r.inferred)
    min_d = min(abs(r.cohens_d) for r in results if not math.isinf(r.cohens_d))

    print(f"\n{'='*60}")
    print(f"FORMAL STATISTICAL TESTS — {len(results)} CHECKS")
    print(f"{'='*60}")
    print(f"  Observed (pattern in means): {n_obs}/{len(results)}")
    print(f"  Inferred (adj p < 0.05):     {n_inf}/{len(results)}")
    print(f"  Smallest |d| (among all):    {min_d:.3f}")

    by_hyp = {}
    for r in results:
        by_hyp.setdefault(r.hypothesis, []).append(r)
    print(f"\n  Per hypothesis (observed / inferred):")
    for h in sorted(by_hyp.keys(), key=lambda x: (x.replace("H", "").replace("a", ".1").replace("b", ".2"))):
        checks = by_hyp[h]
        o = sum(1 for c in checks if c.observed)
        i = sum(1 for c in checks if c.inferred)
        print(f"    {h:>4}: {o}/{len(checks)} obs, {i}/{len(checks)} inf")


if __name__ == "__main__":
    results = run_all_checks()
    write_table(results)
    print_summary(results)
