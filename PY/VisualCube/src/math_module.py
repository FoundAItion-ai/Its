"""
Motion dynamics: turn angle, speed, kinematics, and vector math.

This is free and unencumbered software released into the public domain.
For more information, see LICENSE.txt or https://unlicense.org
"""

from __future__ import annotations
import math
from typing import Tuple

import config as cfg

# The time elapsed in a single simulation frame, in seconds.
FRAME_DELTA_TIME_SECONDS = 1.0 / 60.0

def set_frame_dt(delta_time_seconds: float):
    """Sets the simulation's frame delta time."""
    global FRAME_DELTA_TIME_SECONDS
    FRAME_DELTA_TIME_SECONDS = delta_time_seconds

class Vec2D:
    __slots__ = ("x", "y")
    def __init__(self, x: float = 0.0, y: float = 0.0):
        self.x, self.y = float(x), float(y)
    def __add__(self, other: Vec2D) -> Vec2D:
        return Vec2D(self.x + other.x, self.y + other.y)
    def __mul__(self, scalar: float) -> Vec2D:
        return Vec2D(self.x * scalar, self.y * scalar)
    def __repr__(self) -> str:
        return f"Vec2D({self.x:.2f}, {self.y:.2f})"
    @staticmethod
    def from_angle(angle_rad: float) -> Vec2D:
        return Vec2D(math.cos(angle_rad), math.sin(angle_rad))

# ==============================================================================
# I. NEW: Centralized Mathematical Formulas
# ==============================================================================

def calculate_sinusoidal_wave(amplitude: float, period_s: float, sim_time: float) -> float:
    """
    NEW: Calculates the value of a sine wave at a given time.
    This function moves raw math out of the agent logic module.
    """
    if period_s == 0:
        return 0.0
    return amplitude * math.sin(2 * math.pi * sim_time / period_s)

# ==============================================================================
# II. Formulas from "General Overview of Intended Logic"
# ==============================================================================

def apply_smoothing_filter(
    right_side_power_total: float,
    left_side_power_total: float,
    previous_smoothed_right_power: float,
    previous_smoothed_left_power: float,
    smoothing_factor: float
) -> Tuple[float, float]:
    """Applies the exponential smoothing filter for noise reduction."""
    smoothed_right_power = (smoothing_factor * right_side_power_total +
                            (1 - smoothing_factor) * previous_smoothed_right_power)
    smoothed_left_power = (smoothing_factor * left_side_power_total +
                           (1 - smoothing_factor) * previous_smoothed_left_power)
    return smoothed_right_power, smoothed_left_power

# ==============================================================================
# III. Explicit Formulas from "Core Motion Dynamics"
# ==============================================================================

def calculate_total_and_differential_power(
    right_power_input: float, left_power_input: float
) -> Tuple[float, float]:
    """Calculates total power and the power difference (interference)."""
    power_difference = right_power_input - left_power_input
    total_power = right_power_input + left_power_input
    return total_power, power_difference

def calculate_speed_magnitude(total_power: float, speed_scaling_factor: float) -> float:
    """Calculates speed based on total power."""
    speed_magnitude = speed_scaling_factor * total_power
    return speed_magnitude

def calculate_saturated_rotation_angle(
    power_difference: float, angular_proportionality_constant: float
) -> float:
    """Calculates the final, saturated rotation angle in radians."""
    raw_turn_angle_degrees = angular_proportionality_constant * power_difference
    sign = math.copysign(1, raw_turn_angle_degrees) if raw_turn_angle_degrees != 0 else 0
    saturation = 1 - math.exp(-abs(raw_turn_angle_degrees))
    final_turn_angle_degrees = 90.0 * saturation * sign
    return math.radians(final_turn_angle_degrees)

# ------------------------------------------------------------------------------
# IV. Helper Function to Combine Core Motion Dynamics
# ------------------------------------------------------------------------------

def get_motion_outputs_from_power(
    right_power_input: float, left_power_input: float, speed_scaling_factor: float,
    angular_proportionality_constant: float
) -> Tuple[float, float]:
    """A convenience wrapper that executes the Core Motion Dynamics pipeline."""
    total_power, power_difference = calculate_total_and_differential_power(
        right_power_input, left_power_input
    )
    speed = calculate_speed_magnitude(total_power, speed_scaling_factor)
    angle_rad = calculate_saturated_rotation_angle(
        power_difference, angular_proportionality_constant
    )
    return speed, angle_rad

# ==============================================================================
# V. AGENT BODY KINEMATICS
# ==============================================================================

class AgentBody:
    """
    A simple data container for an agent's physical state.
    """
    __slots__ = ("position", "heading_radians")

    def __init__(self, pos: Tuple[float, float] = (0, 0), theta_deg: float = 0.0):
        self.position = Vec2D(*pos)
        self.heading_radians = math.radians(theta_deg)

    def apply_movement(self, distance: float, change_in_heading_radians: float):
        """
        Applies movement by updating heading first, then moving forward.
        """
        self.heading_radians = (self.heading_radians + change_in_heading_radians) % (2 * math.pi)
        displacement_vector = Vec2D.from_angle(self.heading_radians) * distance
        self.position += displacement_vector
        
    @property
    def direction_rad(self) -> float:
        return self.heading_radians

    @property
    def pos(self) -> Vec2D:
        return self.position