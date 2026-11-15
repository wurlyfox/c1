@echo off
rem Double-click launcher for Windows: makes c1-pack.zip from your disc image.
cd /d "%~dp0"
where py >nul 2>nul && (py -3 make_c1_pack.py --pause %* & exit /b)
where python >nul 2>nul && (python make_c1_pack.py --pause %* & exit /b)
echo Python 3 is needed: https://www.python.org/downloads/ (tick "Add Python to PATH")
pause
