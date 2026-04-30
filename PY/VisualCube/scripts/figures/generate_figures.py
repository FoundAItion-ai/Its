"""Generate publication figures 2, 3, 6 from hypothesis eval ZIP archives."""

import json
import os
import zipfile

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.normpath(os.path.join(_SCRIPT_DIR, "..", ".."))
RESULTS_DIR = os.path.join(_REPO_ROOT, "tests", "eval", "results")
FIGS_DIR = os.path.join(_SCRIPT_DIR, "output")
os.makedirs(FIGS_DIR, exist_ok=True)

COLORS = {
    "primary": "#1a3c78",
    "accent": "#c44e52",
    "positive": "#4c8c2b",
    "neutral": "#8c8c8c",
}

plt.rcParams.update({
    "font.size": 10,
    "axes.titlesize": 12,
    "axes.labelsize": 10,
    "xtick.labelsize": 9,
    "ytick.labelsize": 9,
    "figure.dpi": 200,
    "savefig.dpi": 200,
    "axes.spines.top": False,
    "axes.spines.right": False,
})


def load_aggregates(zip_name):
    """Load aggregated metrics from analysis.json inside a ZIP archive.

    Returns dict: {config_key: {metric_name: mean_value}}
    """
    path = os.path.join(RESULTS_DIR, zip_name)
    with zipfile.ZipFile(path) as z:
        af = [n for n in z.namelist() if n.endswith("analysis.json")][0]
        with z.open(af) as f:
            data = json.load(f)
    result = {}
    for config_key, agg in data[0].get("aggregates", {}).items():
        metrics = {}
        for metric_name, metric_data in agg.items():
            if isinstance(metric_data, dict) and "mean" in metric_data:
                metrics[metric_name] = metric_data["mean"]
            elif isinstance(metric_data, (int, float)):
                metrics[metric_name] = metric_data
        result[config_key] = metrics
    return result


# ---------------------------------------------------------------------------
# Figure 2: H0 Baseline — Radius vs Frequency Ratio
# ---------------------------------------------------------------------------
def figure_02():
    agg = load_aggregates("h0_baseline.zip")

    configs = ["h0b_inverter_void", "h0f_inverter_void", "h0g_inverter_void"]
    labels = ["1.11\n(standard)", "1.50\n(wider)", "2.00\n(widest)"]
    radii = [agg[c]["circle_fit_radius"] for c in configs]

    speed_cfgs = ["h0b_inverter_void", "h0d_inverter_void", "h0e_inverter_void"]
    speed_labels = ["1x\n(70 Hz)", "2x\n(140 Hz)", "4x\n(281 Hz)"]
    speed_vals = [agg[c]["mean_speed"] for c in speed_cfgs]
    speed_radii = [agg[c]["circle_fit_radius"] for c in speed_cfgs]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(8.5, 3.5), gridspec_kw={"width_ratios": [3, 2]})

    bars = ax1.bar(labels, radii, color=[COLORS["primary"], COLORS["accent"], COLORS["accent"]],
                   width=0.55, edgecolor="white", linewidth=0.5)
    ax1.set_ylabel("Circle Fit Radius (px)")
    ax1.set_xlabel("L/R Frequency Ratio")
    ax1.set_title("a) Circle radius tightens with frequency ratio")
    for bar, val in zip(bars, radii):
        ax1.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.8,
                 f"{val:.1f}", ha="center", va="bottom", fontsize=9)

    ax2_bars = ax2.bar(speed_labels, speed_vals, color=COLORS["primary"],
                       width=0.55, edgecolor="white", linewidth=0.5)
    ax2.set_ylabel("Mean Speed (px/s)")
    ax2.set_xlabel("Frequency Multiplier")
    ax2.set_title("b) Speed scales linearly; radius constant")
    for bar, sv, rv in zip(ax2_bars, speed_vals, speed_radii):
        ax2.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 3,
                 f"{sv:.0f} px/s\nr={rv:.1f}", ha="center", va="bottom", fontsize=8)

    fig.tight_layout(w_pad=3)
    out = os.path.join(FIGS_DIR, "figure_02_baseline.png")
    fig.savefig(out)
    plt.close(fig)
    print(f"  Saved: {out}")


