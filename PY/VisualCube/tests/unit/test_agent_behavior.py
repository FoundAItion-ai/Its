"""Tests for agent behavior — InverterAgent and CompositeMotionAgent."""
import pytest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from agent_module import (
    InverterAgent, CompositeMotionAgent, StochasticMotionAgent,
    CorrelatedRandomWalkAgent, LevyWalkAgent, LogSpiralAgent,
    AreaRestrictedSearchAgent, AGENT_CLASSES, _power_from_speed_and_turn,
)
from inverter import NNN_SINGLE_PRESET, NNN_COMPOSITE_PRESET


# ============================================================================
# InverterAgent — analytical rates
# ============================================================================

class TestInverterAgentRates:
    """Verify InverterAgent uses analytical rates from inverter state."""

    def _make_agent(self, **overrides):
        params = dict(NNN_SINGLE_PRESET)
        params.update(overrides)
        return InverterAgent(**params)

    def test_inactive_before_first_window(self):
        """Before C1 window completes, rates are zero → equal power."""
        agent = self._make_agent()
        dt = 0.01
        # Run for less than C1=0.15s
        for _ in range(10):  # 0.1s
            agent._calculate_power_outputs(dt, is_potential_move=False)
        L, R = agent.inverter.get_rates()
        assert L == 0.0 and R == 0.0

    def test_low_mode_rates_after_activation(self):
        """After C1 window with no food, rates are ~1/C1 and ~1/C3 (with jitter)."""
        agent = self._make_agent()
        dt = 0.01
        # Run past C1=0.15s
        for _ in range(40):  # 0.4s
            agent._calculate_power_outputs(dt, is_potential_move=False)
        L, R = agent.inverter.get_rates()
        # Rates vary due to spike timing jitter (CV=0.1), allow ±50% tolerance
        expected_L = 1.0 / NNN_SINGLE_PRESET['C1']
        expected_R = 1.0 / NNN_SINGLE_PRESET['C3']
        assert expected_L * 0.5 < L < expected_L * 1.5
        assert expected_R * 0.5 < R < expected_R * 1.5

    def test_high_mode_rates_with_food(self):
        """With food input during window, rates switch to ~1/C2 and ~1/C4 (with jitter)."""
        agent = self._make_agent()
        dt = 0.01
        # Feed food every frame for 0.4s (past C1 window)
        for _ in range(40):
            agent._calculate_power_outputs(dt, is_potential_move=False, food_eaten_count=1)
        L, R = agent.inverter.get_rates()
        expected_L = 1.0 / NNN_SINGLE_PRESET['C2']
        expected_R = 1.0 / NNN_SINGLE_PRESET['C4']
        assert expected_L * 0.5 < L < expected_L * 1.5
        assert expected_R * 0.5 < R < expected_R * 1.5

    def test_power_differential_in_low_mode(self):
        """LOW mode should produce P_r != P_l (agent turns)."""
        agent = self._make_agent()
        dt = 0.01
        for _ in range(40):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        P_r, P_l = agent._calculate_power_outputs(dt, is_potential_move=True)
        assert P_r != P_l, "Should have turn differential in LOW mode"

    def test_mode_switch_reverses_turn_direction(self):
        """Switching from HIGH to LOW should reverse which side has more power."""
        agent = self._make_agent(C1=0.3, C2=0.5, C3=0.15, C4=1.0)
        dt = 0.01
        # Activate in HIGH state (no food = void)
        for _ in range(40):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        P_r_high, P_l_high = agent._calculate_power_outputs(dt, is_potential_move=True)
        high_bias = P_r_high - P_l_high  # R=1/0.15=6.67 > L=1/0.3=3.33 → positive

        # New agent, activate in LOW state (food = suppressed)
        agent2 = self._make_agent(C1=0.3, C2=0.5, C3=0.15, C4=1.0)
        for _ in range(40):
            agent2._calculate_power_outputs(dt, is_potential_move=False, food_eaten_count=1)
        P_r_low, P_l_low = agent2._calculate_power_outputs(dt, is_potential_move=True)
        low_bias = P_r_low - P_l_low  # R=1/1.0=1.0 < L=1/0.5=2.0 → negative

        assert high_bias * low_bias < 0, \
            f"Turn direction should reverse: HIGH bias={high_bias:.3f}, LOW bias={low_bias:.3f}"

    def test_low_mode_faster_than_high_mode(self):
        """LOW mode (void) should produce more total power than HIGH mode (food)."""
        agent_low = self._make_agent()
        agent_high = self._make_agent()
        dt = 0.01
        for _ in range(40):
            agent_low._calculate_power_outputs(dt, is_potential_move=False)
            agent_high._calculate_power_outputs(dt, is_potential_move=False, food_eaten_count=1)
        P_r_low, P_l_low = agent_low._calculate_power_outputs(dt, is_potential_move=True)
        P_r_high, P_l_high = agent_high._calculate_power_outputs(dt, is_potential_move=True)
        assert (P_r_low + P_l_low) > (P_r_high + P_l_high), \
            "LOW mode should be faster (more total power) than HIGH mode"

    def test_power_is_rate_directly(self):
        """Power output should equal inverter rate — no gain, baseline, or floor."""
        agent = self._make_agent()
        dt = 0.01
        for _ in range(40):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        P_r, P_l = agent._calculate_power_outputs(dt, is_potential_move=True)
        L_rate, R_rate = agent.inverter.get_rates()
        assert abs(P_r - R_rate) < 1e-9
        assert abs(P_l - L_rate) < 1e-9

    def test_inactive_power_is_zero(self):
        """Before first C1 window, power should be exactly zero."""
        agent = self._make_agent()
        dt = 0.01
        for _ in range(10):  # 0.1s < C1=0.15s
            P_r, P_l = agent._calculate_power_outputs(dt, is_potential_move=False)
        P_r, P_l = agent._calculate_power_outputs(dt, is_potential_move=True)
        assert P_r == 0.0 and P_l == 0.0

    def test_potential_move_does_not_update_inverter(self):
        """is_potential_move=True should not advance inverter state."""
        agent = self._make_agent()
        dt = 0.01
        for _ in range(40):
            agent._calculate_power_outputs(dt, is_potential_move=True)
        assert not agent.inverter.is_active

    def test_default_uses_single_preset(self):
        """InverterAgent with no args should use NNN_SINGLE_PRESET values."""
        agent = InverterAgent()
        assert agent.inverter.C1 == NNN_SINGLE_PRESET['C1']
        assert agent.inverter.C2 == NNN_SINGLE_PRESET['C2']
        assert agent.inverter.C3 == NNN_SINGLE_PRESET['C3']
        assert agent.inverter.C4 == NNN_SINGLE_PRESET['C4']

    def test_num_inverters(self):
        agent = InverterAgent()
        assert agent.num_inverters == 1

    def test_food_freq_window_is_c1(self):
        """Food frequency window should be tied to inverter C1."""
        agent = self._make_agent(C1=0.5)
        assert agent._food_freq_window == 0.5


