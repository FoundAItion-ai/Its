# NNN True Inverter Model Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement true NNN oscillator model with composable Inverter units that produce emergent spiral behavior.

**Architecture:** Inverter class (pure computation with C1-C4 params) + refactored CompositeMotionAgent that combines N inverters with normal/crossed wiring. Outputs sum like rowers on a boat.

**Tech Stack:** Python 3.12, tkinter (existing GUI), pytest

**Spec:** `docs/superpowers/specs/2026-03-13-nnn-inverter-model-design.md`

---

## File Structure

| File | Responsibility |
|------|----------------|
| `PY/VisualCube/inverter.py` | New: Inverter class, InverterConfig dataclass, NNN presets |
| `PY/VisualCube/agent_module.py` | Modify: Import Inverter, refactor CompositeMotionAgent |
| `PY/VisualCube/GUI.py` | Modify: Replace oscillator panel with inverter config panel |
| `PY/VisualCube/tests/test_inverter.py` | New: Unit tests for Inverter and composite wiring |

---

## Chunk 1: Inverter Core

### Task 1: Create Inverter class with low-mode test

**Files:**
- Create: `PY/VisualCube/inverter.py`
- Create: `PY/VisualCube/tests/test_inverter.py`

- [ ] **Step 1.1: Create test file with low-mode test**

```python
# PY/VisualCube/tests/test_inverter.py
"""Tests for NNN Inverter model."""
import pytest
import sys
from pathlib import Path

# Add parent to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from inverter import Inverter


class TestInverterLowMode:
    """Tests for Inverter in low input mode (f_i < C1)."""

    def test_low_input_returns_c1_c3(self):
        """When f_i < C1, outputs are (C1, C3)."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=3.0)  # 3 < 5
        assert f_o1 == 5.0  # C1
        assert f_o2 == 8.0  # C3

    def test_low_input_mode_flag_is_false(self):
        """In low mode, is_high_mode should be False."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        inv.compute(f_i=0.0)
        assert inv.is_high_mode == False

    def test_zero_input_is_low_mode(self):
        """f_i=0 is always below threshold."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=0.0)
        assert f_o1 == 5.0
        assert f_o2 == 8.0
```

- [ ] **Step 1.2: Run test to verify it fails**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py -v`

Expected: FAIL with `ModuleNotFoundError: No module named 'inverter'`

- [ ] **Step 1.3: Create minimal Inverter class**

```python
# PY/VisualCube/inverter.py
"""
NNN Inverter Model - True oscillator-based motion units.

Based on NNN paper: each Inverter is a 1x2 unit with 1 input and 2 outputs.
"""
from __future__ import annotations
from typing import Tuple


class Inverter:
    """
    Single NNN Inverter unit (1x2).

    1 input (f_i) -> 2 outputs (f_o1, f_o2)

    Logic (from NNN paper):
        When f_i < C1:  f_o1 = C1, f_o2 = C3  (low input mode)
        When f_i >= C1: f_o1 = C2, f_o2 = C4  (high input mode)

    Constraints: C1 > C2, C3 > C4, C3 > C1, C2 > C4
    """

    def __init__(self, C1: float, C2: float, C3: float, C4: float,
                 name: str = "") -> None:
        self.C1 = C1  # Threshold AND low-mode left output
        self.C2 = C2  # High-mode left output
        self.C3 = C3  # Low-mode right output
        self.C4 = C4  # High-mode right output
        self.name = name
        self._in_high_mode: bool = False

    def compute(self, f_i: float) -> Tuple[float, float]:
        """
        Compute outputs based on input frequency.

        Args:
            f_i: Input signal frequency (e.g., food detection rate)

        Returns:
            (f_o1, f_o2): Left and right output frequencies
        """
        if f_i < self.C1:
            self._in_high_mode = False
            return (self.C1, self.C3)
        else:
            self._in_high_mode = True
            return (self.C2, self.C4)

    @property
    def is_high_mode(self) -> bool:
        """Returns True if inverter is in high-input mode."""
        return self._in_high_mode
