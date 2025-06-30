from __future__ import annotations
import math
from typing import Tuple

FRAME_DT = 1.0 / 60.0  # will be overwritten by GUI via set_frame_dt

def set_frame_dt(dt: float):
    """Sets the global frame time delta."""
    global FRAME_DT
    FRAME_DT = dt

class Vec2D:
    __slots__ = ("x", "y")
    def __init__(self, x: float = 0.0, y: float = 0.0):
        self.x, self.y = float(x), float(y)
    def __add__(self, o):
        return Vec2D(self.x + o.x, self.y + o.y)
    def __mul__(self, s: float):
        return Vec2D(self.x * s, self.y * s)
    def __rmul__(self, s: float):
        return self.__mul__(s)
    def __repr__(self):
        return f"Vec2D({self.x:.2f},{self.y:.2f})"
    @staticmethod
    def from_angle(angle: float):
        return Vec2D(math.cos(angle), math.sin(angle))

class RowboatKinematics:
    """Simplified rigid‑body row‑boat model.  Translational speed is linear in
    sum of oar frequencies; angular velocity proportional to their difference.

    x_dot = v * cos(θ)
    y_dot = v * sin(θ)
    θ_dot = (v_R - v_L) / w   (w = effective beam width ≈ agent diameter)
    where v_L = k * f_L, v_R = k * f_R.  k chosen so that *1 Hz on both oars*
    yields speed ≈ k px/s.
    """
    __slots__ = ("pos", "theta", "k_lin", "beam_w")
    def __init__(self, pos: Tuple[float, float] = (0, 0), theta_deg: float = 0.0,
                 lin_coeff: float = 75.0, beam_w: float = 12.0):
        self.pos = Vec2D(*pos)
        self.theta = math.radians(theta_deg)
        self.k_lin = lin_coeff  # px/s per Hz (speed coefficient)
        self.beam_w = beam_w
    
    def update(self, f_left_hz: float, f_right_hz: float):
        dt = FRAME_DT
        v_left  = self.k_lin * f_left_hz
        v_right = self.k_lin * f_right_hz
        v_avg   = 0.5 * (v_left + v_right)
        
        # Positive angular velocity (theta_dot) should correspond to a counter-clockwise (left) turn.
        # If v_right > v_left, the boat should turn right (clockwise, negative theta_dot).
        # Therefore, theta_dot is proportional to (v_left - v_right).
        # A wider beam_w results in a slower turn for the same speed difference.
        theta_dot = (v_right - v_left) / self.beam_w if self.beam_w > 1e-6 else 0.0  # rad/s
        
        # Integrate
        self.theta = (self.theta + theta_dot * dt) % (2 * math.pi)
        dir_vec = Vec2D.from_angle(self.theta)
        self.pos += dir_vec * (v_avg * dt)
    
    @property
    def direction_rad(self):
        return self.theta
    
    @property
    def position(self):
        return self.pos