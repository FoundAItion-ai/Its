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
class CoverageRatioResult:
    fcr: float            # Final Coverage Ratio = (r_max / r1)^2
    lcr: float            # Linear Coverage Ratio = N_hit / N_total
    r1: float             # Radius at first revolution peak
    r_max: float          # Maximum radial distance from centroid
    n_peaks: int          # Number of radial peaks detected


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
    coverage_ratios: CoverageRatioResult


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


def _find_radial_peaks(
    xs: np.ndarray, ys: np.ndarray,
) -> Tuple[List[int], List[float], float, float, np.ndarray]:
    """Find per-revolution radial distance peaks from centroid.

    Returns (peak_indices, peak_dists, cx, cy, distances).
    Returns empty lists if fewer than 10 points.
    """
    n = len(xs)
    if n < 10:
        return [], [], 0.0, 0.0, np.array([])

    cx, cy = float(np.mean(xs)), float(np.mean(ys))
    distances = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)

    min_peak_gap = max(30, n // 100)
    peak_indices: List[int] = []
    peak_dists: List[float] = []
    last_peak = -min_peak_gap
    for i in range(1, n - 1):
        if distances[i] > distances[i - 1] and distances[i] > distances[i + 1]:
            if i - last_peak >= min_peak_gap:
                peak_indices.append(i)
                peak_dists.append(float(distances[i]))
                last_peak = i

    return peak_indices, peak_dists, cx, cy, distances


def spiral_score(xs: np.ndarray, ys: np.ndarray) -> SpiralFitResult:
    """Measure spiral quality: radial growth + consistent turning.

    Uses envelope-based radial growth: fits a line through the per-revolution
    peak distances from centroid, rather than raw distance vs frame. This
    correctly captures outward expansion even when the spiral loops back past
    the centroid each revolution.
    """
    n = len(xs)
    if n < 10:
        return SpiralFitResult(0.0, 0.0, 0.0, 0.0)

    peak_indices, peak_dists_list, cx, cy, distances = _find_radial_peaks(xs, ys)

    # Need at least 3 peaks (revolutions) to identify a spiral.
    if len(peak_indices) < 3:
        return SpiralFitResult(0.0, 0.0, 0.0, 0.0)

    pk_frames = np.array(peak_indices, dtype=float)
    pk_dists = np.array(peak_dists_list)

    # Linear regression on envelope peaks
    x_mean = np.mean(pk_frames)
    y_mean = np.mean(pk_dists)
    ss_xy = np.sum((pk_frames - x_mean) * (pk_dists - y_mean))
    ss_xx = np.sum((pk_frames - x_mean) ** 2)
    ss_yy = np.sum((pk_dists - y_mean) ** 2)

    if ss_xx == 0 or ss_yy == 0:
        r_squared = 0.0
        slope = 0.0
    else:
        slope = ss_xy / ss_xx
        r_squared = (ss_xy ** 2) / (ss_xx * ss_yy)

    # Angular consistency: sign consistency of per-segment heading changes.
    # Per-frame heading changes are too small at high frame rates for MRL to
    # discriminate turning direction.  Instead, divide the trajectory into
    # ~20 equal segments and measure what fraction of segments turn in the
    # dominant direction.  A spiral turns consistently one way (~1.0); a
    # random walk alternates (~0.0).
    dx = np.diff(xs)
    dy = np.diff(ys)
    headings = np.arctan2(dy, dx)
    dheadings = np.diff(headings)
    # Wrap to [-pi, pi]
    dheadings = (dheadings + np.pi) % (2 * np.pi) - np.pi

    n_segments = 20
    seg_len = max(1, len(dheadings) // n_segments)
    seg_signs = []
    for i in range(0, len(dheadings) - seg_len + 1, seg_len):
        seg_cumulative = np.sum(dheadings[i:i + seg_len])
        if abs(seg_cumulative) > 1e-10:
            seg_signs.append(np.sign(seg_cumulative))

    if len(seg_signs) >= 3:
        angular_consistency = abs(np.mean(seg_signs))
    else:
        angular_consistency = 0.0

    quality = r_squared * angular_consistency
    return SpiralFitResult(
        score=r_squared,
        quality=quality,
        growth_rate=slope,
        angular_consistency=angular_consistency,
    )


def coverage_ratios(xs: np.ndarray, ys: np.ndarray) -> CoverageRatioResult:
    """Compute FCR (Final Coverage Ratio) and LCR (Linear Coverage Ratio).

    FCR measures how much the spiral expanded: (r_max / r1)^2.
    LCR measures what fraction of the expanded area the path swept through.

    The "unit area" is the square inscribing the first revolution circle
    (side = 2 * r1). The final area is the square inscribing the maximum
    extent (side = 2 * r_max). LCR = cells_hit / total_cells in that grid.
    """
    peak_indices, peak_dists, cx, cy, distances = _find_radial_peaks(xs, ys)

    if len(peak_indices) < 1 or len(distances) == 0:
        return CoverageRatioResult(0.0, 0.0, 0.0, 0.0, 0)

    r1 = peak_dists[0]
    r_max = float(np.max(distances))

    if r1 < 1.0:
        return CoverageRatioResult(0.0, 0.0, r1, r_max, len(peak_indices))

    fcr = (r_max / r1) ** 2

    # LCR: tile the inscribed square with cells of size 2*r1
    cell_size = 2.0 * r1
    grid_origin_x = cx - r_max
    grid_origin_y = cy - r_max
    grid_side = 2.0 * r_max
    n_cells_per_side = math.ceil(grid_side / cell_size)
    n_total = n_cells_per_side * n_cells_per_side

    if n_total < 1:
        return CoverageRatioResult(fcr, 0.0, r1, r_max, len(peak_indices))

    visited: set = set()
    for i in range(len(xs)):
        col = int((xs[i] - grid_origin_x) / cell_size)
        row = int((ys[i] - grid_origin_y) / cell_size)
        col = max(0, min(col, n_cells_per_side - 1))
        row = max(0, min(row, n_cells_per_side - 1))
        visited.add((col, row))

    lcr = len(visited) / n_total

    return CoverageRatioResult(
        fcr=fcr, lcr=lcr, r1=r1, r_max=r_max, n_peaks=len(peak_indices),
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
        coverage_ratios=coverage_ratios(xs, ys),
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
    fcr: Tuple[float, float]
    lcr: Tuple[float, float]
    n_transitions_mean: float
    spiral_detection_rate: float = 0.0      # fraction of trials with spiral_quality > 0.1


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
        return AggregateMetrics(0, z, z, z, z, z, z, z, z, z, z, 0.0, 0.0)

    spiral_qualities = [m.spiral_fit.quality for m in trial_metrics]

    return AggregateMetrics(
        n_trials=n,
        straightness=_mean_std([m.straightness for m in trial_metrics]),
        circle_fit_score=_mean_std([m.circle_fit.score for m in trial_metrics]),
        circle_fit_radius=_mean_std([m.circle_fit.radius for m in trial_metrics]),
        mean_speed=_mean_std([m.mean_speed for m in trial_metrics]),
        spiral_quality=_mean_std(spiral_qualities),
        spiral_growth_rate=_mean_std([m.spiral_fit.growth_rate for m in trial_metrics]),
        area_cells_visited=_mean_std(
            [float(m.area_coverage.total_cells_visited) for m in trial_metrics]),
        revisitation_rate=_mean_std([m.revisitation_rate for m in trial_metrics]),
        fcr=_mean_std([m.coverage_ratios.fcr for m in trial_metrics]),
        lcr=_mean_std([m.coverage_ratios.lcr for m in trial_metrics]),
        n_transitions_mean=float(np.mean([len(m.transitions) for m in trial_metrics])),
        spiral_detection_rate=sum(1 for q in spiral_qualities if q > 0.1) / n,
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
        f"  FCR:                {_fmt(agg.fcr)}",
        f"  LCR:                {_fmt(agg.lcr)}",
        f"  Transitions (mean): {agg.n_transitions_mean:.1f}",
    ]
    return "\n".join(lines)