# ============================================================================
# CompositeMotionAgent — crossed wiring and multi-inverter rates
# ============================================================================

class TestCompositeAgentRates:
    """Verify CompositeMotionAgent sums rates with correct wiring."""

    def test_default_uses_composite_preset(self):
        """No args → uses NNN_COMPOSITE_PRESET."""
        agent = CompositeMotionAgent()
        assert agent.num_inverters == len(NNN_COMPOSITE_PRESET)
        assert agent.configs[0].crossed == False  # f1 normal
        for i in range(1, len(NNN_COMPOSITE_PRESET)):
            assert agent.configs[i].crossed == True

    def test_custom_inverters(self):
        """Can create composite with custom inverter list."""
        inverters = [
            {'C1': 1.0, 'C2': 2.0, 'C3': 3.0, 'C4': 4.0, 'crossed': False, 'name': 'a'},
            {'C1': 5.0, 'C2': 6.0, 'C3': 7.0, 'C4': 8.0, 'crossed': True, 'name': 'b'},
        ]
        agent = CompositeMotionAgent(inverters=inverters)
        assert agent.num_inverters == 2
        assert agent.inverters[0].C1 == 1.0
        assert agent.configs[1].crossed == True

    def test_staggered_activation(self):
        """Inverters with different C1 activate at different times."""
        inverters = [
            {'C1': 0.3, 'C2': 0.5, 'C3': 0.5, 'C4': 0.3, 'crossed': False, 'name': 'fast'},
            {'C1': 1.0, 'C2': 1.5, 'C3': 1.5, 'C4': 1.0, 'crossed': True, 'name': 'slow'},
        ]
        agent = CompositeMotionAgent(inverters=inverters)
        dt = 0.01
        # Run 0.5s — fast (C1=0.3) should be active, slow (C1=1.0) should not
        for _ in range(50):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        assert agent.inverters[0].is_active
        assert not agent.inverters[1].is_active

        # Run another 0.6s (total 1.1s) — both active
        for _ in range(60):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        assert agent.inverters[0].is_active
        assert agent.inverters[1].is_active

    def test_crossed_wiring_swaps_rates(self):
        """Crossed inverter's L rate goes to R motor and vice versa."""
        inverters = [
            {'C1': 0.3, 'C2': 0.5, 'C3': 0.8, 'C4': 0.5, 'crossed': True, 'name': 'x'},
        ]
        agent = CompositeMotionAgent(inverters=inverters)
        dt = 0.01
        for _ in range(40):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        # Inverter rates: L=1/0.3=3.33, R=1/0.8=1.25
        # After crossing: L_motor=1.25, R_motor=3.33
        P_r, P_l = agent._calculate_power_outputs(dt, is_potential_move=True)
        assert P_r > P_l, "Crossed wiring should swap: R_motor gets fast L rate"

    def test_normal_wiring_preserves_rates(self):
        """Normal (non-crossed) inverter passes rates straight through."""
        inverters = [
            {'C1': 0.3, 'C2': 0.5, 'C3': 0.8, 'C4': 0.5, 'crossed': False, 'name': 'n'},
        ]
        agent = CompositeMotionAgent(inverters=inverters)
        dt = 0.01
        for _ in range(40):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        # Inverter rates: L=1/0.3=3.33, R=1/0.8=1.25
        # No crossing: L_motor=3.33, R_motor=1.25
        P_r, P_l = agent._calculate_power_outputs(dt, is_potential_move=True)
        assert P_l > P_r, "Normal wiring: L_motor gets fast L rate"

    def test_composite_power_above_zero_when_active(self):
        """Total power should be above zero when inverters are active."""
        agent = CompositeMotionAgent()
        dt = 0.01
        # Activate all inverters (run past longest C1=6.0s)
        for _ in range(650):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        P_r, P_l = agent._calculate_power_outputs(dt, is_potential_move=True)
        assert (P_r + P_l) > 0, "Active inverters should produce power"

    def test_food_freq_window_is_first_c1(self):
        """Food frequency window should be tied to first inverter's C1."""
        inverters = [
            {'C1': 0.5, 'C2': 1.0, 'C3': 0.5, 'C4': 1.0, 'crossed': False, 'name': 'a'},
            {'C1': 2.0, 'C2': 3.0, 'C3': 2.0, 'C4': 3.0, 'crossed': True, 'name': 'b'},
        ]
        agent = CompositeMotionAgent(inverters=inverters)
        assert agent._food_freq_window == 0.5


