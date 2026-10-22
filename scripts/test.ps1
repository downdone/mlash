param([string]$Preset='windows-mingw', [string]$TestRegex='')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $ctestArgs = @('--preset', $Preset)
    if ($TestRegex) { $ctestArgs += @('--tests-regex', $TestRegex) }
    & ctest @ctestArgs
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { Pop-Location }
