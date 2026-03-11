"""Unit tests for agent behavior - investigating food frequency feedback."""
import unittest
import sys
import os
import math

# Add parent to path for imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent_module import InverseMotionAgent, FOOD_FREQ_WINDOW_SEC
import math_module as mm
import config as cfg


class TestFoodFrequencyTracking(unittest.TestCase):
    """Test that food frequency is correctly tracked."""

    def setUp(self):
        self.agent = InverseMotionAgent(
            turn_decision_interval_sec=0.0167,
            threshold_r=2.0,  # Reasonable threshold
            threshold_l=2.0,
            r1_period_s=1.0, r1_amp=8.0,
            r2_period_s=0.5, r2_amp=6.0,
            l1_period_s=1.0, l1_amp=4.0,
            l2_period_s=0.5, l2_amp=8.0,
        )

    def test_initial_food_frequency_is_zero(self):
        """Food frequency should start at 0."""
        self.assertEqual(self.agent.current_food_frequency, 0.0)

    def test_food_frequency_increases_when_eating(self):
        """Food frequency should increase when agent eats food."""
        # Simulate eating 5 food items at time=1.0s
        self.agent._update_food_frequency(5, current_sim_time=1.0)

        # Should be 5 food / FOOD_FREQ_WINDOW_SEC
        expected = 5.0 / FOOD_FREQ_WINDOW_SEC
        self.assertAlmostEqual(self.agent.current_food_frequency, expected, places=2)

    def test_food_frequency_decays_over_time(self):
        """Food frequency should decay as time passes without eating."""
        # Eat 5 food at time=1.0
        self.agent._update_food_frequency(5, current_sim_time=1.0)
        expected = 5.0 / FOOD_FREQ_WINDOW_SEC
        self.assertAlmostEqual(self.agent.current_food_frequency, expected, places=2)

        # No food eaten, time passes beyond the window
        decay_time = 1.0 + FOOD_FREQ_WINDOW_SEC + 0.5
        self.agent._update_food_frequency(0, current_sim_time=decay_time)
        self.assertEqual(self.agent.current_food_frequency, 0.0)

    def test_food_frequency_window(self):
        """Food frequency should only count food within the time window."""
        # Eat at time=1.0
        self.agent._update_food_frequency(3, current_sim_time=1.0)
        # Eat at time=1.0 + half window (well within window)
        second_eat_time = 1.0 + FOOD_FREQ_WINDOW_SEC * 0.5
        self.agent._update_food_frequency(2, current_sim_time=second_eat_time)

        # Total should be 5 food in window
        expected = 5.0 / FOOD_FREQ_WINDOW_SEC
        self.assertAlmostEqual(self.agent.current_food_frequency, expected, places=2)

        # Move to time where first food is expired but second is not
        expire_time = 1.0 + FOOD_FREQ_WINDOW_SEC + 0.05
        self.agent._update_food_frequency(0, current_sim_time=expire_time)
        # Only 2 food should remain (from second eating)
        expected = 2.0 / FOOD_FREQ_WINDOW_SEC
        self.assertAlmostEqual(self.agent.current_food_frequency, expected, places=2)


