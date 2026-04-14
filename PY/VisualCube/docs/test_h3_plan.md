# H3 — Exploitation (Mode Switching)

## Hypothesis

A composite agent spiraling in void autonomously transitions to exploitative tracking upon encountering a structured food source. The transition is input-driven with no explicit controller — the inverters' own suppression mechanism (HIGH→LOW mode switch) collapses the spiral into focused tracking when food input is detected. Single-inverter controls track food when encountered but do not exhibit the prior exploratory spiral phase.

## Mechanism

In void (no food), all inverters are in HIGH mode (C1/C3 periods). The composite agent produces an outward spiral via frequency beating between normal and crossed inverters (established in H1). When the spiral path reaches a food region, the inverters begin receiving input signals. After each inverter's C1 decision window, it detects the input and switches to LOW mode (C2/C4 periods), suppressing its output. This changes the power balance:

- f1 (normal wiring): HIGH→LOW reduces its turn differential contribution
- f2-f4 (crossed wiring): HIGH→LOW reduces their opposition to f1

The net effect is a collapse from the wide spiral arc to tighter, slower tracking behavior — the agent stays near the food. This mode switch requires no explicit controller; it emerges from the same inverter reset mechanism that defines the NNN model.

For single-inverter controls, there is no spiral phase. The single inverter traces a circle in void. If the circle path intersects food, the inverter switches to LOW mode and tracks, but the pre-contact trajectory is a circle, not a spiral.

## Sub-hypotheses

| ID  | Agent | Food Environment | Prediction |
|-----|-------|------------------|------------|
| H3a | Composite (4-inv) | Line (`sparse_band`) | Spiral→tracking transition; at least 1 explore→exploit transition |
| H3b | Composite (4-inv) | Circle (`filled_circle`) | Spiral→tracking transition; at least 1 explore→exploit transition |
| H3c | Single inverter | Line (`sparse_band`) | Tracking only (control); no spiral phase |
| H3d | Single inverter | Circle (`filled_circle`) | Tracking only (control); no spiral phase |

**Key distinction from other hypotheses:**
- H1 proved composite agents make spirals in void
- H2 proved the spiral requires both opposition and staggering
- H3 shows the spiral is not just a fixed pattern — it *adapts* when the environment changes
- H4 will characterize the operating regime (frequency spacing, robustness)

## Test Configuration

**Spec file**: `tests/eval/specs/h3_exploitation.json`

**Duration**: 60s — enough for spiral exploration phase (~10-20s for 4-inv composite to expand), food encounter, and sustained tracking phase.

**Trials**: 1,000 per configuration, 60 fps, logging enabled.

### Composite agent (H3a, H3b) — 4 inverters (same as H1b)

| Inv | C1    | C2    | C3    | C4    | Crossed | Name |
|-----|-------|-------|-------|-------|---------|------|
| f1  | 0.15  | 0.45  | 0.135 | 1.35  | No      | f1   |
| f2  | 3.0   | 3.99  | 1.5   | 7.98  | Yes     | f2   |
| f3  | 6.0   | 7.98  | 2.571 | 15.96 | Yes     | f3   |
| f4  | 8.0   | 10.64 | 6.171 | 21.28 | Yes     | f4   |

### Single inverter (H3c, H3d) — optimized preset

| Param | Value |
|-------|-------|
| C1    | 0.15  |
| C2    | 0.45  |
| C3    | 0.135 |
| C4    | 1.35  |

### Food environments

**Line food** (`sparse_band`): Horizontal band at y = 0.5 * WINDOW_H (450px). 5 rows of dots, gap=15px, x from 40px to 1360px. ~440 food dots total.

**Circle food** (`filled_circle`): Filled disk at window center (700, 450), radius = 0.35 * min(1400, 900) = 315px. Dots packed at 5px gap. ~12,400 food dots total.

### Spawn points

Agents must start in the void region **away** from food to allow a spiral exploration phase before food contact. The default spawn hints for `sparse_band` and `filled_circle` place agents near the food — not suitable for H3.

**Required**: spawn_point override in the spec JSON run entries.

| Config | Spawn point | Distance to food |
|--------|-------------|------------------|
| H3a (line) | (700, 90) — top center | ~360px above band |
| H3b (circle) | (120, 90) — top-left | ~420px from circle edge |
| H3c (line) | (700, 90) — top center | ~360px above band |
| H3d (circle) | (120, 90) — top-left | ~420px from circle edge |

**Implementation note**: `batch_test.py` currently resolves spawn from the environment name via `resolve_spawn_point()`. A `spawn_point` field in the run entry must be supported, with batch_test preferring it over the environment hint when present.

