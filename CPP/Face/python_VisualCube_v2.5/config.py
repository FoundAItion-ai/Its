"""Static configurations and presets for the agent simulation."""
from __future__ import annotations
from typing import List, Tuple, Callable, Dict
import math

# Window dimensions
WINDOW_W, WINDOW_H = 700, 600

# Visual properties (pixels)
AGENT_RADIUS = 6
FOOD_RADIUS = 4

# Type alias for coordinates
Coord = Tuple[float, float]

# --- Agent Motion Properties ---
AGENT_CONSTANT_SPEED = 30.0
AGENT_DEFAULT_TURN_RATE_DEG_PER_DT = 5.0

# --- Default Agent Physics Properties (Legacy/Structural) ---
AGENT_DEFAULT_MASS = 1.0
AGENT_DEFAULT_MAX_THRUST = 15.0
AGENT_DEFAULT_DRAG_COEFF = 0.8
AGENT_DEFAULT_FRONTAL_AREA = 0.01


# --- Food Layout Preset Functions ---
def _line_y(y_coord: float, count: int = 200, x_margin: float = 20.0) -> List[Coord]:
    """Helper to create a horizontal line of food items."""
    if count <= 0: return []
    effective_width = WINDOW_W - 2 * x_margin
    if count == 1: return [(WINDOW_W / 2, y_coord)]
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

def filled_box_food(step: int = 5) -> List[Coord]:
    """Food preset: A grid of food items tightly packed in a central box."""
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

# Dictionary mapping preset names to their generator functions
FOOD_PRESETS: Dict[str, Callable[[], List[Coord]]] = {
    "three_lines_dense": three_lines_food,
    "void": void_food,
    "filled_box_dense": filled_box_food,
}