@echo off
cd /d "%~dp0.."

venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h0_baseline.json --results-dir tests\eval\results
venv\Scripts\python src\analyze_logs.py tests\eval\results\
