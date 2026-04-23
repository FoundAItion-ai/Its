"""
Agent controllers: single inverter, composite, and stochastic motion.

This is free and unencumbered software released into the public domain.
For more information, see LICENSE.txt or https://unlicense.org
"""

from __future__ import annotations
import math
import random
from collections import deque
from typing import Tuple, Dict, Any, Type, List

import math_module as mm
from math_module import Vec2D
import config as cfg
from inverter import Inverter, InverterConfig, NNN_SINGLE_PRESET, NNN_COMPOSITE_PRESET, combine_outputs


class _BaseAgent:
    # Rotational diffusion coefficient (rad²/s) — biological motor noise.
    # Per-step heading noise: sigma = sqrt(2 * D_ROT * dt), Gaussian.
    D_ROT: float = 0.1

    def __init__(self, **kwargs: Any) -> None:
        self.D_ROT = kwargs.get('d_rot', 0.1)
        self.food_timestamps: deque = deque()
        self.current_food_frequency: float = 0.0
        self._food_freq_window: float = 1.0  # Subclasses override with C1
        self.turn_decision_interval_sec: float = kwargs.get(
            'turn_decision_interval_sec', cfg.DEFAULT_TURN_DECISION_INTERVAL_SEC
        )
        self.last_turn_decision_time: float = -1.0
        self.decision_count: int = 0

    def _update_food_frequency(self, food_eaten_count: int, current_sim_time: float) -> None:
        for _ in range(food_eaten_count):
            self.food_timestamps.append(current_sim_time)
        cutoff_time = current_sim_time - self._food_freq_window
        while self.food_timestamps and self.food_timestamps[0] < cutoff_time:
            self.food_timestamps.popleft()
        self.current_food_frequency = len(self.food_timestamps) / self._food_freq_window

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
                self.decision_count += 1

            _, delta_angle_rad = mm.get_motion_outputs_from_power(
                P_r, P_l, cfg.AGENT_SPEED_SCALING_FACTOR, cfg.ANGULAR_PROPORTIONALITY_CONSTANT
            )
            if self.D_ROT > 0:
                sigma = math.sqrt(2.0 * self.D_ROT * dt)
                delta_angle_rad += random.gauss(0, sigma)
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

    Power = inverter rate (Hz). No extra gain, baseline, or floor.
    C1-C4 define everything.
    """

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)

        self.inverter = Inverter(
            C1=kwargs.get('C1', NNN_SINGLE_PRESET['C1']),
            C2=kwargs.get('C2', NNN_SINGLE_PRESET['C2']),
            C3=kwargs.get('C3', NNN_SINGLE_PRESET['C3']),
            C4=kwargs.get('C4', NNN_SINGLE_PRESET['C4']),
            name='inv0'
        )
        self._food_freq_window = self.inverter.C1

        self.last_signals: List[Tuple[int, str, int]] = []

    @property
    def num_inverters(self) -> int:
        return 1

    def _calculate_power_outputs(self, dt: float, is_potential_move: bool, food_eaten_count: int = 0) -> Tuple[float, float]:
        """Compute motor outputs directly from inverter rates (Hz)."""
        if not is_potential_move:
            left_spike, right_spike = self.inverter.update(dt, food_eaten_count)

            self.last_signals = []
            if left_spike:
                self.last_signals.append((0, 'L', left_spike))
            if right_spike:
                self.last_signals.append((0, 'R', right_spike))

        L_rate, R_rate = self.inverter.get_rates()
        return R_rate, L_rate


class CompositeMotionAgent(_BaseAgent):
    """
    NNN Composite Motion using true spiking Inverter units.

    Each inverter fires discrete spikes (0/1) at periods determined by C1-C4.
    Crossed wiring swaps left/right outputs for counter-phase behavior.

    Power = summed inverter rates (Hz). No extra gain, baseline, or floor.
    C1-C4 define everything.
    """

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)

        self.last_signals: List[Tuple[int, str, int]] = []

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

        if not self.inverters:
            self._setup_default_nnn_composite()

        if self.inverters:
            self._food_freq_window = self.inverters[0].C1

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
        """Compute motor outputs from inverter rates with linear summation.

        Each inverter's firing rate contributes directly to motor power.
        Crossed wiring swaps L/R contributions to the motor totals.
        Returns (P_r, P_l) for differential drive.
        """
        if not is_potential_move:
            self.last_signals = []
            for idx, inverter in enumerate(self.inverters):
                left_spike, right_spike = inverter.update(dt, food_eaten_count)
                if left_spike:
                    self.last_signals.append((idx, 'L', left_spike))
                if right_spike:
                    self.last_signals.append((idx, 'R', right_spike))

        L_rate, R_rate = 0.0, 0.0
        for inverter, config in zip(self.inverters, self.configs):
            inv_L, inv_R = inverter.get_rates()
            if config.crossed:
                L_rate += inv_R
                R_rate += inv_L
            else:
                L_rate += inv_L
                R_rate += inv_R

        return R_rate, L_rate

AGENT_CLASSES: Dict[str, Type[_BaseAgent]] = {
    "stochastic": StochasticMotionAgent,
    "inverter": InverterAgent,
    "composite": CompositeMotionAgent,
}
