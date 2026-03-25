"""Tests for NNN Inverter model."""
import pytest
import sys
from pathlib import Path

# Add parent to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from inverter import Inverter, InverterConfig, NNN_SINGLE_PRESET, NNN_COMPOSITE_PRESET, combine_outputs


class TestInverterLowMode:
    """Tests for Inverter in low input mode (f_i < C1)."""

    def test_low_input_returns_c1_c3(self):
        """When f_i < C1, outputs are (C1, C3)."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=3.0)  # 3 < 5
        assert f_o1 == 5.0  # C1
        assert f_o2 == 8.0  # C3

    def test_low_input_mode_flag_is_false(self):
        """In low mode, is_high_mode should be False."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        inv.compute(f_i=0.0)
        assert inv.is_high_mode == False

    def test_zero_input_is_low_mode(self):
        """f_i=0 is always below threshold."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=0.0)
        assert f_o1 == 5.0
        assert f_o2 == 8.0


class TestInverterHighMode:
    """Tests for Inverter in high input mode (f_i >= C1)."""

    def test_high_input_returns_c2_c4(self):
        """When f_i >= C1, outputs are (C2, C4)."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=6.0)  # 6 >= 5
        assert f_o1 == 2.0  # C2
        assert f_o2 == 3.0  # C4

    def test_high_input_mode_flag_is_true(self):
        """In high mode, is_high_mode should be True."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        inv.compute(f_i=10.0)
        assert inv.is_high_mode == True

    def test_exact_threshold_is_high_mode(self):
        """f_i == C1 should trigger high mode (>= comparison)."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=5.0)  # Exactly at threshold
        assert f_o1 == 2.0  # C2
        assert f_o2 == 3.0  # C4
        assert inv.is_high_mode == True


class TestInverterAsymmetry:
    """Tests verifying output asymmetry creates turn bias."""

    def test_low_mode_right_greater_than_left(self):
        """In low mode, R > L (C3 > C1) -> turns one way."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        f_o1, f_o2 = inv.compute(f_i=0.0)
        assert f_o2 > f_o1  # R=8 > L=5

    def test_mode_switch_reverses_dominant_side(self):
        """Switching modes reverses which output is dominant."""
        # Use values that satisfy NNN constraints: C2 > C4
        inv = Inverter(C1=5.0, C2=3.0, C3=8.0, C4=1.0)

        # Low mode: L=5, R=8 → R > L
        f_o1_low, f_o2_low = inv.compute(f_i=0.0)
        low_dominant = "R" if f_o2_low > f_o1_low else "L"

        # High mode: L=3, R=1 → L > R (reversed!)
        f_o1_high, f_o2_high = inv.compute(f_i=10.0)
        high_dominant = "R" if f_o2_high > f_o1_high else "L"

        assert low_dominant != high_dominant


class TestInverterConfig:
    """Tests for InverterConfig dataclass."""

    def test_config_stores_parameters(self):
        """Config stores C1-C4 and crossed flag."""
        cfg = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=True, name="f1")
        assert cfg.C1 == 5.0
        assert cfg.crossed == True
        assert cfg.name == "f1"

    def test_config_default_crossed_is_false(self):
        """Default crossed should be False."""
        cfg = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        assert cfg.crossed == False


class TestNNNPreset:
    """Tests for NNN 3x1x2 preset configuration."""

    def test_preset_has_four_inverters(self):
        """NNN preset has exactly 4 inverters."""
        assert len(NNN_COMPOSITE_PRESET) == 4

    def test_preset_crossed_wiring(self):
        """f1 is normal, f2-f4 should be crossed (counter-phase)."""
        assert NNN_COMPOSITE_PRESET[0]['crossed'] == False  # f1: normal
        for i in range(1, len(NNN_COMPOSITE_PRESET)):
            assert NNN_COMPOSITE_PRESET[i]['crossed'] == True

    def test_preset_thresholds_ordered(self):
        """C1 values should be ordered: f3 > f2 > f1."""
        c1_f1 = NNN_COMPOSITE_PRESET[0]['C1']
        c1_f2 = NNN_COMPOSITE_PRESET[1]['C1']
        c1_f3 = NNN_COMPOSITE_PRESET[2]['C1']
        assert c1_f3 > c1_f2 > c1_f1

    def test_nnn_constraint_single_preset(self):
        """Single preset must satisfy C3 > C1 > C2 > C4 (frequencies)."""
        p = NNN_SINGLE_PRESET
        f1, f2, f3, f4 = 1/p['C1'], 1/p['C2'], 1/p['C3'], 1/p['C4']
        assert f3 > f1 > f2 > f4, \
            f"NNN constraint violated: C3={f3:.2f} > C1={f1:.2f} > C2={f2:.2f} > C4={f4:.2f}"

    def test_nnn_constraint_composite_preset(self):
        """Each composite inverter must satisfy C3 > C1 > C2 > C4 (frequencies)."""
        for inv in NNN_COMPOSITE_PRESET:
            f1, f2, f3, f4 = 1/inv['C1'], 1/inv['C2'], 1/inv['C3'], 1/inv['C4']
            assert f3 > f1 > f2 > f4, \
                f"NNN constraint violated for {inv['name']}: " \
                f"C3={f3:.2f} > C1={f1:.2f} > C2={f2:.2f} > C4={f4:.2f}"


class TestCombineOutputs:
    """Tests for combining multiple inverter outputs."""

    def test_single_inverter_normal_wiring(self):
        """Single inverter with normal wiring passes through."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        cfg = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=False)

        L, R = combine_outputs([inv], [cfg], f_i=0.0)
        assert L == 5.0  # f_o1 = C1
        assert R == 8.0  # f_o2 = C3

    def test_single_inverter_crossed_wiring(self):
        """Single inverter with crossed wiring swaps outputs."""
        inv = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        cfg = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=True)

        L, R = combine_outputs([inv], [cfg], f_i=0.0)
        assert L == 8.0  # f_o2 -> L (swapped)
        assert R == 5.0  # f_o1 -> R (swapped)

    def test_two_inverters_sum(self):
        """Two inverters with normal wiring sum their outputs."""
        inv1 = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        inv2 = Inverter(C1=5.0, C2=2.0, C3=8.0, C4=3.0)
        cfg1 = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=False)
        cfg2 = InverterConfig(C1=5.0, C2=2.0, C3=8.0, C4=3.0, crossed=False)

        L, R = combine_outputs([inv1, inv2], [cfg1, cfg2], f_i=0.0)
        assert L == 10.0  # 5 + 5
        assert R == 16.0  # 8 + 8


