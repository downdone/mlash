param([string]$DataDirectory = '', [switch]$SmokeTest)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!(Test-Path "$env:MLASH_MINGW/bin/g++.exe") -or !(Test-Path "$env:MLASH_QT/bin/Qt6Core.dll")) { throw 'Set MLASH_MINGW and MLASH_QT to the matching development kit directories' }
$env:PATH = "$env:MLASH_MINGW\bin;$env:MLASH_QT\bin;$env:PATH"
$programPath = Join-Path $projectRoot 'build/windows-mingw/bin/Mlash.exe'
if (!(Test-Path $programPath)) { throw 'Build Mlash first' }
$arguments = @()
if ($DataDirectory) { $arguments += @('--data-dir', ('"' + $DataDirectory + '"')) }
if ($SmokeTest) { $arguments += '--smoke-test' }
if ($arguments.Count -gt 0) { Start-Process -FilePath $programPath -ArgumentList $arguments -WindowStyle Hidden }
else { Start-Process -FilePath $programPath -WindowStyle Hidden }
