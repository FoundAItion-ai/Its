# H4a — Parameter Characterization

## Hypothesis

The emergent spiral produced by a composite oscillator can be characterized by a single parameter: the crossed inverters' internal C1:C3 ratio (opposition strength), with all other parameters held fixed. As opposition strength increases from weak to exact cancellation and beyond, the trajectory transitions from tight donut → expanding spiral → fast sweeping arcs → reversed spiral → tight reversed donut. An optimal balance exists, and the transition is symmetric around the cancellation point.

## Physics

### Fixed structure: 1:5:10 C1 spacing

All 7 configs share the same 3-inverter composite with C1 ratio 1:5:10:
- f1 (uncrossed): C1=0.15
- f2 (crossed): C1=0.75
- f3 (crossed): C1=1.50

C2 and C4 are derived from C1 and constant across all configs:
- C2 = C1 × 1.33, C4 = C2 × 2.0
- f1: C2=0.45, C4=1.35; f2: C2=0.9975, C4=1.995; f3: C2=1.995, C4=3.99

### f1 (uncrossed) — the base oscillator

f1 uses a 4:3 internal ratio (C3 = C1 × 0.75). Its turn differential:

```
f1_diff = 1/C3 - 1/C1 = 1/0.1125 - 1/0.15 = 2.222
```

### Swept parameter: crossed C1:C3 ratio (k = C3/C1)

Each crossed inverter uses the same ratio k. Opposition per inverter = (1/k - 1)/C1.

```
Net = f1_diff - (1/k - 1) × (1/C1_f2 + 1/C1_f3)
    = 2.222 - (1/k - 1) × 2.0
```

Solving for k: `k = 2.0 / (4.222 - Net)`

### Seven configs spanning the full regime

| Config | k (C3/C1) | Ratio | Net | Character |
|--------|-----------|-------|-----|-----------|
| H4a1 | 0.858 | 1.17:1 | +1.89 | Tight: weak opposition |
| H4a2 | 0.600 | 1.67:1 | +0.89 | Medium spiral |
| H4a3 | 0.530 | 1.89:1 | +0.45 | Wide expanding spiral |
| H4a4 | 0.500 | 2.00:1 | +0.22 | Near-balance spiral |
| H4a5 | 0.474 | 2.11:1 | 0.00 | Balance — exact cancellation |
| H4a6 | 0.428 | 2.34:1 | -0.45 | Slight reverse (symmetric with H4a3) |
| H4a7 | 0.375 | 2.67:1 | -1.11 | Strong reverse |

## Metrics

### FCR (Final Coverage Ratio)

Measures expansion power — how much the spiral grew from its first loop.

1. Detect radial distance peaks from trajectory centroid (per-revolution maxima)
2. `r1` = distance at first peak (first completed loop radius)
3. `r_max` = maximum radial distance reached
4. `FCR = (r_max / r1)^2`

High FCR = the spiral expanded a lot (large exploration range).

### LCR (Linear Coverage Ratio)

Measures sweep efficiency — what fraction of the expanded area the path actually covered.

1. Inscribe the first loop in a square: side = `2 × r1` → this is the unit cell
2. Inscribe the maximum extent in a square: side = `2 × r_max`
3. Tile the large square with unit cells → `N_total = ceil(2*r_max / (2*r1))^2`
4. Count unique cells the trajectory passes through → `N_hit`
5. `LCR = N_hit / N_total`

High LCR = the spiral didn't skip regions during expansion (thorough search).

### Derived columns (computed from C1-C4, not measured)

- `net_diff` = Σ sign_i × (1/C3_i - 1/C1_i) — predicts curvature direction and magnitude
- `total_power` = Σ (1/C1_i + 1/C3_i) — predicts overall speed

## Sub-hypotheses

| ID   | Crossed ratio | C3_f2  | C3_f3  | Net   | Prediction |
|------|---------------|--------|--------|-------|------------|
| H4a1 | 1.17:1       | 0.6432 | 1.2864 | +1.89 | Tight donut, low area |
| H4a2 | 1.67:1       | 0.4502 | 0.9004 | +0.89 | Medium spiral |
| H4a3 | 1.89:1       | 0.3977 | 0.7953 | +0.45 | Wide expanding spiral |
| H4a4 | 2.00:1       | 0.3748 | 0.7496 | +0.22 | Near-balance, large area |
| H4a5 | 2.11:1       | 0.3553 | 0.7106 | 0.00  | Balance — fast sweeping arcs, max area |
| H4a6 | 2.34:1       | 0.3211 | 0.6421 | -0.45 | Slight reverse (mirrors H4a3) |
| H4a7 | 2.67:1       | 0.2813 | 0.5626 | -1.11 | Strong reverse, bounded |

