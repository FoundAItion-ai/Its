# H1 Emergence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the H1 emergence hypothesis testable end-to-end — update convenience scripts to support multiple specs, verify composite configs work with 5 inverters, run batch simulations, and validate results against pass criteria.

**Architecture:** The batch infrastructure (`batch_test.py`, `analyze_logs.py`, `trajectory_analyzer.py`) already supports composite agents with arbitrary inverter counts. The H1 spec (`h1_emergence.json`) and preset files (`h1a.txt`, `h1b.txt`, `h1c.txt`) are already created. This plan covers the remaining gaps: parameterizing convenience scripts, adding a smoke test for 5-inverter composites, running the full evaluation, and validating results.

**Tech Stack:** Python 3.12, pytest, pygame (headless), numpy

---

### Task 1: Parameterize Convenience Scripts

The scripts `test.cmd` and `test.sh` currently hardcode `h0_baseline.json`. Update them to accept an optional spec argument so any hypothesis can be run.

**Files:**
- Modify: `scripts/test.cmd`
- Modify: `scripts/test.sh`

- [ ] **Step 1: Update test.cmd to accept optional spec argument**

```batch
@echo off
cd /d "%~dp0.."

set SPEC=%~1
if "%SPEC%"=="" set SPEC=tests\eval\specs\h0_baseline.json

for /L %%i in (1,1,10) do (
    echo === Run %%i of 10 ===
    venv\Scripts\python tests\eval\batch_test.py %SPEC% --results-dir tests\eval\results --no-screenshots
)
venv\Scripts\python src\analyze_logs.py tests\eval\results\
```

Usage: `scripts\test.cmd` (defaults to H0) or `scripts\test.cmd tests\eval\specs\h1_emergence.json`

- [ ] **Step 2: Update test.sh to accept optional spec argument**

```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

SPEC="${1:-tests/eval/specs/h0_baseline.json}"

for i in $(seq 1 10); do
    echo "=== Run $i of 10 ==="
    venv/bin/python tests/eval/batch_test.py "$SPEC" --results-dir tests/eval/results --no-screenshots
done
venv/bin/python src/analyze_logs.py tests/eval/results/
```

Usage: `./scripts/test.sh` (defaults to H0) or `./scripts/test.sh tests/eval/specs/h1_emergence.json`

- [ ] **Step 3: Verify scripts parse the argument correctly**

Run: `scripts\test.cmd tests\eval\specs\h1_emergence.json` and confirm it picks up the H1 spec (check the first line of output mentions `h1_emergence`). Cancel after the first trial starts to avoid waiting 120s.

- [ ] **Step 4: Commit**

```bash
git add scripts/test.cmd scripts/test.sh
git commit -m "feat: parameterize test scripts to accept spec file argument"
```

---

### Task 2: Smoke Test — 5-Inverter Composite Agent

Verify that `CompositeMotionAgent` works correctly with 5 inverters and produces valid trajectory data. This catches any issues before committing to the full 120s x 5 trial batch run.

**Files:**
- Create: `tests/unit/test_h1_smoke.py`

- [ ] **Step 1: Write the smoke test**

```python
"""Smoke test: 5-inverter composite agent produces valid trajectory in void."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from agent_module import CompositeMotionAgent
from inverter import recalculate_composite


def test_5_inverter_composite_runs():
    """A 5-inverter composite agent should run for 300 frames without error
    and produce non-zero movement."""
    preset = [
        {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": False, "name": "f1"},
        {"C1": 3.0, "crossed": True, "name": "f2"},
        {"C1": 6.0, "crossed": True, "name": "f3"},
        {"C1": 10.0, "crossed": True, "name": "f4"},
        {"C1": 15.0, "crossed": True, "name": "f5"},
    ]
    recalculate_composite(preset)

    agent = CompositeMotionAgent(
        x=400, y=400,
        inverters=preset,
        speed_scaling_factor=5.0,
        angular_proportionality_constant=0.08,
    )

    xs, ys = [], []
    dt = 1.0 / 60
    for _ in range(300):  # 5 seconds at 60fps
        agent.update(dt, food_frequency=0.0)
        xs.append(agent.x)
        ys.append(agent.y)

    # Agent should have moved (not stuck at origin)
    total_dx = abs(xs[-1] - xs[0])
    total_dy = abs(ys[-1] - ys[0])
    assert total_dx + total_dy > 10, f"Agent barely moved: dx={total_dx}, dy={total_dy}"

    # All 5 inverters should exist
    assert len(agent.inverters) == 5


def test_5_inverter_recalculate_composite_values():
    """recalculate_composite should derive C2/C3/C4 for all 5 inverters
    including the 4th crossed inverter (f5) using diminishing fraction."""
    preset = [
        {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": False, "name": "f1"},
        {"C1": 3.0, "crossed": True, "name": "f2"},
        {"C1": 6.0, "crossed": True, "name": "f3"},
        {"C1": 10.0, "crossed": True, "name": "f4"},
        {"C1": 15.0, "crossed": True, "name": "f5"},
    ]
    recalculate_composite(preset)

    # f5 should have derived values (not missing keys)
    f5 = preset[4]
    assert "C2" in f5, "f5 missing C2"
    assert "C3" in f5, "f5 missing C3"
    assert "C4" in f5, "f5 missing C4"

    # NNN constraint: C3 < C1 < C2 < C4 (period space)
    assert f5["C3"] < f5["C1"] < f5["C2"] < f5["C4"], (
        f"NNN violated for f5: C3={f5['C3']}, C1={f5['C1']}, C2={f5['C2']}, C4={f5['C4']}"
    )

    # f5 opposition should be smaller than f4's (diminishing fractions)
    f4_opposition = 1.0 / preset[3]["C3"] - 1.0 / preset[3]["C1"]
    f5_opposition = 1.0 / f5["C3"] - 1.0 / f5["C1"]
    assert f5_opposition < f4_opposition, (
        f"f5 opposition ({f5_opposition:.4f}) should be < f4 ({f4_opposition:.4f})"
    )
```

