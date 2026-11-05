@echo off
cd /d "%~dp0"
if exist "scapms.exe" (
    start "" "scapms.exe"
) else if exist "dist\SCAPMS-v3\scapms.exe" (
    start "" "dist\SCAPMS-v3\scapms.exe"
) else (
    echo Build the application first: powershell -File scripts\build.ps1 -Test -Package
    pause
)
