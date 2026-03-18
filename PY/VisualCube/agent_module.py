from __future__ import annotations
import random
from collections import deque
from typing import Tuple, Dict, Any, Type, List

import math_module as mm
from math_module import Vec2D
import config as cfg
from inverter import Inverter, InverterConfig, NNN_SINGLE_PRESET, NNN_COMPOSITE_PRESET, combine_outputs

SMOOTHING_FACTOR = 0.1
FOOD_FREQ_WINDOW_SEC = 0.3  # Shorter window for more responsive mode switching
# Baseline power ensures minimum forward speed (prevents stalling)
BASELINE_POWER = 0.0

# ==============================================================================
# Dendritic Integration Model
# ==============================================================================
#
# Models how biological motor neurons integrate multiple synaptic inputs.
# Real dendrites exhibit two key nonlinearities:
#
# 1. SUBLINEAR SUMMATION (dendritic saturation)
#    When multiple spikes arrive at the same dendrite simultaneously,
#    the postsynaptic response is less than the arithmetic sum.
#    This happens because:
#    - Ion channels near active synapses partially depolarize the membrane,
#      reducing the driving force for neighboring synapses
#    - Glutamate receptor saturation at high input rates
#    Modeled as: effective_input = sqrt(sum of spike²)
#    Two simultaneous spikes give ~1.41x effect, not 2x.
#
# 2. MEMBRANE SATURATION (reversal potential ceiling)
#    The membrane potential cannot exceed the reversal potential of the
#    excitatory ion channels (~0mV for AMPA/NMDA). No matter how many
#    synapses fire, the voltage saturates at this ceiling.
#    Modeled as: output = V_max * tanh(integrated / V_max)
#    This gives a soft ceiling that the integrated signal cannot exceed.
#
# Combined, these produce a response that:
# - Is nearly linear for small inputs (1-2 spikes)
# - Shows diminishing returns for moderate inputs (3-5 spikes)
# - Saturates at a hard ceiling for large inputs (10+ spikes)
# - Works correctly for any number of inverters (1 to 100+)
#
# The V_max parameter represents the physical capacity of the motor:
# the maximum force a muscle can exert, or the maximum voltage a
# capacitor can hold, regardless of input current.
# ==============================================================================


class _BaseAgent:
    def __init__(self, **kwargs: Any) -> None:
        self.food_timestamps: deque = deque()
        self.current_food_frequency: float = 0.0
        self.turn_decision_interval_sec: float = kwargs.get(
            'turn_decision_interval_sec', cfg.DEFAULT_TURN_DECISION_INTERVAL_SEC
        )
        self.last_turn_decision_time: float = -1.0
        # NEW: Track total decisions made
        self.decision_count: int = 0

    def _update_food_frequency(self, food_eaten_count: int, current_sim_time: float) -> None:
        for _ in range(food_eaten_count):
            self.food_timestamps.append(current_sim_time)
        cutoff_time = current_sim_time - FOOD_FREQ_WINDOW_SEC
        while self.food_timestamps and self.food_timestamps[0] < cutoff_time:
            self.food_timestamps.popleft()
        self.current_food_frequency = len(self.food_timestamps) / FOOD_FREQ_WINDOW_SEC

    def _calculate_power_outputs(self, dt: float, is_potential_move: bool, food_eaten_count: int = 0) -> Tuple[float, float]:
        raise NotImplementedError("Agent subclasses must implement _calculate_power_outputs.")

    def update(self, food_eaten_count: int, current_sim_time: float, dt: float, *, is_potential_move: bool = False) -> Tuple[float, float]:
        self._update_food_frequency(food_eaten_count, current_sim_time)

        P_r, P_l = self._calculate_power_outputs(dt, is_potential_move, food_eaten_count)
        
        speed_px_per_sec, _ = mm.get_motion_outputs_from_power(
            P_r, P_l, cfg.AGENT_SPEED_SCALING_FACTOR, cfg.ANGULAR_PROPORTIONALITY_CONSTANT
        )
        final_speed = speed_px_per_sec * cfg.GLOBAL_SPEED_MODIFIER
        distance_this_frame = final_speed * dt
        
        is_turn_frame = current_sim_time - self.last_turn_decision_time >= self.turn_decision_interval_sec

        if is_turn_frame:
            if not is_potential_move:
                self.last_turn_decision_time = current_sim_time
                # NEW: Increment decision counter when a real turn updates
                self.decision_count += 1
            
            _, delta_angle_rad = mm.get_motion_outputs_from_power(
                P_r, P_l, cfg.AGENT_SPEED_SCALING_FACTOR, cfg.ANGULAR_PROPORTIONALITY_CONSTANT
            )
            return distance_this_frame, delta_angle_rad
        else:
            return distance_this_frame, 0.0


