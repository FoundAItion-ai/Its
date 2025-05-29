from __future__ import annotations
import math
from typing import Tuple

# Global physics constants, modifiable via GUI
FRAME_DT: float = 1.0 / 60.0  # Simulation timestep (delta time) in seconds
ENVIRONMENT_DENSITY: float = 1.225 # kg/m^3 (standard air density)

class Vec2D:
    """A 2D vector class with common operations."""
    __slots__ = ('x', 'y')

    def __init__(self, x: float = 0.0, y: float = 0.0):
        self.x = float(x)
        self.y = float(y)

    def __add__(self, other: Vec2D) -> Vec2D:
        return Vec2D(self.x + other.x, self.y + other.y)

    def __sub__(self, other: Vec2D) -> Vec2D:
        return Vec2D(self.x - other.x, self.y - other.y)

    def __mul__(self, scalar: float) -> Vec2D:
        return Vec2D(self.x * scalar, self.y * scalar)

    def __rmul__(self, scalar: float) -> Vec2D: # For scalar * Vec2D
        return self.__mul__(scalar)

    def __truediv__(self, scalar: float) -> Vec2D:
        if scalar == 0:
            # Avoid division by zero; return a large vector or raise error
            return Vec2D(self.x * 1e9, self.y * 1e9) 
        return Vec2D(self.x / scalar, self.y / scalar)
    
    @property
    def magnitude_sq(self) -> float:
        """Returns the squared magnitude of the vector."""
        return self.x**2 + self.y**2

    @property
    def magnitude(self) -> float:
        """Returns the magnitude (length) of the vector."""
        return math.sqrt(self.magnitude_sq)
    
    def normalize(self) -> Vec2D:
        """Returns a unit vector in the same direction."""
        mag = self.magnitude
        if mag == 0:
            return Vec2D(0, 0) # Avoid division by zero for zero vector
        return Vec2D(self.x / mag, self.y / mag)

    @staticmethod
    def from_angle_magnitude(angle_rad: float, magnitude: float) -> Vec2D:
        """Creates a vector from an angle (radians) and magnitude."""
        return Vec2D(magnitude * math.cos(angle_rad), magnitude * math.sin(angle_rad))

    def __repr__(self) -> str:
        return f"Vec2D({self.x:.3f}, {self.y:.3f})"

class AgentPhysicsProps:
    """Stores physical properties of an agent relevant to motion."""
    __slots__ = ("mass", "max_thrust_force", "drag_coefficient", "frontal_area")

    def __init__(self, 
                 mass: float = 1.0,              # kilograms
                 max_thrust_force: float = 10.0, # Newtons
                 drag_coefficient: float = 0.5,  # Dimensionless (Cd)
                 frontal_area: float = 0.01      # square meters (m^2)
                ):
        self.mass = max(0.1, mass) # Prevent non-positive mass
        self.max_thrust_force = max_thrust_force
        self.drag_coefficient = drag_coefficient
        self.frontal_area = frontal_area

class AgentKinematics:
    """Manages an agent's position, velocity, and orientation using vector physics."""
    __slots__ = ("position", "velocity", "direction_rad", 
                 "props", "_dt", "max_speed_cap", "current_turn_rate_rad_per_dt")

    _DEG_TO_RAD: float = math.pi / 180.0
    _RAD_TO_DEG: float = 180.0 / math.pi

    def __init__(self,
                 initial_position: Tuple[float, float] = (0.0, 0.0),
                 initial_direction_deg: float = 0.0,
                 initial_velocity: Tuple[float, float] = (0.0, 0.0),
                 physics_props: AgentPhysicsProps | None = None,
                 dt: float = FRAME_DT, # Timestep for this agent's updates
                 max_speed_cap: float | None = None, # Optional speed limit
                 initial_turn_rate_rad_per_dt: float = (5.0 * _DEG_TO_RAD) # Default turn agility
                ):
        self.position = Vec2D(initial_position[0], initial_position[1])
        self.velocity = Vec2D(initial_velocity[0], initial_velocity[1])
        self.direction_rad = (initial_direction_deg % 360.0) * self._DEG_TO_RAD
        
        self.props = physics_props if physics_props is not None else AgentPhysicsProps()
        self._dt = dt # Stores the timestep duration for its calculations
        self.max_speed_cap = max_speed_cap
        self.current_turn_rate_rad_per_dt = initial_turn_rate_rad_per_dt # Base turning speed

    @property
    def direction_deg(self) -> float:
        """Current direction in degrees."""
        return self.direction_rad * self._RAD_TO_DEG

    @property
    def speed(self) -> float:
        """Current speed (magnitude of velocity)."""
        return self.velocity.magnitude

    def _calculate_drag_force(self) -> Vec2D:
        """Calculates drag force opposing velocity, based on fluid dynamics."""
        if self.velocity.magnitude_sq == 0:
            return Vec2D(0, 0)
        
        # Drag Force: F_drag = 0.5 * rho * v^2 * Cd * A
        # rho: environment density, v: speed, Cd: drag coefficient, A: frontal area
        speed_val = self.velocity.magnitude
        drag_magnitude = 0.5 * ENVIRONMENT_DENSITY * (speed_val**2) * \
                         self.props.drag_coefficient * self.props.frontal_area
        
        # Drag acts opposite to velocity
        drag_force_direction = self.velocity.normalize() * -1.0
        return drag_force_direction * drag_magnitude

    def update(self, 
               thrust_input_normalized: float, # Normalized thrust [0, 1]
               turn_command: float             # Normalized turn command [-1 (left), 1 (right)]
              ):
        """Updates agent's state based on thrust, turn command, and physics."""
        
        # --- Forces ---
        # 1. Thrust Force (along agent's current direction)
        actual_thrust_magnitude = thrust_input_normalized * self.props.max_thrust_force
        thrust_force_vec = Vec2D.from_angle_magnitude(self.direction_rad, actual_thrust_magnitude)

        # 2. Drag Force
        drag_force_vec = self._calculate_drag_force()

        # 3. Total Force
        total_force_vec = thrust_force_vec + drag_force_vec
        
        # --- Linear Kinematics (Newton's 2nd Law: F=ma => a=F/m) ---
        acceleration_vec = total_force_vec / self.props.mass
        
        # Update velocity (v_new = v_old + a * dt)
        self.velocity += acceleration_vec * self._dt
        
        # Apply speed cap if set
        if self.max_speed_cap is not None and self.velocity.magnitude > self.max_speed_cap:
            self.velocity = self.velocity.normalize() * self.max_speed_cap
        
        # Update position (p_new = p_old + v_new * dt - semi-implicit Euler)
        self.position += self.velocity * self._dt

        # --- Rotational Kinematics ---
        # turn_command scales the agent's base turning rate for this frame
        delta_angle_rad = turn_command * self.current_turn_rate_rad_per_dt # current_turn_rate is per_dt
        
        self.direction_rad = (self.direction_rad + delta_angle_rad) % (2 * math.pi)
        if self.direction_rad < 0: # Normalize to [0, 2*pi)
            self.direction_rad += (2 * math.pi)

# --- Global functions to modify simulation environment parameters ---
def set_frame_dt(dt: float):
    """Sets the global simulation timestep (FRAME_DT)."""
    global FRAME_DT
    FRAME_DT = float(dt)

def set_environment_density(density: float):
    """Sets the global environment density for drag calculations."""
    global ENVIRONMENT_DENSITY
    ENVIRONMENT_DENSITY = float(density)