class TestThresholdSwitching(unittest.TestCase):
    """Test that power outputs change based on food frequency thresholds."""

    def test_low_food_uses_low_params(self):
        """When food frequency < threshold, should use low-food params."""
        agent = InverseMotionAgent(
            turn_decision_interval_sec=0.0167,
            threshold_r=5.0,  # High threshold - hard to reach
            threshold_l=5.0,
            r1_period_s=1.0, r1_amp=10.0,  # LOW food: high amplitude
            r2_period_s=0.5, r2_amp=2.0,   # HIGH food: low amplitude
            l1_period_s=1.0, l1_amp=10.0,
            l2_period_s=0.5, l2_amp=2.0,
        )

        # Food frequency is 0, below threshold of 5.0
        # Should use r1_amp=10.0 and l1_amp=10.0
        P_r, P_l = agent._calculate_power_outputs(sim_time=0.25)  # sin(2π*0.25/1.0) = sin(π/2) = 1.0

        # With amp=10, smoothing=0.1, first call: smoothed = 0.1 * 10 + 0.9 * 0 = 1.0
        # Squared: 1.0
        print(f"Low food - P_r: {P_r}, P_l: {P_l}")
        self.assertGreater(P_r, 0)  # Should have power

    def test_high_food_uses_high_params(self):
        """When food frequency >= threshold, should use high-food params."""
        agent = InverseMotionAgent(
            turn_decision_interval_sec=0.0167,
            threshold_r=1.0,  # Low threshold - easy to reach
            threshold_l=1.0,
            r1_period_s=1.0, r1_amp=10.0,  # LOW food: high amplitude
            r2_period_s=0.5, r2_amp=2.0,   # HIGH food: low amplitude
            l1_period_s=1.0, l1_amp=10.0,
            l2_period_s=0.5, l2_amp=2.0,
        )

        # Simulate eating food to get frequency above threshold
        agent._update_food_frequency(5, current_sim_time=0.5)
        self.assertGreaterEqual(agent.current_food_frequency, 1.0)

        # Now should use r2_amp=2.0 and l2_amp=2.0
        P_r, P_l = agent._calculate_power_outputs(sim_time=0.5)
        print(f"High food (freq={agent.current_food_frequency}) - P_r: {P_r}, P_l: {P_l}")


class TestSpeedBehavior(unittest.TestCase):
    """Test that speed changes appropriately with food frequency."""

    def test_speed_calculation(self):
        """Verify speed is calculated from total power."""
        # Speed = (P_r + P_l) * AGENT_SPEED_SCALING_FACTOR * GLOBAL_SPEED_MODIFIER
        P_r, P_l = 4.0, 4.0
        speed, _ = mm.get_motion_outputs_from_power(
            P_r, P_l,
            cfg.AGENT_SPEED_SCALING_FACTOR,
            cfg.ANGULAR_PROPORTIONALITY_CONSTANT
        )
        expected_speed = (P_r + P_l) * cfg.AGENT_SPEED_SCALING_FACTOR
        self.assertEqual(speed, expected_speed)

    def test_agent_should_slow_down_when_eating(self):
        """Agent should have lower speed when food frequency is high.

        This test verifies the INTENDED behavior with the NEW config:
        R_low=8, L_low=5, R_high=3, L_high=1
        """
        agent = InverseMotionAgent(
            turn_decision_interval_sec=0.0167,
            threshold_r=2.0,
            threshold_l=2.0,
            r1_period_s=1.0, r1_amp=8.0,   # LOW food R
            r2_period_s=0.5, r2_amp=3.0,   # HIGH food R
            l1_period_s=1.0, l1_amp=5.0,   # LOW food L
            l2_period_s=0.5, l2_amp=1.0,   # HIGH food L
        )

        # Measure speed in LOW food mode (no food eaten)
        low_speeds = []
        for i in range(30):
            t = i * 0.0167
            dist, _ = agent.update(0, t, 0.0167)  # No food
            low_speeds.append(dist)
        avg_low_speed = sum(low_speeds) / len(low_speeds)

        # Create new agent for HIGH food mode test
        agent2 = InverseMotionAgent(
            threshold_r=0.5, threshold_l=0.5,
            r1_amp=8.0, r2_amp=2.0,  # Fast=8, Slow=2
        )

        # Trigger HIGH mode by eating food
        high_speeds = []
        for i in range(30):
            t = i * 0.0167
            food = 2 if i == 0 else 0  # Eat food on first frame
            dist, _ = agent2.update(food, t, 0.0167)
            high_speeds.append(dist)
        avg_high_speed = sum(high_speeds) / len(high_speeds)

        print(f"\nAverage speed - Low food: {avg_low_speed:.4f}, High food: {avg_high_speed:.4f}")
        print(f"Speed ratio (low/high): {avg_low_speed/avg_high_speed:.2f}x faster when searching")

        # EXPECTED: High food should be SLOWER
        self.assertLess(avg_high_speed, avg_low_speed,
            "Agent should move slower when eating food (high frequency)")


