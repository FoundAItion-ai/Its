# H2 — Specificity of Emergence: Implementation Plan

## Context

H1 proved that 3+ inverter composite agents produce outward spirals in void. H2 asks the harder question: **is this specific to the multi-inverter composite configuration, or could simpler agents produce similar behavior?**

Proving a universal negative ("simpler agents CAN'T spiral") is epistemologically impossible. Instead, H2 is reframed as a **statistical separation claim**: the spiral quality of composite 3+ inverter agents is statistically distinguishable from all simpler configurations under identical conditions. The strongest evidence is a **phase transition** at 3 inverters with the right wiring+staggering combination.

The test also isolates **which ingredients** are necessary for spiral emergence by testing wiring topology and temporal staggering independently.

## Deliverables

1. `PY/VisualCube/docs/test_h2_plan.md` — hypothesis doc (matches H0/H1 format)
2. `PY/VisualCube/tests/eval/specs/h2_specificity.json` — test spec (50 trials, 20s each)
3. `PY/VisualCube/tests/eval/validate_hypothesis.py` — add `validate_h2()` + register in VALIDATORS
4. `Docs/superpowers/plans/2026-04-07-h2-specificity.md` — this plan (moved to final name)

## H2 Scenario Design

Seven scenarios test progressively more complex agents, plus factor isolation:

| ID  | Agent | Wiring | C1 values | What it tests |
|-----|-------|--------|-----------|---------------|
| H2a | Stochastic | — | — | No structure baseline (random walk) |
| H2b | 1 inverter | N | 0.15 | Single oscillator (stable circle from H0) |
| H2c | 2-inv composite | N + C | 0.15, 3.0 | Opposition without 3rd frequency |
| H2d | 2-inv composite | N + N | 0.15, 3.0 | Staggering without opposition |
| H2e | 3-inv composite | N + C + C | 0.15, 0.15, 0.15 | Opposition WITHOUT temporal staggering |
| H2f | 3-inv composite | N + N + N | 0.15, 3.0, 6.0 | Staggering WITHOUT opposition |
| H2g | 3-inv composite | N + C + C | 0.15, 3.0, 6.0 | = H1a positive control (both ingredients) |

### Why these specific scenarios

- **H2a-H2d**: Establish that <3 inverters cannot spiral regardless of wiring
- **H2e**: 3 inverters with opposition but NO staggering (all C1=0.15) — tests if opposition alone is sufficient. Prediction: NO spiral (all inverters activate simultaneously, no progressive arc widening)
- **H2f**: 3 inverters with staggering but NO opposition (all normal wiring) — tests if staggering alone is sufficient. Prediction: NO spiral (all turn same direction, creates faster circle not expanding spiral)
- **H2g**: 3 inverters with BOTH opposition + staggering — positive control, should reproduce H1a spiral
- The **phase transition** is between H2e/H2f (missing one ingredient) and H2g (has both)

### Spec configurations

**H2a — Stochastic**
```json
{"agent_type": "stochastic", "environment": "void", "agent_config": {"update_interval_sec": 1.0}}
```

**H2b — Single inverter (= H0b params)**
```json
{"agent_type": "inverter", "environment": "void", "agent_config": {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35}}
```

**H2c — 2-inv, normal + crossed (standard opposition)**
```json
{"agent_type": "composite", "environment": "void", "agent_config": {"inverters": [
  {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": false, "name": "f1"},
  {"C1": 3.0, "C2": 3.99, "C3": 1.5, "C4": 7.98, "crossed": true, "name": "f2"}
]}}
```

**H2d — 2-inv, both normal (no opposition, staggered C1)**
```json
{"agent_type": "composite", "environment": "void", "agent_config": {"inverters": [
  {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": false, "name": "f1"},
  {"C1": 3.0, "C2": 3.99, "C3": 1.5, "C4": 7.98, "crossed": false, "name": "f2"}
]}}
```

**H2e — 3-inv, N+C+C, ALL SAME C1 (opposition without staggering)**
All three inverters have C1=0.15 so they activate simultaneously. f2/f3 are crossed to oppose f1.
```json
{"agent_type": "composite", "environment": "void", "agent_config": {"inverters": [
  {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": false, "name": "f1"},
  {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": true, "name": "f2"},
  {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": true, "name": "f3"}
]}}
```
Note: f2/f3 with crossed=true and same C1/C3 as f1 means they swap L/R. Net effect: f1 pushes R>L, f2+f3 push L>R. Two crossed vs one normal → net reversal to L-dominant circle. No staggered activation → no spiral.

**H2f — 3-inv, N+N+N, staggered C1 (staggering without opposition)**
```json
{"agent_type": "composite", "environment": "void", "agent_config": {"inverters": [
  {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": false, "name": "f1"},
  {"C1": 3.0, "C2": 3.99, "C3": 1.5, "C4": 7.98, "crossed": false, "name": "f2"},
  {"C1": 6.0, "C2": 7.98, "C3": 2.571, "C4": 15.96, "crossed": false, "name": "f3"}
]}}
```
Note: All normal wiring, all push same direction. Staggered activation adds more power over time but doesn't change turn direction → accelerating circle, not spiral.