# ============================================================================
# Food frequency tracking (_BaseAgent)
# ============================================================================

class TestFoodFrequency:
    """Test food frequency tracking in _BaseAgent via InverterAgent."""

    def test_initial_frequency_zero(self):
        agent = InverterAgent()
        assert agent.current_food_frequency == 0.0

    def test_frequency_increases_on_food(self):
        agent = InverterAgent()
        agent._update_food_frequency(5, current_sim_time=1.0)
        assert agent.current_food_frequency == 5.0 / agent._food_freq_window

    def test_frequency_decays_after_window(self):
        agent = InverterAgent()
        agent._update_food_frequency(3, current_sim_time=1.0)
        assert agent.current_food_frequency > 0
        # Move past the window
        agent._update_food_frequency(0, current_sim_time=1.0 + agent._food_freq_window + 0.1)
        assert agent.current_food_frequency == 0.0

    def test_frequency_accumulates_within_window(self):
        agent = InverterAgent()
        agent._update_food_frequency(2, current_sim_time=1.0)
        agent._update_food_frequency(3, current_sim_time=1.0 + agent._food_freq_window * 0.5)
        assert agent.current_food_frequency == 5.0 / agent._food_freq_window


# ============================================================================
# Agent.update() integration
# ============================================================================

class TestAgentUpdate:
    """Test the full update() pipeline."""

    def test_update_returns_distance_and_angle(self):
        agent = InverterAgent()
        dist, angle = agent.update(0, current_sim_time=0.0, dt=1/60)
        assert isinstance(dist, float)
        assert isinstance(angle, float)

    def test_distance_positive_when_active(self):
        """After activation, agent should move forward."""
        agent = InverterAgent()
        # Run past C1 window
        for i in range(40):
            agent.update(0, current_sim_time=i * 0.01, dt=0.01)
        dist, _ = agent.update(0, current_sim_time=0.41, dt=0.01)
        assert dist > 0

    def test_angle_nonzero_on_turn_frame(self):
        """On turn frames, angle should be nonzero after activation."""
        agent = InverterAgent(turn_decision_interval_sec=0.01)
        # Activate
        for i in range(40):
            agent.update(0, current_sim_time=i * 0.01, dt=0.01)
        # Next frame should be a turn frame
        _, angle = agent.update(0, current_sim_time=0.41, dt=0.01)
        assert angle != 0.0

    def test_potential_move_does_not_count_decision(self):
        """is_potential_move should not increment decision_count."""
        agent = InverterAgent()
        agent.update(0, current_sim_time=0.0, dt=0.01, is_potential_move=True)
        assert agent.decision_count == 0


