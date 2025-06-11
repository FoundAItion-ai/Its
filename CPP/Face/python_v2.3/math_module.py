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

    # Note: Magnitude and normalize were present but are not strictly needed
    # for the current constant-speed kinematic update model.
    # Keeping them might be useful later or for debugging, but velocity is directly set.
    # @property
    # def magnitude_sq(self) -> float:
    #     """Returns the squared magnitude of the vector."""
    #     return self.x**2 + self.y**2

    # @property
    # def magnitude(self) -> float:
    #     """Returns the magnitude (length) of the vector."""
    #     return math.sqrt(self.magnitude_sq)

    # def normalize(self) -> Vec2D:
    #     """Returns a unit vector in the same direction, or (0,0) for a zero vector."""
    #     mag = self.magnitude
    #     if mag == 0:
    #         return Vec2D(0, 0)
    #     return Vec2D(self.x / mag, self.y / mag)

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
                 "_dt", "constant_speed", "max_turn_rate_rad_per_dt") # Renamed for clarity

    _DEG_TO_RAD: float = math.pi / 180.0
    _RAD_TO_DEG: float = 180.0 / math.pi

    def __init__(self,
                 initial_position: Tuple[float, float] = (0.0, 0.0),
                 initial_direction_deg: float = 0.0,
                 dt: float = FRAME_DT,
                 constant_speed_pixels_sec: float = 50.0,
                 max_turn_rate_deg_per_dt: float = 5.0 # This is the agent's maximum agility per frame
                ):
        self.position = Vec2D(initial_position[0], initial_position[1])
        # Velocity is recalculated each frame based on direction and constant speed
        self.velocity = Vec2D(0,0)
        self.direction_rad = (initial_direction_deg % 360.0) * self._DEG_TO_RAD

        self._dt = dt
        self.constant_speed = constant_speed_pixels_sec # pixels per second
        # Convert max turn rate from degrees/dt to radians/dt
        self.max_turn_rate_rad_per_dt = max_turn_rate_deg_per_dt * self._DEG_TO_RAD

    @property
    def direction_deg(self) -> float:
        """Current direction in degrees [0, 360)."""
        deg = self.direction_rad * self._RAD_TO_DEG
        # Ensure it's within [0, 360) range
        deg = deg % 360.0
        if deg < 0:
            deg += 360.0
        return deg

    # @property
    # def speed(self) -> float:
    #     """Current speed (magnitude of velocity). Should equal constant_speed if moving."""
    #     # return self.velocity.magnitude # Not needed, it's constant_speed if thrust > 0
    #     return self.constant_speed if self.velocity.magnitude_sq > 1e-9 else 0.0


    def update(self,
               thrust_input_normalized: float, # Effectively boolean: 1.0 to move, 0.0 to stop (currently always 1.0)
               turn_command_normalized: float  # Normalized turn command [-1 (left), 1 (right)]
              ):
        """
        Updates agent's state based on move command, normalized turn command, and constant speed.
        The actual turn applied is turn_command_normalized * max_turn_rate_rad_per_dt.
        """

        # --- Linear Kinematics ---
        # Agent moves at constant speed if commanded
        if thrust_input_normalized > 0.5:
            # Calculate velocity vector based on current direction and constant speed
            self.velocity = Vec2D.from_angle_magnitude(self.direction_rad, self.constant_speed)
        else:
            # If not commanded to move (though current setup always commands movement)
            self.velocity = Vec2D(0, 0)

        # Update position based on velocity and timestep
        self.position += self.velocity * self._dt # p_new = p_old + v * dt

        # --- Rotational Kinematics ---
        # turn_command_normalized [-1, 1] scales the agent's maximum turning rate for this frame
        # A value of 1 means turn right at max rate, -1 means turn left at max rate, 0 means no turn.
        delta_angle_rad = turn_command_normalized * self.max_turn_rate_rad_per_dt

        # Update direction, keeping it within [0, 2*pi)
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