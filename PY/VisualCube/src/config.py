"""Static configurations and presets for the agent simulation."""
from __future__ import annotations
from typing import List, Tuple, Callable, Dict
import math

# Window dimensions
WINDOW_W, WINDOW_H = 1400, 900

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

# Stats history cap: 60 seconds at 60fps
STATS_HISTORY_MAX_LEN = 3600

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
    # Scale food count so dots stay dense regardless of window size (~1 dot per 5px)
    count = max(150, WINDOW_W // 5)
    return (_line_y(WINDOW_H * 0.35, count=count) +
            _line_y(WINDOW_H * 0.50, count=count) +
            _line_y(WINDOW_H * 0.65, count=count))

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

def gradient_band_food() -> list:
    """Horizontal band at mid-height with left-to-right density gradient (sparse→dense).

    Returns 3-tuples (x, y, radius) with small dots (r=2) to create a solid-bar
    appearance at high density.  Multiple rows fill a narrow band (~20px tall).
    """
    import random as _rnd
    y_center = WINDOW_H * 0.5
    x_margin = 40.0
    x_start = x_margin
    x_end = WINDOW_W - x_margin
    x_span = x_end - x_start
    dot_r = 2  # small dots so dense end looks like a solid bar

    # Exponential interpolation of gap: sparse (15px) on left → dense (2px) on right
    gap_left, gap_right = 15.0, 2.0
    food_coords = []
    # Fill multiple rows within a narrow vertical band
    row_offsets = [-8, -4, 0, 4, 8]
    for y_off in row_offsets:
        x = x_start + _rnd.uniform(0, 3)
        while x <= x_end:
            t = (x - x_start) / x_span if x_span > 0 else 0.0
            y = y_center + y_off + _rnd.uniform(-2, 2)
            food_coords.append((x, y, dot_r))
            gap = gap_left * (gap_right / gap_left) ** t
            x += gap
    return food_coords

def _uniform_band_food(gap: float) -> list:
    """Horizontal band at mid-height with uniform density."""
    import random as _rnd
    y_center = WINDOW_H * 0.5
    x_margin = 40.0
    x_start = x_margin
    x_end = WINDOW_W - x_margin
    dot_r = 2
    food_coords = []
    row_offsets = [-8, -4, 0, 4, 8]
    for y_off in row_offsets:
        x = x_start + _rnd.uniform(0, gap * 0.5)
        while x <= x_end:
            y = y_center + y_off + _rnd.uniform(-2, 2)
            food_coords.append((x, y, dot_r))
            x += gap
    return food_coords

def sparse_band_food() -> list:
    """Uniform sparse horizontal band (gap ~15px)."""
    return _uniform_band_food(gap=15.0)

def dense_band_food() -> list:
    """Uniform dense horizontal band (gap ~3px)."""
    return _uniform_band_food(gap=3.0)

def patchy_food() -> List[Coord]:
    """10 Gaussian clusters (~45 dots each, sigma=15px) scattered across 80% of window."""
    import random as _rnd
    food_coords: List[Coord] = []
    margin_x = WINDOW_W * 0.1
    margin_y = WINDOW_H * 0.1
    for _ in range(10):
        cx = _rnd.uniform(margin_x, WINDOW_W - margin_x)
        cy = _rnd.uniform(margin_y, WINDOW_H - margin_y)
        for _ in range(45):
            x = cx + _rnd.gauss(0, 15)
            y = cy + _rnd.gauss(0, 15)
            x = max(0, min(WINDOW_W, x))
            y = max(0, min(WINDOW_H, y))
            food_coords.append((x, y, 2))
    return food_coords

def diagonal_band_food() -> list:
    """Band running bottom-left to top-right using rotated multi-row logic."""
    import random as _rnd
    food_coords = []
    dot_r = 2
    gap = 5
    row_offsets = [-10, -5, 0, 5, 10]
    # Diagonal from (margin, H-margin) to (W-margin, margin)
    margin = 40.0
    x0, y0 = margin, WINDOW_H - margin
    x1, y1 = WINDOW_W - margin, margin
    dx, dy = x1 - x0, y1 - y0
    length = math.sqrt(dx * dx + dy * dy)
    ux, uy = dx / length, dy / length  # unit along band
    px, py = -uy, ux  # perpendicular
    for row_off in row_offsets:
        t = _rnd.uniform(0, gap * 0.5)
        while t <= length:
            jx = _rnd.uniform(-2, 2)
            jy = _rnd.uniform(-2, 2)
            x = x0 + ux * t + px * row_off + jx
            y = y0 + uy * t + py * row_off + jy
            food_coords.append((x, y, dot_r))
            t += gap
    return food_coords

def filled_circle_food() -> List[Coord]:
    """Filled disk of food at center, radius=35% of min(W,H), dots packed at ~5px gap."""
    food_coords: List[Coord] = []
    cx, cy = WINDOW_W / 2, WINDOW_H / 2
    radius = 0.35 * min(WINDOW_W, WINDOW_H)
    gap = 5
    r_sq = radius * radius
    y = cy - radius
    while y <= cy + radius:
        x = cx - radius
        while x <= cx + radius:
            if (x - cx) ** 2 + (y - cy) ** 2 <= r_sq:
                food_coords.append((x, y, 2))
            x += gap
        y += gap
    return food_coords

def two_bands_food() -> list:
    """Two horizontal bands at 35% and 65% height."""
    import random as _rnd
    food_coords = []
    dot_r = 2
    gap = 5
    x_margin = 40.0
    row_offsets = [-8, -4, 0, 4, 8]
    for y_frac in [0.35, 0.65]:
        y_center = WINDOW_H * y_frac
        for y_off in row_offsets:
            x = x_margin + _rnd.uniform(0, gap * 0.5)
            while x <= WINDOW_W - x_margin:
                y = y_center + y_off + _rnd.uniform(-2, 2)
                food_coords.append((x, y, dot_r))
                x += gap
    return food_coords

def dot_cluster_food() -> List[Coord]:
    """Single tight cluster (~200 dots, sigma=8px) at a fixed position."""
    import random as _rnd
    food_coords: List[Coord] = []
    cx, cy = WINDOW_W * 0.7, WINDOW_H * 0.4
    for _ in range(200):
        x = cx + _rnd.gauss(0, 8)
        y = cy + _rnd.gauss(0, 8)
        x = max(0, min(WINDOW_W, x))
        y = max(0, min(WINDOW_H, y))
        food_coords.append((x, y, 2))
    return food_coords

FOOD_PRESETS: Dict[str, Callable[[], List[Coord]]] = {
    "three_lines_dense": three_lines_food,
    "void": void_food,
    "filled_box_dense": filled_box_food,
    "gradient_band": gradient_band_food,
    "sparse_band": sparse_band_food,
    "dense_band": dense_band_food,
    "patchy": patchy_food,
    "diagonal_band": diagonal_band_food,
    "filled_circle": filled_circle_food,
    "two_bands": two_bands_food,
    "dot_cluster": dot_cluster_food,
}

# Spawn hints as (x, y_fraction_of_WINDOW_H) — y is a fraction, resolved at runtime
# so auto-resize is accounted for.
FOOD_SPAWN_HINTS: Dict[str, Tuple[float, float]] = {
    "void":          (-1, 0.5),              # center — -1 means use WINDOW_W/2 at runtime
    "gradient_band": (60, 0.5 - 30/900),   # just above band center
    "sparse_band":   (60, 0.5 - 30/900),
    "dense_band":    (60, 0.5 - 30/900),
    "three_lines_dense": (120, 0.5),
    "filled_box_dense":  (120, 0.5),
    "patchy":        (60, 0.5),
    "diagonal_band": (60, 0.85),
    "filled_circle": (120, 0.15),
    "two_bands":     (60, 0.5),
    "dot_cluster":   (60, 0.5),
}