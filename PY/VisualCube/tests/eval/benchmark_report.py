"""
Benchmark baseline comparison report with effect sizes.

Compares NNN against standard alternatives: correlated random walks,
Levy-like exploration, prescribed logarithmic spirals, and rule-based
area-restricted search.

Loads analysis.json from benchmark_baselines runs, groups by environment, and
produces comparison tables with Cohen's d effect sizes between NNN and each
baseline.

Usage:
    python benchmark_report.py <results_dir>
    python benchmark_report.py <results_dir> --runs "benchmark_baselines*"
    python benchmark_report.py <results_dir> --output benchmark_report.json
"""
import argparse
import json
import math
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from validate_hypothesis import load_aggregates, avg, cohens_d


COMPARISON_METRICS = ['food_eaten', 'area_cells_visited', 'spiral_quality', 'mean_speed']

AGENT_DISPLAY_ORDER = [
    'nnn3', 'nnn7', 'stochastic', 'crw', 'levy', 'logspiral', 'ars',
]

ENVIRONMENT_ORDER = ['void', 'dense_band', 'dense_bands']


def _extract_agent_env(config_key: str) -> Tuple[str, str]:
    """Parse config_key like 'bm_nnn3_composite_void' into (agent_label, environment)."""
    key = config_key.lower()
    # Label is between 'bm_' and the agent_type
    if not key.startswith('bm_'):
        return (config_key, 'unknown')

    for env in ENVIRONMENT_ORDER:
        if key.endswith(f'_{env}'):
            label_part = key[3:-(len(env) + 1)]
            # Strip agent_type suffix
            for at in ('_composite', '_stochastic', '_crw', '_levy', '_log_spiral', '_ars'):
                if label_part.endswith(at):
                    label_part = label_part[:-(len(at))]
                    break
            return (label_part, env)

    return (config_key, 'unknown')


def load_benchmark_data(results_dir: str, run_pattern: Optional[str] = None
                        ) -> Dict[str, Dict[str, Dict[str, float]]]:
    """Load benchmark results grouped by environment -> agent -> metrics.

    Returns: {environment: {agent_label: {metric: (mean, std)}}}
    """
    config_data, config_std, n_runs, n_trials = load_aggregates(
        results_dir, 'benchmark_baselines', run_pattern)

    if not config_data:
        return {}

    result: Dict[str, Dict[str, Dict[str, Tuple[float, float]]]] = defaultdict(
        lambda: defaultdict(dict))

    for ck in config_data:
        agent_label, env = _extract_agent_env(ck)
        for m in COMPARISON_METRICS:
            vals = config_data[ck].get(m, [])
            stds = config_std[ck].get(m, [])
            if vals:
                mean_val = avg(vals)
                std_val = avg(stds) if stds else 0.0
                result[env][agent_label][m] = (mean_val, std_val)

    return dict(result)


def compute_effect_sizes(data: Dict[str, Dict[str, Dict[str, Tuple[float, float]]]],
                         nnn_label: str = 'nnn3') -> Dict[str, Any]:
    """Compute Cohen's d for NNN vs each baseline, per environment."""
    effects = {}
    for env in ENVIRONMENT_ORDER:
        if env not in data:
            continue
        agents = data[env]
        if nnn_label not in agents:
            continue
        nnn = agents[nnn_label]
        env_effects = {}
        for agent_label, metrics in agents.items():
            if agent_label == nnn_label:
                continue
            agent_effects = {}
            for m in COMPARISON_METRICS:
                if m in nnn and m in metrics:
                    nnn_m, nnn_s = nnn[m]
                    bl_m, bl_s = metrics[m]
                    d = cohens_d(nnn_m, nnn_s, bl_m, bl_s)
                    pct = ((nnn_m - bl_m) / bl_m * 100) if bl_m != 0 else float('inf')
                    agent_effects[m] = {
                        'cohens_d': round(d, 2),
                        'nnn_mean': round(nnn_m, 4),
                        'baseline_mean': round(bl_m, 4),
                        'pct_improvement': round(pct, 1),
                    }
            env_effects[agent_label] = agent_effects
        effects[env] = env_effects
    return effects


def _d_label(d: float) -> str:
    ad = abs(d)
    if ad >= 1.2:
        return "very large"
    elif ad >= 0.8:
        return "large"
    elif ad >= 0.5:
        return "medium"
    elif ad >= 0.2:
        return "small"
    return "negligible"


