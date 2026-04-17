# H5 — 2nd-Degree Symmetry

## Hypothesis

The open spiral configuration (H4a4, crossed 2:1, Net=+0.22) is optimal for finding and tracking two parallel food bands (2nd-degree symmetry). H3 showed agents can find and track a single food line (1st-degree symmetry). H5 tests whether agents can discover and navigate between two parallel bands — requiring broader exploration to find both and efficient switching between them.

Agents with more open spirals should consume more food because:
1. Wider spirals reach food bands sooner from a central starting position
2. Higher speed means more food dots encountered per unit time
3. Open spirals are more likely to discover both bands (not just the nearest one)

## Physics

### Fixed structure: 1:5:10 C1 spacing (same as H4a)

All 4 configs share the same 3-inverter composite:
- f1 (uncrossed): C1=0.15, C2=0.45, C3=0.1125, C4=1.35
- f2 (crossed): C1=0.75, C2=0.9975, C4=1.995 (C3 varies)
- f3 (crossed): C1=1.50, C2=1.995, C4=3.99 (C3 varies)

### Swept parameter: crossed C1:C3 ratio

| Config | k (C3/C1) | Ratio | Net  | C3_f2  | C3_f3  | Character |
|--------|-----------|-------|------|--------|--------|-----------|
| H5a1   | 0.858     | 1.17:1| +1.89| 0.6432 | 1.2864 | Tight spiral |
| H5a2   | 0.600     | 1.67:1| +0.89| 0.4502 | 0.9004 | Medium spiral |
| H5a3   | 0.530     | 1.89:1| +0.45| 0.3977 | 0.7953 | Wide spiral |
| H5a4   | 0.500     | 2.00:1| +0.22| 0.3748 | 0.7496 | Open spiral |

## Environment: dense_bands

Two horizontal bands of food dots, denser and shorter than `two_bands`:

- **Band positions**: y = 315 (35% of 900) and y = 585 (65% of 900)
- **Band width**: 660px (50% of original 1320px), centered: x from 370 to 1030
- **Dot spacing**: gap = 3px (same density as `dense_band`)
- **Rows per band**: 5 (y offsets: -8, -4, 0, 4, 8), dot radius 2
- **Total dots**: ~2200

The shorter centered bands ensure the agent must search actively from the center spawn point. A tight spiral that loops near its origin will not reach the bands as easily as an open spiral.

## Metrics

### Primary: food_eaten

From log footer FINAL_STATS. Direct measure of search-and-track success.

### Secondary: area_cells_visited

Confirms that food ordering correlates with exploration area.

## Sub-hypotheses

| ID   | Crossed ratio | Net   | Prediction |
|------|---------------|-------|------------|
| H5a1 | 1.17:1       | +1.89 | Least food: tight spiral barely reaches bands |
| H5a2 | 1.67:1       | +0.89 | More food: medium spiral reaches bands slowly |
| H5a3 | 1.89:1       | +0.45 | More food: wide spiral reaches bands quickly |
| H5a4 | 2.00:1       | +0.22 | Most food: open spiral reaches bands fastest, tracks fastest |

**Key distinction from earlier hypotheses:**
- H3 proved agents can find and track a single food source (1st-degree symmetry)
- H4a characterized the spiral operating regime in void
- H5 tests which spiral configuration is optimal for 2nd-degree symmetry (two parallel bands)

## Test Configuration

**Spec file**: `tests/eval/specs/h5_symmetry.json`

**Duration**: 60s per trial, 60 fps, logging enabled.

**Trials**: 1000 per configuration (4 configs = 4000 total).

**Environment**: dense_bands (two dense centered bands).

**Spawn point**: (700, 420) — horizontally centered, ~95px from upper band edge, between both bands.

### Common parameters (all configs)

| Inv | C1   | C2     | C4    | Crossed |
|-----|------|--------|-------|---------|
| f1  | 0.15 | 0.45   | 1.35  | No      |
| f2  | 0.75 | 0.9975 | 1.995 | Yes     |
| f3  | 1.50 | 1.995  | 3.99  | Yes     |

f1 C3 = 0.1125 (constant). f2/f3 C3 varies per config (see sub-hypotheses table).

## Execution

### Step 1: Run simulations

```bash
cd PY/VisualCube
venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h5_symmetry.json --results-dir tests\eval\results
```

**Output**: `tests/eval/results/<timestamp>_h5_symmetry/logs/` with log files:
- `h5a1_composite_dense_bands_trial{0-999}.log` through `h5a4_composite_dense_bands_trial{0-999}.log`

### Step 2: Analyze logs

```bash
venv\Scripts\python src\analyze_logs.py tests\eval\results\
```

### Step 3: Validate predictions

```bash
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h5
```

### Step 4: Summary table

Fill in after running:

| Config | food_eaten | area_cells | mean_speed | Expected | Result |
|--------|-----------|------------|------------|----------|--------|
| H5a1 (1.17:1, +1.89) | | | | least food | |
| H5a2 (1.67:1, +0.89) | | | | more food | |
| H5a3 (1.89:1, +0.45) | | | | more food | |
| H5a4 (2.00:1, +0.22) | | | | most food | |

## Pass Criteria

H5 passes if **all 7 checks** below hold when comparing means across trials.

### A. Monotonic food ordering
- food_eaten: H5a4 >= H5a3 >= H5a2 >= H5a1

### B. Open spiral substantially outperforms tight
- H5a4 food > 1.5 * H5a1 food

### C. Open spiral is the best
- H5a4 food > H5a3 food

### D. Wide > medium
- H5a3 food > H5a2 food

### E. Medium > tight
- H5a2 food > H5a1 food

### F. Open spiral finds meaningful food
- H5a4 food > 10

### G. Area coverage ordering matches food ordering
- area_cells_visited: H5a4 >= H5a3 >= H5a2 >= H5a1

## Files Involved

| File | Role |
|------|------|
| `tests/eval/specs/h5_symmetry.json` | Test specification (4 configurations) |
| `tests/eval/batch_test.py` | Runs headless simulations |
| `src/config.py` | `dense_bands_food()` environment function |
| `src/trajectory_analyzer.py` | Computes metrics from trajectory data |
| `src/analyze_logs.py` | Bridges logs to analyzer, extracts food_eaten |
| `tests/eval/validate_hypothesis.py` | `validate_h5` function (7 checks) |
| `tests/eval/presets/h5a1.txt` - `h5a4.txt` | Manual verification presets |
| `docs/test_h5_plan.md` | This plan |
