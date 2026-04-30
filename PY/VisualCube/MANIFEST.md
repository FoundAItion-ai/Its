# File Manifest — NNN Agent Simulation

Data and code manifest for the companion paper:
*Intelligence from Nothing: How Three Oscillators Produce Untrained Adaptive Behavior*

## Archive Contents

Each ZIP archive in `tests/eval/results/` is self-contained and includes:
- **Raw simulation logs** (`.log` files, one per trial per configuration)
- **Processed results** (`results.jsonl`, per-trial metric summaries)
- **Aggregated analysis** (`analysis.json`, per-configuration means, stds, and per-trial metrics)
- **Test specification** (`.md`, the hypothesis and configuration details)

For example, `h2_specificity.zip` contains 7,001 files: 7,000 raw trial logs
(1,000 per configuration × 7 configurations) plus the analysis.json aggregate.

## Stochastic Reproducibility

Per-trial random seeds are **not recorded**. Each trial uses Python's default OS-seeded
random state (`random.seed()` with no argument), which draws entropy from the operating
system at runtime. The simulation physics is deterministic given a seed, but because seeds
were not captured, exact stochastic re-runs from code alone are not possible. The archived
raw logs and per-trial metrics preserve the exact outputs of every trial, enabling full
verification of all reported statistics without re-running simulations.

## Source Code

| Path | Description |
|------|-------------|
| `src/inverter.py` | Core Inverter class (NNN spiking model) |
| `src/agent_module.py` | InverterAgent, CompositeMotionAgent, StochasticMotionAgent |
| `src/math_module.py` | Motion dynamics (turn angle, speed formulas) |
| `src/trajectory_analyzer.py` | Trajectory metrics (straightness, circle fit, spiral quality, etc.) |
| `src/analyze_logs.py` | Batch log to trajectory metrics to JSON pipeline |
| `src/config.py` | Global constants and defaults |
| `src/main.py` | Entry point (GUI + simulation loop) |
| `src/GUI.py` | Tkinter GUI with inverter config panel |

## Test Specifications and Validation

| Path | Description |
|------|-------------|
| `tests/eval/specs/` | JSON test specifications for H0-H9 hypotheses |
| `tests/eval/batch_test.py` | Headless batch simulation runner |
| `tests/eval/validate_hypothesis.py` | 95-check validation engine |
| `docs/test_h0_plan.md` ... `test_h9_plan.md` | Per-hypothesis test plans |

## Figure and Table Regeneration Scripts

All scripts read from the archived ZIP data and regenerate the paper's figures and tables.
Located in `scripts/figures/` within this repository.

| Script | Outputs | Data Source |
|--------|---------|-------------|
| `scripts/figures/generate_figures.py` | Figures 2-7 (main article) | `h0_baseline*.zip`, `h2_specificity.zip`, `h5_symmetry.zip`, `h6_complexity.zip`, `h8_reconciliation.zip` |
| `scripts/figures/generate_convergence_plot.py` | Supplementary Figure S1 | `h2_specificity.zip`, `h3_exploitation.zip` |
| `scripts/figures/generate_stat_tests.py` | Supplementary Table S14 | All hypothesis ZIPs (`h0_baseline.zip` through `h9_negative.zip`) |

Dependencies: `numpy`, `matplotlib`, `scipy` (see `requirements.txt`).

## Data → Figure/Table Mapping

| Paper Element | Data Source | How to Regenerate |
|---------------|-------------|-------------------|
| Table 7 (H2 ablation) | `h2_specificity.zip` → `analysis.json` | Values from per-config aggregates |
| Table 8 (H3 mode switching) | `h3_exploitation.zip` → `analysis.json` | Values from per-config aggregates |
| Table 9 (H6 complexity) | `h6_complexity.zip` → `analysis.json` | Values from per-config aggregates |
| Table 10 (H8 reconciliation) | `h8_reconciliation.zip` → `analysis.json` | Values from per-config aggregates |
| Table 11 (H5 geometry) | `h5_symmetry.zip` → `analysis.json` | Values from per-config aggregates |
| Tables 12-13 (baselines) | `benchmark_baselines.zip` → `analysis.json` | Values from per-config aggregates |
| Table 14 (summary) | All ZIPs | `validate_hypothesis.py` run on each |
| Figure 2 (H2 ablation) | `h2_specificity.zip` | `generate_figures.py` |
| Figure 3 (H3 trajectory) | `h3_exploitation.zip` | `generate_figures.py` |
| Figure 4 (H6/H8 paradox) | `h6_complexity.zip`, `h8_reconciliation.zip` | `generate_figures.py` |
| Figure 5 (H0 baseline) | `h0_baseline.zip`, `h0_baseline_2.zip` | `generate_figures.py` |
| Figures 6-7 (H5 trajectories) | `h5_symmetry.zip` | `generate_figures.py` |
| Supp. Figure S1 (convergence) | `h2_specificity.zip`, `h3_exploitation.zip` | `generate_convergence_plot.py` |
| Supp. Table S1 (sensitivity) | `sensitivity_report.zip` | Pre-computed in archive |
| Supp. Tables S2-S12 (distributions) | `distribution_reports.zip` | Pre-computed in archive |
| Supp. Table S13 (baselines) | `benchmark_baselines.zip` | Pre-computed in archive |
| Supp. Table S14 (formal tests) | All hypothesis ZIPs | `generate_stat_tests.py` |