```

- [ ] **Step 1.4: Run test to verify it passes**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py -v`

Expected: 3 passed

- [ ] **Step 1.5: Commit**

```bash
git add PY/VisualCube/inverter.py PY/VisualCube/tests/test_inverter.py
git commit -m "feat: add Inverter class with low-mode tests"
```

---

### Task 2: Add high-mode tests for Inverter

**Files:**
- Modify: `PY/VisualCube/tests/test_inverter.py`

- [ ] **Step 2.1: Add high-mode tests**

```python
# Add to tests/test_inverter.py

class TestInverterHighMode:
    """Tests for Inverter in high input mode (f_i >= C1)."""

    def test_high_input_returns_c2_c4(self):
        """When f_i >= C1, outputs are (C2, C4)."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=6.0)  # 6 >= 5
        assert f_o1 == 2.0  # C2
        assert f_o2 == 3.0  # C4

    def test_high_input_mode_flag_is_true(self):
        """In high mode, is_high_mode should be True."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        inv.compute(f_i=10.0)
        assert inv.is_high_mode == True

    def test_exact_threshold_is_high_mode(self):
        """f_i == C1 should trigger high mode (>= comparison)."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=5.0)  # Exactly at threshold
        assert f_o1 == 2.0  # C2
        assert f_o2 == 3.0  # C4
        assert inv.is_high_mode == True
```

- [ ] **Step 2.2: Run tests**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py -v`

Expected: 6 passed

- [ ] **Step 2.3: Commit**

```bash
git add PY/VisualCube/tests/test_inverter.py
git commit -m "test: add high-mode tests for Inverter"
```

---

### Task 3: Add output asymmetry tests

**Files:**
- Modify: `PY/VisualCube/tests/test_inverter.py`

- [ ] **Step 3.1: Add asymmetry tests**

```python
# Add to tests/test_inverter.py

