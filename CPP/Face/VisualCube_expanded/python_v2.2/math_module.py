from __future__ import annotations
import math
from typing import Tuple

# Core simulation timestep (delta time) in seconds. Modifiable via GUI.
FRAME_DT: float = 1.0 / 60.0

# --- Core 2D Vector and Kinematics for Constant Speed Motion ---
# Physics-based properties (mass, force, drag) have been deprecated
# for a simpler constant-speed motion model.

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
    
    @property
    def magnitude_sq(self) -> float:
        """Returns the squared magnitude of the vector."""
        return self.x**2 + self.y**2

    @property
    def magnitude(self) -> float:
        """Returns the magnitude (length) of the vector."""
        return math.sqrt(self.magnitude_sq)
    
    def normalize(self) -> Vec2D:
        """Returns a unit vector in the same direction, or (0,0) for a zero vector."""
        mag = self.magnitude
        if mag == 0:
            return Vec2D(0, 0)
        return Vec2D(self.x / mag, self.y / mag)

    @staticmethod
    def from_angle_magnitude(angle_rad: float, magnitude: float) -> Vec2D:
        """Creates a vector from an angle (radians) and magnitude."""
        return Vec2D(magnitude * math.cos(angle_rad), magnitude * math.sin(angle_rad))

    def __repr__(self) -> str:
        return f"Vec2D({self.x:.3f}, {self.y:.3f})"

# AgentPhysicsProps class removed.
# ENVIRONMENT_DENSITY constant removed.

class AgentKinematics:
    """
    Manages an agent's position and orientation for constant speed movement.
    Velocity is derived from constant speed and current direction.
    """
    __slots__ = ("position", "velocity", "direction_rad", 
                 "_dt", "constant_speed", "current_turn_rate_rad_per_dt")

    _DEG_TO_RAD: float = math.pi / 180.0
    _RAD_TO_DEG: float = 180.0 / math.pi

    def __init__(self,
                 initial_position: Tuple[float, float] = (0.0, 0.0),
                 initial_direction_deg: float = 0.0,
                 # initial_velocity_pixels_sec is not strictly needed if speed is constant and derived
                 dt: float = FRAME_DT,
                 constant_speed_pixels_sec: float = 50.0, 
                 initial_turn_rate_rad_per_dt: float = (5.0 * _DEG_TO_RAD) # Base turning agility
                ):
        self.position = Vec2D(initial_position[0], initial_position[1])
        # Velocity is recalculated each frame based on direction and constant speed
        self.velocity = Vec2D(0,0) 
        self.direction_rad = (initial_direction_deg % 360.0) * self._DEG_TO_RAD
        
        self._dt = dt
        self.constant_speed = constant_speed_pixels_sec # pixels per second
        self.current_turn_rate_rad_per_dt = initial_turn_rate_rad_per_dt # Fixed base turn rate

    @property
    def direction_deg(self) -> float:
        """Current direction in degrees."""
        return self.direction_rad * self._RAD_TO_DEG

    @property
    def speed(self) -> float: # Speed in pixels/second
        """Current speed (magnitude of velocity). This will be self.constant_speed if moving."""
        return self.velocity.magnitude

    def update(self, 
               thrust_input_normalized: float, # Effectively boolean: 1.0 to move, 0.0 to stop (currently always 1.0)
               turn_command: float             # Normalized turn command [-1 (left), 1 (right)]
              ):
        """Updates agent's state based on move command, turn command, and constant speed."""
        
        # --- Linear Kinematics ---
        if thrust_input_normalized > 0.5: # Agent is commanded to move
            self.velocity = Vec2D.from_angle_magnitude(self.direction_rad, self.constant_speed)
        else: # Agent is commanded to stop (though current design makes agents always move)
            self.velocity = Vec2D(0, 0)
        
        self.position += self.velocity * self._dt # p_new = p_old + v * dt

        # --- Rotational Kinematics ---
        # turn_command scales the agent's fixed base turning rate for this frame
        delta_angle_rad = turn_command * self.current_turn_rate_rad_per_dt
        
        self.direction_rad = (self.direction_rad + delta_angle_rad) % (2 * math.pi)
        if self.direction_rad < 0: # Normalize to [0, 2*pi)
            self.direction_rad += (2 * math.pi)

def set_frame_dt(dt: float):
    """Sets the global simulation timestep (FRAME_DT)."""
    global FRAME_DT
    FRAME_DT = float(dt)