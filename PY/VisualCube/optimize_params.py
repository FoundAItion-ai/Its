"""
Headless parameter optimizer for NNN inverter C1-C4 values.

Runs the simulation without GUI, varying C1-C4 parameters to maximize
food-eating efficiency (food/second) on the three_lines_dense preset.

Usage:
    python optimize_params.py [--seconds 30] [--runs 3] [--agents 1]
"""
import sys
import os
import math
import itertools
import argparse
from typing import Dict, Any, List, Tuple

# Must init pygame (even headless) before importing main
import pygame
pygame.init()

import config as cfg
from math_module import set_frame_dt
from main import configure, run_frame, get_simulation_stats, init_pygame_surface


def run_single_trial(
    agent_type: str,
    food_preset: str,
    agent_params: Dict[str, Any],
    seconds: float,
    n_agents: int = 1,
    fps: int = 60,
    spawn_point: Tuple[float, float] = None,
    log_file_handle=None,
    on_configured=None,
) -> Dict[str, Any]:
    """Run one headless simulation and return stats.

    Args:
        on_configured: optional callback invoked after configure(), before
            the frame loop starts. Use for start screenshots, etc.

    Returns dict with: food_eaten, efficiency, food_remaining,
    excursion_dist, input_freq, sim_time.
    """
    if spawn_point is None:
        spawn_point = (120.0, cfg.WINDOW_H / 2)
    set_frame_dt(1.0 / fps)
    cfg.set_global_speed_modifier(1.0)

    configure(
        food_preset=food_preset,
        agent_type=agent_type,
        n_agents=n_agents,
        spawn_point=spawn_point,
        initial_direction_deg=-1.0,
        agent_params_for_main_agent=agent_params,
        draw_trace=False,
        permanent_trace=False,
        log_file_handle=log_file_handle,
    )
    init_pygame_surface(None)

    if on_configured:
        on_configured()

    frames = int(seconds * fps)
    for _ in range(frames):
        run_frame(is_visual=False)

    stats = get_simulation_stats()
    food_eaten = stats.get("food_eaten_run", 0)
    efficiency = food_eaten / seconds if seconds > 0 else 0
    food_remaining = stats.get("total_food", 0)
    input_freq = stats.get("input_food_frequency", 0.0)
    history = stats.get("history", {})
    excursion_vals = list(history.get("excursion_dist", []))
    excursion_dist = excursion_vals[-1] if excursion_vals else 0.0
    return {
        "food_eaten": food_eaten,
        "efficiency": efficiency,
        "food_remaining": food_remaining,
        "excursion_dist": excursion_dist,
        "input_freq": input_freq,
        "sim_time": seconds,
    }


def evaluate_params(
    C1: float, C2: float, C3: float, C4: float,
    seconds: float, runs: int, n_agents: int,
) -> Tuple[float, float]:
    """Run multiple trials and return (mean_efficiency, mean_food)."""
    params = {
        "C1": C1, "C2": C2, "C3": C3, "C4": C4,
        "turn_decision_interval_sec": 0.0167,
    }
    efficiencies = []
    foods = []
    for _ in range(runs):
        result = run_single_trial("inverter", "three_lines_dense", params, seconds, n_agents)
        efficiencies.append(result["efficiency"])
        foods.append(result["food_eaten"])
    mean_eff = sum(efficiencies) / len(efficiencies)
    mean_food = sum(foods) / len(foods)
    return mean_eff, mean_food


