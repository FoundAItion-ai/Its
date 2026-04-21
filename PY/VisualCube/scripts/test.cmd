@echo off
cd /d "%~dp0.."

set SPEC=%~1
if "%SPEC%"=="" set SPEC=tests\eval\specs\h0_baseline.json

venv\Scripts\python tests\eval\batch_test.py %SPEC% --results-dir tests\eval\results
venv\Scripts\python src\analyze_logs.py tests\eval\results\

REM Auto-detect hypothesis from spec filename and run validation
echo %SPEC% | findstr /i "h0" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h0
)
echo %SPEC% | findstr /i "h1" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h1
)
echo %SPEC% | findstr /i "h2" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h2
)
echo %SPEC% | findstr /i "h3" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h3
)
echo %SPEC% | findstr /i "h4a" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h4a
)
echo %SPEC% | findstr /i "h4b" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h4b
)
echo %SPEC% | findstr /i "h5" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h5
)
echo %SPEC% | findstr /i "h6" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h6
)
echo %SPEC% | findstr /i "h7" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h7
)
echo %SPEC% | findstr /i "h8" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h8
)
echo %SPEC% | findstr /i "h9" >nul && (
    venv\Scripts\python tests\eval\validate_hypothesis.py tests\eval\results h9
)
