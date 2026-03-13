"""
NNN Inverter Model - True oscillator-based motion units.

Based on NNN paper: each Inverter is a 1x2 unit with 1 input and 2 outputs.
"""
from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple, List


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


@dataclass
class InverterConfig:
    """Configuration for a single inverter in a composite."""
    C1: float
    C2: float
    C3: float
    C4: float
    crossed: bool = False
    name: str = ""


# NNN 3x1x2 preset: 3 inverters, f3 crossed (counter-phase).
# Threshold ordering: C1_f3 (4) > C1_f2 (3) > C1_f1 (2)
# - f1 reacts first (lowest threshold)
# - f3 reacts last (highest threshold)
NNN_3x1x2_PRESET = [
    {'C1': 2.0, 'C2': 1.0, 'C3': 4.0, 'C4': 0.5, 'crossed': False, 'name': 'f1'},
    {'C1': 3.0, 'C2': 1.5, 'C3': 5.0, 'C4': 0.8, 'crossed': False, 'name': 'f2'},
    {'C1': 4.0, 'C2': 2.0, 'C3': 6.0, 'C4': 1.0, 'crossed': True,  'name': 'f3'},
]


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