## Results Data (ZIP Archives)

| Archive | Hypothesis | Configs | Trials | Raw Logs | SHA-256 |
|---------|-----------|---------|--------|----------|---------|
| `h0_baseline.zip` | H0 (a-c) | 3 | 3,000 | 3,000 | `90a5b7d9c74d54c4280477a99136f516388023e21f789b24715f16e9cde63ec8` |
| `h0_baseline_2.zip` | H0 (d-g) | 4 | 4,000 | 4,000 | `b607176fbe8363bc87b5ae92602630bdbfa3f479f65bd0d5e567ffad20f95845` |
| `h1_emergence.zip` | H1 | 3 | 3,000 | 3,000 | `83df91ed8c50af293d746d3a38bc4ea3bca33dfa0d7798d11d60f58875375d15` |
| `h2_specificity.zip` | H2 | 7 | 7,000 | 7,000 | `82afba216b45155edbec4c5dfd5ac862985402510db9a7b0d4e09d4084ba9515` |
| `h3_exploitation.zip` | H3 | 4 | 4,000 | 4,000 | `a821f64f7c1032d6202fb6da5dd22cacfc9d50fe04fbf656a6509d83851e1224` |
| `h4a_resonance.zip` | H4a | 7 | 7,000 | 7,000 | `b707255fb3ce8422536db2c67935a8786b64d9974fd8284321f474619408d3b4` |
| `h4b_robustness.zip` | H4b | 4 | 4,000 | 4,000 | `1a51fd6b8be58a1eefd8ffc3e25513dc34bd62aaa1fa3c3ebb952a6be16115c5` |
| `h5_symmetry.zip` | H5 | 4 | 4,000 | 4,000 | `df15ed401abb7f96c51f640752dbdc235de8b37376cced549e0d6cd7c129cdc1` |
| `h6_complexity.zip` | H6 | 5 | 5,000 | 5,000 | `851b14062b1ee463272a8d143be97ccbd44c994ddfd5a4180ab2c2607aa14337` |
| `h7_sensitivity.zip` | H7 | 6 | 6,000 | 6,000 | `46856c401ca71a9cddaa474ac7201aea51d3fbcb8bd3e8ba8dad6b5a3c340239` |
| `h8_reconciliation.zip` | H8 | 5 | 5,000 | 5,000 | `1f6e1c60a8829b33e4d311e06e3c09c60957a233bb3617e70e6a757864291e5c` |
| `h9_negative.zip` | H9 | 11 | 11,000 | 11,000 | `667de44d988873c9b38b1658c5eae5dc9c9693d3e4ff7976305dc058a1d6797f` |
| `benchmark_baselines.zip` | Baselines | 21 | 21,000 | 21,000 | `1cd185ef1e5cd07f196340fb203aa08f3424a08aca5391196df3fa3e930a7274` |
| `sensitivity_report.zip` | Sensitivity | — | — | — | `6b44df9f23e0603fc23e9f97da974f18a81d7b5076c706d4f392630326bc8594` |
| `distribution_reports.zip` | Distributions | — | — | — | `37a6ecf763e083e29f0d4fb9ef5a17bc97922400ffd50938190e1277ad99b9c9` |
| `convergence_analysis.zip` | Convergence | — | — | — | `42d5f0e432a6f7a717c3f7e09cf389ddfed681125946b98067a9eec8f97a01e2` |

**Total:** 84,000 trials with 84,000 raw simulation logs across 84 configurations.

## Supplementary Materials

| Path | Description |
|------|-------------|
| `docs/Supplementary_Materials.pdf` | Tables S1-S14, Figures S1-S12 |

## Verification

To verify checksums:
```bash
cd PY/VisualCube/tests/eval/results
sha256sum *.zip
```

To run the 95-check validation:
```bash
cd PY/VisualCube
for h in h0 h1 h2 h3 h4a h4b h5 h6 h7 h8 h9; do
  python tests/eval/validate_hypothesis.py tests/eval/results $h
done
```

To regenerate figures:
```bash
cd PY/VisualCube
python scripts/figures/generate_figures.py
python scripts/figures/generate_convergence_plot.py
python scripts/figures/generate_stat_tests.py
```