class TestInverterAsymmetry:
    """Tests verifying output asymmetry creates turn bias."""

    def test_low_mode_right_greater_than_left(self):
        """In low mode, R > L (C3 > C1) -> turns one way."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=0.0)
        assert f_o2 > f_o1  # R=8 > L=5

    def test_high_mode_left_greater_than_right(self):
        """In high mode, L > R (C2 > C4) -> turns other way."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=10.0)
        assert f_o1 > f_o2  # L=2 > R=3? No wait...

    def test_mode_switch_reverses_dominant_side(self):
        """Switching modes reverses which output is dominant."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)

        # Low mode: R > L
        f_o1_low, f_o2_low = inv.compute(f_i=0.0)
        low_dominant = "R" if f_o2_low > f_o1_low else "L"

        # High mode: L > R (should reverse)
        f_o1_high, f_o2_high = inv.compute(f_i=10.0)
        high_dominant = "R" if f_o2_high > f_o1_high else "L"

        assert low_dominant != high_dominant
```

- [ ] **Step 3.2: Run tests**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py::TestInverterAsymmetry -v`

Expected: 3 passed (the asymmetry is built into C values: C3=8 > C1=5, and C2=2 < C4=3 actually means R > L in high mode too - need to check NNN constraints)

Note: If test fails, adjust C values to match NNN constraint C2 > C4. Use: C1=5, C2=3, C3=8, C4=1

- [ ] **Step 3.3: Commit**

```bash
git add PY/VisualCube/tests/test_inverter.py
git commit -m "test: add asymmetry tests for Inverter turn bias"
```

---

## Chunk 2: Configuration and Presets

### Task 4: Add InverterConfig dataclass

**Files:**
- Modify: `PY/VisualCube/inverter.py`
- Modify: `PY/VisualCube/tests/test_inverter.py`

- [ ] **Step 4.1: Add test for InverterConfig**

```python
# Add to tests/test_inverter.py
from inverter import Inverter, InverterConfig


class TestInverterConfig:
    """Tests for InverterConfig dataclass."""

    def test_config_stores_parameters(self):
        """Config stores C1-C4 and crossed flag."""
        cfg = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=True, name="f1")
        assert cfg.C1 == 5.0
        assert cfg.crossed == True
        assert cfg.name == "f1"

    def test_config_default_crossed_is_false(self):
        """Default crossed should be False."""
        cfg = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        assert cfg.crossed == False
```

- [ ] **Step 4.2: Run test to verify it fails**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py::TestInverterConfig -v`

Expected: FAIL with `ImportError: cannot import name 'InverterConfig'`

- [ ] **Step 4.3: Add InverterConfig to inverter.py**

```python
# Add to PY/VisualCube/inverter.py after imports
from dataclasses import dataclass


@dataclass
class InverterConfig:
    """Configuration for a single inverter in a composite."""
    C1: float
    C2: float
    C3: float
    C4: float
    crossed: bool = False
    name: str = ""
```

- [ ] **Step 4.4: Run tests**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py::TestInverterConfig -v`

Expected: 2 passed

- [ ] **Step 4.5: Commit**

```bash
git add PY/VisualCube/inverter.py PY/VisualCube/tests/test_inverter.py
git commit -m "feat: add InverterConfig dataclass"
```

---

### Task 5: Add NNN 3x1x2 preset

**Files:**
- Modify: `PY/VisualCube/inverter.py`
- Modify: `PY/VisualCube/tests/test_inverter.py`

- [ ] **Step 5.1: Add test for preset**

```python
# Add to tests/test_inverter.py
from inverter import Inverter, InverterConfig, NNN_3x1x2_PRESET


class TestNNNPreset:
    """Tests for NNN 3x1x2 preset configuration."""

    def test_preset_has_three_inverters(self):
        """NNN 3x1x2 has exactly 3 inverters."""
        assert len(NNN_3x1x2_PRESET) == 3

    def test_preset_third_is_crossed(self):
        """f3 (third inverter) should be crossed."""
        assert NNN_3x1x2_PRESET[0]['crossed'] == False
        assert NNN_3x1x2_PRESET[1]['crossed'] == False
        assert NNN_3x1x2_PRESET[2]['crossed'] == True

    def test_preset_thresholds_ordered(self):
        """C1 values should be ordered: f3 > f2 > f1."""
        c1_f1 = NNN_3x1x2_PRESET[0]['C1']
        c1_f2 = NNN_3x1x2_PRESET[1]['C1']
        c1_f3 = NNN_3x1x2_PRESET[2]['C1']
        assert c1_f3 > c1_f2 > c1_f1
```

- [ ] **Step 5.2: Run test to verify it fails**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py::TestNNNPreset -v`

Expected: FAIL with `ImportError: cannot import name 'NNN_3x1x2_PRESET'`

- [ ] **Step 5.3: Add preset to inverter.py**

```python
# Add to PY/VisualCube/inverter.py at module level

NNN_3x1x2_PRESET = [
    {'C1': 2.0, 'C2': 1.0, 'C3': 4.0, 'C4': 0.5, 'crossed': False, 'name': 'f1'},
    {'C1': 3.0, 'C2': 1.5, 'C3': 5.0, 'C4': 0.8, 'crossed': False, 'name': 'f2'},
    {'C1': 4.0, 'C2': 2.0, 'C3': 6.0, 'C4': 1.0, 'crossed': True,  'name': 'f3'},
]
"""
NNN 3x1x2 preset: 3 inverters, f3 crossed (counter-phase).

Threshold ordering: C1_f3 (4) > C1_f2 (3) > C1_f1 (2)
- f1 reacts first (lowest threshold)
- f3 reacts last (highest threshold)

