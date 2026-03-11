"""Static configurations and presets for the agent simulation."""
from __future__ import annotations
from typing import List, Tuple, Callable, Dict
import math

# Window dimensions
WINDOW_W, WINDOW_H = 700, 600

# Visual properties (pixels)
AGENT_RADIUS = 6
FOOD_RADIUS = 4

# --- Agent Behavior & Physics ---

# This constant scales the power difference into a turn angle.
ANGULAR_PROPORTIONALITY_CONSTANT = 0.08

# This constant controls how total power translates into forward speed.
AGENT_SPEED_SCALING_FACTOR = 5.0

# NEW: The default interval at which agents make a new turn decision.
# Set to frame time (1/60) for smooth continuous turning every frame
DEFAULT_TURN_DECISION_INTERVAL_SEC = 0.0167

# This modifier can still be used to scale the final output from the GUI
GLOBAL_SPEED_MODIFIER = 1.0

def set_global_speed_modifier(mod: float):
    """Sets the global speed modifier."""
    global GLOBAL_SPEED_MODIFIER
    GLOBAL_SPEED_MODIFIER = mod

def set_agent_speed_scaling_factor(val: float):
    """Sets the agent speed scaling factor."""
    global AGENT_SPEED_SCALING_FACTOR
    AGENT_SPEED_SCALING_FACTOR = val

def set_angular_proportionality_constant(val: float):
    """Sets the angular proportionality constant."""
    global ANGULAR_PROPORTIONALITY_CONSTANT
    ANGULAR_PROPORTIONALITY_CONSTANT = val

# Type alias for coordinates
Coord = Tuple[float, float]

# --- Food Layout Preset Functions (unchanged) ---
def _line_y(y_coord: float, count: int = 200, x_margin: float = 20.0) -> List[Coord]:
    if count <= 0: return []
    effective_width = WINDOW_W - 2 * x_margin
    if count == 1: return [(WINDOW_W / 2, y_coord)]
    step = effective_width / (count -1) if count > 1 else effective_width
    return [(x_margin + step * i, y_coord) for i in range(count)]

def three_lines_food() -> List[Coord]:
    return (_line_y(WINDOW_H * 0.35, count=150) +
            _line_y(WINDOW_H * 0.50, count=150) +
            _line_y(WINDOW_H * 0.65, count=150))

def void_food() -> List[Coord]: return []

def filled_box_food(step: int = 5) -> List[Coord]:
    box_w = WINDOW_W * 0.6; box_h = WINDOW_H * 0.6
    start_x = (WINDOW_W - box_w) / 2; start_y = (WINDOW_H - box_h) / 2
    end_x = start_x + box_w; end_y = start_y + box_h
    food_coords: List[Coord] = []
    current_x = start_x
    while current_x < end_x:
        current_y = start_y
        while current_y < end_y:
            food_coords.append((current_x, current_y))
            current_y += step
        current_x += step
    return food_coords

FOOD_PRESETS: Dict[str, Callable[[], List[Coord]]] = {
    "three_lines_dense": three_lines_food,
    "void": void_food,
    "filled_box_dense": filled_box_food,
}