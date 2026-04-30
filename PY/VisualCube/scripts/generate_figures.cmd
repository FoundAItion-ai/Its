@echo off
cd /d "%~dp0.."

echo Generating main article figures...
venv\Scripts\python scripts\figures\generate_figures.py

echo Generating convergence plot (Supplementary Figure S1)...
venv\Scripts\python scripts\figures\generate_convergence_plot.py

echo Generating formal statistical tests (Supplementary Table S14)...
venv\Scripts\python scripts\figures\generate_stat_tests.py

echo.
echo Output saved to scripts\figures\output\
dir /b scripts\figures\output\