# ============================================================================
# StochasticMotionAgent
# ============================================================================

class TestStochasticAgent:
    """Basic tests for StochasticMotionAgent."""

    def test_power_always_positive(self):
        agent = StochasticMotionAgent()
        P_r, P_l = agent._calculate_power_outputs(1/60, is_potential_move=False)
        assert P_r >= 0 and P_l >= 0

    def test_regenerates_power_after_interval(self):
        agent = StochasticMotionAgent(update_interval_sec=0.1)
        initial_r, initial_l = agent.cached_P_r, agent.cached_P_l
        changed = False
        for _ in range(100):
            agent._calculate_power_outputs(0.1, is_potential_move=False)
            if agent.cached_P_r != initial_r or agent.cached_P_l != initial_l:
                changed = True
                break
        assert changed, "Stochastic agent should regenerate power choices"


# ============================================================================
# Visualization signals tracking
# ============================================================================

class TestSignalTracking:
    """Test that last_signals is populated correctly for visualization."""

    def test_inverter_agent_tracks_signals(self):
        agent = InverterAgent(C1=0.1, C3=0.1)  # Fast firing
        dt = 0.01
        all_signals = []
        for _ in range(20):
            agent._calculate_power_outputs(dt, is_potential_move=False)
            all_signals.extend(agent.last_signals)
        assert len(all_signals) > 0, "Should have recorded some signals"
        for idx, side, val in all_signals:
            assert idx == 0
            assert side in ('L', 'R')
            assert val == 1

    def test_composite_agent_tracks_signals_with_index(self):
        inverters = [
            {'C1': 0.1, 'C2': 0.2, 'C3': 0.1, 'C4': 0.2, 'crossed': False, 'name': 'a'},
            {'C1': 0.1, 'C2': 0.2, 'C3': 0.1, 'C4': 0.2, 'crossed': True, 'name': 'b'},
        ]
        agent = CompositeMotionAgent(inverters=inverters)
        dt = 0.01
        all_signals = []
        for _ in range(20):
            agent._calculate_power_outputs(dt, is_potential_move=False)
            all_signals.extend(agent.last_signals)
        indices = {s[0] for s in all_signals}
        assert 0 in indices and 1 in indices, "Should have signals from both inverters"


