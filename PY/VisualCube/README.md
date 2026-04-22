# NNN Agent Simulation

Agent-based simulation of spiking oscillator circuits demonstrating autonomous exploration-exploitation
switching as an emergent property. Built to validate the hypotheses in the companion paper:
*Exploration-exploitation switching as an emergent property of composite frequency-tuned oscillators*.

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

# GUI
venv\Scripts\python src\main.py

# Unit tests
venv\Scripts\python -m pytest tests/unit/ -v

# Hypothesis evaluation (e.g., H0 baseline)
venv\Scripts\python tests/eval/batch_test.py tests/eval/specs/h0_baseline.json
venv\Scripts\python src/analyze_logs.py tests/eval/results/
```

## Project structure

```
src/                    Core simulation code
  inverter.py           Spiking inverter (NNN model)
  agent_module.py       InverterAgent, CompositeMotionAgent, StochasticMotionAgent
  math_module.py        Motion dynamics (turn angle, speed)
  trajectory_analyzer.py  Trajectory metrics (spiral quality, circle fit, etc.)
  analyze_logs.py       Batch log analysis pipeline
  GUI.py                Tkinter GUI
  main.py               Entry point
  config.py             Constants and defaults
  optimize_params.py    Headless parameter grid search

tests/
  unit/                 146 unit tests (pytest)
  eval/
    batch_test.py       Headless batch simulation runner
    specs/              JSON test specifications (H0-H9)
    results/            Generated output (gitignored)

docs/
  NNN_MODEL.md          NNN spiking inverter model reference
  test_h0_plan.md ..    Per-hypothesis test execution plans (H0-H9)
```

## Hypotheses

| ID | Name | What it tests |
|----|------|---------------|
| H0 | Baseline | Single inverter produces circles; speed/radius scale predictably |
| H1 | Emergence | 3+ inverters produce outward spiral in void |
| H2 | Specificity | Both opposition AND staggering required (ablation) |
| H3 | Exploitation | Autonomous spiral-to-tracking mode switch |
| H4a | Opposition | Net opposition strength controls trajectory character |
| H4b | Robustness | Graceful degradation under parameter perturbation |
| H5 | Geometry | Open spiral catches more food than tight spiral |
| H6 | Complexity | More inverters = less food (quality-efficiency paradox) |
| H7 | Sensitivity | Spiral is structural, not a noise artifact |
| H8 | Reconciliation | More inverters = better spiral quality (resolves H6 paradox) |
| H9 | Negative | 11 adversarial configs confirm metric discriminative power |

## Data

All code, data, and article: https://github.com/FoundAItion-ai/Its
