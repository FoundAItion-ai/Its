"""
Trajectory analysis metrics for NNN agent simulations.

Pure math module - no pygame, no simulation dependencies.
Operates on numpy arrays of (x, y) positions and optional metadata.

Designed to validate hypotheses H0-H4 from the NNN research paper.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

import numpy as np


# ---------------------------------------------------------------------------
# Result data classes
# ---------------------------------------------------------------------------

@dataclass
class CircleFitResult:
    center_x: float
    center_y: float
    radius: float
    residual_rms: float
    score: float  # 1.0 = perfect circle


@dataclass
class SpiralFitResult:
    score: float             # R^2 of distance-from-centroid vs time
    quality: float           # score * angular_consistency
    growth_rate: float       # slope px/frame of radial expansion
    angular_consistency: float  # mean resultant length of heading changes


@dataclass
class AreaCoverageResult:
    total_cells_visited: int
    growth_rates: np.ndarray   # new cells per window
    mean_growth_rate: float
    final_growth_rate: float


@dataclass
class TransitionPoint:
    frame: int
    transition_type: str       # "explore_to_exploit" or "exploit_to_explore"
    growth_rate_before: float
    growth_rate_after: float


@dataclass
class TrajectoryMetrics:
    straightness: float
    circle_fit: CircleFitResult
    mean_speed: float
    spiral_fit: SpiralFitResult
    area_coverage: AreaCoverageResult
    revisitation_rate: float
    transitions: List[TransitionPoint]
    bounding_box_growth: np.ndarray


# ---------------------------------------------------------------------------
# Core metric functions
# ---------------------------------------------------------------------------

def straightness_index(xs: np.ndarray, ys: np.ndarray) -> float:
    """Ratio of net displacement to total path length. 1.0 = straight, 0.0 = loop."""
    n = len(xs)
    if n < 2:
        return 1.0
    dx = np.diff(xs)
    dy = np.diff(ys)
    step_lengths = np.sqrt(dx ** 2 + dy ** 2)
    path_length = np.sum(step_lengths)
    if path_length == 0.0:
        return 1.0
    displacement = math.sqrt((xs[-1] - xs[0]) ** 2 + (ys[-1] - ys[0]) ** 2)
    return displacement / path_length


def fit_circle(xs: np.ndarray, ys: np.ndarray) -> CircleFitResult:
    """Kasa algebraic circle fit. Returns CircleFitResult with score in [0, 1]."""
    n = len(xs)
    if n < 3:
        return CircleFitResult(0.0, 0.0, 0.0, 0.0, 0.0)

    # Solve: x*a + y*b + c = x^2 + y^2
    A = np.column_stack([xs, ys, np.ones(n)])
    b = xs ** 2 + ys ** 2
    result, _, _, _ = np.linalg.lstsq(A, b, rcond=None)
    a, bv, c = result

    cx = a / 2.0
    cy = bv / 2.0
    r_sq = c + cx ** 2 + cy ** 2
    if r_sq <= 0:
        return CircleFitResult(cx, cy, 0.0, 0.0, 0.0)
    radius = math.sqrt(r_sq)

    distances = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
    residuals = distances - radius
    rms = math.sqrt(np.mean(residuals ** 2))

    score = max(0.0, 1.0 - rms / radius)
    return CircleFitResult(cx, cy, radius, rms, score)


def mean_speed(xs: np.ndarray, ys: np.ndarray, dt: float = 1.0 / 60.0) -> float:
    """Average speed in distance-units per second."""
    n = len(xs)
    if n < 2:
        return 0.0
    dx = np.diff(xs)
    dy = np.diff(ys)
    step_lengths = np.sqrt(dx ** 2 + dy ** 2)
    speeds = step_lengths / dt
    return float(np.mean(speeds))


def spiral_score(xs: np.ndarray, ys: np.ndarray) -> SpiralFitResult:
    """Measure spiral quality: radial growth + consistent turning."""
    n = len(xs)
    if n < 10:
        return SpiralFitResult(0.0, 0.0, 0.0, 0.0)

    # Radial growth: R^2 of distance-from-centroid vs frame index
    cx, cy = np.mean(xs), np.mean(ys)
    distances = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
    frames = np.arange(n, dtype=float)

    x_mean = np.mean(frames)
    y_mean = np.mean(distances)
    ss_xy = np.sum((frames - x_mean) * (distances - y_mean))
    ss_xx = np.sum((frames - x_mean) ** 2)
    ss_yy = np.sum((distances - y_mean) ** 2)

    if ss_xx == 0 or ss_yy == 0:
        r_squared = 0.0
        slope = 0.0
    else:
        slope = ss_xy / ss_xx
        r_squared = (ss_xy ** 2) / (ss_xx * ss_yy)

    # Angular consistency: mean resultant length of per-step heading changes
    dx = np.diff(xs)
    dy = np.diff(ys)
    headings = np.arctan2(dy, dx)
    dheadings = np.diff(headings)
    # Wrap to [-pi, pi]
    dheadings = (dheadings + np.pi) % (2 * np.pi) - np.pi

    if len(dheadings) == 0:
        angular_consistency = 0.0
    else:
        C = np.mean(np.cos(dheadings))
        S = np.mean(np.sin(dheadings))
        angular_consistency = math.sqrt(C ** 2 + S ** 2)

    quality = r_squared * angular_consistency
    return SpiralFitResult(
        score=r_squared,
        quality=quality,
        growth_rate=slope,
        angular_consistency=angular_consistency,
    )


def area_coverage(
    xs: np.ndarray,
    ys: np.ndarray,
    cell_size: float = 20.0,
    window_frames: int = 60,
) -> AreaCoverageResult:
    """Count new grid cells visited per time window."""
    n = len(xs)
    cell_xs = np.floor(xs / cell_size).astype(int)
    cell_ys = np.floor(ys / cell_size).astype(int)

    visited: set = set()
    growth_rates = []

    for w_start in range(0, n, window_frames):
        w_end = min(w_start + window_frames, n)
        new_count = 0
        for i in range(w_start, w_end):
            cell = (cell_xs[i], cell_ys[i])
            if cell not in visited:
                visited.add(cell)
                new_count += 1
        growth_rates.append(new_count)

    rates = np.array(growth_rates, dtype=float)
    total = len(visited)
    mean_rate = float(np.mean(rates)) if len(rates) > 0 else 0.0
    final_rate = float(rates[-1]) if len(rates) > 0 else 0.0

    return AreaCoverageResult(
        total_cells_visited=total,
        growth_rates=rates,
        mean_growth_rate=mean_rate,
        final_growth_rate=final_rate,
    )


def revisitation_rate(
    xs: np.ndarray,
    ys: np.ndarray,
    cell_size: float = 20.0,
) -> float:
    """Fraction of frames (after the first) that land in an already-visited cell."""
    n = len(xs)
    if n < 2:
        return 0.0

    cell_xs = np.floor(xs / cell_size).astype(int)
    cell_ys = np.floor(ys / cell_size).astype(int)

    visited: set = set()
    visited.add((cell_xs[0], cell_ys[0]))
    revisits = 0

    for i in range(1, n):
        cell = (cell_xs[i], cell_ys[i])
        if cell in visited:
            revisits += 1
        else:
            visited.add(cell)

    return revisits / (n - 1)


def detect_transitions(
    xs: np.ndarray,
    ys: np.ndarray,
    cell_size: float = 20.0,
    window_frames: int = 60,
    threshold_ratio: float = 0.3,
) -> List[TransitionPoint]:
    """Detect explore/exploit transitions from area coverage changepoints."""
    cov = area_coverage(xs, ys, cell_size, window_frames)
    rates = cov.growth_rates

    if len(rates) < 2:
        return []

    peak_rate = float(np.max(rates))
    if peak_rate == 0:
        return []

    threshold = threshold_ratio * peak_rate
    transitions: List[TransitionPoint] = []

    for i in range(1, len(rates)):
        prev = float(rates[i - 1])
        curr = float(rates[i])

        if prev >= threshold and curr < threshold:
            transitions.append(TransitionPoint(
                frame=i * window_frames,
                transition_type="explore_to_exploit",
                growth_rate_before=prev,
                growth_rate_after=curr,
            ))
        elif prev < threshold and curr >= threshold:
            transitions.append(TransitionPoint(
                frame=i * window_frames,
                transition_type="exploit_to_explore",
                growth_rate_before=prev,
                growth_rate_after=curr,
            ))

    return transitions


def bounding_box_growth(
    xs: np.ndarray,
    ys: np.ndarray,
    window_frames: int = 60,
) -> np.ndarray:
    """Cumulative bounding box area at each window boundary."""
    n = len(xs)
    areas = []

    for w_end in range(window_frames, n + 1, window_frames):
        x_slice = xs[:w_end]
        y_slice = ys[:w_end]
        width = float(np.max(x_slice) - np.min(x_slice))
        height = float(np.max(y_slice) - np.min(y_slice))
        areas.append(width * height)

    # Include final partial window if it wasn't already counted
    if n % window_frames != 0:
        width = float(np.max(xs) - np.min(xs))
        height = float(np.max(ys) - np.min(ys))
        areas.append(width * height)

    return np.array(areas, dtype=float)


# ---------------------------------------------------------------------------
# Convenience wrapper
# ---------------------------------------------------------------------------

def analyze_trajectory(
    xs: np.ndarray,
    ys: np.ndarray,
    dt: float = 1.0 / 60.0,
    cell_size: float = 20.0,
    window_frames: int = 60,
) -> TrajectoryMetrics:
    """Compute all trajectory metrics in one call."""
    return TrajectoryMetrics(
        straightness=straightness_index(xs, ys),
        circle_fit=fit_circle(xs, ys),
        mean_speed=mean_speed(xs, ys, dt),
        spiral_fit=spiral_score(xs, ys),
        area_coverage=area_coverage(xs, ys, cell_size, window_frames),
        revisitation_rate=revisitation_rate(xs, ys, cell_size),
        transitions=detect_transitions(xs, ys, cell_size, window_frames),
        bounding_box_growth=bounding_box_growth(xs, ys, window_frames),
    )


# ---------------------------------------------------------------------------
# Log file parser
# ---------------------------------------------------------------------------

def load_trajectory_from_log(
    log_path: str,
    agent_id: int = 0,
) -> dict:
    """Parse a batch_test log file and return numpy arrays for one agent.

    Accepts a file path string or a file-like object (for testing with StringIO).

    Returns dict with keys: xs, ys, angles, speeds, modes, frames,
    delta_dists, delta_thetas, food_freqs.
    """
    frames_list = []
    xs_list = []
    ys_list = []
    angles_list = []
    delta_dists_list = []
    delta_thetas_list = []
    food_freqs_list = []
    modes_list = []
    speeds_list = []

    if hasattr(log_path, 'read'):
        lines = log_path.read().splitlines()
    else:
        with open(log_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()

    for line in lines:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = line.split(',')
        if len(parts) < 10:
            continue
        try:
            aid = int(parts[1])
        except ValueError:
            continue
        if aid != agent_id:
            continue

        frames_list.append(int(parts[0]))
        xs_list.append(float(parts[2]))
        ys_list.append(float(parts[3]))
        angles_list.append(float(parts[4]))
        delta_dists_list.append(float(parts[5]))
        delta_thetas_list.append(float(parts[6]))
        food_freqs_list.append(float(parts[7]))
        modes_list.append(parts[8])
        speeds_list.append(float(parts[9]))

    return {
        'frames': np.array(frames_list, dtype=int),
        'xs': np.array(xs_list),
        'ys': np.array(ys_list),
        'angles': np.array(angles_list),
        'speeds': np.array(speeds_list),
        'modes': modes_list,
        'delta_dists': np.array(delta_dists_list),
        'delta_thetas': np.array(delta_thetas_list),
        'food_freqs': np.array(food_freqs_list),
    }


# ---------------------------------------------------------------------------
# Multi-trial aggregation (noise-aware)
# ---------------------------------------------------------------------------

@dataclass
class AggregateMetrics:
    """Statistics across multiple trials for the same configuration."""
    n_trials: int
    straightness: Tuple[float, float]       # (mean, std)
    circle_fit_score: Tuple[float, float]
    circle_fit_radius: Tuple[float, float]
    mean_speed: Tuple[float, float]
    spiral_quality: Tuple[float, float]
    spiral_growth_rate: Tuple[float, float]
    area_cells_visited: Tuple[float, float]
    revisitation_rate: Tuple[float, float]
    n_transitions_mean: float


def _mean_std(values: List[float]) -> Tuple[float, float]:
    """Return (mean, std) for a list of floats. Std=0 if single value."""
    if not values:
        return (0.0, 0.0)
    m = float(np.mean(values))
    s = float(np.std(values, ddof=1)) if len(values) > 1 else 0.0
    return (m, s)


def aggregate_trials(trial_metrics: List[TrajectoryMetrics]) -> AggregateMetrics:
    """Compute mean +/- std across multiple trial results.

    Accounts for stochastic noise (D_ROT) by summarising variability
    so that hypothesis checks compare means rather than single runs.
    """
    n = len(trial_metrics)
    if n == 0:
        z = (0.0, 0.0)
        return AggregateMetrics(0, z, z, z, z, z, z, z, z, 0.0)

    return AggregateMetrics(
        n_trials=n,
        straightness=_mean_std([m.straightness for m in trial_metrics]),
        circle_fit_score=_mean_std([m.circle_fit.score for m in trial_metrics]),
        circle_fit_radius=_mean_std([m.circle_fit.radius for m in trial_metrics]),
        mean_speed=_mean_std([m.mean_speed for m in trial_metrics]),
        spiral_quality=_mean_std([m.spiral_fit.quality for m in trial_metrics]),
        spiral_growth_rate=_mean_std([m.spiral_fit.growth_rate for m in trial_metrics]),
        area_cells_visited=_mean_std(
            [float(m.area_coverage.total_cells_visited) for m in trial_metrics]),
        revisitation_rate=_mean_std([m.revisitation_rate for m in trial_metrics]),
        n_transitions_mean=float(np.mean([len(m.transitions) for m in trial_metrics])),
    )


def format_aggregate(agg: AggregateMetrics) -> str:
    """Human-readable summary of aggregate metrics."""
    def _fmt(pair: Tuple[float, float]) -> str:
        return f"{pair[0]:.4f} +/- {pair[1]:.4f}"

    lines = [
        f"  Trials:             {agg.n_trials}",
        f"  Straightness:       {_fmt(agg.straightness)}",
        f"  Circle fit score:   {_fmt(agg.circle_fit_score)}",
        f"  Circle fit radius:  {_fmt(agg.circle_fit_radius)}",
        f"  Mean speed (px/s):  {_fmt(agg.mean_speed)}",
        f"  Spiral quality:     {_fmt(agg.spiral_quality)}",
        f"  Spiral growth rate: {_fmt(agg.spiral_growth_rate)}",
        f"  Area cells visited: {_fmt(agg.area_cells_visited)}",
        f"  Revisitation rate:  {_fmt(agg.revisitation_rate)}",
        f"  Transitions (mean): {agg.n_transitions_mean:.1f}",
    ]
    return "\n".join(lines)
