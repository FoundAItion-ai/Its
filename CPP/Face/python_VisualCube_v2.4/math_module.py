from __future__ import annotations
import math
from typing import Tuple

# Core simulation timestep (delta time) in seconds. Modifiable via GUI.
# Initial value matches default FPS (60).
FRAME_DT: float = 1.0 / 60.0

# --- Core 2D Vector and Kinematics for Constant Speed Motion ---

class Vec2D:
    """
    A minimal 2D vector class for position, velocity, and direction representation.
    Supports basic vector operations required for constant speed kinematics.
    """
    __slots__ = ('x', 'y')

    def __init__(self, x: float = 0.0, y: float = 0.0):
        self.x = float(x)
        self.y = float(y)

    def __add__(self, other: Vec2D) -> Vec2D:
        return Vec2D(self.x + other.x, self.y + other.y)

    def __mul__(self, scalar: float) -> Vec2D:
        return Vec2D(self.x * scalar, self.y * scalar)

    def __rmul__(self, scalar: float) -> Vec2D: # For scalar * Vec2D
        return self.__mul__(scalar)

    def __repr__(self) -> str:
        return f"Vec2D({self.x:.3f}, {self.y:.3f})"

    @staticmethod
    def from_angle_magnitude(angle_rad: float, magnitude: float) -> Vec2D:
        """Creates a vector from an angle (radians) and magnitude."""
        return Vec2D(magnitude * math.cos(angle_rad), magnitude * math.sin(angle_rad))


class AgentKinematics:
    """
    Manages an agent's position and orientation for constant speed movement.
    Velocity magnitude is constant (self.constant_speed). Velocity direction matches direction_rad.
    Rotation rate is controlled by the normalized turn command and the agent's max turn rate.
    """
    __slots__ = ("position", "velocity", "direction_rad",
                 "_dt", "constant_speed", "max_turn_rate_rad_per_dt")

    _DEG_TO_RAD: float = math.pi / 180.0
    _RAD_TO_DEG: float = 180.0 / math.pi

    def __init__(self,
                 initial_position: Tuple[float, float] = (0.0, 0.0),
                 initial_direction_deg: float = 0.0,
                 dt: float = FRAME_DT,
                 constant_speed_pixels_sec: float = 50.0,
                 max_turn_rate_deg_per_dt: float = 5.0
                ):
        self.position = Vec2D(initial_position[0], initial_position[1])
        self.velocity = Vec2D(0,0)
        self.direction_rad = (initial_direction_deg % 360.0) * self._DEG_TO_RAD

        self._dt = dt
        self.constant_speed = constant_speed_pixels_sec
        self.max_turn_rate_rad_per_dt = max_turn_rate_deg_per_dt * self._DEG_TO_RAD

    @property
    def direction_deg(self) -> float:
        """Current direction in degrees [0, 360)."""
        deg = (self.direction_rad * self._RAD_TO_DEG) % 360.0
        return deg if deg >= 0 else deg + 360.0

    def update(self,
               thrust_input_normalized: float,
               turn_command_normalized: float
              ):
        """
        Updates agent's state based on move command, normalized turn command, and constant speed.
        """
        if thrust_input_normalized > 0.5:
            self.velocity = Vec2D.from_angle_magnitude(self.direction_rad, self.constant_speed)
        else:
            self.velocity = Vec2D(0, 0)

        self.position += self.velocity * self._dt

        delta_angle_rad = turn_command_normalized * self.max_turn_rate_rad_per_dt
        self.direction_rad = (self.direction_rad + delta_angle_rad) % (2 * math.pi)
        if self.direction_rad < 0:
            self.direction_rad += (2 * math.pi)

def set_frame_dt(dt: float):
    """Sets the global simulation timestep (FRAME_DT)."""
    global FRAME_DT
    FRAME_DT = float(dt)
    if FRAME_DT <= 0:
        print(f"Warning: FRAME_DT set to non-positive value ({dt}). Setting to 1/60.")
        FRAME_DT = 1.0/60.0