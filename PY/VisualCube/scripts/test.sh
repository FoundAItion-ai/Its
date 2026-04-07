#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

SPEC="${1:-tests/eval/specs/h0_baseline.json}"

venv/bin/python tests/eval/batch_test.py "$SPEC" --results-dir tests/eval/results --no-screenshots
venv/bin/python src/analyze_logs.py tests/eval/results/

# Auto-detect hypothesis from spec filename and run validation
if echo "$SPEC" | grep -qi "h0"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h0
fi
if echo "$SPEC" | grep -qi "h1"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h1
fi
if echo "$SPEC" | grep -qi "h2"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h2
fi
