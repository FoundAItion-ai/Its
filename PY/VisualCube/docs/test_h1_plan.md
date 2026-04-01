# H1 — Emergence of Exploratory Behavior

## Hypothesis

A composite agent with three or more oscillatory inverters, tuned to distinct frequencies, will exhibit an emergent outward spiral trajectory in the absence of external stimuli ("food"). This tests whether the internal frequency interference between multiple oscillators can self-organize into coherent, expanding trajectories — a sign of emergent search dynamics. The spiral pattern should not be directly programmed but should arise naturally from the system's own feedback structure and phase relations.

## Mechanism

In void mode (no food), all inverters stay in HIGH state using C1/C3 periods. The f1 inverter (normal wiring) creates a base turn differential (R > L frequency). Each crossed inverter opposes that differential by a diminishing fraction (27%, 15%, 9%, 4.5%), widening the arc in discrete steps as slower inverters complete their C1 decision windows. This staggered activation produces progressively wider arcs — an outward spiral — from pure frequency beating with no explicit spiral programming.

## Sub-hypotheses

| ID  | Condition | Prediction |
|-----|-----------|------------|
| H1a | 3 inverters (f1 + f2 + f3), void | Outward spiral — high spiral_quality, positive growth_rate, low straightness |
| H1b | 4 inverters (f1 + f2 + f3 + f4), void | Spiral at least as strong as H1a (spiral_quality >= H1a) |
| H1c | 5 inverters (f1 through f5), void | Spiral at least as strong as H1b |
| H1d | Monotonicity check across H1a-H1c | spiral_quality non-decreasing with inverter count |

**Key distinction from other hypotheses:**
- H0 proved single inverters make circles/lines
- H2 will prove simpler agents (stochastic, 1-inv, 2-inv composite) do NOT make spirals
- H4 will test frequency spacing effects
- H1 shows the positive case: spirals emerge and strengthen with more inverters

## Test Configuration

**Spec file**: `tests/eval/specs/h1_emergence.json`

All configs use f1 = optimized single-inverter preset (C1=0.15, C3=0.135). Crossed inverters are derived via `recalculate_composite()` with default opposition fractions.

### H1a — 3 inverters

| Inv | C1    | C2    | C3    | C4    | Crossed | Opposition | L freq (Hz) | R freq (Hz) |
|-----|-------|-------|-------|-------|---------|------------|-------------|-------------|
| f1  | 0.15  | 0.45  | 0.135 | 1.35  | No      | —          | 6.67        | 7.41        |
| f2  | 3.0   | 3.99  | 1.875 | 7.98  | Yes     | 27%        | 0.33        | 0.53        |
| f3  | 6.0   | 7.98  | 3.6   | 15.96 | Yes     | 15%        | 0.17        | 0.28        |

### H1b — 4 inverters (H1a + f4)

| Inv | C1    | C2    | C3    | C4    | Crossed | Opposition | L freq (Hz) | R freq (Hz) |
|-----|-------|-------|-------|-------|---------|------------|-------------|-------------|
| f4  | 10.0  | 13.3  | 6.0   | 26.6  | Yes     | 9%         | 0.10        | 0.17        |

### H1c — 5 inverters (H1b + f5)

| Inv | C1    | C2    | C3    | C4    | Crossed | Opposition | L freq (Hz) | R freq (Hz) |
|-----|-------|-------|-------|-------|---------|------------|-------------|-------------|
| f5  | 15.0  | 19.95 | 10.0  | 39.9  | Yes     | 4.5%       | 0.07        | 0.10        |

**Spiral progression in void (linear rate summation):**
- t=0.15s: f1 fires. diff=+0.74 -> tight circle
- t=3.0s:  f2 (crossed) opposes by 0.20 -> diff=+0.54 -> wider arc
- t=6.0s:  f3 (crossed) opposes by 0.11 -> diff=+0.43 -> wider arc
- t=10.0s: f4 (crossed) opposes by 0.07 -> diff=+0.36 -> wider arc
- t=15.0s: f5 (crossed) opposes by 0.03 -> diff=+0.33 -> widest arc

**Environment**: `void` (no food) — agent stays in HIGH mode using C1/C3 periods.

