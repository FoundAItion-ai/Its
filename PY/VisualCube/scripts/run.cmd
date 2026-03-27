@echo off
cd /d "%~dp0.."

pause

venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\all_agents_bands.json
rem venv\Scripts\python tests\eval\batch_test.py tests\eval\specs\single_inverter_all_envs.json tests\eval\specs\composite_configs.json --parallel

rem python -m pytest tests\unit\ -v
