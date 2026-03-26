"""Unit tests for trajectory_analyzer using synthetic trajectories."""
import io
import math
import sys
from pathlib import Path
from typing import Tuple

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).parent.parent))

from trajectory_analyzer import (
    straightness_index,
    fit_circle,
    mean_speed,
    spiral_score,
    area_coverage,
    revisitation_rate,
    detect_transitions,
    bounding_box_growth,
    analyze_trajectory,
    load_trajectory_from_log,
)


# ---------------------------------------------------------------------------
# Synthetic trajectory generators
# ---------------------------------------------------------------------------

def _make_straight_line(
    n: int = 300, angle_deg: float = 45.0, speed: float = 5.0,
    start: Tuple[float, float] = (0.0, 0.0),
) -> Tuple[np.ndarray, np.ndarray]:
    angle = math.radians(angle_deg)
    dx = speed * math.cos(angle)
    dy = speed * math.sin(angle)
    xs = start[0] + np.arange(n) * dx
    ys = start[1] + np.arange(n) * dy
    return xs, ys


def _make_circle(
    n: int = 360, radius: float = 100.0,
    center: Tuple[float, float] = (700.0, 450.0),
) -> Tuple[np.ndarray, np.ndarray]:
    theta = np.linspace(0, 2 * np.pi, n, endpoint=False)
    xs = center[0] + radius * np.cos(theta)
    ys = center[1] + radius * np.sin(theta)
    return xs, ys


def _make_spiral(
    n: int = 600, start_radius: float = 10.0, growth_rate: float = 0.5,
    center: Tuple[float, float] = (700.0, 450.0),
) -> Tuple[np.ndarray, np.ndarray]:
    angular_step = 2 * np.pi / 60  # one revolution per 60 frames
    theta = np.arange(n) * angular_step
    r = start_radius + growth_rate * np.arange(n)
    xs = center[0] + r * np.cos(theta)
    ys = center[1] + r * np.sin(theta)
    return xs, ys


def _make_random_walk(
    n: int = 300, step_size: float = 5.0, seed: int = 42,
) -> Tuple[np.ndarray, np.ndarray]:
    rng = np.random.default_rng(seed)
    angles = rng.uniform(0, 2 * np.pi, n - 1)
    dx = step_size * np.cos(angles)
    dy = step_size * np.sin(angles)
    xs = np.concatenate([[0.0], np.cumsum(dx)])
    ys = np.concatenate([[0.0], np.cumsum(dy)])
    return xs, ys


def _make_bounded_oscillation(
    n: int = 600,
    center: Tuple[float, float] = (700.0, 450.0),
    amplitude: float = 50.0,
) -> Tuple[np.ndarray, np.ndarray]:
    t = np.linspace(0, 4 * np.pi, n)
    xs = center[0] + amplitude * np.sin(t)
    ys = center[1] + amplitude * np.sin(2 * t)
    return xs, ys


# ---------------------------------------------------------------------------
# Tests: Straightness Index
# ---------------------------------------------------------------------------

class TestStraightnessIndex:
    def test_straight_line_near_one(self):
        xs, ys = _make_straight_line()
        assert straightness_index(xs, ys) > 0.99

    def test_circle_near_zero(self):
        # Full circle returns near start
        xs, ys = _make_circle(n=361)
        # Close the loop by appending the first point
        xs = np.append(xs, xs[0])
        ys = np.append(ys, ys[0])
        assert straightness_index(xs, ys) < 0.05

    def test_spiral_moderate(self):
        xs, ys = _make_spiral()
        s = straightness_index(xs, ys)
        assert 0.01 < s < 0.9

    def test_random_walk_low(self):
        xs, ys = _make_random_walk()
        assert straightness_index(xs, ys) < 0.5

    def test_single_point(self):
        assert straightness_index(np.array([5.0]), np.array([5.0])) == 1.0

    def test_two_points(self):
        assert straightness_index(np.array([0.0, 1.0]), np.array([0.0, 0.0])) == 1.0


# ---------------------------------------------------------------------------
# Tests: Circle Fit
# ---------------------------------------------------------------------------

class TestCircleFit:
    def test_perfect_circle_high_score(self):
        xs, ys = _make_circle(radius=100)
        result = fit_circle(xs, ys)
        assert result.score > 0.95
        assert abs(result.radius - 100) < 5

    def test_straight_line_low_score(self):
        xs, ys = _make_straight_line()
        result = fit_circle(xs, ys)
        # Kasa fit maps a line to a very large circle; score is moderate, not high
        assert result.score < 0.6

    def test_spiral_moderate_score(self):
        xs, ys = _make_spiral()
        result = fit_circle(xs, ys)
        assert 0.0 <= result.score < 0.8

    def test_radius_estimation_accuracy(self):
        for r in [50, 100, 200]:
            xs, ys = _make_circle(radius=r)
            result = fit_circle(xs, ys)
            assert abs(result.radius - r) / r < 0.05

    def test_degenerate_too_few_points(self):
        result = fit_circle(np.array([1.0, 2.0]), np.array([1.0, 2.0]))
        assert result.score == 0.0