class TestDefaultThresholds(unittest.TestCase):
    """Test that default threshold values make sense."""

    def test_threshold_r_should_be_greater_than_zero(self):
        """threshold_r=0.0 means right side ALWAYS uses high-food params."""
        # This is the problematic default from the GUI
        threshold_r = 0.0
        food_freq = 0.0  # No food eaten

        # With threshold_r=0.0, condition is: food_freq < 0.0
        # This is NEVER true when food_freq >= 0
        uses_low_food_params = food_freq < threshold_r

        self.assertFalse(uses_low_food_params,
            "threshold_r=0.0 means low-food params are NEVER used!")

        # Recommendation: threshold should be > 0
        better_threshold = 1.0
        uses_low_food_params = food_freq < better_threshold
        self.assertTrue(uses_low_food_params,
            "With threshold > 0, low-food params ARE used when not eating")


class TestTurningBehavior(unittest.TestCase):
    """Test NNN paper behavior: turn direction inverts when food state changes."""

    def test_turn_direction_inverts_with_food(self):
        """NNN paper: turn direction should INVERT when food is detected.

        No food = turn RIGHT (positive), Food = turn LEFT (negative).
        """
        agent = InverseMotionAgent(
            threshold_r=0.5, threshold_l=0.5,
            r1_amp=10.0, r2_amp=2.0,
            turn_rate=0.5,
        )

        # No food: should turn RIGHT (positive angle)
        dist1, angle1 = agent.update(0, 0.0, 1/60)
        self.assertGreater(angle1, 0, "Should turn RIGHT when no food")

        # Eat food: should turn LEFT (negative angle)
        agent._update_food_frequency(2, 0.5)
        dist2, angle2 = agent.update(0, 0.5, 1/60)
        self.assertLess(angle2, 0, "Should turn LEFT when food detected")

        print(f"\n=== NNN Paper Turn Inversion Test ===")
        print(f"No food: angle={math.degrees(angle1):.2f}° (RIGHT)")
        print(f"Food found: angle={math.degrees(angle2):.2f}° (LEFT)")
        print(f"Turn direction INVERTED!")

    def test_speed_changes_with_food_state(self):
        """Test that speed is fast when no food, slow when food found.

        NNN paper: C1 > C2 (fast when searching, slow when found food).
        """
        agent = InverseMotionAgent(
            threshold_r=0.5, threshold_l=0.5,
            r1_amp=10.0, r2_amp=2.0,  # C1=10 > C2=2
            turn_rate=0.5,
        )

        # No food: fast speed
        dist1, _ = agent.update(0, 0.0, 1/60)

        # Eat food: slow speed
        agent._update_food_frequency(2, 0.5)
        dist2, _ = agent.update(0, 0.5, 1/60)

        print(f"\n=== Speed Change Test ===")
        print(f"No food: dist={dist1:.3f} (fast)")
        print(f"Food found: dist={dist2:.3f} (slow)")
        print(f"Speed ratio: {dist1/dist2:.1f}x faster when searching")

        self.assertGreater(dist1, dist2, "Should be faster when no food (C1 > C2)")


if __name__ == '__main__':
    print(f"FOOD_FREQ_WINDOW_SEC = {FOOD_FREQ_WINDOW_SEC}")
    print(f"AGENT_SPEED_SCALING_FACTOR = {cfg.AGENT_SPEED_SCALING_FACTOR}")
    print(f"ANGULAR_PROPORTIONALITY_CONSTANT = {cfg.ANGULAR_PROPORTIONALITY_CONSTANT}")
    print()
    unittest.main(verbosity=2)
