# H6 — Complexity Scaling (Inverted)

## Original Hypothesis

Distributing the same total opposition (Net=+0.22, open spiral) across more crossed inverters (3→7) improves food search in a 2nd-degree symmetry environment. More inverters create more activation stages, producing a smoother spiral approximation that covers area more efficiently.

## Inverted Finding

Fewer, stronger activation stages outperform many weak ones. 3 inverters (per_opp=1.0011 Hz) eat ~44% more food and explore ~71% more area than 7 inverters (per_opp=0.3337 Hz). Performance decreases monotonically with inverter count. The likely cause: high-C1 inverters in larger configs take up to 16s to activate, and each contributes too little opposition to create decisive turns individually, producing a mushy, delayed spiral.

## Physics

### Equal opposition design

All configs target Net=+0.22. f1 (uncrossed) is identical across configs:
- C1=0.15, C2=0.45, C3=0.1125, C4=1.35
- base_diff = 1/C3 - 1/C1 = 2.2222 Hz
- total crossed opposition = 2.0022 Hz

Each crossed inverter contributes equal opposition:
- per_opp = 2.0022 / (N-1) where N = total inverters
- C3_i = 1 / (per_opp + 1/C1_i)
- C2_i = C1_i * 1.33, C4_i = C2_i * 2.0

### C1 spacing

Geometric x2 progression for staggered activation:
- 3 inv: 0.15, 0.75, 1.50 (matches H4a C1 spacing)
- 4 inv: 0.15, 0.50, 1.00, 2.00
- 5 inv: 0.15, 0.50, 1.00, 2.00, 4.00
- 6 inv: 0.15, 0.50, 1.00, 2.00, 4.00, 8.00
- 7 inv: 0.15, 0.50, 1.00, 2.00, 4.00, 8.00, 16.00

### Parameter tables

**H6a1 (3 inv, per_opp=1.0011):**

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|--------|--------|--------|---------|
| f1 | 0.15 | 0.45 | 0.1125 | 1.35 | No |
| f2 | 0.75 | 0.9975 | 0.4284 | 1.995 | Yes |
| f3 | 1.50 | 1.995 | 0.5996 | 3.99 | Yes |

**H6a2 (4 inv, per_opp=0.6674):**

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|--------|--------|--------|---------|
| f1 | 0.15 | 0.45 | 0.1125 | 1.35 | No |
| f2 | 0.50 | 0.665 | 0.3749 | 1.33 | Yes |
| f3 | 1.00 | 1.33 | 0.5997 | 2.66 | Yes |
| f4 | 2.00 | 2.66 | 0.8566 | 5.32 | Yes |

**H6a3 (5 inv, per_opp=0.5006):**

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|--------|--------|--------|---------|
| f1 | 0.15 | 0.45 | 0.1125 | 1.35 | No |
| f2 | 0.50 | 0.665 | 0.3999 | 1.33 | Yes |
| f3 | 1.00 | 1.33 | 0.6664 | 2.66 | Yes |
| f4 | 2.00 | 2.66 | 0.9994 | 5.32 | Yes |
| f5 | 4.00 | 5.32 | 1.3323 | 10.64 | Yes |

**H6a4 (6 inv, per_opp=0.4004):**

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|--------|--------|--------|---------|
| f1 | 0.15 | 0.45 | 0.1125 | 1.35 | No |
| f2 | 0.50 | 0.665 | 0.4166 | 1.33 | Yes |
| f3 | 1.00 | 1.33 | 0.7141 | 2.66 | Yes |
| f4 | 2.00 | 2.66 | 1.1106 | 5.32 | Yes |
| f5 | 4.00 | 5.32 | 1.5374 | 10.64 | Yes |
| f6 | 8.00 | 10.64 | 1.9032 | 21.28 | Yes |