# ---------------------------------------------------------------------------
# Tests: Mean Speed
# ---------------------------------------------------------------------------

class TestMeanSpeed:
    def test_constant_speed(self):
        # speed=5 px/frame, dt=1/60 => 300 px/s
        xs, ys = _make_straight_line(n=100, angle_deg=0, speed=5.0)
        s = mean_speed(xs, ys, dt=1.0 / 60.0)
        assert abs(s - 300.0) < 1.0

    def test_stationary(self):
        xs = np.ones(50) * 100.0
        ys = np.ones(50) * 200.0
        assert mean_speed(xs, ys) == 0.0


# ---------------------------------------------------------------------------
# Tests: Spiral Score
# ---------------------------------------------------------------------------

class TestSpiralScore:
    def test_spiral_high_quality(self):
        xs, ys = _make_spiral()
        result = spiral_score(xs, ys)
        assert result.quality > 0.6

    def test_circle_low_quality(self):
        # Circle has no radial growth, so R^2 of distance vs time should be low
        xs, ys = _make_circle(n=360)
        result = spiral_score(xs, ys)
        assert result.quality < 0.3

    def test_straight_line_low_quality(self):
        # Straight line: high R^2 but low angular consistency (no turning)
        xs, ys = _make_straight_line()
        result = spiral_score(xs, ys)
        assert result.quality < 0.3

    def test_random_walk_low_quality(self):
        xs, ys = _make_random_walk()
        result = spiral_score(xs, ys)
        assert result.quality < 0.3

    def test_growth_rate_positive_for_spiral(self):
        xs, ys = _make_spiral()
        result = spiral_score(xs, ys)
        assert result.growth_rate > 0

    def test_quality_degrades_with_noise(self):
        xs, ys = _make_spiral(n=600)
        clean = spiral_score(xs, ys)
        rng = np.random.default_rng(42)
        noisy_xs = xs + rng.normal(0, 10, len(xs))
        noisy_ys = ys + rng.normal(0, 10, len(ys))
        noisy = spiral_score(noisy_xs, noisy_ys)
        assert noisy.quality < clean.quality


# ---------------------------------------------------------------------------
# Tests: Area Coverage
# ---------------------------------------------------------------------------