# ============================================================================
# _power_from_speed_and_turn utility
# ============================================================================

class TestPowerFromSpeedAndTurn:
    """Verify the inverse saturation function."""

    def test_zero_turn_gives_equal_power(self):
        P_r, P_l = _power_from_speed_and_turn(60.0, 0.0)
        assert abs(P_r - P_l) < 1e-9

    def test_power_sum_preserves_speed(self):
        # At small turns, sum is exactly preserved. At large turns,
        # clamping P_l to 0 makes sum >= expected (never less).
        for turn in [0.0, 0.3, -0.5]:
            P_r, P_l = _power_from_speed_and_turn(60.0, turn)
            import config as cfg
            expected_sum = 60.0 / (cfg.AGENT_SPEED_SCALING_FACTOR * cfg.GLOBAL_SPEED_MODIFIER)
            assert abs((P_r + P_l) - expected_sum) < 0.01, \
                f"Sum should preserve speed at turn={turn}"

    def test_positive_turn_gives_more_right_power(self):
        P_r, P_l = _power_from_speed_and_turn(60.0, 0.5)
        assert P_r > P_l

    def test_negative_turn_gives_more_left_power(self):
        P_r, P_l = _power_from_speed_and_turn(60.0, -0.5)
        assert P_l > P_r

    def test_powers_non_negative(self):
        for turn in [0.0, 0.5, -0.5, 1.2, -1.2]:
            P_r, P_l = _power_from_speed_and_turn(60.0, turn)
            assert P_r >= 0, f"P_r negative at turn={turn}"
            assert P_l >= 0, f"P_l negative at turn={turn}"

    def test_zero_speed_gives_zero_power(self):
        P_r, P_l = _power_from_speed_and_turn(0.0, 0.5)
        assert P_r >= 0 and P_l >= 0
        assert (P_r + P_l) < 0.01


# ============================================================================
# CorrelatedRandomWalkAgent
# ============================================================================

class TestCorrelatedRandomWalkAgent:

    def test_registered_in_agent_classes(self):
        assert 'crw' in AGENT_CLASSES
        assert AGENT_CLASSES['crw'] is CorrelatedRandomWalkAgent

    def test_default_construction(self):
        agent = CorrelatedRandomWalkAgent()
        assert agent.D_ROT == 0.0
        assert agent.persistence == 0.7
        assert agent.base_speed == 60.0

    def test_persistence_one_gives_zero_turn_sigma(self):
        agent = CorrelatedRandomWalkAgent(persistence=1.0)
        assert agent._turn_sigma == 0.0

    def test_persistence_zero_gives_max_turn_sigma(self):
        import math
        agent = CorrelatedRandomWalkAgent(persistence=0.0)
        assert abs(agent._turn_sigma - math.pi / 2.0) < 1e-9

    def test_produces_valid_output(self):
        agent = CorrelatedRandomWalkAgent()
        for i in range(120):
            dist, angle = agent.update(0, i / 60.0, 1 / 60.0)
            assert dist >= 0, f"Negative distance at frame {i}"

    def test_nonzero_turns_over_time(self):
        agent = CorrelatedRandomWalkAgent(step_duration_sec=0.1)
        turns = []
        for i in range(600):
            _, angle = agent.update(0, i / 60.0, 1 / 60.0)
            if abs(angle) > 1e-6:
                turns.append(angle)
        assert len(turns) > 5, "Should produce multiple nonzero turns"

    def test_potential_move_does_not_advance_state(self):
        agent = CorrelatedRandomWalkAgent()
        acc_before = agent._time_acc
        agent._calculate_power_outputs(1 / 60, is_potential_move=True)
        assert agent._time_acc == acc_before