class TestNNNComposite:
    """Integration tests for NNN 3x1x2 composite behavior."""

    def _create_composite(self):
        """Helper to create NNN 3x1x2 inverters and configs."""
        inverters = []
        configs = []
        for preset in NNN_COMPOSITE_PRESET:
            inv = Inverter(
                C1=preset['C1'], C2=preset['C2'],
                C3=preset['C3'], C4=preset['C4'],
                name=preset['name']
            )
            cfg = InverterConfig(
                C1=preset['C1'], C2=preset['C2'],
                C3=preset['C3'], C4=preset['C4'],
                crossed=preset['crossed'],
                name=preset['name']
            )
            inverters.append(inv)
            configs.append(cfg)
        return inverters, configs

    def test_void_produces_turn_differential(self):
        """In void (f_i=0), composite produces L != R."""
        inverters, configs = self._create_composite()
        L, R = combine_outputs(inverters, configs, f_i=0.0)
        assert L != R  # Should have turn bias

    def test_crossed_inverter_opposes_normal(self):
        """Crossed inverters partially oppose f1's differential."""
        inverters, configs = self._create_composite()
        L, R = combine_outputs(inverters, configs, f_i=0.0)

        # Compute expected from preset values (instant mode returns periods)
        expected_L, expected_R = 0.0, 0.0
        for p in NNN_COMPOSITE_PRESET:
            if p['crossed']:
                expected_L += p['C3']  # R_spike -> L_motor
                expected_R += p['C1']  # L_spike -> R_motor
            else:
                expected_L += p['C1']
                expected_R += p['C3']
        assert abs(L - expected_L) < 0.01
        assert abs(R - expected_R) < 0.01