class StochasticMotionAgent(_BaseAgent):
    def __init__(self, **kwargs: Any) -> None:
        kwargs['turn_decision_interval_sec'] = kwargs.get('update_interval_sec', 1.0)
        super().__init__(**kwargs)
        
        self.cached_P_r: float = 0.0
        self.cached_P_l: float = 0.0
        self._time_acc: float = 0.0
        self._generate_new_power_choice()
        
    def _generate_new_power_choice(self) -> None:
        R_rand = random.uniform(-3.0, 3.0)
        L_rand = random.uniform(-3.0, 3.0)
        self.cached_P_r = R_rand**2
        self.cached_P_l = L_rand**2

    def _calculate_power_outputs(self, dt: float, is_potential_move: bool, food_eaten_count: int = 0) -> Tuple[float, float]:
        if not is_potential_move:
            self._time_acc += dt
            if self._time_acc >= self.turn_decision_interval_sec:
                self._time_acc -= self.turn_decision_interval_sec
                self._generate_new_power_choice()
        return self.cached_P_r, self.cached_P_l
    

class InverterAgent(_BaseAgent):
    """
    Single Inverter unit - the base NNN model.

    One inverter with 1 input (food frequency) and 2 outputs (L and R).
    Fires discrete signals at periods determined by C1-C4 parameters.
    This is the building block for CompositeMotionAgent.

    Power model: rates drive motor power directly.
    P_l = RATE_GAIN * L_rate + BASELINE_POWER
    P_r = RATE_GAIN * R_rate + BASELINE_POWER

    This means:
    - HIGH state (void, high rates) → faster movement + strong turn
    - LOW state (food, low rates)   → slower movement + gentle turn
    - Asymmetric rates → differential steering
    """

    RATE_GAIN: float = 1.0  # Hz → power conversion
    MIN_POWER: float = 0.1   # Minimum per-side power

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)

        # Create single inverter from params
        self.inverter = Inverter(
            C1=kwargs.get('C1', NNN_SINGLE_PRESET['C1']),
            C2=kwargs.get('C2', NNN_SINGLE_PRESET['C2']),
            C3=kwargs.get('C3', NNN_SINGLE_PRESET['C3']),
            C4=kwargs.get('C4', NNN_SINGLE_PRESET['C4']),
            name='inv0'
        )

        # Track signals for visualization
        self.last_signals: List[Tuple[int, str, int]] = []

    @property
    def num_inverters(self) -> int:
        return 1

    def _calculate_power_outputs(self, dt: float, is_potential_move: bool, food_eaten_count: int = 0) -> Tuple[float, float]:
        """Compute motor outputs from analytical inverter rates."""
        if not is_potential_move:
            # Update inverter: pass food count as input signal
            left_spike, right_spike = self.inverter.update(dt, food_eaten_count)

            # Track signals for visualization
            self.last_signals = []
            if left_spike:
                self.last_signals.append((0, 'L', left_spike))
            if right_spike:
                self.last_signals.append((0, 'R', right_spike))

        # Rates drive motor power directly: more spikes = more power
        L_rate, R_rate = self.inverter.get_rates()

        P_l = max(self.MIN_POWER, self.RATE_GAIN * L_rate + BASELINE_POWER)
        P_r = max(self.MIN_POWER, self.RATE_GAIN * R_rate + BASELINE_POWER)

        return P_r, P_l


