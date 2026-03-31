#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

SPEC="${1:-tests/eval/specs/h0_baseline.json}"

for i in $(seq 1 10); do
    echo "=== Run $i of 10 ==="
    venv/bin/python tests/eval/batch_test.py "$SPEC" --results-dir tests/eval/results --no-screenshots
done
venv/bin/python src/analyze_logs.py tests/eval/results/
