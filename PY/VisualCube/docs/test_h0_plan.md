# H0 — Baseline Inverter Dynamics (Sanity Check)

## Hypothesis

A single inverter with equal left-right frequencies should produce straight-line motion. When one side's frequency is higher, the agent should follow a stable circular path toward the lower-frequency side. Increasing the absolute frequency scale (while keeping the ratio fixed) should increase the speed but not the radius. Changing the ratio should affect the circle's radius.

## Sub-hypotheses

| ID  | Condition | Prediction |
|-----|-----------|------------|
| H0a | Equal L/R frequencies (C1=C3) in void | Straight-line motion — no turning bias, high straightness |
| H0b | R-dominant inverter (C3 < C1) in void | Stable circle curving right — high circle fit score |
| H0c | L-dominant inverter (C1 < C3, mirror of H0b) in void | Stable circle curving left — same radius as H0b, opposite direction |
| H0d | 2x absolute frequency, same R/L ratio as H0b | Same circle radius as H0b, ~2x mean speed |
| H0e | 4x absolute frequency, same R/L ratio as H0b | Same circle radius as H0b, ~4x mean speed |
| H0f | Wider R/L ratio (1.50x) than H0b (1.11x) | Tighter circle — smaller radius than H0b |
| H0g | Widest R/L ratio (2.00x) | Tightest circle — smallest radius of all |

## Test Configuration

**Spec file**: `tests/eval/specs/h0_baseline.json`

### Direction tests (H0a–H0c)

| ID  | C1 | C2 | C3 | C4 | L freq (Hz) | R freq (Hz) | Ratio R/L | NNN ordering |
|-----|------|------|-------|------|-------------|-------------|-----------|--------------|
| H0a | 0.15 | 0.45 | 0.15  | 0.45 | 6.67 | 6.67 | 1.00 | equal (intentional) |
| H0b | 0.15 | 0.45 | 0.135 | 1.35 | 6.67 | 7.41 | 1.11 | normal (R-dominant) |
| H0c | 0.135| 1.35 | 0.15  | 0.45 | 7.41 | 6.67 | 0.90 | mirror (L-dominant) |

### Speed scaling tests (H0d–H0e) — same ratio as H0b, higher absolute frequencies

| ID  | C1 | C2 | C3 | C4 | L freq (Hz) | R freq (Hz) | Ratio R/L | Scale |
|-----|------|------|--------|------|-------------|-------------|-----------|-------|
| H0d | 0.075 | 0.225 | 0.0675 | 0.675 | 13.33 | 14.81 | 1.11 | 2x |
| H0e | 0.0375| 0.1125| 0.03375| 0.3375| 26.67 | 29.63 | 1.11 | 4x |

### Radius control tests (H0f–H0g) — same base frequency, wider L/R ratio

| ID  | C1 | C2 | C3 | C4 | L freq (Hz) | R freq (Hz) | Ratio R/L |
|-----|------|------|-------|------|-------------|-------------|-----------|
| H0f | 0.15 | 0.45 | 0.10  | 1.35 | 6.67 | 10.00 | 1.50 |
| H0g | 0.15 | 0.45 | 0.075 | 1.35 | 6.67 | 13.33 | 2.00 |

**Environment**: `void` (no food) — agent stays in HIGH mode using C1/C3 periods.

**Run parameters**: 5 trials x 30 seconds each, 60 fps, logging enabled, screenshots enabled.

## Noise Consideration

The agent has rotational diffusion noise (D_ROT = 0.1 rad^2/s), which adds ~3.3 deg/frame random heading perturbation. This means:
- H0a will NOT produce a perfectly straight line — expect straightness ~0.5-0.8 rather than 1.0
- H0b/c/f/g circles will have jitter around the fitted radius
- All comparisons use **mean +/- std across 5 trials** to account for this

The noise discrimination tests in `tests/unit/test_trajectory_analyzer.py::TestNoiseDiscrimination` confirm that all metrics can still distinguish shapes under this noise level.

## Execution

### Step 1: Run simulations

```bash
cd PY/VisualCube
venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h0_baseline.json --results-dir tests\eval\results
```

Or use the convenience scripts which also run analysis:
```bash
# Windows
scripts\test.cmd

# macOS / Linux
./scripts/test.sh
```

