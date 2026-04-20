#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

SPEC="${1:-tests/eval/specs/h0_baseline.json}"

venv/bin/python tests/eval/batch_test.py "$SPEC" --results-dir tests/eval/results
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
if echo "$SPEC" | grep -qi "h3"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h3
fi
if echo "$SPEC" | grep -qi "h4a"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h4a
fi
if echo "$SPEC" | grep -qi "h4b"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h4b
fi
if echo "$SPEC" | grep -qi "h5"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h5
fi
if echo "$SPEC" | grep -qi "h6"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h6
fi
if echo "$SPEC" | grep -qi "h7"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h7
fi
if echo "$SPEC" | grep -qi "h8"; then
    venv/bin/python tests/eval/validate_hypothesis.py tests/eval/results h8
fi
