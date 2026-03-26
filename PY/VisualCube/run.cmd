pause

venv\Scripts\python src\batch_test.py test_specs\all_agents_bands.json
rem venv\Scripts\python src\batch_test.py test_specs\single_inverter_all_envs.json test_specs\composite_configs.json --parallel

rem python -m pytest tests/ -v