# ============================================================================
# LevyWalkAgent
# ============================================================================

class TestLevyWalkAgent:

    def test_registered_in_agent_classes(self):
        assert 'levy' in AGENT_CLASSES
        assert AGENT_CLASSES['levy'] is LevyWalkAgent

    def test_default_construction(self):
        agent = LevyWalkAgent()
        assert agent.D_ROT == 0.0
        assert agent.levy_exponent == 2.0
        assert agent.base_speed == 60.0

    def test_step_durations_vary(self):
        agent = LevyWalkAgent()
        durations = [agent._draw_step_duration() for _ in range(100)]
        assert min(durations) >= agent.min_step_sec
        assert max(durations) <= agent.max_step_sec
        assert len(set(round(d, 4) for d in durations)) > 5, "Should have varied step lengths"

    def test_heavy_tail_many_short_few_long(self):
        agent = LevyWalkAgent(min_step_sec=0.1, max_step_sec=10.0)
        durations = [agent._draw_step_duration() for _ in range(1000)]
        median = sorted(durations)[500]
        assert median < 1.0, "Median step should be short (heavy tail)"

    def test_produces_valid_output(self):
        agent = LevyWalkAgent()
        for i in range(600):
            dist, angle = agent.update(0, i / 60.0, 1 / 60.0)
            assert dist >= 0, f"Negative distance at frame {i}"

    def test_direction_changes_occur(self):
        agent = LevyWalkAgent(min_step_sec=0.1, max_step_sec=0.5)
        turns = []
        for i in range(600):
            _, angle = agent.update(0, i / 60.0, 1 / 60.0)
            if abs(angle) > 0.1:
                turns.append(angle)
        assert len(turns) >= 2, "Should have at least 2 large direction changes in 10s"

    def test_potential_move_does_not_advance_state(self):
        agent = LevyWalkAgent()
        remaining_before = agent._step_remaining
        agent._calculate_power_outputs(1 / 60, is_potential_move=True)
        assert agent._step_remaining == remaining_before


# ============================================================================
# LogSpiralAgent
# ============================================================================

class TestLogSpiralAgent:

    def test_registered_in_agent_classes(self):
        assert 'log_spiral' in AGENT_CLASSES
        assert AGENT_CLASSES['log_spiral'] is LogSpiralAgent

    def test_default_construction(self):
        agent = LogSpiralAgent()
        assert agent.D_ROT == 0.0
        assert agent.spiral_rate_b == 0.06
        assert agent.base_radius_a == 10.0

    def test_theta_increases_monotonically(self):
        agent = LogSpiralAgent()
        prev_theta = agent._theta
        for i in range(60):
            agent.update(0, i / 60.0, 1 / 60.0)
        assert agent._theta > prev_theta

    def test_speed_increases_over_time(self):
        agent = LogSpiralAgent()
        speeds_early = []
        speeds_late = []
        for i in range(600):
            dist, _ = agent.update(0, i / 60.0, 1 / 60.0)
            if 10 <= i < 20:
                speeds_early.append(dist)
            elif 580 <= i < 600:
                speeds_late.append(dist)
        avg_early = sum(speeds_early) / len(speeds_early)
        avg_late = sum(speeds_late) / len(speeds_late)
        assert avg_late > avg_early, "Log spiral speed should increase as radius grows"

    def test_speed_clamped(self):
        agent = LogSpiralAgent(spiral_rate_b=1.0, angular_speed_rad_s=10.0, max_speed=300.0)
        for i in range(6000):
            dist, _ = agent.update(0, i / 60.0, 1 / 60.0)
        # With extreme params, speed would explode without clamping
        # dist = speed * dt, max speed = 300 px/s, dt = 1/60
        assert dist <= 300.0 / 60.0 + 0.1

    def test_produces_valid_output(self):
        agent = LogSpiralAgent()
        for i in range(300):
            dist, angle = agent.update(0, i / 60.0, 1 / 60.0)
            assert dist >= 0, f"Negative distance at frame {i}"

    def test_consistent_turning(self):
        agent = LogSpiralAgent()
        turns = []
        for i in range(600):
            _, angle = agent.update(0, i / 60.0, 1 / 60.0)
            if abs(angle) > 1e-6:
                turns.append(angle)
        assert len(turns) > 100, "Log spiral should turn nearly every frame"
        signs = [1 if t > 0 else -1 for t in turns]
        dominant = abs(sum(signs)) / len(signs)
        assert dominant > 0.9, "Turns should be consistently in one direction"


