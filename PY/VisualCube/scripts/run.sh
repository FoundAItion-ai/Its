#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

venv/bin/python src/batch_test.py test_specs/all_agents_bands.json
# venv/bin/python src/batch_test.py test_specs/single_inverter_all_envs.json test_specs/composite_configs.json --parallel

# python -m pytest tests/ -v
