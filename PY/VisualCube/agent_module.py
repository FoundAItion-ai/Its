from __future__ import annotations
import math
import random
from collections import deque
from typing import Tuple, Dict, Any, Type

import math_module as mm
from math_module import Vec2D
import config as cfg

SMOOTHING_FACTOR = 0.1
FOOD_FREQ_WINDOW_SEC = 1.0

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
    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.r_smooth_prev: float = 0.0
        self.l_smooth_prev: float = 0.0
        self.threshold_r: float = kwargs['threshold_r']
        self.threshold_l: float = kwargs['threshold_l']
        self.r1_period_s: float = kwargs['r1_period_s']
        self.r1_amp: float = kwargs['r1_amp']
        self.r2_period_s: float = kwargs['r2_period_s']
        self.r2_amp: float = kwargs['r2_amp']
        self.l1_period_s: float = kwargs['l1_period_s']
        self.l1_amp: float = kwargs['l1_amp']
        self.l2_period_s: float = kwargs['l2_period_s']
        self.l2_amp: float = kwargs['l2_amp']

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        if self.current_food_frequency < self.threshold_r:
            R_total = mm.calculate_sinusoidal_wave(self.r1_amp, self.r1_period_s, sim_time)
        else:
            R_total = mm.calculate_sinusoidal_wave(self.r2_amp, self.r2_period_s, sim_time)
        if self.current_food_frequency < self.threshold_l:
            L_total = mm.calculate_sinusoidal_wave(self.l1_amp, self.l1_period_s, sim_time)
        else:
            L_total = mm.calculate_sinusoidal_wave(self.l2_amp, self.l2_period_s, sim_time)

        R_smooth, L_smooth = mm.apply_smoothing_filter(
            R_total, L_total, self.r_smooth_prev, self.l_smooth_prev, SMOOTHING_FACTOR
        )
        self.r_smooth_prev, self.l_smooth_prev = R_smooth, L_smooth
        return R_smooth**2, L_smooth**2


class CompositeMotionAgent(_BaseAgent):
    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.r_smooth_prev: float = 0.0
        self.l_smooth_prev: float = 0.0
        self.layer_pairs: list[dict[str, Any]] = kwargs.get('layer_pairs', [])

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        R_total, L_total = 0.0, 0.0
        for layer in self.layer_pairs:
            if self.current_food_frequency >= layer['r_threshold_hz']:
                R_total += mm.calculate_sinusoidal_wave(layer['r_amp'], layer['r_period_s'], sim_time)
            if self.current_food_frequency >= layer['l_threshold_hz']:
                L_total += mm.calculate_sinusoidal_wave(layer['l_amp'], layer['l_period_s'], sim_time)

        R_smooth, L_smooth = mm.apply_smoothing_filter(
            R_total, L_total, self.r_smooth_prev, self.l_smooth_prev, SMOOTHING_FACTOR
        )
        self.r_smooth_prev, self.l_smooth_prev = R_smooth, L_smooth
        return R_smooth**2, L_smooth**2

AGENT_CLASSES: Dict[str, Type[_BaseAgent]] = {
    "stochastic": StochasticMotionAgent,
    "inverse_turn": InverseMotionAgent,
    "composite": CompositeMotionAgent,
}