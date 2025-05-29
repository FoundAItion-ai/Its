"""Static configurations and presets for the agent simulation."""
from __future__ import annotations
from typing import List, Tuple, Callable, Dict # Added Dict

# Window dimensions
WINDOW_W, WINDOW_H = 700, 600

# Visual properties (pixels)
AGENT_RADIUS = 6
FOOD_RADIUS = 4

# Type alias for coordinates
Coord = Tuple[float, float]

# --- Default Agent Physics Properties ---
# These are used by SimAgent if not overridden by GUI settings at runtime.
# They are applied when AgentPhysicsProps is instantiated.
AGENT_DEFAULT_MASS = 1.0                # kg
AGENT_DEFAULT_MAX_THRUST = 15.0         # Newtons (force capability)
AGENT_DEFAULT_DRAG_COEFF = 0.8          # Dimensionless (shape/friction factor)
AGENT_DEFAULT_FRONTAL_AREA = 0.01       # m^2 (cross-sectional area for drag)

# Default base turn rate in degrees per frame (dt).
# Used as initial turn agility if not specified otherwise or modified by agent logic.
AGENT_DEFAULT_TURN_RATE_DEG_PER_DT = 5.0 


# --- Food Layout Preset Functions ---
def _line_y(y_coord: float, count: int = 200, x_margin: float = 20.0) -> List[Coord]:
    """Helper to create a horizontal line of food items."""
    if count <= 0: return []
    effective_width = WINDOW_W - 2 * x_margin
    if count == 1: # Single item centered
        return [(WINDOW_W / 2, y_coord)]
    step = effective_width / (count -1) if count > 1 else effective_width
    return [(x_margin + step * i, y_coord) for i in range(count)]

def three_lines_food() -> List[Coord]:
    """Food preset: Three dense horizontal lines of food."""
    return (_line_y(WINDOW_H * 0.25, count=150) +
            _line_y(WINDOW_H * 0.50, count=150) +
            _line_y(WINDOW_H * 0.75, count=150))

def void_food() -> List[Coord]:
    """Food preset: No food."""
    return []

def filled_box_food(step: int = 20) -> List[Coord]:
    """Food preset: A grid of food items filling most of the screen."""
    return [(float(x), float(y))
            for x in range(step, WINDOW_W - step // 2, step)
            for y in range(step, WINDOW_H - step // 2, step)]

# Dictionary mapping preset names to their generator functions
FOOD_PRESETS: Dict[str, Callable[[], List[Coord]]] = {
    "three_lines_dense": three_lines_food,
    "void": void_food,
    "filled_box_dense": filled_box_food,
}

# Example AGENT_PRESETS (currently unused by main.py but could be integrated)
# AGENT_PRESETS = {
# "stochastic_default": {"class_name": "stochastic", "params": {}},