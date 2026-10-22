param([string]$Preset = 'windows-mingw')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    if ($Preset -like 'windows-mingw*' -and !(Test-Path "$env:MLASH_MINGW/bin/g++.exe")) { throw 'Set MLASH_MINGW to the matching MinGW toolchain directory' }
    if ($Preset -eq 'windows-mingw' -and !(Test-Path "$env:MLASH_QT/bin/qmake.exe")) { throw 'Set MLASH_QT to the matching Qt kit directory' }
    if ($Preset -eq 'windows-mingw') { $env:PATH = "$env:MLASH_MINGW\bin;$env:MLASH_QT\bin;$env:PATH" }
    if ($Preset -eq 'windows-mingw-open') { $env:PATH="$env:MLASH_MINGW\bin;$projectRoot/build/_deps/qt-open/bin;$env:PATH"; $env:QT_PLUGIN_PATH="$projectRoot/build/_deps/qt-open/plugins" }
    if ($Preset -eq 'windows-ci') { if (!(Test-Path "$env:MLASH_MINGW/bin/g++.exe")) { throw 'Run CI toolchain preparation first' }; $env:PATH="$env:MLASH_MINGW/bin;$projectRoot/build/_deps/qt-open/bin;$env:PATH"; $env:QT_PLUGIN_PATH="$projectRoot/build/_deps/qt-open/plugins" }
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
    & cmake --build --preset $Preset --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
} finally { Pop-Location }
