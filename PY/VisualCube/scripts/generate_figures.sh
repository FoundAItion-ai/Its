#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

echo "Generating main article figures..."
venv/bin/python scripts/figures/generate_figures.py

echo "Generating convergence plot (Supplementary Figure S1)..."
venv/bin/python scripts/figures/generate_convergence_plot.py

echo "Generating formal statistical tests (Supplementary Table S14)..."
venv/bin/python scripts/figures/generate_stat_tests.py

echo ""
echo "Output saved to scripts/figures/output/"
ls scripts/figures/output/
