@echo off
cd /d "%~dp0.."

venv\Scripts\python -m pytest tests\unit\ -v
