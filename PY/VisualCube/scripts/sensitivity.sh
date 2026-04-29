#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

echo "=== Threshold sensitivity analysis (uses existing results) ==="
venv/bin/python tests/eval/sensitivity_analysis.py tests/eval/results --output tests/eval/results/sensitivity_report.json
