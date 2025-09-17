# math_module.py

from __future__ import annotations
import math
from typing import Tuple

import config as cfg

FRAME_DT = 1.0 / 60.0

def set_frame_dt(dt: float):
    global FRAME_DT
    FRAME_DT = dt

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
# II. Formulas from "General Overview of Intended Logic"
# ==============================================================================

def apply_smoothing_filter(
    R_total: float,
    L_total: float,
    R_smooth_prev: float,
    L_smooth_prev: float,
    alpha: float
) -> Tuple[float, float]:
    """Applies the exponential smoothing filter for noise reduction."""
    r_smooth = alpha * R_total + (1 - alpha) * R_smooth_prev
    l_smooth = alpha * L_total + (1 - alpha) * L_smooth_prev
    return r_smooth, l_smooth

# ==============================================================================
# III. Explicit Formulas from "Core Motion Dynamics"
# ==============================================================================

def calculate_power_totals(P_r: float, P_l: float) -> Tuple[float, float]:
    """Calculates total power and the power difference (interference)."""
    delta_P = P_r - P_l
    P_total = P_r + P_l
    return P_total, delta_P

def calculate_speed_magnitude(P_total: float, speed_scaling_factor: float) -> float:
    """
    FIXED: Calculates speed based on total power, which is the physically
    correct model for forward movement, as identified in the analysis.
    
    :param P_total: The sum of the power from both signals (P_r + P_l).
    :param speed_scaling_factor: A global constant to tune overall speed.
    """
    # Formula: v = k_v * P_total (aligns with the Composite Agent's logic)
    speed_magnitude = speed_scaling_factor * P_total
    return speed_magnitude

def calculate_saturated_rotation_angle(delta_P: float, k_theta_deg: float) -> float:
    """Calculates the final, saturated rotation angle in radians."""
    theta_raw_deg = k_theta_deg * delta_P
    sign = math.copysign(1, theta_raw_deg) if theta_raw_deg != 0 else 0
    saturation = 1 - math.exp(-abs(theta_raw_deg))
    theta_deg = 90.0 * saturation * sign
    return math.radians(theta_deg)

# ------------------------------------------------------------------------------
# IV. Helper Function to Combine Core Motion Dynamics
# ------------------------------------------------------------------------------

def get_motion_outputs_from_power(
    P_r: float,
    P_l: float,
    speed_scaling_factor: float,
    k_theta_deg: float
) -> Tuple[float, float]:
    """
    A convenience wrapper that executes the Core Motion Dynamics pipeline.
    """
    # 1. Get power totals and difference
    P_total, delta_P = calculate_power_totals(P_r, P_l)
    
    # 2. Calculate speed using the CORRECTED total power formula
    speed = calculate_speed_magnitude(P_total, speed_scaling_factor)
    
    # 3. Calculate rotation angle
    angle_rad = calculate_saturated_rotation_angle(delta_P, k_theta_deg)
    
    return speed, angle_rad

# ==============================================================================
# V. AGENT BODY KINEMATICS
# ==============================================================================

class AgentBody:
    """
    A simple data container for an agent's physical state. It has one job:
    to correctly apply the final, calculated movement vector to its position
    and orientation. It contains no physics logic itself.
    """
    __slots__ = ("pos", "theta")

    def __init__(self, pos: Tuple[float, float] = (0, 0), theta_deg: float = 0.0):
        self.pos = Vec2D(*pos)
        self.theta = math.radians(theta_deg) # Agent's heading in radians

    def apply_movement(self, distance: float, delta_theta: float):
        """
        Applies movement in a physically correct way.
        1. The agent's heading is updated by delta_theta.
        2. The agent moves forward by 'distance' along its NEW heading.
        """
        self.theta = (self.theta + delta_theta) % (2 * math.pi)
        displacement_vector = Vec2D.from_angle(self.theta) * distance
        self.pos += displacement_vector
        
    @property
    def direction_rad(self) -> float:
        return self.theta

    @property
    def position(self) -> Vec2D:
        return self.pos