class TestAreaCoverage:
    def test_spiral_growing_rate(self):
        xs, ys = _make_spiral(n=600)
        result = area_coverage(xs, ys, cell_size=20, window_frames=60)
        assert result.mean_growth_rate > 0

    def test_bounded_oscillation_declining_rate(self):
        xs, ys = _make_bounded_oscillation(n=600)
        result = area_coverage(xs, ys, cell_size=10, window_frames=60)
        # Later windows should discover fewer new cells
        if len(result.growth_rates) >= 4:
            first_half = float(np.mean(result.growth_rates[:len(result.growth_rates) // 2]))
            second_half = float(np.mean(result.growth_rates[len(result.growth_rates) // 2:]))
            assert second_half <= first_half

    def test_straight_line_positive_growth(self):
        xs, ys = _make_straight_line(n=300)
        result = area_coverage(xs, ys, cell_size=20, window_frames=60)
        assert result.total_cells_visited > 1

    def test_stationary_one_cell(self):
        xs = np.ones(120) * 100.0
        ys = np.ones(120) * 200.0
        result = area_coverage(xs, ys, cell_size=20, window_frames=60)
        assert result.total_cells_visited == 1
        # After first window, no new cells
        assert result.growth_rates[-1] == 0


# ---------------------------------------------------------------------------
# Tests: Revisitation Rate
# ---------------------------------------------------------------------------

class TestRevisitationRate:
    def test_bounded_oscillation_high(self):
        xs, ys = _make_bounded_oscillation(n=600, amplitude=50)
        r = revisitation_rate(xs, ys, cell_size=10)
        assert r > 0.5

    def test_straight_line_low(self):
        xs, ys = _make_straight_line(n=300, speed=25)
        r = revisitation_rate(xs, ys, cell_size=20)
        assert r < 0.15

    def test_spiral_low_to_moderate(self):
        xs, ys = _make_spiral(n=600)
        r = revisitation_rate(xs, ys, cell_size=20)
        assert r < 0.5

    def test_stationary_maximum(self):
        xs = np.ones(50) * 100.0
        ys = np.ones(50) * 200.0
        r = revisitation_rate(xs, ys, cell_size=20)
        assert r == 1.0


# ---------------------------------------------------------------------------
# Tests: Transition Detection
# ---------------------------------------------------------------------------

class TestTransitionDetection:
    def test_spiral_then_bounded_detects_transition(self):
        # Concatenate an expanding spiral with a bounded oscillation
        xs1, ys1 = _make_spiral(n=300, start_radius=10, growth_rate=1.0,
                                center=(400, 400))
        # Start bounded part where spiral ended
        xs2, ys2 = _make_bounded_oscillation(n=300,
                                             center=(float(xs1[-1]), float(ys1[-1])),
                                             amplitude=20)
        xs = np.concatenate([xs1, xs2])
        ys = np.concatenate([ys1, ys2])
        transitions = detect_transitions(xs, ys, cell_size=15, window_frames=60)
        explore_to_exploit = [t for t in transitions
                              if t.transition_type == "explore_to_exploit"]
        assert len(explore_to_exploit) >= 1

    def test_pure_spiral_no_exploit_transition(self):
        xs, ys = _make_spiral(n=600, growth_rate=1.0)
        transitions = detect_transitions(xs, ys, cell_size=15, window_frames=60)
        explore_to_exploit = [t for t in transitions
                              if t.transition_type == "explore_to_exploit"]
        assert len(explore_to_exploit) == 0

    def test_pure_bounded_no_explore_transition(self):
        xs, ys = _make_bounded_oscillation(n=600)
        transitions = detect_transitions(xs, ys, cell_size=10, window_frames=60)
        exploit_to_explore = [t for t in transitions
                              if t.transition_type == "exploit_to_explore"]
        assert len(exploit_to_explore) == 0


# ---------------------------------------------------------------------------
# Tests: Bounding Box Growth
# ---------------------------------------------------------------------------

class TestBoundingBoxGrowth:
    def test_spiral_monotonic_growth(self):
        xs, ys = _make_spiral(n=600, growth_rate=1.0)
        areas = bounding_box_growth(xs, ys, window_frames=60)
        # Each window's cumulative area should be >= previous
        for i in range(1, len(areas)):
            assert areas[i] >= areas[i - 1]

    def test_bounded_oscillation_plateaus(self):
        xs, ys = _make_bounded_oscillation(n=600)
        areas = bounding_box_growth(xs, ys, window_frames=60)
        if len(areas) >= 4:
            # Later areas should be very close to each other (plateau)
            late_diff = abs(areas[-1] - areas[-2])
            early_diff = abs(areas[1] - areas[0])
            assert late_diff <= early_diff + 1.0

    def test_straight_line_grows(self):
        xs, ys = _make_straight_line(n=300, speed=5)
        areas = bounding_box_growth(xs, ys, window_frames=60)
        assert areas[-1] > areas[0]


# ---------------------------------------------------------------------------
# Tests: analyze_trajectory (integration)
# ---------------------------------------------------------------------------

class TestAnalyzeTrajectory:
    def test_returns_all_fields(self):
        xs, ys = _make_spiral(n=300)
        m = analyze_trajectory(xs, ys)
        assert isinstance(m.straightness, float)
        assert isinstance(m.circle_fit, object)
        assert isinstance(m.mean_speed, float)
        assert isinstance(m.spiral_fit, object)
        assert isinstance(m.area_coverage, object)
        assert isinstance(m.revisitation_rate, float)
        assert isinstance(m.transitions, list)
        assert isinstance(m.bounding_box_growth, np.ndarray)

    def test_distinct_profiles(self):
        spiral_xs, spiral_ys = _make_spiral(n=300)
        line_xs, line_ys = _make_straight_line(n=300)

        m_spiral = analyze_trajectory(spiral_xs, spiral_ys)
        m_line = analyze_trajectory(line_xs, line_ys)

        # Spiral should have higher spiral quality than line
        assert m_spiral.spiral_fit.quality > m_line.spiral_fit.quality
        # Line should have higher straightness
        assert m_line.straightness > m_spiral.straightness


# ---------------------------------------------------------------------------
# Tests: load_trajectory_from_log
# ---------------------------------------------------------------------------

class TestLoadTrajectoryFromLog:
    def test_parses_sample_log(self):
        sample = io.StringIO(
            "# Simulation Log\n"
            "# --- CONFIGURATION ---\n"
            "# Agent Type: inverter\n"
            "# --- DATA ---\n"
            "# frame,agent_id,pos_x,pos_y,angle_rad,delta_dist,delta_theta_rad,"
            "food_freq,mode,speed,inv0_L,inv0_R\n"
            "1,0,60.00,765.00,2.3908,0.0000,-0.041165,0.00,HIGH,0.00,0,0\n"
            "2,0,60.50,764.50,2.4000,1.2000,0.009200,0.00,HIGH,72.00,1,0\n"
            "3,0,61.00,764.00,2.4100,1.1000,0.010000,5.00,LOW,66.00,0,1\n"
            "# --- FINAL_STATS ---\n"
            "# food_eaten=1\n"
        )
        data = load_trajectory_from_log(sample, agent_id=0)
        assert len(data['xs']) == 3
        assert data['frames'][0] == 1
        assert abs(data['xs'][1] - 60.5) < 0.01
        assert data['modes'][2] == 'LOW'
        assert abs(data['speeds'][1] - 72.0) < 0.01
        assert abs(data['food_freqs'][2] - 5.0) < 0.01
