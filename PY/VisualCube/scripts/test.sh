#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

venv/bin/python tests/eval/batch_test.py tests/eval/specs/h0_baseline.json --results-dir tests/eval/results
venv/bin/python src/analyze_logs.py tests/eval/results/
