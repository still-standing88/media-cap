@echo off
where py.exe >nul 2>&1
if errorlevel 1 goto use_python
py -3 -c "import sys; sys.exit(sys.version_info < (3, 9))" >nul 2>&1
if errorlevel 1 goto use_python
py -3 "%~dp0scripts\build_library.py" %*
exit /b %errorlevel%

:use_python
python -c "import sys; sys.exit(sys.version_info < (3, 9))" >nul 2>&1
if errorlevel 1 (
    echo Python 3.9 or newer is required. 1>&2
    exit /b 1
)
python "%~dp0scripts\build_library.py" %*
exit /b %errorlevel%