class CompositeMotionAgent(_BaseAgent):
    """
    NNN Composite Motion using true spiking Inverter units.

    Each inverter fires discrete spikes (0/1) at periods determined by C1-C4.
    Rates drive motor power directly: P = RATE_GAIN * rate + BASELINE.
    Crossed wiring swaps left/right outputs for counter-phase behavior.

    NNN Composite: multiple inverters with crossed wiring, 2 outputs (L/R motors)
    """

    RATE_GAIN: float = 1.0  # Hz → power conversion
    MIN_POWER: float = 0.1   # Minimum per-side power

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)

        # Track last fired signals for visualization: [(inverter_idx, side, value), ...]
        self.last_signals: List[Tuple[int, str, int]] = []

        # Build inverters from config
        inverter_configs = kwargs.get('inverters', [])
        self.inverters: List[Inverter] = []
        self.configs: List[InverterConfig] = []

        for cfg_dict in inverter_configs:
            inv = Inverter(
                C1=cfg_dict.get('C1', 5.0),
                C2=cfg_dict.get('C2', 2.0),
                C3=cfg_dict.get('C3', 8.0),
                C4=cfg_dict.get('C4', 3.0),
                name=cfg_dict.get('name', '')
            )
            self.inverters.append(inv)
            self.configs.append(InverterConfig(
                C1=inv.C1, C2=inv.C2, C3=inv.C3, C4=inv.C4,
                crossed=cfg_dict.get('crossed', False),
                name=inv.name
            ))

        # Default to NNN composite preset if no config provided
        if not self.inverters:
            self._setup_default_nnn_composite()

    def _setup_default_nnn_composite(self) -> None:
        """Initialize with default NNN composite preset."""
        for preset in NNN_COMPOSITE_PRESET:
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

    @property
    def num_inverters(self) -> int:
        return len(self.inverters)

    def _calculate_power_outputs(self, dt: float, is_potential_move: bool, food_eaten_count: int = 0) -> Tuple[float, float]:
        """
        Compute motor outputs from analytical inverter rates (pure NNN model).

        Each inverter's firing rate is deterministic from its state:
        - Not active: 0 Hz
        - LOW mode: L=1/C1, R=1/C3
        - HIGH mode: L=1/C2, R=1/C4

        Crossed wiring swaps L/R contributions to the motor totals.
        Returns (P_r, P_l) for differential drive.
        """
        if not is_potential_move:
            # Update all inverters and track signals for visualization
            self.last_signals = []
            for idx, inverter in enumerate(self.inverters):
                left_spike, right_spike = inverter.update(dt, food_eaten_count)
                if left_spike:
                    self.last_signals.append((idx, 'L', left_spike))
                if right_spike:
                    self.last_signals.append((idx, 'R', right_spike))

        # Sum rates from all inverters with wiring applied
        L_rate, R_rate = 0.0, 0.0
        for inverter, config in zip(self.inverters, self.configs):
            inv_L, inv_R = inverter.get_rates()
            if config.crossed:
                L_rate += inv_R
                R_rate += inv_L
            else:
                L_rate += inv_L
                R_rate += inv_R

        # Rates drive motor power directly
        P_l = max(self.MIN_POWER, self.RATE_GAIN * L_rate + BASELINE_POWER)
        P_r = max(self.MIN_POWER, self.RATE_GAIN * R_rate + BASELINE_POWER)

        return P_r, P_l

AGENT_CLASSES: Dict[str, Type[_BaseAgent]] = {
    "stochastic": StochasticMotionAgent,
    "inverter": InverterAgent,
    "composite": CompositeMotionAgent,
}