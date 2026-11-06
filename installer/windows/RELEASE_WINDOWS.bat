@echo off
rem AJT JOY16 - Windows release (double-click). Keeps the window open and writes RELEASE_WINDOWS_log.txt.
cd /d "%~dp0..\.."
powershell -NoProfile -ExecutionPolicy Bypass -Command "& '%~dp0release_windows.ps1' %* 2>&1 | Tee-Object -FilePath '%~dp0RELEASE_WINDOWS_log.txt'"
echo.
pause
