param([switch]$SkipCore, [string]$CoreFile = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$dependencyLock = Get-Content (Join-Path $projectRoot 'dependencies.lock.json') -Raw | ConvertFrom-Json
if (!$SkipCore) {
    $coreDirectory = Join-Path $projectRoot 'cores'
    New-Item -ItemType Directory -Force $coreDirectory | Out-Null
    $pinnedRelease = $dependencyLock.'mihomo-release'
    $usePinnedRelease = !$CoreFile -and $null -ne $pinnedRelease
    if ($usePinnedRelease) {
        $CoreFile = Join-Path $coreDirectory 'mihomo.exe'
        if (!(Test-Path -LiteralPath $CoreFile -PathType Leaf)) {
            $CoreFile = Join-Path $coreDirectory 'mihomo.exe'
            if (!(Test-Path -LiteralPath $CoreFile -PathType Leaf)) { throw 'Pinned local release core unavailable; supply the locked artifact before bootstrap' }
        }
        if ((Get-FileHash -LiteralPath $CoreFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pinnedRelease.executable_sha256) { throw 'Pinned local release core SHA256 mismatch; existing core retained' }
    }
    if ($CoreFile) {
        $sourceFile = (Resolve-Path -LiteralPath $CoreFile).Path
        if (!(Test-Path -LiteralPath $sourceFile -PathType Leaf)) { throw 'Supplied core is not a file' }
        $coreVersion = & $sourceFile -v
        if ($LASTEXITCODE -ne 0 -or "$coreVersion" -notmatch '1\.19\.32') { throw 'Supplied core must report v1.19.32' }
        if ($usePinnedRelease -and "$coreVersion" -notmatch [regex]::Escape($pinnedRelease.version)) { throw 'Pinned release core version mismatch; existing core retained' }
        $targetFile = Join-Path $coreDirectory 'mihomo.exe'
        if ([IO.Path]::GetFullPath($sourceFile) -ne [IO.Path]::GetFullPath($targetFile)) { Copy-Item -LiteralPath $sourceFile -Destination $targetFile -Force }
        $coreOrigin = if ($usePinnedRelease) { $pinnedRelease.origin } else { 'supplied-local' }
        $version = if ($usePinnedRelease) { $pinnedRelease.version } else { $dependencyLock.mihomo.version }
        [ordered]@{ version=$version; origin=$coreOrigin; source=$(if ($usePinnedRelease) { $pinnedRelease.asset_url } else { 'supplied-artifact' });
            source_commit=$(if ($usePinnedRelease) { $pinnedRelease.source_commit } else { '' });
            executable_sha256=(Get-FileHash (Join-Path $coreDirectory 'mihomo.exe') -Algorithm SHA256).Hash.ToLowerInvariant();
            reported_version=@($coreVersion) } | ConvertTo-Json | Set-Content (Join-Path $coreDirectory 'manifest.json') -Encoding utf8
        Write-Output $coreVersion
        return
    }
    $archivePath = Join-Path $coreDirectory $dependencyLock.mihomo.asset
    if (!(Test-Path $archivePath)) {
        $partialPath = $archivePath + '.download'
        & curl.exe --location --fail --show-error --retry 1 --connect-timeout 15 --max-time 240 $dependencyLock.mihomo.url -o $partialPath
        if ($LASTEXITCODE -ne 0) {
            $metadataPath = Join-Path $coreDirectory 'release-metadata.json'
            & curl.exe --location --fail --silent --show-error --connect-timeout 15 --max-time 30 'https://api.github.com/repos/MetaCubeX/mihomo/releases/tags/v1.19.32' -o $metadataPath
            if ($LASTEXITCODE -ne 0) { throw 'Official release metadata unavailable' }
            $metadata = Get-Content $metadataPath -Raw | ConvertFrom-Json
            $asset = @($metadata.assets | Where-Object name -eq $dependencyLock.mihomo.asset)
            if ($asset.Count -ne 1 -or $asset[0].url -notmatch '^https://api.github.com/repos/MetaCubeX/mihomo/releases/assets/[0-9]+$') { throw 'Unexpected official asset metadata' }
            $apiUrl = $asset[0].url + '?download=' + [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
            & curl.exe --location --fail --silent --show-error --connect-timeout 15 --max-time 360 -H 'Accept: application/octet-stream' $apiUrl -o $partialPath
            if ($LASTEXITCODE -ne 0) { throw 'Official core download failed; existing core retained' }
        }
        if ((Get-FileHash -LiteralPath $partialPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $dependencyLock.mihomo.sha256) { throw 'Downloaded core SHA256 mismatch' }
        Move-Item -LiteralPath $partialPath -Destination $archivePath -Force
    }
    $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($archiveHash -ne $dependencyLock.mihomo.sha256) { throw 'Core archive SHA256 mismatch' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $coreArchive = [IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $executables = @($coreArchive.Entries | Where-Object { $_.Name.EndsWith('.exe') })
        if ($executables.Count -ne 1) { throw 'Expected one executable in official archive' }
        [IO.Compression.ZipFileExtensions]::ExtractToFile($executables[0], (Join-Path $coreDirectory 'mihomo.exe'), $true)
    } finally { $coreArchive.Dispose() }
    $coreVersion = & (Join-Path $coreDirectory 'mihomo.exe') -v
    if ($LASTEXITCODE -ne 0 -or "$coreVersion" -notmatch '1\.19\.32') { throw 'Unexpected core version' }
    [ordered]@{ version=$dependencyLock.mihomo.version; origin='official-sha256'; source=$dependencyLock.mihomo.url; archive_sha256=$archiveHash;
        executable_sha256=(Get-FileHash (Join-Path $coreDirectory 'mihomo.exe') -Algorithm SHA256).Hash.ToLowerInvariant();
        reported_version=@($coreVersion) } | ConvertTo-Json | Set-Content (Join-Path $coreDirectory 'manifest.json') -Encoding utf8
    Write-Output $coreVersion
}
