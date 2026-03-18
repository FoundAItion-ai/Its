"""
NNN Inverter Model - Frequency inversion through temporal integration.

Core principle: The inverter counts input signals over a full decision window
(C1 period). Only after the window completes does it know the input frequency
and decide whether to fire outputs.

- Input frequency >= threshold → suppress output (high input = no output)
- Input frequency < threshold → emit signals (low input = output at own periods)

Spiral search emerges from STAGGERED ACTIVATION: different inverters have
different C1 periods, so they detect silence at different times after food
stops. Fast inverters react first, slow ones gradually add counter-phase
output, progressively widening the turn radius.

When food is found, ALL inverters reset immediately — restarting their
counting windows and stopping all output.
"""
from __future__ import annotations
import random
from dataclasses import dataclass
from typing import Tuple, List


class Inverter:
    """
    NNN Inverter unit (1x2) — frequency inverter with temporal integration.

    Counts input signals over a decision window of C1 seconds.
    After the window completes, the inverter INVERTS the input:
    - Input detected → SUPPRESSED (low output): fires at C2/C4 periods
    - No input      → ACTIVE (high output):     fires at C1/C3 periods

    Output periods (HIGH state = no input = searching):
    - C1: Decision window length AND left output period in HIGH state
    - C3: RIGHT channel period in HIGH state

    Output periods (LOW state = input detected = suppressed):
    - C2: LEFT channel period in LOW/suppressed state
    - C4: RIGHT channel period in LOW/suppressed state

    The decision window restarts after each evaluation. reset() can be called
    externally (e.g., when food is found) to immediately restart the window
    and stop all output.
    """

    def __init__(self, C1: float, C2: float, C3: float, C4: float,
                 name: str = "", jitter: float = 0.1) -> None:
        self.C1 = C1  # Decision window + LEFT period in HIGH (active) state
        self.C2 = C2  # LEFT period in LOW (suppressed) state
        self.C3 = C3  # RIGHT period in HIGH (active) state
        self.C4 = C4  # RIGHT period in LOW (suppressed) state
        self.name = name
        self.jitter = jitter  # CV of inter-spike interval (0=deterministic, 0.1=10% variation)

        # Decision window: counts input signals over C1 seconds
        self._window_accumulator: float = 0.0
        self._input_count: int = 0

        # Output firing accumulators (only active after decision)
        self._left_accumulator: float = 0.0
        self._right_accumulator: float = 0.0

        # Jittered thresholds for current spike interval
        self._left_threshold: float = 0.0
        self._right_threshold: float = 0.0

        # State: whether this inverter has completed at least one decision window
        self._active: bool = False
        self._suppressed: bool = False  # True when input detected (LOW output)

        # Smoothed rate estimates from actual inter-spike intervals (ISIs).
        # Updated each time a spike fires: rate += (1/ISI - rate) * alpha.
        # This tracks the real jittered firing rate for organic motor variation.
        self._smooth_L_rate: float = 0.0
        self._smooth_R_rate: float = 0.0
        self._rate_alpha: float = 0.3  # EMA factor: ~3 spikes to converge

    def _jittered_period(self, base_period: float) -> float:
        """Apply Gaussian jitter to a firing period.

        Models biological inter-spike interval variability (CV = jitter).
        Result is clamped to [0.5x, 1.5x] base period to prevent extremes.
        """
        if self.jitter <= 0:
            return base_period
        noisy = base_period * (1.0 + random.gauss(0, self.jitter))
        return max(base_period * 0.5, min(noisy, base_period * 1.5))

    def update(self, dt: float, input_count: float = 0.0) -> Tuple[int, int]:
        """
        Update inverter and return spikes for this timestep.

        The inverter operates in two phases:
        1. COUNTING PHASE: accumulate time and input signals until window (C1) completes
        2. FIRING PHASE: emit spikes on L/R channels at own periods (if low input detected)

        Args:
            dt: Time delta in seconds
            input_count: Number of input signals this frame (e.g., food eaten count,
                        or food_frequency * dt for continuous signal)

        Returns:
            (left_spike, right_spike): Each is 0 or 1
        """
        # Accumulate input signals in the current decision window
        self._window_accumulator += dt
        if input_count > 0:
            self._input_count += 1

        # Check if decision window (C1 period) is complete
        if self._window_accumulator >= self.C1:
            # INVERSION decision: input detected → suppress output, no input → fire
            prev_suppressed = self._suppressed
            if self._input_count > 0:
                self._suppressed = True   # Input detected → LOW output state
            else:
                self._suppressed = False  # No input → HIGH output state (active)

            # On mode transition, reset smoothed rates to new base values
            if self._active and self._suppressed != prev_suppressed:
                if self._suppressed:
                    self._smooth_L_rate = 1.0 / self.C2
                    self._smooth_R_rate = 1.0 / self.C4
                else:
                    self._smooth_L_rate = 1.0 / self.C1
                    self._smooth_R_rate = 1.0 / self.C3

            # Carry over excess time into the next window
            self._window_accumulator -= self.C1
            self._input_count = 0

            if not self._active:
                # First activation: pre-load accumulators for immediate first spike
                if self._suppressed:
                    self._left_accumulator = self.C2
                    self._right_accumulator = self.C4
                else:
                    self._left_accumulator = self.C1
                    self._right_accumulator = self.C3
                # Set initial jittered thresholds
                self._left_threshold = self._jittered_period(self._left_accumulator)
                self._right_threshold = self._jittered_period(self._right_accumulator)
                # Initialize smoothed rates to base rates
                if self._suppressed:
                    self._smooth_L_rate = 1.0 / self.C2
                    self._smooth_R_rate = 1.0 / self.C4
                else:
                    self._smooth_L_rate = 1.0 / self.C1
                    self._smooth_R_rate = 1.0 / self.C3

            # Inverter is now active (has made at least one decision)
            self._active = True

        # Only fire if active (has completed at least one decision window)
        if not self._active:
            return (0, 0)

        # Fire at state-dependent periods with spike timing jitter
        # HIGH state (active, no input): L at C1, R at C3 → strong search pattern
        # LOW state (suppressed, input): L at C2, R at C4 → gentle food tracking
        self._left_accumulator += dt
        self._right_accumulator += dt

        left_spike = 0
        right_spike = 0

        if self._suppressed:
            L_period = self.C2
            R_period = self.C4
        else:
            L_period = self.C1
            R_period = self.C3

        if self._left_accumulator >= self._left_threshold:
            left_spike = 1
            # Update smoothed rate from actual ISI before resetting
            isi = self._left_accumulator
            self._smooth_L_rate += (1.0 / isi - self._smooth_L_rate) * self._rate_alpha
            self._left_accumulator = 0.0
            self._left_threshold = self._jittered_period(L_period)

        if self._right_accumulator >= self._right_threshold:
            right_spike = 1
            isi = self._right_accumulator
            self._smooth_R_rate += (1.0 / isi - self._smooth_R_rate) * self._rate_alpha
            self._right_accumulator = 0.0
            self._right_threshold = self._jittered_period(R_period)

        return (left_spike, right_spike)

    def compute(self, f_i: float, dt: float = 0.0) -> Tuple[float, float]:
        """
        Legacy compute method - returns period values for instant evaluation.
        For backward compatibility with tests that don't use temporal integration.
        """
        if dt > 0:
            left_spike, right_spike = self.update(dt, f_i)
            return (float(left_spike), float(right_spike))
        else:
            # Instant mode: return output period values based on threshold
            if f_i >= self.C1:
                self._suppressed = True
                return (self.C2, self.C4)
            else:
                self._suppressed = False
                return (self.C1, self.C3)

    def reset(self) -> None:
        """
        Reset inverter — called when food is found.

        Restarts the decision window, clears input count, stops all output.
        The inverter must complete a full C1 window again before firing.
        This creates the staggered activation effect: after reset, fast
        inverters (small C1) reactivate before slow ones (large C1).
        """
        self._window_accumulator = 0.0
        self._input_count = 0
        self._left_accumulator = 0.0
        self._right_accumulator = 0.0
        self._left_threshold = 0.0
        self._right_threshold = 0.0
        self._active = False
        self._suppressed = False
        self._smooth_L_rate = 0.0
        self._smooth_R_rate = 0.0

    @property
    def is_suppressed(self) -> bool:
        """Returns True if inverter detected input and is in LOW output state."""
        return self._suppressed

    @property
    def is_high_mode(self) -> bool:
        """Deprecated alias — use is_suppressed instead. Returns True when suppressed."""
        return self._suppressed

    @property
    def is_active(self) -> bool:
        """Returns True if inverter has completed at least one decision window."""
        return self._active

    def get_rates(self) -> Tuple[float, float]:
        """Return (L_rate, R_rate) in Hz — smoothed from actual spike ISIs.

        Rates are exponential moving averages of 1/ISI, updated each time
        a spike fires. This reflects the real jittered firing rate, giving
        organic variation in motor output (speed and turn radius vary slightly
        between loops). The EMA smoothing prevents erratic frame-to-frame jumps.

        - Not active: (0, 0)
        - Active: smoothed rates tracking actual inter-spike intervals
        """
        if not self._active:
            return (0.0, 0.0)
        return (self._smooth_L_rate, self._smooth_R_rate)


