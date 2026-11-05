param([switch]$Test, [switch]$Package)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$qtRoot = Join-Path $projectRoot '.tools/Qt/6.8.3/mingw_64'
$compilerRoot = Join-Path $projectRoot '.tools/Qt/Tools/mingw1310_64/bin'
$cmake = Join-Path $projectRoot '.tools/python/cmake/data/bin/cmake.exe'
$ninja = Join-Path $projectRoot '.tools/python/bin/ninja.exe'
if (!(Test-Path $cmake) -or !(Test-Path $qtRoot) -or !(Test-Path $compilerRoot)) {
    throw 'Local toolchain missing. Run scripts/setup.ps1, or use Qt Creator with CMakeLists.txt.'
}
$env:QT_QPA_FONTDIR = "$env:WINDIR/Fonts"
$env:PATH = "$qtRoot/bin;$compilerRoot;$(Split-Path $ninja);$env:PATH"
& $cmake -S $projectRoot -B "$projectRoot/build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_PREFIX_PATH=$qtRoot" "-DCMAKE_CXX_COMPILER=$compilerRoot/g++.exe" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build "$projectRoot/build" --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
if ($Test) {
    & (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir "$projectRoot/build" --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
}
if ($Package) {
    $packageDir = Join-Path $projectRoot 'dist/SCAPMS-v3'
    New-Item -ItemType Directory -Force -Path $packageDir | Out-Null
    Copy-Item -LiteralPath "$projectRoot/build/scapms.exe" -Destination $packageDir -Force
    & "$qtRoot/bin/windeployqt.exe" --release --no-translations --no-opengl-sw --no-system-d3d-compiler "$packageDir/scapms.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
    Copy-Item -LiteralPath "$qtRoot/plugins/platforms/qoffscreen.dll" -Destination "$packageDir/platforms" -Force
    if (Test-Path "$projectRoot/licenses") { Copy-Item -LiteralPath "$projectRoot/licenses" -Destination $packageDir -Recurse -Force }
    Copy-Item -LiteralPath "$projectRoot/data/sample-clinic.json" -Destination $packageDir -Force
    Copy-Item -LiteralPath "$projectRoot/data/benchmark-2000.json" -Destination $packageDir -Force
    Copy-Item -LiteralPath "$projectRoot/README.md" -Destination $packageDir -Force
    Copy-Item -LiteralPath "$projectRoot/THIRD_PARTY_NOTICES.md" -Destination $packageDir -Force
    Copy-Item -LiteralPath "$projectRoot/Launch-SCAPMS.cmd" -Destination $packageDir -Force
}




