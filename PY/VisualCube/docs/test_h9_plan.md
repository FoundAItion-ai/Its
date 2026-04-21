# H9 — Negative Controls

## Hypothesis

The H0-H8 validation checks have sufficient discriminative power to reject incorrect or degenerate agent configurations. For each hypothesis, an adversarial ("wrong") config is constructed that intentionally violates the hypothesis mechanism. When the same checks from H0-H8 are applied to these adversarial configs, the checks correctly identify them as failing.

## Mechanism

Each adversarial config removes exactly one necessary ingredient from the target hypothesis:
- **H0**: Symmetric C1=C3 removes turn bias (straight line instead of circle)
- **H1**: All-normal wiring removes opposition (no spiral from staggering alone)
- **H2**: Same C1 for all inverters removes temporal staggering (opposition alone insufficient)
- **H3**: Stochastic agent removes structured dynamics (random walk can't exploit food)
- **H4a**: All-normal wiring removes spiral entirely (Net value irrelevant without opposition)
- **H4b**: 80% C1 jitter destroys the temporal precision needed for spiral coherence
- **H5**: Single inverter can't produce spiral, so can't exploit dual-band geometry
- **H6**: All-normal 5-inv has complexity without opposition (useless complexity)
- **H7**: All-normal at D_ROT=0 has neither opposition nor noise (no spiral at all)
- **H8**: All-normal 3-inv and 7-inv both fail to spiral (no complexity ordering possible)

## Adversarial Configs

| ID | Config Key | Agent | Environment | Strategy |
|----|-----------|-------|-------------|----------|
| neg_h0 | neg_h0_inverter_void | inverter (C1=C3=0.15) | void | Symmetric L/R |
| neg_h1 | neg_h1_composite_void | 3-inv all-normal | void | No opposition |
| neg_h2 | neg_h2_composite_void | 3-inv same-C1 crossed | void | No staggering |
| neg_h3 | neg_h3_stochastic_dense_band | stochastic | dense_band | Random walk |
| neg_h4a | neg_h4a_composite_void | 3-inv all-normal | void | No spiral |
| neg_h4b | neg_h4b_composite_void | 3-inv crossed, 80% jitter | void | Destructive noise |
| neg_h5 | neg_h5_inverter_dense_bands | single inverter | dense_bands | No spiral |
| neg_h6 | neg_h6_composite_dense_bands | 5-inv all-normal | dense_bands | No opposition |
| neg_h7 | neg_h7_composite_void | 3-inv all-normal, D_ROT=0 | void | No opposition |
| neg_h8a | neg_h8a_composite_void | 3-inv all-normal | void | No opposition |
| neg_h8b | neg_h8b_composite_void | 7-inv all-normal | void | No opposition |

## Test Configuration

**Spec file**: `tests/eval/specs/h9_negative.json`

**Defaults**: 30s, 1000 trials, 60fps, logging enabled, log_stride=5.

## Pass Criteria (14 checks)

All checks assert that adversarial configs produce **bad** results. A passing H9 suite means the validators have discriminative power.

| # | Check | Threshold | Rationale |
|---|-------|-----------|-----------|
| 1 | NEG-H0: circle_fit_score | < 0.3 | Symmetric inverter goes straight |
| 2 | NEG-H1: spiral_quality | < 0.15 | No opposition = no spiral |
| 3 | NEG-H2: spiral_quality | < 0.20 | No staggering = no spiral |
| 4 | NEG-H3: pre_food_spiral | < 0.10 | Random walk has no spiral |
| 5 | NEG-H3: food_eaten | < 50 | Random walk can't track food |
| 6 | NEG-H4a: spiral_quality | < 0.15 | All-normal = no spiral |
| 7 | NEG-H4b: spiral_quality | < 0.05 | 80% jitter destroys spiral |
| 8 | NEG-H5: food_eaten | < 10 | Single inverter can't exploit bands |
| 9 | NEG-H6: food_eaten | < 10 | All-normal can't exploit bands |
| 10 | NEG-H7: spiral_quality | < 0.10 | All-normal at D_ROT=0 |
| 11 | NEG-H7: detection_rate | < 0.50 | No spiral to detect |
| 12 | NEG-H8: 3-inv spiral | < 0.10 | All-normal = no spiral |
| 13 | NEG-H8: 7-inv spiral | < 0.10 | All-normal = no spiral |
| 14 | NEG-H8: no ordering | 7-inv NOT >> 3-inv | Neither spirals |

## Execution

```bash
# Windows
scripts\test.cmd tests\eval\specs\h9_negative.json

# macOS/Linux
./scripts/test.sh tests/eval/specs/h9_negative.json
```
