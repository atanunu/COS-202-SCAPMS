param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & $Python -m pip install --target .tools/python aqtinstall==3.3.0 cmake ninja
    if ($LASTEXITCODE -ne 0) { throw 'Python dependency installation failed.' }
    $env:PYTHONPATH = Join-Path $projectRoot '.tools/python'
    & $Python -m aqt install-qt windows desktop 6.8.3 win64_mingw -O .tools/Qt --archives qtbase
    if ($LASTEXITCODE -ne 0) { throw 'Qt installation failed.' }
    & $Python -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O .tools/Qt
    if ($LASTEXITCODE -ne 0) { throw 'Compiler installation failed.' }
} finally { Pop-Location }
