# H2 — Specificity of Emergence

## Hypothesis

The spiral behavior observed in H1 is specific to the multi-inverter (composite) configuration with both **crossed wiring (opposition)** and **temporal staggering (different C1 periods)**. Simpler agents — stochastic, single-inverter, and 2-inverter composites — do not produce coherent expanding spirals. Furthermore, 3-inverter composites missing either ingredient (opposition alone, or staggering alone) also fail to spiral. This establishes that the spiral is a genuine emergent property requiring the combination of frequency opposition and staggered activation, not an artifact of movement equations, random noise, or simple oscillator dynamics.

**Epistemological note:** Proving a universal negative ("simpler agents CAN'T spiral") is impossible. Instead, H2 is a **statistical separation claim**: the spiral quality of composite 3+ inverter agents (with correct wiring) is statistically distinguishable from all simpler or incomplete configurations under identical conditions.

## Mechanism

Spiral emergence requires two ingredients working together:

1. **Crossed wiring (opposition):** Counter-phase inverters oppose the primary turn differential, progressively widening the arc radius as each crossed inverter activates.
2. **Temporal staggering (different C1 periods):** Each inverter has a different decision window (C1), so they activate at different times. This creates a time-varying differential — the arc widens in discrete steps.

Without opposition (all normal wiring), staggered activation just adds more power in the same turn direction — a faster circle, not a spiral. Without staggering (all same C1), opposition creates an instant net differential — a fixed circle (possibly reversed), not a progressively widening one.

## Sub-hypotheses

| ID  | Agent Config | Wiring | C1 Values | Prediction |
|-----|-------------|--------|-----------|------------|
| H2a | Stochastic | — | — | Random walk — no spiral, high revisitation |
| H2b | 1 inverter | N | 0.15 | Stable circle — no spiral (= H0b) |
| H2c | 2-inv composite | N + C | 0.15, 3.0 | Wider arc — no spiral (opposition, insufficient frequencies) |
| H2d | 2-inv composite | N + N | 0.15, 3.0 | Faster circle — no spiral (staggering, no opposition) |
| H2e | 3-inv composite | N + C + C | 0.15, 0.15, 0.15 | Reversed circle — no spiral (opposition WITHOUT staggering) |
| H2f | 3-inv composite | N + N + N | 0.15, 3.0, 6.0 | Accelerating circle — no spiral (staggering WITHOUT opposition) |
| H2g | 3-inv composite | N + C + C | 0.15, 3.0, 6.0 | Outward spiral — positive control (= H1a, both ingredients) |

**Key distinction from other hypotheses:**
- H0 proved single inverters make circles/lines (baseline dynamics)
- H1 proved 3+ inverters with correct wiring make spirals (emergence)
- H2 proves the spiral requires BOTH opposition + staggering (specificity)
- H2e and H2f are the critical discriminators — they isolate each ingredient

## Test Configuration

**Spec file**: `tests/eval/specs/h2_specificity.json`

### H2a — Stochastic agent (no oscillators)

Random power every 1 second. No internal structure.

| Parameter | Value |
|-----------|-------|
| agent_type | stochastic |
| update_interval_sec | 1.0 |

### H2b — Single inverter (= H0b params)

| Inv | C1 | C2 | C3 | C4 | Crossed | L freq (Hz) | R freq (Hz) |
|-----|------|------|-------|------|---------|-------------|-------------|
| f1  | 0.15 | 0.45 | 0.135 | 1.35 | No | 6.67 | 7.41 |

### H2c — 2-inverter, normal + crossed

Standard opposition pair. One beat frequency, constant differential after t=3s.

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|------|-------|------|---------|
| f1  | 0.15 | 0.45 | 0.135 | 1.35 | No |
| f2  | 3.0  | 3.99 | 1.5   | 7.98 | Yes |

### H2d — 2-inverter, both normal

Same C1 staggering as H2c, but no opposition. Both push same turn direction.

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|------|-------|------|---------|
| f1  | 0.15 | 0.45 | 0.135 | 1.35 | No |
| f2  | 3.0  | 3.99 | 1.5   | 7.98 | No |

### H2e — 3-inverter, opposition WITHOUT staggering (all C1=0.15)

All three inverters activate simultaneously (same C1). f2/f3 are crossed with identical params as f1. Net effect: 2 crossed vs 1 normal → L-dominant circle (reversed direction). No temporal progression → no spiral.

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|------|-------|------|---------|
| f1  | 0.15 | 0.45 | 0.135 | 1.35 | No |
| f2  | 0.15 | 0.45 | 0.135 | 1.35 | Yes |
| f3  | 0.15 | 0.45 | 0.135 | 1.35 | Yes |

### H2f — 3-inverter, staggering WITHOUT opposition (all normal)

