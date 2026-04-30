"""Generate convergence analysis plot (Supplementary Figure S1).

Shows that standard error of the mean decreases as 1/sqrt(N),
and that 95% CI half-width drops below 5% of the mean well before N=1000.
"""

import json
import os
import zipfile
import numpy as np
import matplotlib.pyplot as plt

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.normpath(os.path.join(_SCRIPT_DIR, "..", ".."))
RESULTS_DIR = os.path.join(_REPO_ROOT, "tests", "eval", "results")
OUTPUT_PATH = os.path.join(_SCRIPT_DIR, "output", "convergence_analysis.png")
os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)

SOURCES = [
    {
        "zip": f"{RESULTS_DIR}/h2_specificity.zip",
        "config_key": "h2g_composite_void",
        "metric": "spiral_quality",
        "label": "Spiral quality (H2g)",
    },
    {
        "zip": f"{RESULTS_DIR}/h3_exploitation.zip",
        "config_key": "h3a_composite_dense_band",
        "metric": "food_eaten",
        "label": "Food eaten (H3a)",
    },
]

SAMPLE_SIZES = [25, 50, 100, 200, 300, 500, 750, 1000]


def load_metric_values(zip_path, config_key, metric):
    with zipfile.ZipFile(zip_path) as z:
        analysis_file = [n for n in z.namelist() if n.endswith("analysis.json")][0]
        with z.open(analysis_file) as f:
            data = json.load(f)
    values = [
        t["metrics"][metric]
        for t in data[0]["trials"]
        if t["config_key"] == config_key
    ]
    return np.array(values, dtype=float)


def compute_convergence(values):
    ses, ci_pcts = [], []
    full_mean = np.mean(values)
    for n in SAMPLE_SIZES:
        subset = values[:n]
        se = np.std(subset, ddof=1) / np.sqrt(n)
        ci_half = 1.96 * se
        ci_pct = 100.0 * ci_half / full_mean if full_mean > 0 else 0.0
        ses.append(se)
        ci_pcts.append(ci_pct)
    return ses, ci_pcts


def main():
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

    colors = ["#2166ac", "#b2182b"]
    markers = ["o", "s"]

    for i, src in enumerate(SOURCES):
        values = load_metric_values(src["zip"], src["config_key"], src["metric"])
        ses, ci_pcts = compute_convergence(values)

        ax1.plot(SAMPLE_SIZES, ses, f"-{markers[i]}", color=colors[i],
                 label=src["label"], markersize=6, linewidth=1.5)
        ax2.plot(SAMPLE_SIZES, ci_pcts, f"-{markers[i]}", color=colors[i],
                 label=src["label"], markersize=6, linewidth=1.5)

    # Reference 1/sqrt(N) line on ax1 (scaled to match spiral_quality SE at N=25)
    values_ref = load_metric_values(SOURCES[0]["zip"], SOURCES[0]["config_key"],
                                    SOURCES[0]["metric"])
    se_ref = np.std(values_ref[:25], ddof=1) / np.sqrt(25)
    ref_line = [se_ref * np.sqrt(25) / np.sqrt(n) for n in SAMPLE_SIZES]
    ax1.plot(SAMPLE_SIZES, ref_line, "--", color="gray", alpha=0.5,
             label=r"$1/\sqrt{N}$ reference", linewidth=1)

    ax1.set_xscale("log")
    ax1.set_yscale("log")
    ax1.set_xlabel("Sample size (N)")
    ax1.set_ylabel("Standard error of the mean")
    ax1.set_title("a) SE convergence")
    ax1.legend(fontsize=9)
    ax1.grid(True, alpha=0.3)

    ax2.axhline(y=5, color="gray", linestyle="--", alpha=0.5, label="5% threshold")
    ax2.set_xlabel("Sample size (N)")
    ax2.set_ylabel("95% CI half-width (% of mean)")
    ax2.set_title("b) CI precision")
    ax2.legend(fontsize=9)
    ax2.grid(True, alpha=0.3)

    fig.suptitle("Convergence Analysis: SE and CI Precision vs. Sample Size",
                 fontsize=13, fontweight="bold")
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    fig.savefig(OUTPUT_PATH, dpi=200, bbox_inches="tight")
    print(f"Saved to: {OUTPUT_PATH}")

    # Print summary table
    for src in SOURCES:
        values = load_metric_values(src["zip"], src["config_key"], src["metric"])
        ses, ci_pcts = compute_convergence(values)
        print(f"\n{src['label']}  (mean={np.mean(values):.3f}, std={np.std(values, ddof=1):.3f})")
        print(f"  {'N':>6}  {'SE':>10}  {'CI half':>10}  {'CI %':>8}")
        for n, se, pct in zip(SAMPLE_SIZES, ses, ci_pcts):
            print(f"  {n:>6}  {se:>10.4f}  {1.96*se:>10.4f}  {pct:>7.2f}%")


if __name__ == "__main__":
    main()
