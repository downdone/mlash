param([string]$Preset='windows-mingw', [string]$TestRegex='')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$savedFontDirectory=$env:QT_QPA_FONTDIR
Push-Location $projectRoot
try {
    if (!$env:QT_QPA_FONTDIR -and $env:OS -eq 'Windows_NT') { $env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts' }
    $ctestArgs = @('--preset', $Preset)
    if ($TestRegex) { $ctestArgs += @('--tests-regex', $TestRegex) }
    & ctest @ctestArgs
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { $env:QT_QPA_FONTDIR=$savedFontDirectory; Pop-Location }