@dataclass
class InverterConfig:
    """Configuration for a single inverter in a composite."""
    C1: float
    C2: float
    C3: float
    C4: float
    crossed: bool = False
    name: str = ""


# Single inverter defaults: asymmetric inversion for line tracking
# NNN constraint (frequencies): C3 > C1 > C2 > C4
# In period space: C3 < C1 < C2 < C4
#
# Key design: void rates CLOSE (curved sweeping arcs), food rates FAR (sharp deflection)
# HIGH state (void, active):     L=1/0.3=3.33Hz, R=1/0.275=3.64Hz -> fast, gentle R>L curve
# LOW state (food, suppressed):  L=1/0.6=1.67Hz, R=1/1.2=0.83Hz  -> slow, sharp L>R turn
# Void: curved arcs back toward food (~31px radius). Food: quick deflection away.
# Speed ratio: ~2.8x. Tracking: sinusoidal wave along food line.
NNN_SINGLE_PRESET = {'C1': 0.3, 'C2': 0.6, 'C3': 0.28, 'C4': 1.2}

# NNN composite preset: inverters for spiral search
# Each inverter satisfies C3 < C1 < C2 < C4 (periods) = C3 > C1 > C2 > C4 (freq)
# Key: void rates close together (wide arcs), food rates far apart (tight circles)
#
# After food stops, staggered activation creates expanding spiral:
#   t=0.5s: f1 (fastest, normal) fires. Gentle R>L → wide search arc.
#   t=1.0s: f2 (crossed) fires. Opposes f1 → even wider arc.
#   t=2.5s: f3 (crossed) fires. Further opposes → near-straight path.
#
# Phase analysis (void mode, spike rates to motors):
#   f1 normal:  inv L=2.00 R=2.22 → motor L=2.00 R=2.22 → R>L (gentle)
#   f2 crossed: inv L=1.00 R=1.11 → motor L+=1.11 R+=1.00 → adds L>R (opposes)
#   f3 crossed: inv L=0.40 R=0.43 → motor L+=0.43 R+=0.40 → adds L>R (opposes)
NNN_COMPOSITE_PRESET = [
    {'C1': 0.5, 'C2': 0.7, 'C3': 0.45, 'C4': 1.5, 'crossed': False, 'name': 'f1'},
    {'C1': 1.0, 'C2': 1.3, 'C3': 0.9,  'C4': 3.0, 'crossed': True,  'name': 'f2'},
    {'C1': 2.5, 'C2': 3.0, 'C3': 2.3,  'C4': 7.0, 'crossed': True,  'name': 'f3'},
    {'C1': 4.0, 'C2': 4.8, 'C3': 3.7,  'C4': 10.0, 'crossed': True,  'name': 'f4'},
    {'C1': 6.0, 'C2': 7.0, 'C3': 5.5,  'C4': 15.0, 'crossed': True,  'name': 'f5'},
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

    Args:
        inverters: List of Inverter instances
        configs: List of InverterConfig (must match inverters)
        f_i: Input signal for all inverters
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
            L_total += right_spike
            R_total += left_spike
        else:
            L_total += left_spike
            R_total += right_spike

    return L_total, R_total
