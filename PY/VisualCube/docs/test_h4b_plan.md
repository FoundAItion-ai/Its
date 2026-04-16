# H4b — Robustness / Parameter Jitter

## Hypothesis

The composite oscillator spiral degrades gracefully under C1 parameter perturbation, not catastrophically. Even with +/-30% jitter applied to each inverter's C1 value per trial, the agent still produces measurable (though degraded) spiral behavior. This supports evolutionary plausibility: a biological oscillator circuit does not require exact parameter tuning to function.

## Mechanism

The spiral emerges from the temporal staggering of inverter decision windows (established in H2) and the frequency spacing between them (characterized in H4a). When C1 values are perturbed, the staggering pattern shifts:

- **Small perturbation (+/-10%)**: The temporal ordering of activations is preserved. The spiral quality degrades slightly because the arc-widening steps are no longer evenly spaced, but the fundamental mechanism still operates.
- **Moderate perturbation (+/-20%)**: Some trials will have inverters whose effective C1 values overlap, partially collapsing the staggering. Mean spiral quality decreases, but most trials still produce detectable spirals.
- **Large perturbation (+/-30%)**: Significant fraction of trials will have near-degenerate C1 values, but the mean still shows spiral behavior above noise floor.

The key prediction is **graceful degradation**: spiral quality decreases monotonically with jitter level, but does not cliff-drop to zero. This distinguishes a robust computational mechanism from a brittle parameter-sensitive artifact.

## Sub-hypotheses

| ID   | C1 Jitter | Prediction |
|------|-----------|------------|
| H4b1 | 0% (baseline) | Clean spiral (same config as H4a4, 1:5:10 spacing) |
| H4b2 | +/-10% | Slight degradation, spiral still clearly detectable |
| H4b3 | +/-20% | Moderate degradation, spiral measurable |
| H4b4 | +/-30% | Significant degradation, but NOT catastrophic (quality > 0.05) |

**Key distinction from other hypotheses:**
- H4a showed spiral quality depends on frequency spacing (operating regime)
- H4b shows spiral quality is robust to imprecise parameters (manufacturing tolerance)
- Together, H4a+H4b define the conditions under which the mechanism works reliably

## Test Configuration

**Spec file**: `tests/eval/specs/h4b_robustness.json`

**Duration**: 30s per trial, 60 fps, logging enabled.

**Trials**: 1,000 per configuration, 60 fps, logging enabled.

**Environment**: void (no food).

### Jitter implementation

Per trial, each inverter's C1 is perturbed by `uniform(-jitter_pct, +jitter_pct)`:
- For **f1** (uncrossed): all parameters (C1, C2, C3, C4) are scaled proportionally to preserve internal ratios
- For **crossed inverters**: only C1 is jittered, then `recalculate_composite()` re-derives C2/C3/C4

This approach guarantees NNN constraint satisfaction (C3 < C1 < C2 < C4) at all jitter levels. Verified with 1000 random trials at 30% jitter: 0% NNN failures.

The jitter is applied in `batch_test.py` via the `c1_jitter_pct` field in the spec run entry — no model code changes.

### Nominal inverter configuration (all configs, before jitter)

Same as H4a4 baseline (1:5:10 spacing, crossed 2:1, Net=+0.22):

| Inv | C1    | C2     | C3     | C4    | Crossed |
|-----|-------|--------|--------|-------|---------|
| f1  | 0.15  | 0.45   | 0.1125 | 1.35  | No      |
| f2  | 0.75  | 0.9975 | 0.3748 | 1.995 | Yes     |
| f3  | 1.50  | 1.995  | 0.7496 | 3.99  | Yes     |

## Noise Consideration

H4b introduces **two sources of variability**: the existing rotational diffusion noise (D_ROT = 0.1 rad^2/s) and the per-trial C1 jitter. This means:
- Variance in spiral quality increases with jitter level (wider confidence intervals)
- At 30% jitter, some trials will have nearly-degenerate C1 values (close spacing) and produce poor spirals, while others will have well-separated values and produce clean spirals
- The **mean** spiral quality across 1,000 trials is the key metric — it represents the expected performance of a randomly-manufactured oscillator circuit
- Log headers record the actual jittered C1 values per trial, enabling post-hoc analysis of quality vs. realized spacing

## Execution

### Step 1: Run simulations

```bash
cd PY/VisualCube
venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h4b_robustness.json --results-dir tests\eval\results
```

**Output**: `tests/eval/results/<timestamp>_h4b_robustness/logs/` with log files:
- `h4b1_composite_void_trial{0-999}.log`
- `h4b2_composite_void_trial{0-999}.log`
- `h4b3_composite_void_trial{0-999}.log`
- `h4b4_composite_void_trial{0-999}.log`

### Step 2: Analyze logs

```bash
venv\Scripts\python src\analyze_logs.py tests\eval\results\
```

### Step 3: Validate predictions

```bash
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h4b
```

### Step 4: Summary table

Fill in after running:

| Config | spiral_quality | spiral_growth | area_cells | Expected | Result |
|--------|---------------|---------------|------------|----------|--------|
| H4b1 (0%)  | | | | > 0.15, clean spiral | |
| H4b2 (10%) | | | | slight degradation | |
| H4b3 (20%) | | | | moderate degradation | |
| H4b4 (30%) | | | | > 0.05, not catastrophic | |

## Pass Criteria

H4b passes if **all checks** below hold when comparing means across 1,000 trials:

### A. Baseline spiral
- **H4b1**: spiral_quality > 0.15 (0% jitter produces clean spiral)

### B. Monotonic degradation
- spiral_quality: H4b1 >= H4b2 >= H4b3 >= H4b4

### C. Graceful degradation
- **H4b4**: spiral_quality > 0.05 (30% jitter does NOT destroy the spiral)
- **H4b2**: retains > 50% of H4b1 baseline spiral quality

### D. Area coverage
- area_cells_visited: H4b1 >= H4b2 >= H4b3 >= H4b4

## Files Involved

| File | Role |
|------|------|
| `tests/eval/specs/h4b_robustness.json` | Test specification (4 configurations) |
| `tests/eval/batch_test.py` | Runs headless simulations; applies per-trial C1 jitter |
| `src/inverter.py` | `recalculate_composite()` for re-deriving C2/C3/C4 after jitter |
| `src/trajectory_analyzer.py` | Computes metrics from trajectory data |
| `src/analyze_logs.py` | Bridges logs to analyzer |
| `tests/eval/validate_hypothesis.py` | validate_h4b function |
| `tests/eval/presets/h4b1.txt` - `h4b4.txt` | Manual verification presets (nominal params) |
| `docs/test_h4b_plan.md` | This plan |
