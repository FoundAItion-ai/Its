"""Tests for NNN Inverter model."""
import pytest
import sys
from pathlib import Path

# Add parent to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from inverter import Inverter, InverterConfig, NNN_3x1x2_PRESET, combine_outputs


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

    def test_preset_has_three_inverters(self):
        """NNN 3x1x2 has exactly 3 inverters."""
        assert len(NNN_3x1x2_PRESET) == 3

    def test_preset_third_is_crossed(self):
        """f3 (third inverter) should be crossed."""
        assert NNN_3x1x2_PRESET[0]['crossed'] == False
        assert NNN_3x1x2_PRESET[1]['crossed'] == False
        assert NNN_3x1x2_PRESET[2]['crossed'] == True

    def test_preset_thresholds_ordered(self):
        """C1 values should be ordered: f3 > f2 > f1."""
        c1_f1 = NNN_3x1x2_PRESET[0]['C1']
        c1_f2 = NNN_3x1x2_PRESET[1]['C1']
        c1_f3 = NNN_3x1x2_PRESET[2]['C1']
        assert c1_f3 > c1_f2 > c1_f1


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


class TestNNN3x1x2Composite:
    """Integration tests for NNN 3x1x2 composite behavior."""

    def _create_composite(self):
        """Helper to create NNN 3x1x2 inverters and configs."""
        inverters = []
        configs = []
        for preset in NNN_3x1x2_PRESET:
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
        """f3 (crossed) contributes opposite to f1, f2."""
        inverters, configs = self._create_composite()

        # All in low mode (f_i=0), using current preset values (0.2, 0.3, 0.4 scale)
        # f1: L=C1=0.2, R=C3=0.4 (normal)
        # f2: L=C1=0.3, R=C3=0.5 (normal)
        # f3: L=C3=0.6, R=C1=0.4 (crossed: L gets C3, R gets C1)

        L, R = combine_outputs(inverters, configs, f_i=0.0)

        # Expected: L = 0.2+0.3+0.6 = 1.1, R = 0.4+0.5+0.4 = 1.3
        assert abs(L - 1.1) < 0.01
        assert abs(R - 1.3) < 0.01
