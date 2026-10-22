param(
    [Parameter(Mandatory=$true)][string]$SourceDirectory,
    [string]$DataDirectory = (Join-Path $env:LOCALAPPDATA 'Mlash')
)
$ErrorActionPreference = 'Stop'
# Explicit local data import; no proxy changes or network requests.
$sourceRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path
$targetRoot = Join-Path $DataDirectory 'geodata'
New-Item -ItemType Directory -Force $targetRoot | Out-Null
$records = @()
foreach ($name in @('Country.mmdb','geoip.metadb','GeoIP.dat','GeoSite.dat','ASN.mmdb')) {
    $sourceFile = Join-Path $sourceRoot $name
    if (!(Test-Path -LiteralPath $sourceFile -PathType Leaf)) { continue }
    $item = Get-Item -LiteralPath $sourceFile
    if ($item.LinkType -or $item.Length -le 0 -or $item.Length -gt 128MB) { throw "Invalid Geo asset: $name" }
    $targetFile = Join-Path $targetRoot $name
    Copy-Item -LiteralPath $sourceFile -Destination $targetFile -Force
    $records += [ordered]@{name=$name; bytes=$item.Length; sha256=(Get-FileHash -LiteralPath $targetFile).Hash.ToLowerInvariant()}
}
if ($records.Count -eq 0) { throw 'No supported Geo assets found' }
[ordered]@{origin='supplied-local'; source_directory=$sourceRoot; imported_at=[DateTime]::UtcNow.ToString('o'); assets=$records} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $targetRoot 'manifest.json') -Encoding utf8
Write-Output "Imported $($records.Count) local Geo assets. Mihomo validates each used asset during configuration validation."