def print_report(data: Dict[str, Dict[str, Dict[str, Tuple[float, float]]]]) -> None:
    print(f"\n{'='*100}")
    print("BENCHMARK BASELINE COMPARISON")
    print(f"{'='*100}")

    for env in ENVIRONMENT_ORDER:
        if env not in data:
            continue
        agents = data[env]

        print(f"\n--- Environment: {env} ---\n")

        header = f"  {'Agent':<15}"
        for m in COMPARISON_METRICS:
            short = m.replace('area_cells_', 'area_')
            header += f" {short:>20}"
        print(header)
        print(f"  {'-'*15}" + f" {'-'*20}" * len(COMPARISON_METRICS))

        ordered = sorted(agents.keys(),
                         key=lambda a: AGENT_DISPLAY_ORDER.index(a)
                         if a in AGENT_DISPLAY_ORDER else 99)

        for agent in ordered:
            metrics = agents[agent]
            row = f"  {agent:<15}"
            for m in COMPARISON_METRICS:
                if m in metrics:
                    mean, std = metrics[m]
                    row += f" {mean:>9.2f} +/- {std:<6.2f}"
                else:
                    row += f" {'N/A':>20}"
            print(row)

    # Effect sizes
    for nnn_label in ['nnn3', 'nnn7']:
        effects = compute_effect_sizes(data, nnn_label)
        if not effects:
            continue

        print(f"\n{'='*100}")
        print(f"EFFECT SIZES - {nnn_label.upper()} vs baselines (Cohen's d)")
        print(f"{'='*100}")

        for env in ENVIRONMENT_ORDER:
            if env not in effects:
                continue
            print(f"\n  --- {env} ---")
            for agent, agent_effects in sorted(effects[env].items()):
                parts = []
                for m in COMPARISON_METRICS:
                    if m in agent_effects:
                        e = agent_effects[m]
                        d = e['cohens_d']
                        label = _d_label(d)
                        short = m.replace('area_cells_', 'area_')
                        parts.append(f"{short}={d:+.2f} ({label})")
                if parts:
                    print(f"    vs {agent:<12}: {', '.join(parts)}")

    # Rank ordering
    print(f"\n{'='*100}")
    print("RANK ORDERING (best to worst per metric)")
    print(f"{'='*100}")

    for env in ENVIRONMENT_ORDER:
        if env not in data:
            continue
        print(f"\n  --- {env} ---")
        agents = data[env]
        for m in COMPARISON_METRICS:
            ranked = []
            for agent, metrics in agents.items():
                if m in metrics:
                    ranked.append((agent, metrics[m][0]))
            ranked.sort(key=lambda x: x[1], reverse=True)
            short = m.replace('area_cells_', 'area_')
            ranking = " > ".join(f"{a}({v:.2f})" for a, v in ranked)
            print(f"    {short:<20}: {ranking}")

    print()


def main():
    parser = argparse.ArgumentParser(
        description="Benchmark baseline comparison with effect sizes")
    parser.add_argument("results_dir", help="Path to results directory")
    parser.add_argument("--runs", default=None,
                        help="Glob pattern to filter run folders")
    parser.add_argument("--output", default=None,
                        help="Output JSON file path")
    args = parser.parse_args()

    data = load_benchmark_data(args.results_dir, args.runs)

    if not data:
        print("No benchmark data found. Run benchmark_baselines spec first.")
        sys.exit(1)

    print_report(data)

    if args.output:
        # Serialize tuples as dicts
        json_data = {}
        for env, agents in data.items():
            json_data[env] = {}
            for agent, metrics in agents.items():
                json_data[env][agent] = {}
                for m, (mean, std) in metrics.items():
                    json_data[env][agent][m] = {'mean': round(mean, 4), 'std': round(std, 4)}

        effects_3 = compute_effect_sizes(data, 'nnn3')
        effects_7 = compute_effect_sizes(data, 'nnn7')

        output = {
            'data': json_data,
            'effect_sizes_nnn3': effects_3,
            'effect_sizes_nnn7': effects_7,
        }
        with open(args.output, 'w', encoding='utf-8') as f:
            json.dump(output, f, indent=2)
        print(f"Report saved to {args.output}")


if __name__ == "__main__":
    main()
