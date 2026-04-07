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
import logging
import random
from dataclasses import dataclass
from typing import Tuple, List

logger = logging.getLogger(__name__)


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

        # NNN constraint: L/R asymmetry must flip between HIGH and LOW modes.
        # Two valid orderings (period space):
        #   Normal:  C3 < C1 < C2 < C4  (R faster in HIGH, L faster in LOW)
        #   Mirror:  C1 < C3 < C4 < C2  (L faster in HIGH, R faster in LOW)
        # Mixing (e.g. same side faster in both modes) breaks inversion behavior.
        label = f" ({name})" if name else ""
        normal = C3 < C1 < C2 < C4
        mirror = C1 < C3 < C4 < C2
        if normal:
            logger.info(
                "Inverter%s: R-dominant in HIGH, L-dominant in LOW "
                "(C3<C1<C2<C4: %.4f<%.4f<%.4f<%.4f)",
                label, C3, C1, C2, C4,
            )
        elif mirror:
            logger.info(
                "Inverter%s: L-dominant in HIGH, R-dominant in LOW "
                "(C1<C3<C4<C2: %.4f<%.4f<%.4f<%.4f)",
                label, C1, C3, C4, C2,
            )
        else:
            logger.warning(
                "NNN constraint violated%s: C1=%.4f, C2=%.4f, C3=%.4f, C4=%.4f "
                "— must be C3<C1<C2<C4 or C1<C3<C4<C2 (period space)",
                label, C1, C2, C3, C4,
            )

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
            self._left_accumulator = 0.0
            self._left_threshold = self._jittered_period(L_period)

        if self._right_accumulator >= self._right_threshold:
            right_spike = 1
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
        """Return (L_rate, R_rate) in Hz — deterministic base rates.

        Returns the inverse of the current mode's periods:
        - Not active: (0, 0)
        - HIGH (no input): (1/C1, 1/C3)
        - LOW (input detected): (1/C2, 1/C4)

        Organic path variation comes from rotational diffusion noise
        applied at the motor level (biologically: motor neuron noise).
        """
        if not self._active:
            return (0.0, 0.0)
        if self._suppressed:
            return (1.0 / self.C2, 1.0 / self.C4)
        return (1.0 / self.C1, 1.0 / self.C3)


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
# Key design: void rates CLOSE (wide sweeping arcs), food rates FAR (sharp deflection)
# HIGH state (void, active):     L=1/0.15=6.67Hz, R=1/0.135=7.41Hz -> fast, gentle R>L curve
# LOW state (food, suppressed):  L=1/0.45=2.22Hz, R=1/1.35=0.74Hz  -> slow, sharp L>R turn
# Speed ratio: ~4.7x. Tracking: sinusoidal zigzag along food line.
NNN_SINGLE_PRESET = {'C1': 0.15, 'C2': 0.45, 'C3': 0.135, 'C4': 1.35}

# NNN composite preset: inverters for expanding spiral search
# Each inverter satisfies C3 < C1 < C2 < C4 (periods) = C3 > C1 > C2 > C4 (freq)
# f1 matches single preset for tight initial loop. Crossed inverters oppose f1's
# turn differential, widening arcs in discrete steps as each completes its C1 window.
#
# Organic path variation comes from rotational diffusion noise (D_rot) applied
# at the motor level — biologically motivated per-step Gaussian heading noise.
# Rates are deterministic base values (1/period), ensuring stable radius at each stage.
#
# Spiral progression (void mode, linear rate summation):
#   t=0.15s: f1 (normal) fires. diff=+0.74 → tight circle
#   t=3.0s:  f2 (crossed) opposes by 0.20 → diff=+0.54 → wider arc
#   t=6.0s:  f3 (crossed) opposes by 0.11 → diff=+0.43 → wider arc
#   t=10.0s: f4 (crossed) opposes by 0.04 → diff=+0.39 → widest arc
NNN_COMPOSITE_PRESET = [
    {'C1': 0.15, 'C2': 0.45, 'C3': 0.135, 'C4': 1.35, 'crossed': False, 'name': 'f1'},
    {'C1': 3.0,  'C2': 4.0,  'C3': 1.88, 'C4': 8.0,  'crossed': True,  'name': 'f2'},
    {'C1': 6.0,  'C2': 8.0,  'C3': 3.61, 'C4': 15.0, 'crossed': True,  'name': 'f3'},
    {'C1': 10.0, 'C2': 12.0, 'C3': 7.30, 'C4': 20.0, 'crossed': True,  'name': 'f4'},
]


# Default opposition fractions for crossed inverters in the composite.
# Each value is the fraction of f1's turn differential that the crossed
# inverter opposes. Total opposition should be < 1.0 to prevent reversal.
DEFAULT_OPPOSITION_FRACTIONS = [0.27, 0.15, 0.05]

# Default C2/C1 and C4/C2 ratios for auto-generated crossed inverters.
# These satisfy the NNN constraint C3 < C1 < C2 < C4 in period space.
DEFAULT_CROSSED_C2_RATIO = 1.33  # C2 = C1 * ratio
DEFAULT_CROSSED_C4_RATIO = 2.0   # C4 = C2 * ratio


def recalculate_composite(preset: List[dict]) -> List[dict]:
    """Recalculate crossed inverters' C2, C3, C4 based on f1's differential.

    Takes the full composite preset list. f1 (index 0, crossed=False) defines
    the base turn differential. For each crossed inverter, C3 is derived from
    its opposition fraction, and C2/C4 are derived from C1 to satisfy NNN
    constraints.

    Args:
        preset: List of inverter config dicts. Each must have C1, crossed.
                f1 must be index 0 with crossed=False.

    Returns:
        Updated preset list with recalculated C2, C3, C4 for crossed inverters.
    """
    if len(preset) < 2:
        return preset

    f1 = preset[0]
    base_diff = 1.0 / f1['C3'] - 1.0 / f1['C1']  # f1's R-L rate differential

    crossed_idx = 0
    for i in range(1, len(preset)):
        inv = preset[i]
        if not inv.get('crossed', False):
            continue

        # Get opposition fraction (use defaults, cycle if more inverters than fractions)
        if crossed_idx < len(DEFAULT_OPPOSITION_FRACTIONS):
            frac = DEFAULT_OPPOSITION_FRACTIONS[crossed_idx]
        else:
            # Diminishing fractions for extra inverters
            frac = DEFAULT_OPPOSITION_FRACTIONS[-1] * 0.5 ** (crossed_idx - len(DEFAULT_OPPOSITION_FRACTIONS) + 1)
        crossed_idx += 1

        C1 = inv['C1']
        opposition = base_diff * frac

        # C3: 1/C3 = opposition + 1/C1, so C3 = 1 / (opposition + 1/C1)
        C3 = 1.0 / (opposition + 1.0 / C1)

        # C2 and C4 derived from C1 to satisfy C3 < C1 < C2 < C4
        C2 = C1 * DEFAULT_CROSSED_C2_RATIO
        C4 = C2 * DEFAULT_CROSSED_C4_RATIO

        inv['C3'] = round(C3, 4)
        inv['C2'] = round(C2, 4)
        inv['C4'] = round(C4, 4)

    return preset


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