# ---------------------------------------------------------------------------
# Figure 3: H2 Ablation — Spiral Quality Bar Chart
# ---------------------------------------------------------------------------
def figure_03():
    agg = load_aggregates("h2_specificity.zip")

    configs = [
        "h2a_stochastic_void", "h2b_inverter_void", "h2c_composite_void",
        "h2d_composite_void", "h2e_composite_void", "h2f_composite_void",
        "h2g_composite_void",
    ]
    labels = [
        "H2a\nStochastic",
        "H2b\n1 inverter",
        "H2c\n2-inv (N+C)",
        "H2d\n2-inv (N+N)\nno opposition",
        "H2e\nSame C1\nno staggering",
        "H2f\nAll normal\nno opposition",
        "H2g\nCorrect\n(both)",
    ]
    qualities = [agg[c]["spiral_quality"] for c in configs]
    colors = [COLORS["neutral"]] * 6 + [COLORS["positive"]]

    fig, ax = plt.subplots(figsize=(8, 4))
    bars = ax.barh(range(len(labels)), qualities, color=colors, height=0.6,
                   edgecolor="white", linewidth=0.5)
    ax.set_yticks(range(len(labels)))
    ax.set_yticklabels(labels, fontsize=8.5)
    ax.set_xlabel("Spiral Quality")
    ax.set_title("H2 Ablation: Both opposition AND staggering required for spiral")
    ax.invert_yaxis()

    ax.axvline(x=0.20, color=COLORS["accent"], linestyle="--", linewidth=1.2, alpha=0.8)
    ax.text(0.205, -0.3, "threshold = 0.20", color=COLORS["accent"], fontsize=8, va="top")

    for bar, val in zip(bars, qualities):
        x = bar.get_width()
        ax.text(x + 0.008, bar.get_y() + bar.get_height() / 2,
                f"{val:.3f}", va="center", fontsize=8.5,
                fontweight="bold" if val > 0.20 else "normal")

    ax.annotate("3.1x gap", xy=(qualities[-1], 6), xytext=(0.30, 4),
                fontsize=9, fontweight="bold", color=COLORS["primary"],
                arrowprops=dict(arrowstyle="->", color=COLORS["primary"], lw=1.2))

    fig.tight_layout()
    out = os.path.join(FIGS_DIR, "figure_03_ablation.png")
    fig.savefig(out)
    plt.close(fig)
    print(f"  Saved: {out}")


# ---------------------------------------------------------------------------
# Figure 6: H6 vs H8 — Quality-Efficiency Paradox
# ---------------------------------------------------------------------------
def figure_06():
    h6_agg = load_aggregates("h6_complexity.zip")
    h8_agg = load_aggregates("h8_reconciliation.zip")

    h6_configs = ["h6a1_composite_dense_bands", "h6a2_composite_dense_bands",
                  "h6a3_composite_dense_bands", "h6a4_composite_dense_bands",
                  "h6a5_composite_dense_bands"]
    h8_configs = ["h8a1_composite_void", "h8a2_composite_void",
                  "h8a3_composite_void", "h8a4_composite_void",
                  "h8a5_composite_void"]

    inv_counts = [3, 4, 5, 6, 7]
    food = [h6_agg[c]["food_eaten"] for c in h6_configs]
    quality = [h8_agg[c]["spiral_quality"] for c in h8_configs]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(9, 3.8))

    ax1.plot(inv_counts, food, "o-", color=COLORS["accent"], linewidth=2, markersize=7,
             markerfacecolor="white", markeredgewidth=2)
    ax1.fill_between(inv_counts, food, alpha=0.08, color=COLORS["accent"])
    ax1.set_xlabel("Number of Inverters")
    ax1.set_ylabel("Food Eaten (60s)")
    ax1.set_title("a) H6: More inverters = less food")
    ax1.set_xticks(inv_counts)
    ax1.set_ylim(450, 800)
    for x, y in zip(inv_counts, food):
        ax1.annotate(f"{y:.0f}", (x, y), textcoords="offset points",
                     xytext=(0, 10), ha="center", fontsize=8.5)

    ax1.annotate("", xy=(6.5, 540), xytext=(3.5, 740),
                 arrowprops=dict(arrowstyle="-|>", color=COLORS["accent"],
                                 lw=2, ls="--", mutation_scale=15))
    ax1.text(5.2, 660, "41% decline", fontsize=9, color=COLORS["accent"],
             fontweight="bold", ha="center", rotation=-25)

    ax2.plot(inv_counts, quality, "s-", color=COLORS["primary"], linewidth=2, markersize=7,
             markerfacecolor="white", markeredgewidth=2)
    ax2.fill_between(inv_counts, quality, alpha=0.08, color=COLORS["primary"])
    ax2.set_xlabel("Number of Inverters")
    ax2.set_ylabel("Spiral Quality")
    ax2.set_title("b) H8: More inverters = better spiral")
    ax2.set_xticks(inv_counts)
    ax2.set_ylim(0.15, 0.85)
    for x, y in zip(inv_counts, quality):
        ax2.annotate(f"{y:.3f}", (x, y), textcoords="offset points",
                     xytext=(0, 10), ha="center", fontsize=8.5)

    ax2.annotate("", xy=(6.5, 0.71), xytext=(3.5, 0.28),
                 arrowprops=dict(arrowstyle="-|>", color=COLORS["primary"],
                                 lw=2, ls="--", mutation_scale=15))
    ax2.text(5.0, 0.42, "2.66x increase", fontsize=9, color=COLORS["primary"],
             fontweight="bold", ha="center", rotation=27)

    fig.suptitle("Quality-Efficiency Paradox: opposite trends from the same configurations",
                 fontsize=11, fontweight="bold")
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    out = os.path.join(FIGS_DIR, "figure_06_paradox.png")
    fig.savefig(out)
    plt.close(fig)
    print(f"  Saved: {out}")


if __name__ == "__main__":
    print("Generating figures from eval archives...\n")
    print("Figure 2: H0 Baseline Dynamics")
    figure_02()
    print("\nFigure 3: H2 Ablation")
    figure_03()
    print("\nFigure 6: H6/H8 Paradox")
    figure_06()
    print("\nDone. Figures saved to:", FIGS_DIR)
