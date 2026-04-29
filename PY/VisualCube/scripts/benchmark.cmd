@echo off
cd /d "%~dp0.."

echo === Step 1: Run benchmark simulations ===
venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\benchmark_baselines.json --results-dir tests\eval\results

echo === Step 2: Analyze logs ===
venv\Scripts\python src\analyze_logs.py tests\eval\results\

echo === Step 3: Benchmark comparison report ===
venv\Scripts\python tests\eval\benchmark_report.py tests\eval\results --output tests\eval\results\benchmark_report.json

echo === Step 4: Distribution report ===
venv\Scripts\python tests\eval\distribution_report.py tests\eval\results --output tests\eval\results\distribution_report.json

echo === Done ===