**H6a5 (7 inv, per_opp=0.3337):**

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|--------|--------|--------|---------|
| f1 | 0.15 | 0.45 | 0.1125 | 1.35 | No |
| f2 | 0.50 | 0.665 | 0.4285 | 1.33 | Yes |
| f3 | 1.00 | 1.33 | 0.7498 | 2.66 | Yes |
| f4 | 2.00 | 2.66 | 1.1995 | 5.32 | Yes |
| f5 | 4.00 | 5.32 | 1.7132 | 10.64 | Yes |
| f6 | 8.00 | 10.64 | 2.1801 | 21.28 | Yes |
| f7 | 16.00 | 21.28 | 2.524 | 42.56 | Yes |

## Sub-hypotheses

| ID | Inverters | Activation stages | Original Prediction | Actual Result |
|----|-----------|-------------------|---------------------|---------------|
| H6a1 | 3 | 2 expansions | Baseline | **Best** (749.8 food, 166.5 area) |
| H6a2 | 4 | 3 expansions | More food | Less food (704.5) |
| H6a3 | 5 | 4 expansions | More food | Less food (641.1) |
| H6a4 | 6 | 5 expansions | More food | Less food (592.7) |
| H6a5 | 7 | 6 expansions | Most food | **Worst** (521.8 food, 97.3 area) |

**Key distinction from H1:** H1 tested more inverters with increasing total opposition. H6 holds net constant at +0.22 and redistributes — isolating activation granularity from opposition strength.

**Interpretation:** Distributing opposition across many inverters dilutes each one's contribution below the threshold for decisive turns. High-C1 inverters (C1=8, 16) take seconds to activate, leaving the agent under-steered during early exploration when it matters most.

## Test Configuration

**Spec file**: `tests/eval/specs/h6_complexity.json`

**Duration**: 60s per trial, 60 fps, log_stride=2.

**Trials**: 1000 per configuration (5 configs = 5000 total).

**Environment**: dense_bands.

**Spawn point**: (700, 420) — between the two bands, ~95px from upper band.

## Execution

### Step 1: Run simulations

```bash
cd PY/VisualCube
venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h6_complexity.json --results-dir tests\eval\results
```

### Step 2: Analyze logs

```bash
venv\Scripts\python src\analyze_logs.py tests\eval\results\
```

### Step 3: Validate predictions

```bash
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h6
```

### Step 4: Summary table

| Config | food_eaten | area_cells | mean_speed | Expected (inverted) | Result |
|--------|-----------|------------|------------|---------------------|--------|
| H6a1 (3 inv) | 749.8 | 166.5 | 188.9 | most food | PASS |
| H6a2 (4 inv) | 704.5 | 166.0 | 210.5 | less food | PASS |
| H6a3 (5 inv) | 641.1 | 139.9 | 216.8 | less food | PASS |
| H6a4 (6 inv) | 592.7 | 116.8 | 194.2 | less food | PASS |
| H6a5 (7 inv) | 521.8 | 97.3 | 195.0 | least food | PASS |

## Pass Criteria (Inverted)

H6 passes if **all 6 checks** hold.

### A. Monotonic food ordering (inverted)
- food_eaten: H6a1 >= H6a2 >= H6a3 >= H6a4 >= H6a5

### B. 3-inv substantially outperforms 7-inv
- H6a1 food > 1.3 * H6a5 food

### C. Pairwise: fewer always beats more
- H6a1 food > H6a2 food
- H6a2 food > H6a3 food

### D. Best config (3 inv) finds food
- H6a1 food > 10

### E. Area ordering (inverted)
- area: H6a1 >= H6a2 >= H6a3 >= H6a4 >= H6a5

## Files Involved

| File | Role |
|------|------|
| `tests/eval/specs/h6_complexity.json` | Test specification (5 configurations) |
| `tests/eval/batch_test.py` | Runs headless simulations |
| `src/config.py` | `dense_bands_food()` environment |
| `src/analyze_logs.py` | Log analysis pipeline |
| `tests/eval/validate_hypothesis.py` | `validate_h6` function (7 checks) |
| `tests/eval/presets/h6a1.txt` - `h6a5.txt` | Manual verification presets |
| `docs/test_h6_plan.md` | This plan |