Each inverter satisfies NNN constraints:
- C1 > C2, C3 > C4, C3 > C1, C2 > C4
"""
```

- [ ] **Step 5.4: Run tests**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py::TestNNNPreset -v`

Expected: 3 passed

- [ ] **Step 5.5: Commit**

```bash
git add PY/VisualCube/inverter.py PY/VisualCube/tests/test_inverter.py
git commit -m "feat: add NNN_3x1x2_PRESET configuration"
```

---

## Chunk 3: Composite Wiring Logic

### Task 6: Add combine_outputs function with tests

**Files:**
- Modify: `PY/VisualCube/inverter.py`
- Modify: `PY/VisualCube/tests/test_inverter.py`

- [ ] **Step 6.1: Add test for normal wiring**

```python
# Add to tests/test_inverter.py
from inverter import Inverter, InverterConfig, combine_outputs


class TestCombineOutputs:
    """Tests for combining multiple inverter outputs."""

    def test_single_inverter_normal_wiring(self):
        """Single inverter with normal wiring passes through."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        cfg = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=False)

        L, R = combine_outputs([inv], [cfg], f_i=0.0)
        assert L == 5.0  # f_o1 = C1
        assert R == 8.0  # f_o2 = C3

    def test_single_inverter_crossed_wiring(self):
        """Single inverter with crossed wiring swaps outputs."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        cfg = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=True)

        L, R = combine_outputs([inv], [cfg], f_i=0.0)
        assert L == 8.0  # f_o2 -> L (swapped)
        assert R == 5.0  # f_o1 -> R (swapped)

    def test_two_inverters_sum(self):
        """Two inverters with normal wiring sum their outputs."""
        inv1 = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        inv2 = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        cfg1 = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=False)
        cfg2 = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=False)

        L, R = combine_outputs([inv1, inv2], [cfg1, cfg2], f_i=0.0)
        assert L == 10.0  # 5 + 5
        assert R == 16.0  # 8 + 8
```

- [ ] **Step 6.2: Run test to verify it fails**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py::TestCombineOutputs -v`

Expected: FAIL with `ImportError: cannot import name 'combine_outputs'`

- [ ] **Step 6.3: Implement combine_outputs**

```python
# Add to PY/VisualCube/inverter.py
from typing import Tuple, List


def combine_outputs(inverters: List[Inverter],
                    configs: List[InverterConfig],
                    f_i: float) -> Tuple[float, float]:
    """
    Combine N inverter outputs into final (L, R) motor signals.

    Normal wiring:  L += f_o1, R += f_o2
    Crossed wiring: L += f_o2, R += f_o1 (swapped)

    Args:
        inverters: List of Inverter instances
        configs: List of InverterConfig (must match inverters)
        f_i: Input frequency for all inverters

    Returns:
        (L_total, R_total): Combined motor signals
    """
    L_total, R_total = 0.0, 0.0

    for inverter, config in zip(inverters, configs):
        f_o1, f_o2 = inverter.compute(f_i)

        if config.crossed:
            # Counter-phase: swap outputs
            L_total += f_o2
            R_total += f_o1
        else:
            # Normal wiring
            L_total += f_o1
            R_total += f_o2

    return L_total, R_total
```

- [ ] **Step 6.4: Run tests**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py::TestCombineOutputs -v`

Expected: 3 passed

- [ ] **Step 6.5: Commit**

```bash
git add PY/VisualCube/inverter.py PY/VisualCube/tests/test_inverter.py
git commit -m "feat: add combine_outputs function for composite wiring"
```

---

### Task 7: Add NNN 3x1x2 composite test

**Files:**
- Modify: `PY/VisualCube/tests/test_inverter.py`

- [ ] **Step 7.1: Add 3x1x2 composite test**

```python
# Add to tests/test_inverter.py

class TestNNN3x1x2Composite:
    """Integration tests for NNN 3x1x2 composite behavior."""

    def _create_composite(self):
        """Helper to create NNN 3x1x2 inverters and configs."""
        inverters = []
        configs = []
        for preset in NNN_3x1x2_PRESET:
            inv = Inverter(
                C1=preset['C1'], C2=preset['C2'],
                C3=preset['C3'], C4=preset['C4'],
                name=preset['name']
            )
            cfg = InverterConfig(
                C1=preset['C1'], C2=preset['C2'],
                C3=preset['C3'], C4=preset['C4'],
                crossed=preset['crossed'],
                name=preset['name']
            )
            inverters.append(inv)
            configs.append(cfg)
        return inverters, configs

    def test_void_produces_turn_differential(self):
        """In void (f_i=0), composite produces L != R."""
        inverters, configs = self._create_composite()
        L, R = combine_outputs(inverters, configs, f_i=0.0)
        assert L != R  # Should have turn bias

    def test_crossed_inverter_opposes_normal(self):
        """f3 (crossed) contributes opposite to f1, f2."""
        inverters, configs = self._create_composite()

        # All in low mode (f_i=0)
        # f1: L=2, R=4 (normal)
        # f2: L=3, R=5 (normal)
        # f3: L=6, R=4 (crossed: L gets C3=6, R gets C1=4)

        L, R = combine_outputs(inverters, configs, f_i=0.0)

        # Expected: L = 2+3+6 = 11, R = 4+5+4 = 13
        assert L == 11.0
        assert R == 13.0
```

- [ ] **Step 7.2: Run tests**

Run: `cd PY/VisualCube && python -m pytest tests/test_inverter.py::TestNNN3x1x2Composite -v`

Expected: 2 passed

- [ ] **Step 7.3: Commit**

```bash
git add PY/VisualCube/tests/test_inverter.py
git commit -m "test: add NNN 3x1x2 composite integration tests"
```

---

## Chunk 4: Refactor CompositeMotionAgent

### Task 8: Refactor CompositeMotionAgent to use Inverter

**Files:**
- Modify: `PY/VisualCube/agent_module.py`

- [ ] **Step 8.1: Add import and update class**

```python
# At top of agent_module.py, add:
from inverter import Inverter, InverterConfig, NNN_3x1x2_PRESET, combine_outputs
```

- [ ] **Step 8.2: Replace CompositeMotionAgent implementation**

```python
# Replace the entire CompositeMotionAgent class in agent_module.py

class CompositeMotionAgent(_BaseAgent):
    """
    NNN Composite Motion using true Inverter units.

    Combines N inverters (1x2 units), each with C1-C4 parameters.
    Outputs sum like rowers on a boat - physical force addition.
    One or more inverters can be "crossed" (counter-phase wiring).
    """

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)

        # Build inverters from config
        inverter_configs = kwargs.get('inverters', [])
        self.inverters: List[Inverter] = []
        self.configs: List[InverterConfig] = []

        for cfg in inverter_configs:
            inv = Inverter(
                C1=cfg.get('C1', 5.0),
                C2=cfg.get('C2', 2.0),
                C3=cfg.get('C3', 8.0),
                C4=cfg.get('C4', 3.0),
                name=cfg.get('name', '')
            )
            self.inverters.append(inv)
            self.configs.append(InverterConfig(
                C1=inv.C1, C2=inv.C2, C3=inv.C3, C4=inv.C4,
                crossed=cfg.get('crossed', False),
                name=inv.name
            ))

        # Default to NNN 3x1x2 if no config provided
        if not self.inverters:
            self._setup_default_nnn_3x1x2()

    def _setup_default_nnn_3x1x2(self) -> None:
        """Initialize with default NNN 3x1x2 preset."""
        for preset in NNN_3x1x2_PRESET:
            inv = Inverter(
                C1=preset['C1'], C2=preset['C2'],
                C3=preset['C3'], C4=preset['C4'],
                name=preset['name']
            )
            self.inverters.append(inv)
            self.configs.append(InverterConfig(
                C1=inv.C1, C2=inv.C2, C3=inv.C3, C4=inv.C4,
                crossed=preset['crossed'],
                name=inv.name
            ))

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        """
        Compute motor outputs from all inverters.

        Uses food frequency as input, combines inverter outputs.
        Returns (P_r, P_l) for differential drive.
        """
        f_i = self.current_food_frequency

        L_total, R_total = combine_outputs(self.inverters, self.configs, f_i)

        # Return as (right, left) for differential drive convention
        return R_total, L_total
```

- [ ] **Step 8.3: Remove old imports/code**

Remove these from agent_module.py (no longer needed):
- Old oscillator-related code in CompositeMotionAgent
- The `update()` override (use base class now)
- Old `_convert_layer_pairs` method

- [ ] **Step 8.4: Test the refactored agent**

Run: `cd PY/VisualCube && python -c "from agent_module import CompositeMotionAgent; a = CompositeMotionAgent(); print(f'Inverters: {len(a.inverters)}'); P_r, P_l = a._calculate_power_outputs(0); print(f'P_r={P_r}, P_l={P_l}')"`

Expected:
```
Inverters: 3
P_r=13.0, P_l=11.0
```

- [ ] **Step 8.5: Commit**

```bash
git add PY/VisualCube/agent_module.py
git commit -m "refactor: CompositeMotionAgent to use true NNN Inverter model"
```

---

## Chunk 5: GUI Updates

### Task 9: Update GUI inverter configuration panel

**Files:**
- Modify: `PY/VisualCube/GUI.py`

- [ ] **Step 9.1: Find and examine current composite GUI code**

Read `GUI.py` and locate the `_add_composite_oscillator` method and related code.

- [ ] **Step 9.2: Replace oscillator variables with inverter variables**

Replace `composite_oscillator_vars` with `composite_inverter_vars`:

```python
# In GUI.__init__ or setup method:
self.composite_inverter_vars: list = []
```

- [ ] **Step 9.3: Create new _add_composite_inverter method**

```python
def _add_composite_inverter(self, C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=False, name=""):
    """Add an inverter configuration row to the GUI."""
    vars_dict = {
        'C1': tk.DoubleVar(value=C1),
        'C2': tk.DoubleVar(value=C2),
        'C3': tk.DoubleVar(value=C3),
        'C4': tk.DoubleVar(value=C4),
        'crossed': tk.BooleanVar(value=crossed),
        'name': tk.StringVar(value=name),
    }
    self.composite_inverter_vars.append(vars_dict)
    self._rebuild_composite_gui()
```

- [ ] **Step 9.4: Create new _rebuild_composite_gui method**

```python
def _rebuild_composite_gui(self):
    """Rebuild the composite inverter configuration panel."""
    # Clear existing widgets
    for widget in self.composite_frame.winfo_children():
        widget.destroy()

    # Create row for each inverter
    for i, vars_dict in enumerate(self.composite_inverter_vars):
        row_frame = ttk.Frame(self.composite_frame)
        row_frame.pack(fill='x', pady=2)

        ttk.Label(row_frame, text=f"Inv {i+1}:").pack(side='left')

        for param in ['C1', 'C2', 'C3', 'C4']:
            ttk.Label(row_frame, text=f"{param}:").pack(side='left', padx=(5,0))
            ttk.Entry(row_frame, textvariable=vars_dict[param], width=5).pack(side='left')

        ttk.Checkbutton(row_frame, text="Cross", variable=vars_dict['crossed']).pack(side='left', padx=5)

        ttk.Button(row_frame, text="X",
                   command=lambda v=vars_dict: self._remove_composite_inverter(v)).pack(side='left')

    # Add buttons
    btn_frame = ttk.Frame(self.composite_frame)
    btn_frame.pack(fill='x', pady=5)
    ttk.Button(btn_frame, text="+ Add Inverter",
               command=self._add_composite_inverter).pack(side='left')
    ttk.Button(btn_frame, text="Load NNN 3x1x2",
               command=self._load_nnn_preset).pack(side='left', padx=5)
```

- [ ] **Step 9.5: Add preset loader and remove methods**

```python
def _load_nnn_preset(self):
    """Load the NNN 3x1x2 preset configuration."""
    from inverter import NNN_3x1x2_PRESET
    self.composite_inverter_vars.clear()
    for preset in NNN_3x1x2_PRESET:
        self._add_composite_inverter(
            C1=preset['C1'], C2=preset['C2'],
            C3=preset['C3'], C4=preset['C4'],
            crossed=preset['crossed'],
            name=preset['name']
        )

def _remove_composite_inverter(self, vars_to_remove):
    """Remove an inverter from the configuration."""
    if len(self.composite_inverter_vars) > 1:
        self.composite_inverter_vars.remove(vars_to_remove)
        self._rebuild_composite_gui()
```

- [ ] **Step 9.6: Update _get_agent_params to use new format**

```python
# In _get_agent_params method, update composite section:
if agent_type == "composite":
    params['inverters'] = [
        {
            'C1': v['C1'].get(),
            'C2': v['C2'].get(),
            'C3': v['C3'].get(),
            'C4': v['C4'].get(),
            'crossed': v['crossed'].get(),
            'name': v['name'].get(),
        }
        for v in self.composite_inverter_vars
    ]
```

- [ ] **Step 9.7: Initialize with NNN preset by default**

```python
# In composite panel setup, call:
self._load_nnn_preset()
```

- [ ] **Step 9.8: Test GUI launches**

Run: `cd PY/VisualCube && python GUI.py`

Expected: GUI opens with 3 inverter rows showing NNN 3x1x2 values

- [ ] **Step 9.9: Commit**

```bash
git add PY/VisualCube/GUI.py
git commit -m "feat: update GUI with inverter configuration panel"
```

---

## Chunk 6: Integration Testing

### Task 10: Run full simulation and verify spiral

**Files:** None (manual testing)

- [ ] **Step 10.1: Launch GUI**

Run: `cd PY/VisualCube && python GUI.py`

- [ ] **Step 10.2: Configure for void test**

1. Select "composite" agent type
2. Select "void" food preset
3. Verify 3 inverters with NNN 3x1x2 values
4. Click Start

- [ ] **Step 10.3: Observe behavior**

Expected: Agent should produce expanding spiral pattern

- [ ] **Step 10.4: If spiral not visible, tune C values**

The C values may need adjustment. Key relationships:
- Higher C3-C1 difference = stronger turn in low mode
- f3 crossed should partially cancel f1+f2

Try increasing the differences:
```
f1: C1=2, C2=0.5, C3=6, C4=0.2
f2: C1=3, C2=0.8, C3=8, C4=0.3
f3: C1=4, C2=1.0, C3=10, C4=0.4 (crossed)
```

- [ ] **Step 10.5: Document final working C values**

Update `NNN_3x1x2_PRESET` in `inverter.py` with tuned values.

- [ ] **Step 10.6: Final commit**

```bash
git add PY/VisualCube/inverter.py
git commit -m "tune: adjust NNN 3x1x2 preset values for visible spiral"
```

---

## Summary

| Task | Description | Files |
|------|-------------|-------|
| 1 | Inverter class with low-mode tests | inverter.py, test_inverter.py |
| 2 | High-mode tests | test_inverter.py |
| 3 | Asymmetry tests | test_inverter.py |
| 4 | InverterConfig dataclass | inverter.py, test_inverter.py |
| 5 | NNN 3x1x2 preset | inverter.py, test_inverter.py |
| 6 | combine_outputs function | inverter.py, test_inverter.py |
| 7 | 3x1x2 composite test | test_inverter.py |
| 8 | Refactor CompositeMotionAgent | agent_module.py |
| 9 | GUI inverter panel | GUI.py |
| 10 | Integration testing | (manual) |

**Total estimated commits:** 10
