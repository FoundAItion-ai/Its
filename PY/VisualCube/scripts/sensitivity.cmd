@echo off
cd /d "%~dp0.."

echo === Threshold sensitivity analysis (uses existing results) ===
venv\Scripts\python tests\eval\sensitivity_analysis.py tests\eval\results --output tests\eval\results\sensitivity_report.json
