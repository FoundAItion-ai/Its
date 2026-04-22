# NNN True Inverter Model Design

**Date:** 2026-03-13
**Status:** Approved
**Author:** Claude + User collaboration

## Overview

Implement the true NNN (Natural Neural Network) oscillator model using composable Inverter units. This replaces the current emulation-based CompositeMotionAgent with a mathematically accurate implementation based on the NNN paper.

## Problem Statement

The current `CompositeMotionAgent` produces spiral-like patterns but uses an emulation approach:
- Constant turn rates that sum together
- Delayed activation to simulate oscillator timing
- Not based on actual inverter computation with C1-C4 parameters

The true NNN model defines inverters as computational units with specific input/output relationships that, when combined, produce emergent spiral behavior through physical signal summation.

## NNN Model Specification

### Inverter Unit (1x2)

Each inverter has:
- **1 input**: `f_i` (input signal frequency, e.g., food detection rate)
- **2 outputs**: `f_o1` (left channel), `f_o2` (right channel)
- **4 parameters**: C1, C2, C3, C4

**Computation logic:**
```
When f_i < C1:   f_o1 = C1, f_o2 = C3   (low input mode)
When f_i >= C1:  f_o1 = C2, f_o2 = C4   (high input mode)
```

**Parameter constraints:**
- C1 > C2 (left output decreases when threshold crossed)
- C3 > C4 (right output decreases when threshold crossed)
- C3 > C1 (right > left in low mode, creates turn bias)
- C2 > C4 (left > right in high mode, reverses turn)

### Composite Motion (3x1x2)

Three inverters combined with one crossed (counter-phase):

```
f_o1 (LEFT motor)  = f1_o1 + f2_o1 + f3_o2   (f3's RIGHT goes to LEFT)
f_o2 (RIGHT motor) = f1_o2 + f2_o2 + f3_o1   (f3's LEFT goes to RIGHT)
```

**Threshold ordering:** C3_1 > C2_1 > C1_1
- f1 has lowest threshold (reacts first)
- f3 has highest threshold (reacts last)

### Physical Analogy

Outputs combine like rowers on a boat:
- Each inverter is a pair of oars
- Forces sum physically
- When rowers "disagree" (crossed wiring), they partially cancel
- This creates varying turn rates that produce the spiral

## Architecture

### Component Diagram

```
+------------------+
|    Inverter      |  Pure computation unit
|    (1x2)         |  - 4 params: C1, C2, C3, C4
|                  |  - compute(f_i) -> (f_o1, f_o2)
+------------------+
         |
         v
+--------------------------------------------------------+
|              CompositeMotionAgent                      |
|  +----------+  +----------+  +----------+              |
|  | Inv 1    |  | Inv 2    |  | Inv 3    |  N inverters |
|  | normal   |  | normal   |  | crossed  |              |
|  +----+-----+  +----+-----+  +----+-----+              |
|       |             |             |                    |
|       +-------------+-------------+                    |
|                     |                                  |
|               SUM (rowing boat)                        |
|                     |                                  |
|               (P_r, P_l) -> motors                     |
+--------------------------------------------------------+
```

### Class Design

#### Inverter Class

```python
class Inverter:
    """
    Single NNN Inverter unit (1x2).

    1 input (f_i) -> 2 outputs (f_o1, f_o2)
    """

    def __init__(self, C1: float, C2: float, C3: float, C4: float,
                 name: str = ""):
        self.C1 = C1  # Threshold AND low-mode left output
        self.C2 = C2  # High-mode left output
        self.C3 = C3  # Low-mode right output
        self.C4 = C4  # High-mode right output
        self.name = name
        self._in_high_mode: bool = False

    def compute(self, f_i: float) -> Tuple[float, float]:
        """
        Compute outputs based on input frequency.

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
        return self._in_high_mode
```

#### Configuration Classes

```python
@dataclass
class InverterConfig:
    """Configuration for a single inverter in a composite."""
    C1: float
    C2: float
    C3: float
    C4: float
    crossed: bool = False
    name: str = ""

@dataclass
class CompositeConfig:
    """Configuration for a composite of N inverters."""
    inverters: List[InverterConfig]
```

#### CompositeMotionAgent Refactoring

