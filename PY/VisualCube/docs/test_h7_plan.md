# H7 — D_ROT Sensitivity (Structural Spiral Proof)

## Hypothesis

The composite oscillator spiral is a structural property of the frequency-beating mechanism, not an artifact of rotational diffusion noise. At D_ROT=0.0 (fully deterministic), the spiral emerges clearly and with the highest detection rate. Noise does not destroy the spiral — it adds heading perturbation that actually increases area coverage while preserving the expanding pattern.

## Mechanism

The spiral arises from temporal staggering of crossed inverters with different C1 windows. Each inverter activates at a different time, progressively widening the arc. This is a deterministic property of the wiring and timing.

Noise (D_ROT) adds per-step Gaussian heading perturbation (`sigma = sqrt(2 * D_ROT * dt)`). This does NOT degrade the spiral monotonically — instead:

- **D_ROT=0.0**: Perfect deterministic spiral. Every trial is identical. Highest detection rate (100%). The spiral is clean but traces the same path, limiting area coverage.
- **D_ROT=0.01-0.10**: Heading variation makes each trial unique. The spiral pattern persists but with organic path variation. Area coverage increases.
- **D_ROT=0.20-0.50**: Heavy noise distorts the spiral shape but the expanding outward pattern still produces more area coverage than the deterministic case.

The key finding: the spiral is structural (exists without noise) AND noise-robust (persists at all tested levels). Noise complements the spiral by adding exploration diversity.

## Sub-hypotheses

| ID   | D_ROT | Prediction |
|------|-------|------------|
| H7a1 | 0.00 | Clean deterministic spiral, quality > 0.20, detection rate > 90% |
| H7a2 | 0.01 | Near-identical to deterministic |
| H7a3 | 0.05 | Spiral with slight path variation |
| H7a4 | 0.10 | Default baseline, quality > 0.10 |
| H7a5 | 0.20 | Noisy but spiral detectable |
| H7a6 | 0.50 | Heavy noise, spiral still present (quality > 0.05), highest area coverage |

## Test Configuration

**Spec file**: `tests/eval/specs/h7_sensitivity.json`

**Duration**: 30s per trial, 60 fps, logging enabled.

**Trials**: 100 per configuration.

**Environment**: void (no food).

**Agent**: 3-inverter composite (same as H1a: C1 spacing 0.15/3.0/6.0).

## Pass Criteria

1. H7a1 (D_ROT=0) spiral_quality > 0.20 — proves structural origin
2. H7a1 (D_ROT=0) detection_rate > 0.90 — deterministic = most reliable
3. H7a4 (D_ROT=0.1) spiral_quality > 0.10 — default noise level works
4. H7a6 (D_ROT=0.5) spiral_quality > 0.05 — heavy noise doesn't destroy it
5. H7a6 area > H7a1 area — noise aids spatial coverage

## Execution

```bash
cd PY/VisualCube
venv\Scripts\python tests/eval/batch_test.py tests/eval/specs/h7_sensitivity.json
venv\Scripts\python src/analyze_logs.py tests/eval/results/h7_sensitivity_*
venv\Scripts\python tests/eval/validate_hypothesis.py tests/eval/results h7
```