## Noise Consideration

The agent has rotational diffusion noise (D_ROT = 0.1 rad^2/s). This means:
- The spiral path is not perfectly symmetric — the frame at which the agent first contacts food varies across trials
- Some trials may not reach food at all within 60s (especially for `filled_circle` which is centered while the spiral starts off-center)
- Food encounter timing has high variance — use **means across 1,000 trials** to establish statistical trends
- Phase-split metrics (pre-contact vs post-contact) will only be computed for trials where food contact occurs

## Execution

### Step 1: Run simulations

```bash
cd PY/VisualCube
venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h3_exploitation.json --results-dir tests\eval\results
```

**Output**: `tests/eval/results/<timestamp>_h3_exploitation/logs/` with log files:
- `h3a_composite_sparse_band_trial{0-999}.log`
- `h3b_composite_filled_circle_trial{0-999}.log`
- `h3c_inverter_sparse_band_trial{0-999}.log`
- `h3d_inverter_filled_circle_trial{0-999}.log`

### Step 2: Analyze logs

```bash
venv\Scripts\python src\analyze_logs.py tests\eval\results\
```

**Output**: `tests/eval/results/analysis.json` with per-config aggregated metrics including phase-split metrics.

### Step 3: Validate predictions

```bash
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h3
```

### Step 4: Summary table

Fill in after running:

| Config | spiral_quality (pre) | n_transitions | revisit_rate (post) | area_cells | Expected | Result |
|--------|---------------------|---------------|--------------------:|------------|----------|--------|
| H3a (comp+line) | | | | | spiral > 0.15, trans >= 1 | |
| H3b (comp+circle) | | | | | spiral > 0.15, trans >= 1 | |
| H3c (inv+line) | | | | | spiral < 0.10, trans ~= 0 | |
| H3d (inv+circle) | | | | | spiral < 0.10, trans ~= 0 | |

## Files Involved

| File | Role |
|------|------|
| `tests/eval/specs/h3_exploitation.json` | Test specification (4 configurations) |
| `tests/eval/batch_test.py` | Runs headless simulations; needs spawn_point override |
| `src/trajectory_analyzer.py` | Computes metrics from trajectory data |
| `src/analyze_logs.py` | Bridges logs to analyzer; needs phase-split metrics |
| `tests/eval/validate_hypothesis.py` | Add validate_h3 function |
| `tests/eval/presets/h3a.txt` - `h3d.txt` | Manual verification presets |
| `docs/test_h3_plan.md` | This plan |

## Pass Criteria

H3 passes if **all checks** below hold when comparing means across 1,000 trials:

### Phase 1: Pre-contact spiral (composite only)
- **H3a/b**: `pre_food_spiral_quality` > 0.15 (spiral present before food contact)
- **H3c/d**: `pre_food_spiral_quality` < 0.10 (no spiral — just circle or truncated path)

### Phase 2: Mode transition
- **H3a/b**: `n_transitions` mean >= 1.0 (at least one explore→exploit transition detected)
- **H3c/d**: `n_transitions` mean < 0.5 (no clear mode switch)

### Phase 3: Post-contact tracking
- **H3a/b**: revisitation_rate (post-contact) > revisitation_rate (pre-contact) — agent revisits food area
- **H3a/b**: area coverage growth rate drops after contact — agent stops expanding into new territory

### Phase 4: Composite vs single-inverter comparison
- **H3a area > H3c area**: composite explores more territory than single inverter (same food)
- **H3b area > H3d area**: same for circle food

## Implementation Notes

Before H3 tests can run, these code changes are needed:

1. **`batch_test.py`**: Support `spawn_point` field in run entries. If present, use `(spawn_point[0], spawn_point[1])` directly instead of calling `resolve_spawn_point(environment)`.

2. **`analyze_logs.py`**: Add phase-split metric computation:
   - Parse `food_freq` column from log data
   - Find `first_food_frame` = first frame where food_freq > 0
   - Compute `pre_food_spiral_quality` on trajectory[:first_food_frame]
   - Compute `post_food_revisitation_rate` on trajectory[first_food_frame:]
   - Compare area coverage growth rates before/after contact
   - Add new fields to the aggregated output JSON

3. **`validate_hypothesis.py`**: Add `validate_h3` function and register in VALIDATORS dict as `'h3': ('h3_exploitation', validate_h3)`.

4. **`tests/eval/specs/h3_exploitation.json`**: Create spec with 4 run entries, spawn_point overrides, 60s duration, 1000 trials.
