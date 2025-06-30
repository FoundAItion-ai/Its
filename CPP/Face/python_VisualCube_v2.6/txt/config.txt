"""Static configurations and presets for the agent simulation."""
from __future__ import annotations
from typing import List, Tuple, Callable, Dict
import math # Added for math.ceil if needed, though not strictly for current plan

# Window dimensions
WINDOW_W, WINDOW_H = 700, 600

# Visual properties (pixels)
AGENT_RADIUS = 6
FOOD_RADIUS = 4

# Type alias for coordinates
Coord = Tuple[float, float]

# --- Agent Motion Properties ---
AGENT_CONSTANT_SPEED = 75.0
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
    if count == 1:
        return [(WINDOW_W / 2, y_coord)]
    # Step calculation ensures 'count' items are placed, including start and end.
    # If count > 1, there are (count - 1) intervals.
    step = effective_width / (count -1) if count > 1 else effective_width
    return [(x_margin + step * i, y_coord) for i in range(count)]

def three_lines_food() -> List[Coord]:
    """Food preset: Three dense horizontal lines of food."""
    # Note: With count=150 and x_margin=20, step is (700-40)/(150-1) = 660/149 approx 4.43px
    # Food diameter is FOOD_RADIUS*2 = 8px. So, these lines have overlapping food items.
    return (_line_y(WINDOW_H * 0.25, count=150) +
            _line_y(WINDOW_H * 0.50, count=150) +
            _line_y(WINDOW_H * 0.75, count=150))

def void_food() -> List[Coord]:
    """Food preset: No food."""
    return []

def filled_box_food(step: int = 5) -> List[Coord]: # Default step for "dense"
    """
    Food preset: A grid of food items tightly packed in a central box.
    The box covers approximately the middle 60% of the screen area.
    A smaller 'step' value leads to higher density and more items.
    Default step of 5px with FOOD_RADIUS=4px (diameter 8px) means items will overlap,
    creating a visual density somewhat comparable to the overlapping items in 'three_lines_food'.
    """
    box_width_factor = 0.6  # Box will take up 60% of window width
    box_height_factor = 0.6 # Box will take up 60% of window height

    box_actual_width = WINDOW_W * box_width_factor
    box_actual_height = WINDOW_H * box_height_factor

    # Calculate top-left corner of the central box
    start_x_coord = (WINDOW_W - box_actual_width) / 2
    start_y_coord = (WINDOW_H - box_actual_height) / 2

    # Determine the boundary for placing food items
    # Food items are placed if their (x,y) is within this conceptual box
    end_x_boundary = start_x_coord + box_actual_width
    end_y_boundary = start_y_coord + box_actual_height

    food_coords: List[Coord] = []

    current_x = start_x_coord
    while current_x < end_x_boundary:
        current_y = start_y_coord
        while current_y < end_y_boundary:
            food_coords.append((current_x, current_y))
            current_y += step
        current_x += step

    return food_coords

# Dictionary mapping preset names to their generator functions
FOOD_PRESETS: Dict[str, Callable[[], List[Coord]]] = {
    "three_lines_dense": three_lines_food,
    "void": void_food,
    "filled_box_dense": filled_box_food, # Calls the updated function
}