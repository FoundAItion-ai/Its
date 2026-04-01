"""Smoke test: 5-inverter composite agent produces valid trajectory in void."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from agent_module import CompositeMotionAgent
from inverter import recalculate_composite


def test_5_inverter_composite_runs():
    """A 5-inverter composite agent should run for 300 frames without error
    and produce non-zero movement."""
    preset = [
        {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": False, "name": "f1"},
        {"C1": 3.0, "crossed": True, "name": "f2"},
        {"C1": 6.0, "crossed": True, "name": "f3"},
        {"C1": 10.0, "crossed": True, "name": "f4"},
        {"C1": 15.0, "crossed": True, "name": "f5"},
    ]
    recalculate_composite(preset)

    import math

    agent = CompositeMotionAgent(
        inverters=preset,
        speed_scaling_factor=5.0,
        angular_proportionality_constant=0.08,
    )

    x, y, angle = 400.0, 400.0, 0.0
    total_dist = 0.0
    dt = 1.0 / 60
    for frame in range(300):  # 5 seconds at 60fps
        sim_time = frame * dt
        dist, d_angle = agent.update(food_eaten_count=0, current_sim_time=sim_time, dt=dt)
        angle += d_angle
        x += dist * math.cos(angle)
        y += dist * math.sin(angle)
        total_dist += dist

    # Agent should have moved
    assert total_dist > 10, f"Agent barely moved: total_dist={total_dist}"

    # All 5 inverters should exist
    assert len(agent.inverters) == 5


def test_5_inverter_recalculate_composite_values():
    """recalculate_composite should derive C2/C3/C4 for all 5 inverters
    including the 4th crossed inverter (f5) using diminishing fraction."""
    preset = [
        {"C1": 0.15, "C2": 0.45, "C3": 0.135, "C4": 1.35, "crossed": False, "name": "f1"},
        {"C1": 3.0, "crossed": True, "name": "f2"},
        {"C1": 6.0, "crossed": True, "name": "f3"},
        {"C1": 10.0, "crossed": True, "name": "f4"},
        {"C1": 15.0, "crossed": True, "name": "f5"},
    ]
    recalculate_composite(preset)

    # f5 should have derived values (not missing keys)
    f5 = preset[4]
    assert "C2" in f5, "f5 missing C2"
    assert "C3" in f5, "f5 missing C3"
    assert "C4" in f5, "f5 missing C4"

    # NNN constraint: C3 < C1 < C2 < C4 (period space)
    assert f5["C3"] < f5["C1"] < f5["C2"] < f5["C4"], (
        f"NNN violated for f5: C3={f5['C3']}, C1={f5['C1']}, C2={f5['C2']}, C4={f5['C4']}"
    )

    # f5 opposition should be smaller than f4's (diminishing fractions)
    f4_opposition = 1.0 / preset[3]["C3"] - 1.0 / preset[3]["C1"]
    f5_opposition = 1.0 / f5["C3"] - 1.0 / f5["C1"]
    assert f5_opposition < f4_opposition, (
        f"f5 opposition ({f5_opposition:.4f}) should be < f4 ({f4_opposition:.4f})"
    )
