@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (
  set "PYTHON=python"
) else (
  set "PYTHON=py -3"
)
if not exist ".venv\Scripts\python.exe" (
  %PYTHON% -m venv .venv
  if errorlevel 1 goto failed
)
".venv\Scripts\python.exe" -m pip install --timeout 30 -r requirements-tested.txt
if errorlevel 1 goto failed
echo Setup complete. Double-click start.bat to open PoseLab.
pause
exit /b 0
:failed
echo Setup failed. Please save the error message for troubleshooting.
echo Python 3.11 or 3.12 is recommended. Internet is required for installation.
pause
exit /b 1
