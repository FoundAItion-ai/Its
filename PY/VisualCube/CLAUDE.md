# VisualCube — NNN Agent Simulation

## Project Structure

```
PY/VisualCube/
├── README.md              # Project overview
├── requirements.txt       # Python deps: pygame, matplotlib, pytest, numpy
├── CLAUDE.md              # This file
├── docs/                  # Documentation
│   ├── NNN_MODEL.md       # NNN spiking inverter model spec
│   ├── llm.md             # LLM context doc
│   ├── user_manual.txt    # GUI user manual
│   ├── test_h0_plan.md    # H0 hypothesis test plan
│   └── test_h1_plan.md    # H1 hypothesis test plan
├── src/                   # Source code
│   ├── config.py          # Global constants and defaults
│   ├── main.py            # Entry point (GUI + sim loop)
│   ├── GUI.py             # Tkinter GUI
│   ├── inverter.py        # Core Inverter class (NNN spiking model)
│   ├── agent_module.py    # InverterAgent, CompositeMotionAgent, StochasticMotionAgent
│   ├── math_module.py     # Motion dynamics (turn angle, speed formulas)
│   ├── optimize_params.py # Headless C1-C4 parameter grid search
│   ├── trajectory_analyzer.py  # Trajectory metrics (straightness, circle fit, spiral, etc.)
│   └── analyze_logs.py    # Batch log → trajectory metrics → JSON pipeline
├── scripts/               # Run/test convenience scripts
│   ├── run.cmd / run.sh   # Run batch simulations
│   └── test.cmd / test.sh # Run H0 eval + analysis
└── tests/
    ├── unit/              # pytest unit tests (run with: pytest tests/unit/ -v)
    │   ├── test_inverter.py
    │   ├── test_agent_behavior.py
    │   ├── test_comprehensive.py
    │   └── test_trajectory_analyzer.py
    └── eval/              # Evaluation/integration tests
        ├── batch_test.py  # Headless batch simulation runner
        ├── specs/         # JSON test specifications (H0, composite, etc.)
        ├── presets/       # Human-readable config snapshots (log-header format) for manual verification before batch runs
        └── results/       # Generated eval output (gitignored)
```

## Setup

```bash
python -m venv venv
venv\Scripts\pip install -r requirements.txt   # Windows
venv/bin/pip install -r requirements.txt        # macOS/Linux
```

Requires numpy (installed as pygame dependency).

## Running

- **GUI**: `venv\Scripts\python src\main.py`
- **Unit tests**: `venv\Scripts\python -m pytest tests/unit/ -v`
- **Eval (H0)**: `scripts\test.cmd` (Windows) or `./scripts/test.sh` (macOS/Linux)
- **Batch run**: `scripts\run.cmd` (Windows) or `./scripts/run.sh` (macOS/Linux)

## Key Conventions

- **sys.path**: Test files add `src/` to sys.path via `sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))` (unit tests are 3 levels deep). Eval's `batch_test.py` does the same.
- **Imports**: Source files import each other directly (e.g., `from inverter import Inverter`), relying on `src/` being on the path.
- **NNN model terminology**: HIGH = active/firing (void, no input, C1/C3 periods), LOW = suppressed (food detected, C2/C4 periods). This is inverter OUTPUT state, not input level.
- **Frequency constraint**: f(C3) > f(C1) > f(C2) > f(C4), i.e., C3 < C1 < C2 < C4 in period space.
- **Power model**: `P = RATE_GAIN * rate + BASELINE_POWER` with direct rate (1/period), no smoothing. Path variation comes from rotational diffusion noise (D_ROT=0.1 rad²/s).
- **No cheating**: All emergent behavior must come from the actual model. Never add artificial hacks to make behavior "look right."

## Testing

128 unit tests covering inverter logic, agent behavior, trajectory analysis, and noise discrimination.

```bash
cd PY/VisualCube
venv\Scripts\python -m pytest tests/unit/ -v
```

Eval tests (H0-H4 hypotheses) run via `batch_test.py` with JSON specs, analyzed by `analyze_logs.py`. See `docs/test_h0_plan.md` for the H0 execution guide.
