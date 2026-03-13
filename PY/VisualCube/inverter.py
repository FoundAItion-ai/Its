"""
NNN Inverter Model - Spiking oscillator-based motion units.

Based on NNN paper: each Inverter is a 1x2 unit with 1 input and 2 outputs.
Fires discrete spikes (0 or 1) at periods determined by C1-C4 parameters.
"""
from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple, List


class Inverter:
    """
    Single NNN Inverter unit (1x2) - Spiking model.

    Each inverter fires discrete spikes on two output channels (L and R).
    Firing periods are determined by C1-C4 parameters:
    - C1: Period for LEFT channel firing (seconds between spikes)
    - C3: Period for RIGHT channel firing (seconds between spikes)

    When input signal is high (>= threshold), switches to high-rate mode:
    - C2: HIGH mode LEFT period (faster = more frequent spikes)
    - C4: HIGH mode RIGHT period

    Each channel maintains its own accumulator and fires independently.
    """

    def __init__(self, C1: float, C2: float, C3: float, C4: float,
                 name: str = "") -> None:
        self.C1 = C1  # LOW mode: LEFT channel period (also threshold)
        self.C2 = C2  # HIGH mode: LEFT channel period
        self.C3 = C3  # LOW mode: RIGHT channel period
        self.C4 = C4  # HIGH mode: RIGHT channel period
        self.name = name

        # Separate accumulators for each output channel
        self._left_accumulator: float = 0.0
        self._right_accumulator: float = 0.0

        # Mode switch accumulator (for input-driven mode changes)
        self._mode_accumulator: float = 0.0
        self._in_high_mode: bool = False

    def update(self, dt: float, input_signal: float = 0.0) -> Tuple[int, int]:
        """
        Update inverter and return spikes for this timestep.

        Args:
            dt: Time delta in seconds
            input_signal: External input (e.g., food frequency)

        Returns:
            (left_spike, right_spike): Each is 0 or 1
        """
        # Accumulate input for mode switching
        self._mode_accumulator += dt * (1.0 + input_signal)
        if self._mode_accumulator >= self.C1:
            self._in_high_mode = not self._in_high_mode
            self._mode_accumulator = 0.0

        # Get current firing periods based on mode
        if self._in_high_mode:
            left_period = self.C2
            right_period = self.C4
        else:
            left_period = self.C1
            right_period = self.C3

        # Accumulate time on each channel
        self._left_accumulator += dt
        self._right_accumulator += dt

        # Check for spikes
        left_spike = 0
        right_spike = 0

        if self._left_accumulator >= left_period:
            left_spike = 1
            self._left_accumulator = 0.0

        if self._right_accumulator >= right_period:
            right_spike = 1
            self._right_accumulator = 0.0

        return (left_spike, right_spike)

    def compute(self, f_i: float, dt: float = 0.0) -> Tuple[float, float]:
        """
        Legacy compute method - returns spike values as floats.
        For backward compatibility with tests.
        """
        if dt > 0:
            left_spike, right_spike = self.update(dt, f_i)
            return (float(left_spike), float(right_spike))
        else:
            # Instant mode for tests - return period values
            if f_i >= self.C1:
                self._in_high_mode = True
                return (self.C2, self.C4)
            else:
                self._in_high_mode = False
                return (self.C1, self.C3)

    def reset(self) -> None:
        """Reset all accumulators and mode."""
        self._left_accumulator = 0.0
        self._right_accumulator = 0.0
        self._mode_accumulator = 0.0
        self._in_high_mode = False

    @property
    def is_high_mode(self) -> bool:
        """Returns True if inverter is in high-input mode."""
        return self._in_high_mode

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


# NNN 3x1x2 preset: 3 inverters for spiral pattern
# f1: Base oscillator - moderate R bias for circle
# f2: Crossed, same amplitude - creates beat pattern with f1
# f3: Crossed, slower - long-term modulation
# When crossed inverters peak together, they cancel f1 -> straighter path -> expansion
NNN_3x1x2_PRESET = [
    {'C1': 2.0, 'C2': 1.5, 'C3': 3.0, 'C4': 2.5, 'crossed': False, 'name': 'f1'},  # R bias
    {'C1': 3.0, 'C2': 2.0, 'C3': 3.0, 'C4': 2.0, 'crossed': True,  'name': 'f2'},  # Equal L counter
    {'C1': 5.0, 'C2': 4.0, 'C3': 2.0, 'C4': 1.5, 'crossed': True,  'name': 'f3'},  # Slow modulator
]


def combine_outputs(inverters: List[Inverter],
                    configs: List[InverterConfig],
                    f_i: float,
                    dt: float = 0.0) -> Tuple[float, float]:
    """
    Combine N inverter spike outputs into final (L, R) motor signals.

    Each inverter fires discrete spikes (0 or 1) on left and right channels.
    Spikes are routed to motors based on wiring:
    - Normal wiring:  L += left_spike, R += right_spike
    - Crossed wiring: L += right_spike, R += left_spike (swapped)

    The sum of spikes determines motor power for this timestep.

    Args:
        inverters: List of Inverter instances
        configs: List of InverterConfig (must match inverters)
        f_i: Input frequency for all inverters (food)
        dt: Time delta for spike updates

    Returns:
        (L_total, R_total): Combined motor spike counts
    """
    L_total, R_total = 0.0, 0.0

    for inverter, config in zip(inverters, configs):
        if dt > 0:
            left_spike, right_spike = inverter.update(dt, f_i)
        else:
            # Legacy: return period values for tests
            left_spike, right_spike = inverter.compute(f_i, 0)

        if config.crossed:
            # Counter-phase: swap outputs
            L_total += right_spike
            R_total += left_spike
        else:
            # Normal wiring
            L_total += left_spike
            R_total += right_spike

    return L_total, R_total
