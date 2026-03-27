#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

venv/bin/python tests/eval/batch_test.py tests/eval/specs/all_agents_bands.json
# venv/bin/python tests/eval/batch_test.py tests/eval/specs/single_inverter_all_envs.json tests/eval/specs/composite_configs.json --parallel

# python -m pytest tests/unit/ -v
