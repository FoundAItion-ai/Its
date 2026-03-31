@echo off
cd /d "%~dp0.."

set SPEC=%~1
if "%SPEC%"=="" set SPEC=tests\eval\specs\h0_baseline.json

for /L %%i in (1,1,10) do (
    echo === Run %%i of 10 ===
    venv\Scripts\python tests\eval\batch_test.py %SPEC% --results-dir tests\eval\results --no-screenshots
)
venv\Scripts\python src\analyze_logs.py tests\eval\results\
