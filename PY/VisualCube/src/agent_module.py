"""
Agent controllers: single inverter, composite, stochastic, and external baselines.

Baseline agents (CRW, Levy, LogSpiral, ARS) provide external comparisons
against standard foraging strategies from the ecology literature.

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


def _power_from_speed_and_turn(desired_speed_px_s: float, desired_turn_rad: float) -> Tuple[float, float]:
    """Reverse the saturation pipeline to get (P_r, P_l) from desired kinematics."""
    power_sum = desired_speed_px_s / (cfg.AGENT_SPEED_SCALING_FACTOR * cfg.GLOBAL_SPEED_MODIFIER)
    power_sum = max(power_sum, 0.0)
    if power_sum < 1e-9 or abs(desired_turn_rad) < 1e-9:
        return (power_sum / 2.0, power_sum / 2.0)
    desired_turn_deg = math.degrees(desired_turn_rad)
    sat = min(abs(desired_turn_deg) / 90.0, 0.999)
    raw = -math.log(1.0 - sat)
    power_diff = raw / cfg.ANGULAR_PROPORTIONALITY_CONSTANT
    if desired_turn_deg < 0:
        power_diff = -power_diff
    P_r = max((power_sum + power_diff) / 2.0, 0.0)
    P_l = max((power_sum - power_diff) / 2.0, 0.0)
    return (P_r, P_l)


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

class CorrelatedRandomWalkAgent(_BaseAgent):
    """Correlated random walk: persistent heading with Gaussian angular noise.

    Standard null model from foraging ecology. At persistence=1, moves in a
    straight line. At persistence=0, each step has an independent random heading.
    """

    def __init__(self, **kwargs: Any) -> None:
        kwargs['d_rot'] = 0.0
        interval = kwargs.get('step_duration_sec', 0.5)
        kwargs['turn_decision_interval_sec'] = interval
        super().__init__(**kwargs)
        self.persistence: float = kwargs.get('persistence', 0.7)
        self.base_speed: float = kwargs.get('base_speed', 60.0)
        self._turn_sigma: float = (1.0 - self.persistence) * (math.pi / 2.0)
        self._cached_P_r: float = 0.0
        self._cached_P_l: float = 0.0
        self._time_acc: float = 0.0
        self._step_duration: float = interval
        self._generate_new_step()

    def _generate_new_step(self) -> None:
        turn = random.gauss(0, self._turn_sigma) if self._turn_sigma > 0 else 0.0
        self._cached_P_r, self._cached_P_l = _power_from_speed_and_turn(
            self.base_speed, turn)

    def _calculate_power_outputs(self, dt: float, is_potential_move: bool,
                                 food_eaten_count: int = 0) -> Tuple[float, float]:
        if not is_potential_move:
            self._time_acc += dt
            if self._time_acc >= self._step_duration:
                self._time_acc -= self._step_duration
                self._generate_new_step()
        return self._cached_P_r, self._cached_P_l


class LevyWalkAgent(_BaseAgent):
    """Levy walk: heavy-tailed step lengths with uniform random directions.

    Classic optimal foraging strategy from ecology. Step durations drawn from
    truncated power-law P(t) ~ t^(-mu). Between steps, moves straight. At
    step end, picks a new uniformly random direction.
    """

    def __init__(self, **kwargs: Any) -> None:
        kwargs['d_rot'] = 0.0
        kwargs['turn_decision_interval_sec'] = 1.0 / 60.0
        super().__init__(**kwargs)
        self.levy_exponent: float = kwargs.get('levy_exponent', 2.0)
        self.min_step_sec: float = kwargs.get('min_step_sec', 0.2)
        self.max_step_sec: float = kwargs.get('max_step_sec', 5.0)
        self.base_speed: float = kwargs.get('base_speed', 60.0)
        self._step_remaining: float = 0.0
        self._cached_P_r: float = 0.0
        self._cached_P_l: float = 0.0
        self._turning_this_frame: bool = False
        self._new_heading_offset: float = 0.0
        self._start_new_step()

    def _draw_step_duration(self) -> float:
        u = max(random.random(), 1e-10)
        exponent = -1.0 / (self.levy_exponent - 1.0) if self.levy_exponent > 1.0 else 1.0
        duration = self.min_step_sec * (u ** exponent)
        return min(duration, self.max_step_sec)

    def _start_new_step(self) -> None:
        self._step_remaining = self._draw_step_duration()
        self._new_heading_offset = random.uniform(-math.pi, math.pi)
        self._turning_this_frame = True
        speed_power = self.base_speed / (cfg.AGENT_SPEED_SCALING_FACTOR * cfg.GLOBAL_SPEED_MODIFIER)
        self._cached_P_r = max(speed_power / 2.0, 0.0)
        self._cached_P_l = max(speed_power / 2.0, 0.0)

    def _calculate_power_outputs(self, dt: float, is_potential_move: bool,
                                 food_eaten_count: int = 0) -> Tuple[float, float]:
        if not is_potential_move:
            self._step_remaining -= dt
            if self._step_remaining <= 0:
                self._start_new_step()
            elif self._turning_this_frame:
                self._turning_this_frame = False

        if self._turning_this_frame:
            return _power_from_speed_and_turn(self.base_speed, self._new_heading_offset)
        return self._cached_P_r, self._cached_P_l


class LogSpiralAgent(_BaseAgent):
    """Prescribed logarithmic spiral r = a * exp(b * theta).

    Theoretical ceiling for spiral exploration. Produces the mathematically
    ideal expanding spiral. NNN performance approaching this agent demonstrates
    strong emergent behavior.
    """

    def __init__(self, **kwargs: Any) -> None:
        kwargs['d_rot'] = 0.0
        kwargs['turn_decision_interval_sec'] = 1.0 / 60.0
        super().__init__(**kwargs)
        self.spiral_rate_b: float = kwargs.get('spiral_rate_b', 0.06)
        self.base_radius_a: float = kwargs.get('base_radius_a', 10.0)
        self.angular_speed: float = kwargs.get('angular_speed_rad_s', 1.5)
        self._theta: float = 0.0
        self._max_speed: float = kwargs.get('max_speed', 300.0)

    def _calculate_power_outputs(self, dt: float, is_potential_move: bool,
                                 food_eaten_count: int = 0) -> Tuple[float, float]:
        if not is_potential_move:
            self._theta += self.angular_speed * dt

        exponent = self.spiral_rate_b * self._theta
        if exponent > 500:
            path_speed = self._max_speed
        else:
            r = self.base_radius_a * math.exp(exponent)
            path_speed = r * self.angular_speed * math.sqrt(1.0 + self.spiral_rate_b ** 2)
            path_speed = min(path_speed, self._max_speed)
        turn_per_frame = self.angular_speed * dt

        return _power_from_speed_and_turn(path_speed, turn_per_frame)


class AreaRestrictedSearchAgent(_BaseAgent):
    """Area-restricted search: wide exploration until food, then tight circling.

    Standard ecological model from behavioral ecology. Switches between
    wide-ranging exploration and local exploitation based on recent food
    encounters (current_food_frequency from _BaseAgent).
    """

    def __init__(self, **kwargs: Any) -> None:
        kwargs['d_rot'] = 0.0
        kwargs['turn_decision_interval_sec'] = kwargs.get(
            'turn_decision_interval_sec', 0.5)
        super().__init__(**kwargs)
        self._food_freq_window = kwargs.get('food_freq_window', 2.0)
        self.explore_speed: float = kwargs.get('explore_speed', 60.0)
        self.exploit_speed: float = kwargs.get('exploit_speed', 30.0)
        self.exploit_turn_rate: float = kwargs.get('exploit_turn_rate', 2.0)
        self.explore_turn_sigma: float = kwargs.get('explore_turn_sigma', 0.3)
        self._cached_P_r: float = 0.0
        self._cached_P_l: float = 0.0
        self._time_acc: float = 0.0
        self._generate_explore_step()

    def _generate_explore_step(self) -> None:
        turn = random.gauss(0, self.explore_turn_sigma)
        self._cached_P_r, self._cached_P_l = _power_from_speed_and_turn(
            self.explore_speed, turn)

    def _generate_exploit_step(self) -> None:
        turn = self.exploit_turn_rate * self.turn_decision_interval_sec
        self._cached_P_r, self._cached_P_l = _power_from_speed_and_turn(
            self.exploit_speed, turn)

    def _calculate_power_outputs(self, dt: float, is_potential_move: bool,
                                 food_eaten_count: int = 0) -> Tuple[float, float]:
        if not is_potential_move:
            self._time_acc += dt
            if self._time_acc >= self.turn_decision_interval_sec:
                self._time_acc -= self.turn_decision_interval_sec
                if self.current_food_frequency > 0:
                    self._generate_exploit_step()
                else:
                    self._generate_explore_step()
        return self._cached_P_r, self._cached_P_l


AGENT_CLASSES: Dict[str, Type[_BaseAgent]] = {
    "stochastic": StochasticMotionAgent,
    "inverter": InverterAgent,
    "composite": CompositeMotionAgent,
    "crw": CorrelatedRandomWalkAgent,
    "levy": LevyWalkAgent,
    "log_spiral": LogSpiralAgent,
    "ars": AreaRestrictedSearchAgent,
}