```python
class CompositeMotionAgent(_BaseAgent):
    """
    NNN Composite Motion using true Inverter units.
    """

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        # Build inverters from config
        inverter_configs = kwargs.get('inverters', [])
        self.inverters: List[Inverter] = []
        self.configs: List[InverterConfig] = []
        # ... initialization

        if not self.inverters:
            self._setup_default_nnn_3x1x2()

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        """Compute motor outputs from all inverters."""
        f_i = self.current_food_frequency

        L_total, R_total = 0.0, 0.0

        for inverter, config in zip(self.inverters, self.configs):
            f_o1, f_o2 = inverter.compute(f_i)

            if config.crossed:
                L_total += f_o2  # Swapped
                R_total += f_o1
            else:
                L_total += f_o1  # Normal
                R_total += f_o2

        return R_total, L_total
```

## GUI Changes

Replace current oscillator panel with inverter configuration:

```
+-------------------------------------------------------------+
| Inverter 1 (f1)                                   [Remove]  |
| +--------+ +--------+ +--------+ +--------+                 |
| | C1: 2.0| | C2: 1.0| | C3: 4.0| | C4: 0.5| [ ] Crossed    |
| +--------+ +--------+ +--------+ +--------+                 |
+-------------------------------------------------------------+
| Inverter 2 (f2)                                   [Remove]  |
| +--------+ +--------+ +--------+ +--------+                 |
| | C1: 3.0| | C2: 1.5| | C3: 5.0| | C4: 0.8| [ ] Crossed    |
| +--------+ +--------+ +--------+ +--------+                 |
+-------------------------------------------------------------+
| Inverter 3 (f3)                                   [Remove]  |
| +--------+ +--------+ +--------+ +--------+                 |
| | C1: 4.0| | C2: 2.0| | C3: 6.0| | C4: 1.0| [x] Crossed    |
| +--------+ +--------+ +--------+ +--------+                 |
+-------------------------------------------------------------+
| [+ Add Inverter]                     [Load NNN 3x1x2]       |
+-------------------------------------------------------------+
```

**Data flow:**
```
GUI Panel -> inverter_configs list -> CompositeMotionAgent.__init__()
```

## Files to Modify

| File | Change |
|------|--------|
| `PY/VisualCube/agent_module.py` | Add `Inverter` class, refactor `CompositeMotionAgent` |
| `PY/VisualCube/GUI.py` | Replace oscillator panel with inverter config panel |
| `PY/VisualCube/tests/test_inverter.py` | New test file |

## Testing Strategy

### Unit Tests for Inverter

```python
def test_low_input_mode():
    """When f_i < C1, outputs are (C1, C3)."""
    inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
    f_o1, f_o2 = inv.compute(f_i=3.0)
    assert f_o1 == 5.0  # C1
    assert f_o2 == 8.0  # C3

def test_high_input_mode():
    """When f_i >= C1, outputs are (C2, C4)."""
    inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
    f_o1, f_o2 = inv.compute(f_i=6.0)
    assert f_o1 == 2.0  # C2
    assert f_o2 == 3.0  # C4
```

### Integration Tests for Composite

```python
def test_crossed_wiring_swaps_outputs():
    """Crossed wiring: L += f_o2, R += f_o1."""
    # Verify f3's outputs are swapped in final sum

def test_nnn_3x1x2_produces_turn_differential():
    """In void (f_i=0), composite produces P_r != P_l."""
    agent = CompositeMotionAgent(inverters=NNN_3x1x2_PRESET)
    P_r, P_l = agent._calculate_power_outputs(sim_time=0.0)
    assert P_r != P_l
```

## Default NNN 3x1x2 Preset

```python
NNN_3x1x2_PRESET = [
    {'C1': 2.0, 'C2': 1.0, 'C3': 4.0, 'C4': 0.5, 'crossed': False, 'name': 'f1'},
    {'C1': 3.0, 'C2': 1.5, 'C3': 5.0, 'C4': 0.8, 'crossed': False, 'name': 'f2'},
    {'C1': 4.0, 'C2': 2.0, 'C3': 6.0, 'C4': 1.0, 'crossed': True,  'name': 'f3'},
]
```

Note: C values may need tuning based on visual results. The constraints are:
- C3_1 (4.0) > C2_1 (3.0) > C1_1 (2.0) for threshold ordering
- Each inverter satisfies C1 > C2, C3 > C4, C3 > C1, C2 > C4

## Success Criteria

1. Inverter class correctly implements NNN 1x2 logic
2. CompositeMotionAgent combines N inverters with normal/crossed wiring
3. NNN 3x1x2 preset produces expanding spiral in void
4. GUI allows adding/removing/configuring inverters
5. All unit tests pass

## Future Extensions

- Visual node-graph editor for wiring
- Inverter output as input to other inverters (neural network topology)
- Save/load composite configurations
- More presets (2x1x2, 5x1x2, etc.)
