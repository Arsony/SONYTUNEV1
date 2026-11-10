@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
  echo Right-click this file and choose Run as administrator
  pause
  exit /b
)
for /d %%D in ("%~dp0build\*_artefacts\Release\VST3\*.vst3") do (
  xcopy /E /I /Y "%%D" "C:\Program Files\Common Files\VST3\%%~nxD"
)
echo.
echo Done. Now rescan plugins in FL Studio.
pause
