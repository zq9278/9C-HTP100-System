@echo off
setlocal
cd /d "%~dp0"
set "ELECTRON_RUN_AS_NODE="
where python >nul 2>nul
if errorlevel 1 goto missing
where npm.cmd >nul 2>nul
if errorlevel 1 goto missing
if not exist "node_modules\.bin\electron.cmd" (
  call npm.cmd ci
  if errorlevel 1 goto failed
)
call npm.cmd start
if errorlevel 1 goto failed
exit /b 0
:missing
echo Python 3 and Node.js / npm are required.
:failed
echo Startup failed. See the error above.
pause
exit /b 1
