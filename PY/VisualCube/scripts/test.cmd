@echo off
cd /d "%~dp0.."

venv\Scripts\python src\batch_test.py test_specs\h0_baseline.json --results-dir test_results
venv\Scripts\python src\analyze_logs.py test_results/
