@echo off
setlocal
cd /d %~dp0..

if not exist env.bat copy env.bat.template env.bat
call env.bat
if not defined WEASEL_ROOT set WEASEL_ROOT=%CD%
if not defined QT_DIR set QT_DIR=C:\Libraries\Qt\6.8.3\msvc2022_64
if not exist "%QT_DIR%\lib\cmake\Qt6" (
  echo Error: Qt6 not found at %QT_DIR%.
  exit /b 1
)
where cmake >nul 2>&1 || set PATH=%DEVTOOLS_PATH%%PATH%

set WEASEL_OUT=%WEASEL_ROOT%\output
cmake -S installer\BangkeInstaller -B installer\BangkeInstaller\build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=%QT_DIR%
if errorlevel 1 exit /b 1
cmake --build installer\BangkeInstaller\build --config Release
if errorlevel 1 exit /b 1
echo Installer UI: output\BangkeInstaller.exe (shares Qt DLLs with BangkeSettings)
exit /b 0
