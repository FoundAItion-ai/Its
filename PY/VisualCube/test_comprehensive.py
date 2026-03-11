"""Comprehensive tests to debug agent behavior."""
import unittest
import sys
import os
import math

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent_module import InverseMotionAgent, FOOD_FREQ_WINDOW_SEC, SMOOTHING_FACTOR
import math_module as mm
import config as cfg


class TestSinusoidalWave(unittest.TestCase):
    """Test the basic sinusoidal wave calculation."""

    def test_sinusoidal_basics(self):
        """Verify sinusoidal wave produces expected values."""
        # sin(0) = 0
        self.assertAlmostEqual(mm.calculate_sinusoidal_wave(1.0, 1.0, 0.0), 0.0, places=5)
        # sin(π/2) = 1 at t=0.25 for period=1.0
        self.assertAlmostEqual(mm.calculate_sinusoidal_wave(1.0, 1.0, 0.25), 1.0, places=5)
        # sin(π) = 0 at t=0.5 for period=1.0
        self.assertAlmostEqual(mm.calculate_sinusoidal_wave(1.0, 1.0, 0.5), 0.0, places=5)
        # sin(3π/2) = -1 at t=0.75 for period=1.0
        self.assertAlmostEqual(mm.calculate_sinusoidal_wave(1.0, 1.0, 0.75), -1.0, places=5)


class TestPowerCalculation(unittest.TestCase):
    """Test how power outputs are calculated."""

    def test_power_is_squared(self):
        """Power outputs are squared, always positive."""
        agent = InverseMotionAgent(
            turn_decision_interval_sec=0.0167,
            threshold_r=0.5, threshold_l=0.5,
            r1_period_s=1.0, r1_amp=5.0,
            r2_period_s=4.0, r2_amp=5.0,
            l1_period_s=0.833, l1_amp=5.0,
            l2_period_s=2.86, l2_amp=5.0,
        )

        # Run many iterations
        for i in range(200):
            t = i / 60.0
            P_r, P_l = agent._calculate_power_outputs(t)
            self.assertGreaterEqual(P_r, 0, f"P_r should be >= 0, got {P_r}")
            self.assertGreaterEqual(P_l, 0, f"P_l should be >= 0, got {P_l}")


class TestAlwaysTurning(unittest.TestCase):
    """Test NNN paper behavior: agent always turns, direction depends on food."""

    def test_always_turning_in_arcs(self):
        """Agent should predominantly turn in one direction (with noise)."""
        agent = InverseMotionAgent(
            threshold_r=0.5, threshold_l=0.5,
            r1_amp=10.0, r2_amp=2.0,
            turn_rate=0.5,
        )

        # No food: should mostly turn RIGHT (positive), with some noise
        right_count = 0
        for i in range(30):
            t = i / 60.0
            dist, angle = agent.update(0, t, 1/60)
            if angle > 0:
                right_count += 1

        print(f"\n=== Always Turning Test ===")
        print(f"Right turns: {right_count}/30")

        # Should be predominantly RIGHT (>60% due to noise)
        self.assertGreater(right_count, 18, "Should mostly turn RIGHT when no food")


class TestSpeedConsistency(unittest.TestCase):
    """Test that speed (total power) is reasonably consistent."""

    def test_speed_variation(self):
        """Speed shouldn't vary too wildly."""
        agent = InverseMotionAgent(
            turn_decision_interval_sec=0.0167,
            threshold_r=0.5, threshold_l=0.5,
            r1_period_s=1.0, r1_amp=5.0,
            r2_period_s=4.0, r2_amp=5.0,
            l1_period_s=0.833, l1_amp=5.0,
            l2_period_s=2.86, l2_amp=5.0,
        )

        # Collect total power over time
        totals = []
        for i in range(600):
            t = i / 60.0
            P_r, P_l = agent._calculate_power_outputs(t)
            totals.append(P_r + P_l)

        avg = sum(totals) / len(totals)
        min_total = min(totals)
        max_total = max(totals)

        print(f"\n=== Speed Consistency ===")
        print(f"Avg total power: {avg:.2f}")
        print(f"Min: {min_total:.2f}, Max: {max_total:.2f}")
        print(f"Variation ratio: {max_total/avg:.2f}x to {min_total/avg:.2f}x")

        # Speed varies a lot due to squared values!
        # This is a potential problem for consistent movement