def main():
    parser = argparse.ArgumentParser(description="Optimize NNN inverter C1-C4 parameters")
    parser.add_argument("--seconds", type=float, default=30, help="Simulation duration per trial (default: 30)")
    parser.add_argument("--runs", type=int, default=3, help="Trials per parameter set (default: 3)")
    parser.add_argument("--agents", type=int, default=1, help="Agents per trial (default: 1)")
    args = parser.parse_args()

    print(f"Optimizer: {args.seconds}s per trial, {args.runs} runs per param set, {args.agents} agent(s)")
    print(f"Window: {cfg.WINDOW_W}x{cfg.WINDOW_H}")
    print(f"Food preset: three_lines_dense")
    print(f"Speed scaling: {cfg.AGENT_SPEED_SCALING_FACTOR}, Angular const: {cfg.ANGULAR_PROPORTIONALITY_CONSTANT}")
    print()

    # NNN constraint: C3 < C1 < C2 < C4 (periods)
    # = f(C3) > f(C1) > f(C2) > f(C4) (frequencies)
    #
    # Current: C1=0.3, C2=0.6, C3=0.25, C4=1.2
    #
    # Key behaviors to tune:
    # - Void (HIGH, no food): fires at C1 (L) and C3 (R) -> curved search arc
    #   R>L rate means right turn. Radius ~ speed / angular_rate
    # - Food (LOW): fires at C2 (L) and C4 (R) -> sharp deflection
    #   L>R rate means left turn (reversal). Sharper = better tracking zigzag.
    #
    # What the NNN paper shows:
    # - Sinusoidal zigzag along food line (tight, fast oscillations)
    # - When off food, wide sweeping arc back to food line
    #
    # Parameters to explore:
    # C1: decision window + L period in void mode (0.1 - 0.5)
    # C3: R period in void mode, must be < C1 (0.05 - C1)
    # C2: L period in food mode, must be > C1 (C1 - 2.0)
    # C4: R period in food mode, must be > C2 (C2 - 5.0)

    # Grid search over key ratios
    C1_values = [0.15, 0.2, 0.25, 0.3, 0.4, 0.5]
    # C3/C1 ratio (must be < 1.0 for NNN constraint)
    C3_ratios = [0.5, 0.65, 0.8, 0.9]
    # C2/C1 ratio (must be > 1.0)
    C2_ratios = [1.5, 2.0, 3.0, 4.0]
    # C4/C2 ratio (must be > 1.0)
    C4_ratios = [1.5, 2.0, 3.0, 4.0]

    total = len(C1_values) * len(C3_ratios) * len(C2_ratios) * len(C4_ratios)
    print(f"Grid search: {total} parameter combinations")
    print(f"{'#':>4} {'C1':>5} {'C2':>5} {'C3':>5} {'C4':>5} {'eff':>7} {'food':>6}")
    print("-" * 50)

    results = []
    count = 0

    for C1 in C1_values:
        for c3r in C3_ratios:
            C3 = round(C1 * c3r, 3)
            for c2r in C2_ratios:
                C2 = round(C1 * c2r, 3)
                for c4r in C4_ratios:
                    C4 = round(C2 * c4r, 3)
                    count += 1

                    # Verify NNN constraint: C3 < C1 < C2 < C4
                    if not (C3 < C1 < C2 < C4):
                        continue

                    mean_eff, mean_food = evaluate_params(
                        C1, C2, C3, C4, args.seconds, args.runs, args.agents
                    )
                    results.append((mean_eff, mean_food, C1, C2, C3, C4))
                    print(f"{count:>4} {C1:>5.2f} {C2:>5.2f} {C3:>5.3f} {C4:>5.2f} {mean_eff:>7.3f} {mean_food:>6.1f}")

    # Sort by efficiency
    results.sort(key=lambda x: x[0], reverse=True)

    print()
    print("=" * 60)
    print("TOP 10 PARAMETER SETS (by food/second):")
    print(f"{'rank':>4} {'C1':>5} {'C2':>5} {'C3':>5} {'C4':>5} {'eff':>7} {'food':>6}")
    print("-" * 50)
    for i, (eff, food, C1, C2, C3, C4) in enumerate(results[:10]):
        print(f"{i+1:>4} {C1:>5.2f} {C2:>5.2f} {C3:>5.3f} {C4:>5.2f} {eff:>7.3f} {food:>6.1f}")

    if results:
        best = results[0]
        print(f"\nBEST: C1={best[2]}, C2={best[3]}, C3={best[4]}, C4={best[5]}")
        print(f"  Efficiency: {best[0]:.3f} food/s, Food eaten: {best[1]:.1f}")
        print(f"\nFor inverter.py:")
        print(f"  NNN_SINGLE_PRESET = {{'C1': {best[2]}, 'C2': {best[3]}, 'C3': {best[4]}, 'C4': {best[5]}}}")

    pygame.quit()


if __name__ == "__main__":
    main()
