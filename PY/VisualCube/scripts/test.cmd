@echo off
cd /d "%~dp0.."

for /L %%i in (1,1,10) do (
    echo === Run %%i of 10 ===
    venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\h0_baseline.json --results-dir tests\eval\results --no-screenshots
)
venv\Scripts\python src\analyze_logs.py tests\eval\results\
