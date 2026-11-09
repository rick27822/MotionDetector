@echo off
setlocal
cd /d "%~dp0"
if exist ".venv\Scripts\python.exe" (
  ".venv\Scripts\python.exe" app.py
) else (
  python app.py
)
if errorlevel 1 (
  echo Start failed. Please run setup.bat first, or share the error above.
  pause
)