**Key distinction from earlier hypotheses:**
- H1 proved composite agents produce spirals (existence)
- H2 proved spiral requires both opposition and staggering (structural conditions)
- H4a characterizes the **operating regime** — maps a single parameter (crossed ratio k) through the full range from tight spiral to reversal

## Test Configuration

**Spec file**: `tests/eval/specs/h4a_resonance.json`

**Duration**: 30s per trial, 60 fps, logging enabled.

**Trials**: 100 per configuration (7 configs = 700 total).

**Environment**: void (no food) — isolates the spiral mechanism from mode-switching.

### Common parameters (all configs)

| Inv | C1   | C2     | C4    | Crossed |
|-----|------|--------|-------|---------|
| f1  | 0.15 | 0.45   | 1.35  | No      |
| f2  | 0.75 | 0.9975 | 1.995 | Yes     |
| f3  | 1.50 | 1.995  | 3.99  | Yes     |

f1 C3 = 0.1125 (constant). f2/f3 C3 varies per config (see sub-hypotheses table).

NNN constraint (C3 < C1 < C2 < C4) verified for all 7 configs.

**No `recalculate_composite()`** — all C values are explicit in the spec JSON.

## Execution

### Step 1: Run simulations

```bash
cd PY/VisualCube
venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h4a_resonance.json --results-dir tests\eval\results
```

**Output**: `tests/eval/results/<timestamp>_h4a_resonance/logs/` with log files:
- `h4a1_composite_void_trial{0-999}.log` through `h4a7_composite_void_trial{0-999}.log`

### Step 2: Analyze logs

```bash
venv\Scripts\python src\analyze_logs.py tests\eval\results\
```

### Step 3: Validate predictions

```bash
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h4a
```

### Step 4: Summary table

Fill in after running:

| Config | net_diff | area | speed | spiral_quality | Expected | Result |
|--------|---------|------|-------|---------------|----------|--------|
| H4a1 (1.17:1) | +1.89 | | | | tight donut | |
| H4a2 (1.67:1) | +0.89 | | | | medium spiral | |
| H4a3 (1.89:1) | +0.45 | | | | wide spiral | |
| H4a4 (2.00:1) | +0.22 | | | | near-balance | |
| H4a5 (2.11:1) | 0.00  | | | | fast arcs, max area | |
| H4a6 (2.34:1) | -0.45 | | | | slight reverse | |
| H4a7 (2.67:1) | -1.11 | | | | strong reverse | |

## Pass Criteria

H4a passes if **all 12 checks** below hold when comparing means across trials.

These checks use **area and speed** — deterministic metrics that are stable even at low trial counts. Spiral_quality is reported but not used for pass/fail due to noise sensitivity of peak detection.

### A. Area ordering: stronger opposition = wider exploration
- area: H4a4 >= H4a3 >= H4a2 >= H4a1

### B. Dramatic expansion difference
- H4a3 area > 5 × H4a1 area (expanding spiral vs tight donut)

### C. Speed gradient: less curvature = faster
- speed: H4a5 >= H4a4 >= H4a3 >= H4a2 >= H4a1

### D. Balance point much faster
- H4a5 speed > 2 × H4a1 speed (cancellation = fast motion)

### E. Balance sweeps widest
- H4a5 area > H4a4 area

### F. Forward spiral > reversed donut
- H4a3 area > H4a7 area

### G. Reversed donut is bounded
- H4a7 area < H4a5 area

### H. Reversed still faster than tight
- H4a6 speed > H4a1 speed

### I. Area symmetry across balance
- H4a6/H4a3 area ratio between 0.3 and 3.0

### J. Tightest is most bounded
- H4a1 area < H4a2 area

### K. Near-balance fills gap (area)
- H4a4 area > H4a3 area (confirms continuous area gradient near balance)

### L. Near-balance fills gap (speed)
- H4a4 speed > H4a3 speed (confirms continuous speed gradient near balance)

## Files Involved

| File | Role |
|------|------|
| `tests/eval/specs/h4a_resonance.json` | Test specification (7 configurations) |
| `tests/eval/batch_test.py` | Runs headless simulations |
| `src/trajectory_analyzer.py` | Computes metrics including FCR/LCR |
| `src/analyze_logs.py` | Bridges logs to analyzer, computes net_diff/total_power |
| `tests/eval/validate_hypothesis.py` | validate_h4a function (12 checks) |
| `tests/eval/presets/h4a1.txt` - `h4a7.txt` | Manual verification presets |
| `docs/test_h4a_plan.md` | This plan |
