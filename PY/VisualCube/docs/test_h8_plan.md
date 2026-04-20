# H8 — Reconciliation (H1 vs H6 Consistency)

## Hypothesis

The H6 agent configurations (3-7 inverters at fixed Net=+0.22) produce better spiral quality and higher detection rates with more inverters when tested in void. This is the REVERSE of the H6 food ordering (fewer inverters = more food eaten) and confirms that H1 and H6 measure different things:

- **H8 (void)**: More inverters = higher spiral quality and detection rate
- **H6 (food)**: Fewer inverters = faster mode switching = more food found

## Mechanism

Each additional crossed inverter adds a longer-C1 temporal stage that creates a more structured, tighter spiral. In void, this improves spiral quality (better fit to the mathematical spiral envelope) and detection reliability. But in food environments, high-C1 inverters (e.g., C1=16s) take their full C1 window to activate, delaying the explore-to-exploit transition. The 3-inverter config makes decisive turns earlier because all inverters activate within 1.5s.

Note: more inverters produce higher spiral *quality* (structure) but not necessarily more *area coverage* — the tighter spiral traces a more compact path. Area coverage is influenced by speed, noise, and wandering, which favor fewer constraints.

## Sub-hypotheses

| ID   | Inverters | Spiral Quality | Detection Rate | H6 Food (reference) |
|------|-----------|---------------|----------------|---------------------|
| H8a1 | 3 | Lowest | Lowest | Best (749.8) |
| H8a2 | 4 | Low | Low | 704.5 |
| H8a3 | 5 | Medium | Medium | 641.1 |
| H8a4 | 6 | High | High | 592.7 |
| H8a5 | 7 | Highest | Highest | Worst (521.8) |

## Test Configuration

**Spec file**: `tests/eval/specs/h8_reconciliation.json`

**Duration**: 40s per trial (matches H1 duration), 60 fps, logging enabled.

**Trials**: 100 per configuration.

**Environment**: void (no food).

**Agents**: Identical to H6 configs (same inverter parameters, same Net=+0.22).

## Pass Criteria

1. H8a5 (7 inv) spiral_quality > H8a1 (3 inv) — reverse of H6
2. H8a5 spiral > 1.5x H8a1 — clear separation
3. H8a5 detection_rate > 0.80 — most reliable spiral
4. H8a5 detection > H8a1 detection — more inverters = more reliable
5. H8a1 (3 inv, weakest) spiral > 0.10 — all configs produce spirals

## Paper Note

"More inverters produce higher-quality spirals with greater detection reliability (H8), confirming the emergence trend from H1. But in food environments, each additional high-C1 inverter delays mode switching, so fewer inverters find more food (H6). This reveals an exploration-exploitation tradeoff intrinsic to the architecture: the same property that enables structured spiral search (temporal staggering across many stages) slows the transition to focused tracking."

## Execution

```bash
cd PY/VisualCube
venv\Scripts\python tests/eval/batch_test.py tests/eval/specs/h8_reconciliation.json
venv\Scripts\python src/analyze_logs.py tests/eval/results/h8_reconciliation_*
venv\Scripts\python tests/eval/validate_hypothesis.py tests/eval/results h8
```