class TestTemporalIntegration:
    """Tests for the new temporal integration model (decision window)."""

    def test_inverter_silent_before_first_window(self):
        """Inverter should not fire until it completes first C1 window."""
        inv = Inverter(C1=0.5, C2=0.3, C3=0.8, C4=0.5)
        # Step through 0.4s in small steps — still within first window
        for _ in range(40):
            l, r = inv.update(0.01, 0.0)
            assert l == 0 and r == 0, "Should be silent before window completes"
        assert not inv.is_active

    def test_inverter_fires_after_window_no_input(self):
        """After C1 window with no input, inverter should start firing."""
        inv = Inverter(C1=0.5, C2=0.3, C3=0.8, C4=0.5)
        # Complete the first decision window (0.5s)
        for _ in range(50):
            inv.update(0.01, 0.0)
        assert inv.is_active
        assert not inv.is_high_mode
        # Now it should fire L spikes at C1=0.5s period
        # Run for 1.0s to guarantee at least one spike
        spikes = []
        for _ in range(100):
            l, r = inv.update(0.01, 0.0)
            if l:
                spikes.append('L')
            if r:
                spikes.append('R')
        assert len(spikes) > 0, "Should fire after detecting silence"

    def test_inverter_fires_at_c2_c4_with_input(self):
        """With input during window, inverter fires at C2/C4 periods (HIGH mode)."""
        inv = Inverter(C1=0.5, C2=0.8, C3=0.8, C4=0.5)
        # Complete first window WITH input
        for _ in range(50):
            inv.update(0.01, 1.0)  # Constant input
        assert inv.is_active
        assert inv.is_high_mode
        # Should fire at C2/C4 periods (reversed from LOW mode)
        spikes = []
        for _ in range(100):
            l, r = inv.update(0.01, 1.0)
            if l: spikes.append('L')
            if r: spikes.append('R')
        assert len(spikes) > 0, "Should fire at C2/C4 rates in HIGH mode"

    def test_reset_restarts_window(self):
        """reset() should deactivate inverter, requiring a new full window."""
        inv = Inverter(C1=0.5, C2=0.3, C3=0.8, C4=0.5)
        # Activate it
        for _ in range(50):
            inv.update(0.01, 0.0)
        assert inv.is_active
        # Reset
        inv.reset()
        assert not inv.is_active
        # Should be silent again until new window completes
        for _ in range(40):
            l, r = inv.update(0.01, 0.0)
            assert l == 0 and r == 0

    def test_staggered_activation_different_c1(self):
        """Inverters with different C1 activate at different times."""
        fast = Inverter(C1=0.5, C2=0.3, C3=0.8, C4=0.5, name='fast')
        slow = Inverter(C1=1.0, C2=0.5, C3=0.5, C4=0.3, name='slow')

        # After 0.6s: fast should be active, slow should not
        for _ in range(60):
            fast.update(0.01, 0.0)
            slow.update(0.01, 0.0)

        assert fast.is_active, "Fast inverter should be active after 0.6s"
        assert not slow.is_active, "Slow inverter should NOT be active after 0.6s"

        # After another 0.5s (total 1.1s): both should be active
        for _ in range(50):
            fast.update(0.01, 0.0)
            slow.update(0.01, 0.0)

        assert fast.is_active
        assert slow.is_active, "Slow inverter should be active after 1.1s"

    def test_food_resets_all_then_staggered_reactivation(self):
        """Simulates the spiral mechanism: food resets, then staggered reactivation."""
        fast = Inverter(C1=0.5, C2=0.3, C3=0.8, C4=0.5, name='fast')
        slow = Inverter(C1=1.3, C2=1.0, C3=0.3, C4=0.2, name='slow')

        # Both activate in void
        for _ in range(150):
            fast.update(0.01, 0.0)
            slow.update(0.01, 0.0)
        assert fast.is_active and slow.is_active

        # Food found → reset both
        fast.reset()
        slow.reset()
        assert not fast.is_active and not slow.is_active

        # Run without food: fast reactivates at 0.5s, slow at 1.3s
        for _ in range(60):  # 0.6s
            fast.update(0.01, 0.0)
            slow.update(0.01, 0.0)

        assert fast.is_active, "Fast should reactivate first"
        assert not slow.is_active, "Slow should still be counting"