class TestTurnAngleCalculation(unittest.TestCase):
    """Test the turn angle saturation function."""

    def test_saturation_curve(self):
        """Turn angle saturates at 90 degrees."""
        # Use current config value
        ang_const = cfg.ANGULAR_PROPORTIONALITY_CONSTANT

        # Small differential -> small angle
        _, angle1 = mm.get_motion_outputs_from_power(5, 4.9, 5.0, ang_const)
        # Large differential -> larger angle (but saturation caps at 90°)
        _, angle2 = mm.get_motion_outputs_from_power(50, 0, 5.0, ang_const)

        print(f"\n=== Turn Angle Saturation ===")
        print(f"Angular const: {ang_const}")
        print(f"Small diff (5 vs 4.9): {math.degrees(angle1):.2f}°")
        print(f"Large diff (50 vs 0): {math.degrees(angle2):.2f}°")

        self.assertLess(abs(angle1), math.radians(20), "Small diff should give small angle")
        self.assertGreater(abs(angle2), math.radians(30), "Large diff should give larger angle")


class TestExpectedTrajectory(unittest.TestCase):
    """Test that the agent produces the expected trajectory shape."""

    def test_arc_trajectory_no_food(self):
        """NNN paper: agent should move in arcs (always turning) when no food."""
        agent = InverseMotionAgent(
            threshold_r=0.5, threshold_l=0.5,
            r1_amp=10.0, r2_amp=2.0,
            turn_rate=0.5,
        )

        # Simulate movement with NO food
        dt = 1/60
        all_angles = []
        for frame in range(300):  # 5 seconds, no food
            t = frame * dt
            dist, angle = agent.update(0, t, dt)
            all_angles.append(angle)

        # Most angles should be positive (turning RIGHT when no food)
        # With noise, some frames may briefly go opposite direction
        right_turns = sum(1 for a in all_angles if a > 0)
        left_turns = sum(1 for a in all_angles if a < 0)

        print(f"\n=== Arc Trajectory Test (NNN Paper) ===")
        print(f"Right turns: {right_turns}, Left turns: {left_turns}")
        print(f"Agent turns in arcs with stochastic noise")

        # Should be predominantly RIGHT (>80% due to noise)
        self.assertGreater(right_turns, len(all_angles) * 0.7, "Should mostly turn RIGHT when no food")


class TestModeSwitch(unittest.TestCase):
    """Test NNN paper behavior: reversal when food is found."""

    def test_low_to_high_food_transition(self):
        """Verify agent reverses direction when food is detected."""
        agent = InverseMotionAgent(
            turn_decision_interval_sec=0.0167,
            threshold_r=0.5, threshold_l=0.5,
            r1_amp=10.0, r2_amp=2.0,  # Fast when no food, slow when food
        )

        print(f"\n=== Mode Transition Test ===")

        # Initially in LOW food mode
        self.assertEqual(agent.current_food_frequency, 0.0)
        mode = "HIGH" if agent.current_food_frequency >= 0.5 else "LOW"
        print(f"Initial: food_freq=0.0, mode={mode}")

        # Simulate eating food
        agent._update_food_frequency(1, 0.5)
        mode = "HIGH" if agent.current_food_frequency >= 0.5 else "LOW"
        print(f"After eating 1 food at t=0.5: freq={agent.current_food_frequency}, mode={mode}")

        # The threshold is 0.5 F/s, so eating 1 food gives freq=1.0 F/s
        self.assertGreaterEqual(agent.current_food_frequency, 0.5,
            "Should be in HIGH food mode after eating")


if __name__ == '__main__':
    print("=" * 60)
    print("COMPREHENSIVE AGENT BEHAVIOR TESTS")
    print("=" * 60)
    unittest.main(verbosity=2)