- [ ] **Step 2: Run the smoke test**

Run: `cd PY/VisualCube && venv\Scripts\python -m pytest tests/unit/test_h1_smoke.py -v`

Expected: 2 tests PASS

- [ ] **Step 3: Commit**

```bash
git add tests/unit/test_h1_smoke.py
git commit -m "test: smoke test for 5-inverter composite agent (H1)"
```

---

### Task 3: Run H1 Batch Evaluation

Execute the full H1 evaluation: 3 configs x 5 trials x 120 seconds.

**Files:**
- Uses: `tests/eval/specs/h1_emergence.json` (already created)
- Uses: `tests/eval/batch_test.py`
- Output: `tests/eval/results/<timestamp>_h1_emergence/`

- [ ] **Step 1: Run the batch simulation**

```bash
cd PY/VisualCube
venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h1_emergence.json --results-dir tests\eval\results
```

This produces 15 log files (3 configs x 5 trials). Each trial runs 120 seconds at 60fps = 7200 frames. Expect ~15-30 minutes total depending on hardware.

- [ ] **Step 2: Verify log files were created**

```bash
ls tests/eval/results/*h1_emergence*/logs/
```

Expected: 15 `.log` files:
- `h1a_composite_void_trial{0-4}.log`
- `h1b_composite_void_trial{0-4}.log`
- `h1c_composite_void_trial{0-4}.log`

- [ ] **Step 3: Run analysis**

```bash
venv\Scripts\python src\analyze_logs.py tests\eval\results/
```

Expected: `analysis.json` created in the results directory, summary table printed to console.

---

### Task 4: Validate Results Against Pass Criteria

Compare the analysis output against the H1 pass criteria defined in `docs/test_h1_plan.md`.

**Files:**
- Read: `tests/eval/results/<timestamp>_h1_emergence/analysis.json`
- Read: `docs/test_h1_plan.md` (Step 3 and Step 4)

- [ ] **Step 1: Extract metrics from analysis.json**

Read the `analysis.json` and collect these metrics for each config (h1a, h1b, h1c):
- `spiral_quality` (mean, std)
- `growth_rate` (mean, std)
- `circle_fit_score` (mean, std)
- `straightness` (mean, std)
- `area_cells_visited` (mean, std)

- [ ] **Step 2: Evaluate pass criteria**

Check each criterion from the plan:

| Comparison | Metric | Condition | Pass? |
|---|---|---|---|
| H1a spiral detected | spiral_quality | mean > 0.3 | |
| H1a outward expansion | growth_rate | mean > 0 | |
| H1a not a circle | spiral_quality > circle_fit_score | for means | |
| H1a not a line | straightness | mean < 0.3 | |
| H1b >= H1a | spiral_quality | H1b mean + 1 std >= H1a mean | |
| H1c >= H1b | spiral_quality | H1c mean + 1 std >= H1b mean | |
| H1d monotonicity | spiral_quality | H1c >= H1b >= H1a (means) | |
| H1d exploration | area_coverage | non-decreasing (means) | |

- [ ] **Step 3: Fill in the summary table in test_h1_plan.md**

Update the Step 4 table in `docs/test_h1_plan.md` with actual values and pass/fail results.

- [ ] **Step 4: Commit results documentation**

```bash
git add docs/test_h1_plan.md
git commit -m "docs: H1 emergence evaluation results"
```

---

### Task 5: Commit H1 Spec and Preset Files

All the test specification and preset files created during design should be committed together.

**Files:**
- Commit: `tests/eval/specs/h1_emergence.json`
- Commit: `tests/eval/presets/h1a.txt`
- Commit: `tests/eval/presets/h1b.txt`
- Commit: `tests/eval/presets/h1c.txt`
- Commit: `docs/test_h1_plan.md`
- Commit: `.gitignore` (presets/ un-ignored)
- Commit: `PY/VisualCube/CLAUDE.md` (updated structure)

- [ ] **Step 1: Stage and commit all H1 artifacts**

```bash
git add tests/eval/specs/h1_emergence.json
git add tests/eval/presets/h1a.txt tests/eval/presets/h1b.txt tests/eval/presets/h1c.txt
git add docs/test_h1_plan.md
git add ../../.gitignore
git add CLAUDE.md
git commit -m "feat: H1 emergence hypothesis — test plan, spec, and presets"
```

Note: This task can be done before Task 3 (running the batch) — the spec and plan are ready for review regardless of results.
