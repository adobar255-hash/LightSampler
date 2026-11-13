@echo off
cd /d "%~dp0"

REM Burahin ang lumang build folder (may maling generator mula sa nakaraang subok)
if exist build rmdir /s /q build

echo [1/2] Cini-configure ang project (magda-download ng JUCE sa unang takbo)...
echo.
echo Sinusubukan ang Visual Studio 2026...
cmake -B build -G "Visual Studio 18 2026" -A x64 -DFETCHCONTENT_BASE_DIR="%cd%\deps"
if not errorlevel 1 goto build

if exist build rmdir /s /q build
echo.
echo Sinusubukan ang Visual Studio 2022...
cmake -B build -G "Visual Studio 17 2022" -A x64 -DFETCHCONTENT_BASE_DIR="%cd%\deps"
if not errorlevel 1 goto build

echo.
echo ======================================================
echo Walang nahanap na Visual Studio na may C++ compiler.
echo.
echo 1. I-install ang "Visual Studio Community" (libre) mula sa visualstudio.microsoft.com
echo    at i-check ang workload na "Desktop development with C++".
echo    Kung naka-install na, buksan ang Visual Studio Installer, i-click ang Modify,
echo    at i-check ang workload na iyon.
echo 2. I-update ang CMake sa pinakabagong bersyon (cmake.org/download).
echo 3. Patakbuhin ulit ang build.bat.
echo ======================================================
goto error

:build
echo.
echo [2/2] Kino-compile ang VST3 (Release)...
cmake --build build --config Release --target LightSampler_VST3
if errorlevel 1 goto error
echo.
echo TAPOS NA! Ang VST3 ay nasa:
echo %cd%\build\LightSampler_artefacts\Release\VST3\LightSampler.vst3
echo.
echo Kopyahin ang LightSampler.vst3 sa:
echo C:\Program Files\Common Files\VST3
pause
exit /b 0

:error
echo.
echo May error. Tingnan ang mensahe sa itaas.
pause
exit /b 1
