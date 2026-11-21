param([string]$Preset='windows-mingw', [string]$TestRegex='', [ValidateSet('unit','integration','scenario','ui','system-gated')][string]$Layer='', [switch]$WireGuardSdk, [switch]$WithMihomo)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$savedFontDirectory=$env:QT_QPA_FONTDIR
$savedTemp=$env:TEMP
$savedTmp=$env:TMP
$savedSdkCore=$env:MLASH_WG_SDK_MIHOMO
Push-Location $projectRoot
try {
    $testTemp=Join-Path $projectRoot 'build/test-temp'
    New-Item -ItemType Directory -Force -Path $testTemp | Out-Null
    $env:TEMP=$testTemp
    $env:TMP=$testTemp
    if ($WithMihomo -and !$WireGuardSdk) { throw 'WithMihomo requires WireGuardSdk' }
    if ($WireGuardSdk) {
        $env:MLASH_WG_SDK_MIHOMO=$null
        if ($WithMihomo) {
            $core=Join-Path $projectRoot 'cores/mihomo.exe'
            $lock=Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'cores/manifest.json') | ConvertFrom-Json
            if ((Get-FileHash -LiteralPath $core -Algorithm SHA256).Hash -ine $lock.executable_sha256) {
                throw 'The selected Mihomo executable does not match cores/manifest.json'
            }
            $env:MLASH_WG_SDK_MIHOMO=$core
        }
        & go -C (Join-Path $projectRoot 'src/core/wireguard') test -mod=readonly -count=1 -timeout 45s -v ./...
        if ($LASTEXITCODE -ne 0) { throw 'WireGuard SDK tests failed' }
        return
    }
    if (!$env:QT_QPA_FONTDIR -and $env:OS -eq 'Windows_NT') { $env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts' }
    $ctestArgs = @('--preset', $Preset, '--no-tests=error')
    if ($TestRegex) { $ctestArgs += @('--tests-regex', $TestRegex) }
    if ($Layer) { $ctestArgs += @('--label-regex', "^$([regex]::Escape($Layer))$") }
    & ctest @ctestArgs
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { $env:QT_QPA_FONTDIR=$savedFontDirectory; $env:TEMP=$savedTemp; $env:TMP=$savedTmp; $env:MLASH_WG_SDK_MIHOMO=$savedSdkCore; Pop-Location }
