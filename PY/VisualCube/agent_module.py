from __future__ import annotations
import math
import random
from collections import deque
from typing import Tuple, Dict, Any, Type, List

import math_module as mm
from math_module import Vec2D
import config as cfg
from inverter import Inverter, InverterConfig, NNN_3x1x2_PRESET, combine_outputs

SMOOTHING_FACTOR = 0.1
FOOD_FREQ_WINDOW_SEC = 0.3  # Shorter window for more responsive mode switching
# Baseline power ensures minimum forward speed (prevents stalling)
BASELINE_POWER = 2.0

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

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        raise NotImplementedError("Agent subclasses must implement _calculate_power_outputs.")

    def update(self, food_eaten_count: int, current_sim_time: float, dt: float, *, is_potential_move: bool = False) -> Tuple[float, float]:
        self._update_food_frequency(food_eaten_count, current_sim_time)
        
        P_r, P_l = self._calculate_power_outputs(current_sim_time)
        
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
        self._generate_new_power_choice()
        
    def _generate_new_power_choice(self) -> None:
        R_rand = random.uniform(-3.0, 3.0)
        L_rand = random.uniform(-3.0, 3.0)
        self.cached_P_r = R_rand**2
        self.cached_P_l = L_rand**2

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        if sim_time - self.last_turn_decision_time >= self.turn_decision_interval_sec:
            self._generate_new_power_choice()
        return self.cached_P_r, self.cached_P_l
    

class InverseMotionAgent(_BaseAgent):
    """
    NNN Paper 'Inverse Motion' Model:
    - Agent ALWAYS turns in arcs (never straight)
    - LOW food (f_i < threshold): FAST speed, turn in direction A (e.g., RIGHT)
    - HIGH food (f_i >= threshold): SLOW speed, turn in direction B (OPPOSITE, e.g., LEFT)

    When food state changes, turn direction REVERSES.
    This creates: large arcs when searching, small arcs when found food (stays in area).

    Hysteresis prevents oscillation when grazing food lines:
    - Enter HIGH mode when food_freq >= threshold
    - Exit HIGH mode only when food_freq drops to ~0
    """
    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.threshold_r: float = kwargs['threshold_r']
        self.threshold_l: float = kwargs.get('threshold_l', kwargs['threshold_r'])

        # Speed settings: C1 > C2 (fast when no food, slow when food)
        self.low_food_speed: float = kwargs.get('r1_amp', 10.0)   # C1: Fast when no food
        self.high_food_speed: float = kwargs.get('r2_amp', 2.0)   # C2: Slow when food found

        # Turn rate (how tight the arcs are)
        self.turn_rate: float = kwargs.get('turn_rate', 0.5)  # radians per second

        # State tracking
        self._in_high_food_mode: bool = False
        self._last_mode_switch_time: float = -10.0  # Allow immediate first switch
        self._mode_cooldown_sec: float = 0.3  # Cooldown to prevent oscillation

        # Turn direction: +1 = RIGHT, -1 = LEFT
        # Only reverses when FOOD IS FOUND (not when food is lost)
        self._turn_direction: int = 1  # Start turning RIGHT

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        """Not used in new implementation - keeping for compatibility."""
        if self._in_high_food_mode:
            return self.high_food_speed, self.high_food_speed
        else:
            return self.low_food_speed, self.low_food_speed

    def update(self, food_eaten_count: int, current_sim_time: float, dt: float, *, is_potential_move: bool = False) -> Tuple[float, float]:
        """NNN Inverse Motion: always turning, direction depends on food state."""
        self._update_food_frequency(food_eaten_count, current_sim_time)

        # Check if cooldown has passed
        can_switch = (current_sim_time - self._last_mode_switch_time) >= self._mode_cooldown_sec

        if can_switch:
            if not self._in_high_food_mode:
                # LOW mode: enter HIGH if food_freq >= threshold
                if self.current_food_frequency >= self.threshold_r:
                    self._in_high_food_mode = True
                    self._last_mode_switch_time = current_sim_time
                    # REVERSE direction when food is FOUND!
                    self._turn_direction *= -1
            else:
                # HIGH mode: exit when food_freq drops
                if self.current_food_frequency < 0.1:
                    self._in_high_food_mode = False
                    self._last_mode_switch_time = current_sim_time
                    # REVERSE direction when food DISAPPEARS too!
                    # This creates zigzag tracking pattern
                    self._turn_direction *= -1

        # Speed: fast when no food (C1), slow when food (C2)
        if self._in_high_food_mode:
            speed = self.high_food_speed * cfg.AGENT_SPEED_SCALING_FACTOR * cfg.GLOBAL_SPEED_MODIFIER
        else:
            speed = self.low_food_speed * cfg.AGENT_SPEED_SCALING_FACTOR * cfg.GLOBAL_SPEED_MODIFIER

        distance_this_frame = speed * dt

        # Turn direction is remembered and only reverses when food is FOUND
        # Direction persists when food disappears (agent keeps going same way)
        # HIGH mode: FAST turn rate for sharp zigzag tracking pattern
        # LOW mode: slower turn rate for gentle searching arcs
        if self._in_high_food_mode:
            effective_turn_rate = self.turn_rate * 4.0  # 4x faster turn when tracking food
        else:
            effective_turn_rate = self.turn_rate

        base_turn = self._turn_direction * effective_turn_rate * dt

        # Stochastic noise for organic movement (30% variation)
        turn_noise = random.gauss(0, effective_turn_rate * dt * 0.3)
        turn_angle = base_turn + turn_noise

        # Also vary speed slightly (±20%) to break up perfect circles
        speed_factor = 1.0 + random.gauss(0, 0.2)
        distance_this_frame *= max(0.5, speed_factor)  # Clamp to avoid negative

        if not is_potential_move:
            self.decision_count += 1

        return distance_this_frame, turn_angle


