"""Integration tests — full simulation loop, presets, and round-trip config."""
import pytest
import sys
import math
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from agent_module import InverterAgent, CompositeMotionAgent, AGENT_CLASSES
from inverter import (
    Inverter, NNN_SINGLE_PRESET, NNN_COMPOSITE_PRESET, combine_outputs, InverterConfig,
)
import math_module as mm
import config as cfg


# ============================================================================
# Preset consistency — single source of truth
# ============================================================================

class TestPresetConsistency:
    """Verify presets are the single source of truth for defaults."""

    def test_inverter_agent_uses_single_preset(self):
        agent = InverterAgent()
        for key in ('C1', 'C2', 'C3', 'C4'):
            assert getattr(agent.inverter, key) == NNN_SINGLE_PRESET[key], \
                f"InverterAgent default {key} should match NNN_SINGLE_PRESET"

    def test_composite_agent_uses_composite_preset(self):
        agent = CompositeMotionAgent()
        assert len(agent.inverters) == len(NNN_COMPOSITE_PRESET)
        for inv, preset in zip(agent.inverters, NNN_COMPOSITE_PRESET):
            assert inv.C1 == preset['C1']
            assert inv.C3 == preset['C3']

    def test_single_preset_has_required_keys(self):
        for key in ('C1', 'C2', 'C3', 'C4'):
            assert key in NNN_SINGLE_PRESET

    def test_composite_preset_has_required_keys(self):
        for p in NNN_COMPOSITE_PRESET:
            for key in ('C1', 'C2', 'C3', 'C4', 'crossed', 'name'):
                assert key in p

    def test_agent_classes_dict(self):
        assert 'stochastic' in AGENT_CLASSES
        assert 'inverter' in AGENT_CLASSES
        assert 'composite' in AGENT_CLASSES


# ============================================================================
# NNN model behavior — mode switching and inversion
# ============================================================================

class TestNNNModeSwitching:
    """Test that the NNN model correctly switches between LOW and HIGH modes."""

    def test_no_food_activates_low_mode(self):
        inv = Inverter(**NNN_SINGLE_PRESET)
        for _ in range(40):
            inv.update(0.01, 0.0)
        assert inv.is_active
        assert not inv.is_high_mode

    def test_food_activates_high_mode(self):
        inv = Inverter(**NNN_SINGLE_PRESET)
        for _ in range(40):
            inv.update(0.01, 1.0)
        assert inv.is_active
        assert inv.is_high_mode

    def test_mode_switch_changes_rates(self):
        """LOW and HIGH modes produce different rate pairs."""
        inv_low = Inverter(**NNN_SINGLE_PRESET)
        inv_high = Inverter(**NNN_SINGLE_PRESET)
        for _ in range(40):
            inv_low.update(0.01, 0.0)
            inv_high.update(0.01, 1.0)
        low_rates = inv_low.get_rates()
        high_rates = inv_high.get_rates()
        assert low_rates != high_rates

    def test_symmetric_preset_gives_opposite_bias(self):
        """With C1=C4 and C2=C3, LOW and HIGH biases should be opposite."""
        p = NNN_SINGLE_PRESET
        # LOW: L=1/C1, R=1/C3 → diff = 1/C3 - 1/C1
        low_diff = 1.0/p['C3'] - 1.0/p['C1']
        # HIGH: L=1/C2, R=1/C4 → diff = 1/C4 - 1/C2
        high_diff = 1.0/p['C4'] - 1.0/p['C2']
        assert low_diff * high_diff < 0, "LOW and HIGH should bias opposite directions"


# ============================================================================
# Spiral mechanism — staggered activation in composite
# ============================================================================

class TestSpiralMechanism:
    """Test that composite staggered activation progressively changes turn."""

    def _get_composite_rate_diff(self, agent):
        """Get the current L-R rate differential from all inverters."""
        L_rate, R_rate = 0.0, 0.0
        for inverter, config in zip(agent.inverters, agent.configs):
            inv_L, inv_R = inverter.get_rates()
            if config.crossed:
                L_rate += inv_R
                R_rate += inv_L
            else:
                L_rate += inv_L
                R_rate += inv_R
        return R_rate - L_rate

    def test_turn_changes_as_inverters_activate(self):
        """As more crossed inverters activate, turn differential should change."""
        agent = CompositeMotionAgent()
        dt = 0.01

        # Phase 1: only f1 (C1=0.3) active
        for _ in range(60):  # 0.6s
            agent._calculate_power_outputs(dt, is_potential_move=False)
        diff_phase1 = self._get_composite_rate_diff(agent)
        active_1 = sum(1 for inv in agent.inverters if inv.is_active)

        # Phase 2: f1 + f2 (C1=3.0) active — run to 3.5s total
        for _ in range(290):  # 0.6 + 2.9 = 3.5s
            agent._calculate_power_outputs(dt, is_potential_move=False)
        diff_phase2 = self._get_composite_rate_diff(agent)
        active_2 = sum(1 for inv in agent.inverters if inv.is_active)

        assert active_2 > active_1, "More inverters should be active in phase 2"
        assert diff_phase1 != diff_phase2, "Turn differential should change as inverters activate"

    def test_crossed_inverters_oppose_f1(self):
        """Crossed inverters should reduce the magnitude of f1's turn bias."""
        agent = CompositeMotionAgent()
        dt = 0.01

        # Phase 1: only f1 active
        for _ in range(60):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        diff_f1_only = abs(self._get_composite_rate_diff(agent))

        # Activate all (past 6.0s)
        for _ in range(600):
            agent._calculate_power_outputs(dt, is_potential_move=False)
        diff_all = abs(self._get_composite_rate_diff(agent))

        assert diff_all < diff_f1_only, \
            "Crossed inverters should reduce turn differential (widen spiral)"