**Run parameters**: 5 trials x 120 seconds each, 60 fps, logging enabled, screenshots enabled. Duration is 120s (vs H0's 30s) because the slowest inverter (f5, C1=15s) needs ~8 full cycles to establish a statistically meaningful spiral pattern.

## Noise Consideration

The agent has rotational diffusion noise (D_ROT = 0.1 rad^2/s), which adds ~3.3 deg/frame random heading perturbation. This means:
- Spirals will not be geometrically perfect — expect spiral_quality ~0.3-0.7 rather than 1.0
- growth_rate will have variance across trials
- All comparisons use **mean +/- std across 5 trials** to account for this

The noise discrimination tests in `tests/unit/test_trajectory_analyzer.py::TestNoiseDiscrimination` confirm that spiral_quality and other metrics can still distinguish shapes under this noise level.

## Execution

### Step 1: Run simulations

```bash
cd PY/VisualCube
venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h1_emergence.json --results-dir tests\eval\results
```

Or use the convenience scripts which also run analysis:
```bash
# Windows
scripts\test.cmd

# macOS / Linux
./scripts/test.sh
```

**Output**: `tests/eval/results/<timestamp>_h1_emergence/logs/` with 15 log files:
- `h1a_composite_void_trial{0-4}.log`
- `h1b_composite_void_trial{0-4}.log`
- `h1c_composite_void_trial{0-4}.log`

### Step 2: Analyze logs

```bash
venv\Scripts\python src\analyze_logs.py tests\eval\results\
```

**Output**: `tests/eval/results/analysis.json` + printed summary table with per-config aggregated metrics.

### Step 3: Validate predictions

Compare the aggregated metrics against H1 predictions:

#### H1a — Spiral detection (3 inverters)
- **Check**: `spiral_quality` mean > 0.3
- **Check**: `growth_rate` mean > 0 (positive outward expansion)
- **Check**: `spiral_quality` > `circle_fit_score` (spiral fits better than a fixed circle)
- **Check**: `straightness` mean < 0.3 (not a straight line)

#### H1b vs H1a — 4 inverters at least as strong
- **Check**: `spiral_quality` of H1b >= H1a (mean, within 1 std tolerance)
- **Check**: `area_coverage` of H1b >= H1a

#### H1c vs H1b — 5 inverters at least as strong
- **Check**: `spiral_quality` of H1c >= H1b (mean, within 1 std tolerance)
- **Check**: `area_coverage` of H1c >= H1b

#### H1d — Monotonicity
- **Check**: `spiral_quality` means: H1c >= H1b >= H1a
- **Check**: `area_coverage` means: non-decreasing with inverter count

### Step 4: Summary table

Fill in after running:

| Comparison | Metric | H1a | H1b | H1c | Expected | Result |
|------------|--------|-----|-----|-----|----------|--------|
| Spiral detected | spiral_quality | | | | > 0.3 | |
| Outward expansion | growth_rate | | | | > 0 | |
| Not a circle | spiral_quality > circle_fit_score | | | | Yes | |
| Not a line | straightness | | | | < 0.3 | |
| Monotonicity | spiral_quality | | | | H1c >= H1b >= H1a | |
| Exploration scaling | area_coverage | | | | Non-decreasing | |

## Files Involved

| File | Role |
|------|------|
| `tests/eval/specs/h1_emergence.json` | Test specification (3 scenarios) |
| `tests/eval/batch_test.py` | Runs headless simulations, produces logs |
| `src/trajectory_analyzer.py` | Computes metrics from trajectory data |
| `src/analyze_logs.py` | Bridges logs to analyzer, aggregates across trials |
| `tests/unit/test_trajectory_analyzer.py` | Unit tests including noise discrimination |
| `docs/test_h1_plan.md` | This plan |

## Pass Criteria

H1 passes if **all 6 comparisons** in Step 4 hold when comparing means. Specifically:
- "> threshold" means the mean exceeds the threshold
- "A >= B" means A's mean + 1 std >= B's mean (not clearly below)
- "Yes" means the condition holds for means of all three configs

If D_ROT noise makes comparisons ambiguous, consider increasing trial count from 5 to 10 or increasing simulation duration from 120s to 180s.
