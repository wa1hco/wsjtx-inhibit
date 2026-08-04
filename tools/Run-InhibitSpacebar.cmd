@echo off
setlocal
cd /d "%~dp0"

where py >nul 2>&1
if %ERRORLEVEL%==0 (
  py -3 "%~dp0inhibit_spacebar_gui.py" %*
  goto :eof
)

where python >nul 2>&1
if %ERRORLEVEL%==0 (
  python "%~dp0inhibit_spacebar_gui.py" %*
  goto :eof
)

where python3 >nul 2>&1
if %ERRORLEVEL%==0 (
  python3 "%~dp0inhibit_spacebar_gui.py" %*
  goto :eof
)

echo Python 3 not found. Install from https://www.python.org/downloads/
echo (enable "tcl/tk and IDLE" / tkinter).
pause
exit /b 1
