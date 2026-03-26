#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

venv/bin/python src/batch_test.py test_specs/h0_baseline.json --results-dir test_results
venv/bin/python src/analyze_logs.py test_results/
