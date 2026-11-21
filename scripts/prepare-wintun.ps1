param([string]$Destination = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$cache = Join-Path $root 'build/_deps/wintun-sdk-review'
$archive = Join-Path $cache 'wintun-0.14.1.zip'
$archiveHash = '07C256185D6EE3652E09FA55C0B673E2624B565E02C4B9091C79CA7D2F24EF51'
$dllHash = 'E5DA8447DC2C320EDC0FC52FA01885C103DE8C118481F683643CACC3220DAFCE'
New-Item -ItemType Directory -Force -Path $cache | Out-Null
if (!(Test-Path -LiteralPath $archive) -or (Get-FileHash -LiteralPath $archive).Hash -ne $archiveHash) {
    $partial = Join-Path $cache 'wintun.download'
    Invoke-WebRequest -Uri 'https://www.wintun.net/builds/wintun-0.14.1.zip' -TimeoutSec 90 -OutFile $partial
    if ((Get-FileHash -LiteralPath $partial).Hash -ne $archiveHash) { throw 'Wintun SDK archive hash mismatch' }
    Move-Item -LiteralPath $partial -Destination $archive -Force
}
Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $cache 'extract') -Force
$dll = Join-Path $cache 'extract/wintun/bin/amd64/wintun.dll'
if ((Get-FileHash -LiteralPath $dll).Hash -ne $dllHash -or
    (Get-AuthenticodeSignature -LiteralPath $dll).Status -ne 'Valid') {
    throw 'Wintun x64 payload hash or signature mismatch'
}
if ($Destination) {
    Copy-Item -LiteralPath $dll -Destination (Join-Path $Destination 'wintun.dll') -Force
    New-Item -ItemType Directory -Force -Path (Join-Path $Destination 'licenses') | Out-Null
    $licenseSource = Join-Path $cache 'extract/wintun/LICENSE.txt'
    Copy-Item -LiteralPath $licenseSource -Destination (Join-Path $Destination 'licenses/Wintun.txt') -Force
}