**H2g — 3-inv, N+C+C, staggered C1 (= H1a, positive control)**
```json
{"agent_type": "composite", "environment": "void", "agent_config": {"inverters": [
  {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": false, "name": "f1"},
  {"C1": 3.0, "C2": 3.99, "C3": 1.5, "C4": 7.98, "crossed": true, "name": "f2"},
  {"C1": 6.0, "C2": 7.98, "C3": 2.571, "C4": 15.96, "crossed": true, "name": "f3"}
]}}
```

### Run parameters
- 50 trials per scenario, 20 seconds each, 60 fps, void environment
- Matches H1 parameters for direct comparison
- 7 scenarios × 50 trials = 350 simulation runs

## Validation Checks

### Tier 1: Simpler agents don't spiral (statistical separation)
- H2a spiral_quality < 0.15 (stochastic — well below spiral threshold)
- H2b spiral_quality < 0.15 (single inverter — circle, not spiral)
- H2c spiral_quality < 0.15 (2-inv opposition — wider arc, not spiral)
- H2d spiral_quality < 0.15 (2-inv same direction — faster circle, not spiral)

### Tier 2: Factor isolation — neither ingredient alone is sufficient
- H2e spiral_quality < 0.15 (opposition without staggering — reversed circle)
- H2f spiral_quality < 0.15 (staggering without opposition — accelerating circle)

### Tier 3: Both ingredients together produce spiral
- H2g spiral_quality > 0.15 (= H1a, positive control)
- H2g spiral_quality > 2× max(H2a..H2f) spiral_quality (clear separation)

### Tier 4: Phase transition evidence
- Order check: spiral_quality of H2g >> H2e and H2g >> H2f (large gap)
- H2b circle_fit_score > H2g circle_fit_score (circle fits circle better than spiral)

Total: ~12 checks

## Implementation Steps

### Task 1: Create test_h2_plan.md
- Write `PY/VisualCube/docs/test_h2_plan.md` following exact format of test_h0_plan.md / test_h1_plan.md
- Sections: Hypothesis, Mechanism, Sub-hypotheses table, Test Configuration (all 7 scenarios with param tables), Noise Consideration, Execution steps, Pass Criteria, Files Involved

### Task 2: Create h2_specificity.json spec
- Write `PY/VisualCube/tests/eval/specs/h2_specificity.json`
- 7 runs with configs from above, defaults: 50 runs, 20 seconds, void

### Task 3: Add validate_h2() to validate_hypothesis.py
- Add `validate_h2()` function with ~12 checks across all tiers
- Register `'h2': ('h2_specificity', validate_h2)` in VALIDATORS dict
- File: `PY/VisualCube/tests/eval/validate_hypothesis.py`

### Task 4: Update test scripts for h2 auto-detection
- Add h2 detection to `scripts/test.cmd` and `scripts/test.sh` (same pattern as h0/h1)
- Files: `PY/VisualCube/scripts/test.cmd`, `PY/VisualCube/scripts/test.sh`

### Task 5: Run and validate
- Execute: `scripts\test.cmd tests\eval\specs\h2_specificity.json`
- Verify all checks pass
- Iterate on thresholds if needed (noise may push some scenarios above 0.15)

### Task 6: Commit
- Commit all H2 artifacts together

## Verification

```bash
cd PY/VisualCube

# Unit tests still pass
venv\Scripts\python -m pytest tests/unit/ -v

# Run H2 evaluation (50 trials × 7 scenarios × 20s)
scripts\test.cmd tests\eval\specs\h2_specificity.json

# Verify H0/H1 not regressed
scripts\test.cmd tests\eval\specs\h0_baseline.json
scripts\test.cmd tests\eval\specs\h1_emergence.json
```

## Critical Files

| File | Action |
|------|--------|
| `PY/VisualCube/docs/test_h2_plan.md` | Create — hypothesis documentation |
| `PY/VisualCube/tests/eval/specs/h2_specificity.json` | Create — test specification |
| `PY/VisualCube/tests/eval/validate_hypothesis.py` | Modify — add validate_h2() |
| `PY/VisualCube/scripts/test.cmd` | Modify — add h2 auto-detection |
| `PY/VisualCube/scripts/test.sh` | Modify — add h2 auto-detection |
| `Docs/superpowers/plans/2026-04-07-h2-specificity.md` | Create — this plan (final location) |

## Open Question: H2e C3 values for crossed inverters

H2e uses 3 inverters all with C1=0.15 but f2/f3 are crossed. When crossed with identical C1/C3 as f1, the net L-R differential reverses (2 crossed vs 1 normal). This should produce a circle in the opposite direction — confirming opposition alone doesn't create spirals. However, the C2/C4 values for f2/f3 could be the same as f1 (simplest) or derived via recalculate_composite (which would give different C3 based on opposition fraction). Using identical params is cleaner for the isolation test.