class CompositeMotionAgent(_BaseAgent):
    """
    NNN Composite Motion using true spiking Inverter units.

    Each inverter fires discrete spikes (0/1) at periods determined by C1-C4.
    Spikes are integrated with exponential decay to produce smooth motor signals.
    Crossed wiring swaps left/right outputs for counter-phase behavior.

    NNN 3x1x2: 3 inverters, 1 crossed, 2 outputs (L/R motors)
    """

    # Spike integration parameters
    SPIKE_STRENGTH: float = 3.0  # Power boost per spike
    DECAY_RATE: float = 5.0  # Exponential decay per second (faster decay)
    BASE_POWER: float = 2.0  # Minimum power to prevent stalling

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)

        # Track time for dt calculation
        self._last_compute_time: float = 0.0

        # Integrated motor signals (smoothed from spikes)
        self._left_integrated: float = 0.0
        self._right_integrated: float = 0.0

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
        Compute motor outputs from spiking inverters.

        Each inverter contributes to motor power based on its current mode.
        In LOW mode: contributes (C1, C3) as power levels
        In HIGH mode: contributes (C2, C4) as power levels
        Crossed wiring swaps L/R contributions.

        The staggered mode switching creates the spiral pattern.

        Returns (P_r, P_l) for differential drive.
        """
        # Calculate dt for mode updates
        dt = sim_time - self._last_compute_time if self._last_compute_time > 0 else 1.0/60.0
        self._last_compute_time = sim_time

        f_i = self.current_food_frequency

        # Update all inverter states and sum power contributions
        L_total, R_total = 0.0, 0.0

        for inverter, config in zip(self.inverters, self.configs):
            # Update inverter (this advances its internal state)
            inverter.update(dt, f_i)

            # Get power contribution based on current mode
            if inverter.is_high_mode:
                left_power = inverter.C2
                right_power = inverter.C4
            else:
                left_power = inverter.C1
                right_power = inverter.C3

            # Apply wiring (crossed swaps L/R)
            if config.crossed:
                L_total += right_power
                R_total += left_power
            else:
                L_total += left_power
                R_total += right_power

        # Scale and add base power
        P_l = self.BASE_POWER + L_total * self.SPIKE_STRENGTH
        P_r = self.BASE_POWER + R_total * self.SPIKE_STRENGTH

        # Return as (right, left) for differential drive convention
        return P_r, P_l

AGENT_CLASSES: Dict[str, Type[_BaseAgent]] = {
    "stochastic": StochasticMotionAgent,
    "inverse_turn": InverseMotionAgent,
    "composite": CompositeMotionAgent,
}