# ============================================================================
# Speed and motion pipeline
# ============================================================================

class TestMotionPipeline:
    """Test the full motion pipeline from power to distance/angle."""

    def test_equal_power_gives_zero_angle(self):
        speed, angle = mm.get_motion_outputs_from_power(
            4.0, 4.0, cfg.AGENT_SPEED_SCALING_FACTOR, cfg.ANGULAR_PROPORTIONALITY_CONSTANT
        )
        assert angle == 0.0
        assert speed > 0

    def test_more_right_power_turns_right(self):
        _, angle = mm.get_motion_outputs_from_power(
            6.0, 2.0, cfg.AGENT_SPEED_SCALING_FACTOR, cfg.ANGULAR_PROPORTIONALITY_CONSTANT
        )
        assert angle > 0, "More right power should turn right (positive angle)"

    def test_more_left_power_turns_left(self):
        _, angle = mm.get_motion_outputs_from_power(
            2.0, 6.0, cfg.AGENT_SPEED_SCALING_FACTOR, cfg.ANGULAR_PROPORTIONALITY_CONSTANT
        )
        assert angle < 0, "More left power should turn left (negative angle)"

    def test_speed_proportional_to_total_power(self):
        speed1, _ = mm.get_motion_outputs_from_power(
            4.0, 4.0, cfg.AGENT_SPEED_SCALING_FACTOR, cfg.ANGULAR_PROPORTIONALITY_CONSTANT
        )
        speed2, _ = mm.get_motion_outputs_from_power(
            2.0, 2.0, cfg.AGENT_SPEED_SCALING_FACTOR, cfg.ANGULAR_PROPORTIONALITY_CONSTANT
        )
        assert speed1 > speed2


# ============================================================================
# Full simulation loop
# ============================================================================

class TestSimulationLoop:
    """End-to-end simulation tests."""

    def test_inverter_agent_5_seconds(self):
        """Run inverter agent for 5s, verify it moves and turns."""
        agent = InverterAgent()
        dt = 1/60
        total_dist = 0.0
        angles = []
        for i in range(300):
            dist, angle = agent.update(0, current_sim_time=i * dt, dt=dt)
            total_dist += dist
            angles.append(angle)
        assert total_dist > 0, "Agent should have moved"
        nonzero_angles = [a for a in angles if a != 0.0]
        assert len(nonzero_angles) > 0, "Agent should have turned"

    def test_composite_agent_5_seconds(self):
        """Run composite agent for 5s, verify it moves and turns."""
        agent = CompositeMotionAgent()
        dt = 1/60
        total_dist = 0.0
        angles = []
        for i in range(300):
            dist, angle = agent.update(0, current_sim_time=i * dt, dt=dt)
            total_dist += dist
            angles.append(angle)
        assert total_dist > 0
        nonzero_angles = [a for a in angles if a != 0.0]
        assert len(nonzero_angles) > 0

    def test_food_changes_inverter_behavior(self):
        """Feeding food to inverter agent should eventually change mode."""
        agent = InverterAgent()
        dt = 1/60
        # Run without food
        for i in range(30):
            agent.update(0, current_sim_time=i * dt, dt=dt)
        mode_before = agent.inverter.is_high_mode

        # Run with food
        for i in range(30, 60):
            agent.update(1, current_sim_time=i * dt, dt=dt)
        mode_after = agent.inverter.is_high_mode

        assert mode_after != mode_before, "Food should switch inverter mode"

    def test_agent_speed_never_negative(self):
        """Distance per frame should never be negative."""
        for AgentClass in AGENT_CLASSES.values():
            agent = AgentClass()
            dt = 1/60
            for i in range(100):
                dist, _ = agent.update(0, current_sim_time=i * dt, dt=dt)
                assert dist >= 0, f"{AgentClass.__name__} produced negative distance"


# ============================================================================
# Inverter.get_rates() correctness
# ============================================================================

class TestGetRates:
    """Verify get_rates() returns correct analytical values."""

    def test_inactive_returns_zero(self):
        inv = Inverter(C1=1.0, C2=2.0, C3=3.0, C4=4.0)
        assert inv.get_rates() == (0.0, 0.0)

    def test_low_mode_returns_inverse_c1_c3(self):
        inv = Inverter(C1=0.5, C2=0.8, C3=2.0, C4=0.3)
        for _ in range(60):
            inv.update(0.01, 0.0)
        L, R = inv.get_rates()
        # Rates vary due to spike timing jitter, allow ±50% tolerance
        assert 1.0 < L < 3.0  # expected ~2.0 (1/0.5)
        assert 0.25 < R < 0.75  # expected ~0.5 (1/2.0)

    def test_high_mode_returns_inverse_c2_c4(self):
        inv = Inverter(C1=0.5, C2=0.8, C3=2.0, C4=0.3)
        for _ in range(60):
            inv.update(0.01, 1.0)
        L, R = inv.get_rates()
        # Rates vary due to spike timing jitter, allow ±50% tolerance
        assert 0.625 < L < 1.875  # expected ~1.25 (1/0.8)
        assert 1.67 < R < 5.0  # expected ~3.33 (1/0.3)