Same C1 staggering as H1a, but all normal wiring. All push R>L. Staggered activation adds more power in same direction → faster circle with stepped speed, not expanding spiral.

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|------|-------|-------|---------|
| f1  | 0.15 | 0.45 | 0.135 | 1.35  | No |
| f2  | 3.0  | 3.99 | 1.5   | 7.98  | No |
| f3  | 6.0  | 7.98 | 2.571 | 15.96 | No |

### H2g — 3-inverter, positive control (= H1a)

Both opposition and staggering present. Should reproduce H1a spiral.

| Inv | C1 | C2 | C3 | C4 | Crossed |
|-----|------|------|-------|-------|---------|
| f1  | 0.15 | 0.45 | 0.135 | 1.35  | No |
| f2  | 3.0  | 3.99 | 1.5   | 7.98  | Yes |
| f3  | 6.0  | 7.98 | 2.571 | 15.96 | Yes |

**Environment**: `void` (no food) — all agents stay in HIGH mode.

**Run parameters**: 50 trials x 20 seconds each, 60 fps, logging enabled.

## Noise Consideration

The agent has rotational diffusion noise (D_ROT = 0.1 rad^2/s), which adds ~3.3 deg/frame random heading perturbation. This means:
- Stochastic agents may occasionally produce spiral-like trajectories by chance
- Single-inverter circles will drift spatially (drifting circle, not true spiral)
- All comparisons use **mean +/- std across 50 trials** to separate systematic from noise-induced behavior
- The 0.15 spiral_quality threshold provides margin above typical noise artifacts (~0.05-0.10)

## Execution

### Step 1: Run simulations

```bash
cd PY/VisualCube
scripts\test.cmd tests\eval\specs\h2_specificity.json
```

**Output**: `tests/eval/results/<timestamp>_h2_specificity/logs/` with 350 log files (7 configs x 50 trials).

### Step 2: Review analysis output

The test script automatically runs `analyze_logs.py` and `validate_hypothesis.py h2`.

### Step 3: Validate predictions

#### Tier 1 — Simpler agents don't spiral (statistical separation)
- **Check**: H2a spiral_quality < 0.15
- **Check**: H2b spiral_quality < 0.15
- **Check**: H2c spiral_quality < 0.15
- **Check**: H2d spiral_quality < 0.15

#### Tier 2 — Neither ingredient alone is sufficient
- **Check**: H2e spiral_quality < 0.15 (opposition without staggering)
- **Check**: H2f spiral_quality < 0.15 (staggering without opposition)

#### Tier 3 — Both ingredients together produce spiral
- **Check**: H2g spiral_quality > 0.15 (positive control)
- **Check**: H2g spiral_quality > 2x max(H2a..H2f) (clear separation)

#### Tier 4 — Phase transition evidence
- **Check**: H2g spiral_quality >> H2e (gap from adding staggering)
- **Check**: H2g spiral_quality >> H2f (gap from adding opposition)
- **Check**: H2b circle_fit_score > H2g circle_fit_score (circle fits circle better)

### Step 4: Summary table

Fill in after running:

| Comparison | Metric | Value | Expected | Result |
|------------|--------|-------|----------|--------|
| H2a no spiral | spiral_quality | | < 0.15 | |
| H2b no spiral | spiral_quality | | < 0.15 | |
| H2c no spiral | spiral_quality | | < 0.15 | |
| H2d no spiral | spiral_quality | | < 0.15 | |
| H2e opposition only | spiral_quality | | < 0.15 | |
| H2f staggering only | spiral_quality | | < 0.15 | |
| H2g positive control | spiral_quality | | > 0.15 | |
| H2g separation | spiral_quality ratio | | > 2x max(a-f) | |
| H2g >> H2e | spiral_quality gap | | large | |
| H2g >> H2f | spiral_quality gap | | large | |
| H2b vs H2g | circle_fit_score | | H2b > H2g | |

## Files Involved

| File | Role |
|------|------|
| `tests/eval/specs/h2_specificity.json` | Test specification (7 scenarios) |
| `tests/eval/validate_hypothesis.py` | Validation checks (validate_h2 function) |
| `tests/eval/batch_test.py` | Runs headless simulations, produces logs |
| `src/trajectory_analyzer.py` | Computes metrics from trajectory data |
| `src/analyze_logs.py` | Bridges logs to analyzer, aggregates across trials |
| `scripts/test.cmd` | Convenience script: run + analyze + validate (Windows) |
| `scripts/test.sh` | Convenience script: run + analyze + validate (macOS/Linux) |

## Pass Criteria

H2 passes if **all ~12 checks** hold when comparing means across 50 trials. Specifically:
- "< 0.15" means the mean spiral_quality is below the threshold
- "> 0.15" means the mean spiral_quality exceeds the threshold
- "> 2x" means the ratio of means exceeds 2.0
- ">>" means a visually obvious gap in the metrics table

The strongest evidence for specificity is the **phase transition**: H2e and H2f (each missing one ingredient) score similarly to simpler agents, while H2g (both ingredients) jumps to spiral-level quality.
