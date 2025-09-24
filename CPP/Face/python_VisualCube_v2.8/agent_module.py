# agent_module.py

from __future__ import annotations
import math
import random
from collections import deque
from typing import Tuple, Dict, Any, Type

import math_module as mm
from math_module import Vec2D
import config as cfg

# --- (GLOBAL CONSTANTS remain the same) ---
K_THETA_DEG = 45.0
ALPHA = 0.1
EPSILON = 1e-6
FOOD_FREQ_WINDOW_SEC = 1.5

def set_k_theta_deg(val: float):
    """Sets the angular proportionality constant (degree-scale)."""
    global K_THETA_DEG
    K_THETA_DEG = val

class _PDFBaseAgent:
    def __init__(self, **kwargs: Any) -> None:
        self.food_timestamps: deque = deque()
        self.current_food_frequency: float = 0.0

    def _update_food_frequency(self, food_eaten_count: int, current_sim_time: float) -> None:
        for _ in range(food_eaten_count):
            self.food_timestamps.append(current_sim_time)
        cutoff_time = current_sim_time - FOOD_FREQ_WINDOW_SEC
        while self.food_timestamps and self.food_timestamps[0] < cutoff_time:
            self.food_timestamps.popleft()
        self.current_food_frequency = len(self.food_timestamps) / FOOD_FREQ_WINDOW_SEC

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        raise NotImplementedError("Agent subclasses must implement _calculate_power_outputs.")

    def update(self, food_eaten_count: int, current_sim_time: float, dt: float) -> Tuple[float, float]:
        P_r, P_l = self._calculate_power_outputs(current_sim_time)
        
        speed_px_per_sec, delta_angle_rad = mm.get_motion_outputs_from_power(
            P_r, P_l, cfg.AGENT_SPEED_SCALING_FACTOR, K_THETA_DEG
        )
        
        final_speed = speed_px_per_sec * cfg.GLOBAL_SPEED_MODIFIER
        distance_this_frame = final_speed * dt
        delta_theta_this_frame = delta_angle_rad
        
        self._update_food_frequency(food_eaten_count, current_sim_time)
        
        return distance_this_frame, delta_theta_this_frame


class StochasticMotionAgent(_PDFBaseAgent):
    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.choice_interval_sec: float = kwargs.get('update_interval_sec', 1.0)
        self.last_choice_time: float = -1.0
        self.cached_P_r: float = 0.0
        self.cached_P_l: float = 0.0
        
    def _generate_new_power_choice(self) -> None:
        R_rand = random.uniform(-3.0, 3.0)
        L_rand = random.uniform(-3.0, 3.0)
        self.cached_P_r = R_rand**2
        self.cached_P_l = L_rand**2

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        if sim_time - self.last_choice_time >= self.choice_interval_sec:
            self.last_choice_time = sim_time
            self._generate_new_power_choice()
            
        return self.cached_P_r, self.cached_P_l
    

class InverseMotionAgent(_PDFBaseAgent):
    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.r_smooth_prev: float = 0.0
        self.l_smooth_prev: float = 0.0
        
        self.threshold_r: float = kwargs.get('threshold_r', 5.0)
        self.threshold_l: float = kwargs.get('threshold_l', 5.0)
        self.r1_period_s: float = kwargs.get('r1_period_s', 2.0)
        self.r1_amp: float = kwargs.get('r1_amp', 0.3)
        self.r2_period_s: float = kwargs.get('r2_period_s', 5.0)
        self.r2_amp: float = kwargs.get('r2_amp', 0.4)
        self.l1_period_s: float = kwargs.get('l1_period_s', 2.0)
        self.l1_amp: float = kwargs.get('l1_amp', 0.3)
        self.l2_period_s: float = kwargs.get('l2_period_s', 5.0)
        self.l2_amp: float = kwargs.get('l2_amp', 0.5)

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        # --- Right Side Calculation ---
        if self.current_food_frequency < self.threshold_r:
            R_total = self.r1_amp * math.sin(2 * math.pi * sim_time / self.r1_period_s)
        else:
            R_total = self.r2_amp * math.sin(2 * math.pi * sim_time / self.r2_period_s)
            
        # --- Left Side Calculation ---
        if self.current_food_frequency < self.threshold_l:
            L_total = self.l1_amp * math.sin(2 * math.pi * sim_time / self.l1_period_s)
        else:
            L_total = self.l2_amp * math.sin(2 * math.pi * sim_time / self.l2_period_s)

        R_smooth, L_smooth = mm.apply_smoothing_filter(
            R_total, L_total, self.r_smooth_prev, self.l_smooth_prev, ALPHA
        )
        self.r_smooth_prev, self.l_smooth_prev = R_smooth, L_smooth
        
        P_r = R_smooth**2
        P_l = L_smooth**2
        
        return P_r, P_l


class CompositeMotionAgent(_PDFBaseAgent):
    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.r_smooth_prev: float = 0.0
        self.l_smooth_prev: float = 0.0
        self.layer_pairs: list[dict[str, Any]] = kwargs.get('layer_pairs', [])

    def _calculate_power_outputs(self, sim_time: float) -> Tuple[float, float]:
        R_total = 0.0
        L_total = 0.0
        
        for layer in self.layer_pairs:
            if self.current_food_frequency >= layer['r_threshold_hz']:
                R_i = layer['r_amp'] * math.sin(2 * math.pi * sim_time / layer['r_period_s'])
                R_total += R_i
            
            if self.current_food_frequency >= layer['l_threshold_hz']:
                L_i = layer['l_amp'] * math.sin(2 * math.pi * sim_time / layer['l_period_s'])
                L_total += L_i
        
        R_smooth, L_smooth = mm.apply_smoothing_filter(
            R_total, L_total, self.r_smooth_prev, self.l_smooth_prev, ALPHA
        )
        self.r_smooth_prev, self.l_smooth_prev = R_smooth, L_smooth
        
        P_r = R_smooth**2
        P_l = L_smooth**2
        
        return P_r, P_l


AGENT_CLASSES: Dict[str, Type[_PDFBaseAgent]] = {
    "stochastic": StochasticMotionAgent,
    "inverse_turn": InverseMotionAgent,
    "composite": CompositeMotionAgent,
}
# The alias "_BaseAgent = _PDFBaseAgent" has been removed.