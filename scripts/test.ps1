param([string]$Preset='windows-mingw', [string]$TestRegex='', [ValidateSet('unit','integration','scenario','ui','system-gated')][string]$Layer='')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$savedFontDirectory=$env:QT_QPA_FONTDIR
$savedTemp=$env:TEMP
$savedTmp=$env:TMP
Push-Location $projectRoot
try {
    $testTemp=Join-Path $projectRoot 'build/test-temp'
    New-Item -ItemType Directory -Force -Path $testTemp | Out-Null
    $env:TEMP=$testTemp
    $env:TMP=$testTemp
    if (!$env:QT_QPA_FONTDIR -and $env:OS -eq 'Windows_NT') { $env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts' }
    $ctestArgs = @('--preset', $Preset, '--no-tests=error')
    if ($TestRegex) { $ctestArgs += @('--tests-regex', $TestRegex) }
    if ($Layer) { $ctestArgs += @('--label-regex', "^$([regex]::Escape($Layer))$") }
    & ctest @ctestArgs
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { $env:QT_QPA_FONTDIR=$savedFontDirectory; $env:TEMP=$savedTemp; $env:TMP=$savedTmp; Pop-Location }
