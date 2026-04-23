# NNN Agent Simulation

Agent-based simulation of spiking oscillator circuits demonstrating autonomous exploration-exploitation
switching as an emergent property. Built to validate the hypotheses in the companion paper:
*Intelligence from Nothing: How Three Oscillators Produce Untrained Adaptive Behavior*.

## What it does

A composite circuit of three or more frequency-tuned spiking inverters, connected in counter-phase,
autonomously switches between spiral search (exploration) and food tracking (exploitation) without
training or explicit programming. This simulation provides the experimental environment and analysis
tools used to validate that claim across 63,000 independent trials and 115 validation checks.

## Quick start

```bash
python -m venv venv
venv\Scripts\pip install -r requirements.txt   # Windows
venv/bin/pip install -r requirements.txt        # Linux/macOS
```

### GUI

```bash
venv\Scripts\python src/main.py
```

### Unit tests

```bash
venv\Scripts\python -m pytest tests/unit/ -v
```

### Hypothesis evaluation

Run a hypothesis test suite, then validate the results:

```bash
# Run batch simulation (e.g., H0 baseline)
scripts\test.cmd tests\eval\specs\h0_baseline.json

# Or run any hypothesis (H0-H9)
scripts\test.cmd tests\eval\specs\h1_emergence.json
scripts\test.cmd tests\eval\specs\h2_specificity.json
scripts\test.cmd tests\eval\specs\h3_exploitation.json
scripts\test.cmd tests\eval\specs\h4a_resonance.json
scripts\test.cmd tests\eval\specs\h4b_robustness.json
scripts\test.cmd tests\eval\specs\h5_symmetry.json
scripts\test.cmd tests\eval\specs\h6_complexity.json
scripts\test.cmd tests\eval\specs\h7_sensitivity.json
scripts\test.cmd tests\eval\specs\h8_reconciliation.json
scripts\test.cmd tests\eval\specs\h9_negative.json
```

`scripts\test.cmd` runs the batch simulation, log analysis, and hypothesis validation
in sequence. To run validation separately:

```bash
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h0
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h1
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h2
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h3
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h4a
venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h4b
```

## Project structure

```
src/                        Core simulation code
  inverter.py               Spiking inverter (NNN model)
  agent_module.py           InverterAgent, CompositeMotionAgent, StochasticMotionAgent
  math_module.py            Motion dynamics (turn angle, speed)
  trajectory_analyzer.py    Trajectory metrics (spiral quality, circle fit, etc.)
  analyze_logs.py           Batch log analysis pipeline
  GUI.py                    Tkinter GUI
  main.py                   Entry point
  config.py                 Constants and defaults
  optimize_params.py        Headless parameter grid search

scripts/
  test.cmd / test.sh        Run hypothesis evaluation + analysis + validation
  run.cmd / run.sh          Run batch simulations
  unit_test.cmd / .sh       Run unit tests

tests/
  unit/                     146 unit tests (pytest)
  eval/
    batch_test.py           Headless batch simulation runner
    validate_hypothesis.py  Universal hypothesis pass/fail validator
    specs/                  JSON test specifications (H0-H9)
    presets/                Human-readable config snapshots for manual verification
    results/                Generated output (gitignored)

docs/
  NNN_MODEL.md              NNN spiking inverter model reference
  test_h0_plan.md ..        Per-hypothesis test execution plans (H0-H9)
```

## Hypotheses

| ID  | Name           | What it tests                                                  |
|-----|----------------|----------------------------------------------------------------|
| H0  | Baseline       | Single inverter produces circles; speed/radius scale predictably |
| H1  | Emergence      | 3+ inverters produce outward spiral in void                    |
| H2  | Specificity    | Both opposition AND staggering required (ablation)             |
| H3  | Exploitation   | Autonomous spiral-to-tracking mode switch                      |
| H4a | Opposition     | Net opposition strength controls trajectory character          |
| H4b | Robustness     | Graceful degradation under parameter perturbation              |
| H5  | Geometry       | Open spiral catches more food than tight spiral                |
| H6  | Complexity     | More inverters = less food (quality-efficiency paradox)        |
| H7  | Sensitivity    | Spiral is structural, not a noise artifact                     |
| H8  | Reconciliation | More inverters = better spiral quality (resolves H6 paradox)  |
| H9  | Negative       | 11 adversarial configs confirm metric discriminative power     |

## Data

All code, data, and article: https://github.com/FoundAItion-ai/Its

Archived dataset: https://doi.org/10.5281/zenodo.19264202