**Output**: `tests/eval/results/<timestamp>_h0_baseline/logs/` with 35 log files:
- `h0a_inverter_void_trial{0-4}.log`
- `h0b_inverter_void_trial{0-4}.log`
- `h0c_inverter_void_trial{0-4}.log`
- `h0d_inverter_void_trial{0-4}.log`
- `h0e_inverter_void_trial{0-4}.log`
- `h0f_inverter_void_trial{0-4}.log`
- `h0g_inverter_void_trial{0-4}.log`

### Step 2: Analyze logs

```bash
venv\Scripts\python src\analyze_logs.py tests\eval\results\
```

**Output**: `tests/eval/results/analysis.json` + printed summary table with per-config aggregated metrics.

### Step 3: Validate predictions

Compare the aggregated metrics against H0 predictions:

#### H0a — Straight line (no turning bias)
- **Check**: `straightness` mean for H0a >> `straightness` mean for H0b/c/f/g
- **Check**: `circle_fit_score` for H0a is low (no circular pattern)
- **Check**: `spiral_quality` for H0a is low (no spiral pattern)

#### H0b vs H0c — Opposite circles (symmetric inverters)
- **Check**: Both have high `circle_fit_score` (>0.3)
- **Check**: `circle_fit_radius` for H0b ≈ H0c (mirror config → same radius)
- **Check**: Both have low `straightness` (<0.3)

#### H0d vs H0b — 2x speed scaling (same ratio)
- **Check**: `mean_speed` of H0d ≈ 2x `mean_speed` of H0b
- **Check**: `circle_fit_radius` of H0d ≈ H0b (same ratio → same radius)

#### H0e vs H0b — 4x speed scaling (same ratio)
- **Check**: `mean_speed` of H0e ≈ 4x `mean_speed` of H0b
- **Check**: `circle_fit_radius` of H0e ≈ H0b (same ratio → same radius)

#### H0d vs H0e — Speed scales linearly with frequency
- **Check**: `mean_speed` of H0e ≈ 2x `mean_speed` of H0d
- **Check**: `circle_fit_radius` of H0d ≈ H0e (both same ratio)

#### H0f vs H0b — Wider ratio → tighter circle
- **Check**: `circle_fit_radius` of H0f < H0b (1.50 ratio → smaller radius than 1.11)
- **Check**: `circle_fit_score` of H0f > 0.3 (still a clean circle)

#### H0g vs H0f vs H0b — Radius decreases monotonically with ratio
- **Check**: `circle_fit_radius`: H0b > H0f > H0g
- **Check**: `circle_fit_score` of H0g > 0.3 (still a clean circle)

### Step 4: Summary table

Fill in after running:

| Comparison | Metric | Value A | Value B | Expected | Result |
|------------|--------|---------|---------|----------|--------|
| H0a vs H0b | straightness | | | H0a >> H0b | |
| H0b vs H0c | circle_fit_radius | | | H0b ≈ H0c | |
| H0d vs H0b | mean_speed ratio | | | ≈ 2.0 | |
| H0d vs H0b | circle_fit_radius | | | H0d ≈ H0b | |
| H0e vs H0b | mean_speed ratio | | | ≈ 4.0 | |
| H0e vs H0b | circle_fit_radius | | | H0e ≈ H0b | |
| H0d vs H0e | circle_fit_radius | | | H0d ≈ H0e | |
| H0f vs H0b | circle_fit_radius | | | H0f < H0b | |
| H0g vs H0f | circle_fit_radius | | | H0g < H0f | |

## Files Involved

| File | Role |
|------|------|
| `tests/eval/specs/h0_baseline.json` | Test specification (7 scenarios) |
| `tests/eval/batch_test.py` | Runs headless simulations, produces logs |
| `src/trajectory_analyzer.py` | Computes metrics from trajectory data |
| `src/analyze_logs.py` | Bridges logs to analyzer, aggregates across trials |
| `tests/unit/test_trajectory_analyzer.py` | Unit tests including noise discrimination |
| `scripts/test.cmd` | Convenience script: run + analyze (Windows) |
| `scripts/test.sh` | Convenience script: run + analyze (macOS/Linux) |

## Pass Criteria

H0 passes if **all 9 comparisons** in Step 4 hold when comparing means. Specifically:
- "A >> B" means A's mean is at least 2x B's mean
- "A ≈ B" means the difference is within 1 standard deviation
- "A < B" means A's mean is clearly below B's mean (non-overlapping mean ± std)

If D_ROT noise makes comparisons ambiguous, consider increasing trial count from 5 to 10 or increasing simulation duration from 30s to 60s.