# ============================================================================
# AreaRestrictedSearchAgent
# ============================================================================

class TestAreaRestrictedSearchAgent:

    def test_registered_in_agent_classes(self):
        assert 'ars' in AGENT_CLASSES
        assert AGENT_CLASSES['ars'] is AreaRestrictedSearchAgent

    def test_default_construction(self):
        agent = AreaRestrictedSearchAgent()
        assert agent.D_ROT == 0.0
        assert agent.explore_speed == 60.0
        assert agent.exploit_speed == 30.0

    def test_explore_mode_without_food(self):
        agent = AreaRestrictedSearchAgent()
        for i in range(120):
            dist, _ = agent.update(0, i / 60.0, 1 / 60.0)
        assert dist > 0

    def test_exploit_mode_with_food(self):
        agent = AreaRestrictedSearchAgent(turn_decision_interval_sec=0.1)
        # Feed food for several seconds to ensure exploit mode
        for i in range(300):
            agent.update(1, i / 60.0, 1 / 60.0)
        assert agent.current_food_frequency > 0, "Should detect food"

    def test_exploit_produces_tighter_turns(self):
        # Use high exploit_turn_rate and low explore_turn_sigma for clear separation
        agent_explore = AreaRestrictedSearchAgent(
            turn_decision_interval_sec=0.1, explore_turn_sigma=0.1, exploit_turn_rate=5.0)
        agent_exploit = AreaRestrictedSearchAgent(
            turn_decision_interval_sec=0.1, explore_turn_sigma=0.1, exploit_turn_rate=5.0)

        explore_turns = []
        exploit_turns = []
        for i in range(600):
            t = i / 60.0
            _, angle_e = agent_explore.update(0, t, 1 / 60.0)
            _, angle_x = agent_exploit.update(1, t, 1 / 60.0)
            if abs(angle_e) > 1e-6:
                explore_turns.append(abs(angle_e))
            if abs(angle_x) > 1e-6:
                exploit_turns.append(abs(angle_x))

        if explore_turns and exploit_turns:
            avg_explore = sum(explore_turns) / len(explore_turns)
            avg_exploit = sum(exploit_turns) / len(exploit_turns)
            assert avg_exploit > avg_explore, \
                f"Exploit turns ({avg_exploit:.4f}) should be tighter than explore ({avg_explore:.4f})"

    def test_exploit_slower_than_explore(self):
        agent = AreaRestrictedSearchAgent()
        # Explore mode: no food
        explore_dists = []
        for i in range(120):
            dist, _ = agent.update(0, i / 60.0, 1 / 60.0)
            if i > 60:
                explore_dists.append(dist)

        # New agent in exploit mode: constant food
        agent2 = AreaRestrictedSearchAgent()
        exploit_dists = []
        for i in range(120):
            dist, _ = agent2.update(1, i / 60.0, 1 / 60.0)
            if i > 60:
                exploit_dists.append(dist)

        avg_explore = sum(explore_dists) / len(explore_dists)
        avg_exploit = sum(exploit_dists) / len(exploit_dists)
        assert avg_exploit < avg_explore, \
            f"Exploit speed ({avg_exploit:.4f}) should be slower than explore ({avg_explore:.4f})"

    def test_potential_move_does_not_advance_state(self):
        agent = AreaRestrictedSearchAgent()
        acc_before = agent._time_acc
        agent._calculate_power_outputs(1 / 60, is_potential_move=True)
        assert agent._time_acc == acc_before
