param(
    [string]$Preset = 'windows-mingw',
    [ValidatePattern('^Mlash-[A-Za-z0-9][A-Za-z0-9._-]*$')][string]$OutputName = '',
    [switch]$Release,
    [string]$EvidenceManifest = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$OutputName) {
    $versionMatch = [regex]::Match((Get-Content (Join-Path $projectRoot 'CMakeLists.txt') -Raw), 'project\(Mlash VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
    if (!$versionMatch.Success) { throw 'CMake version missing' }
    $OutputName = "Mlash-$($versionMatch.Groups[1].Value)-windows-x64-portable"
}
if ($Release) {
    if (!$EvidenceManifest) { throw 'Formal release requires verified evidence manifest' }
    python "$PSScriptRoot/release_gate.py" $EvidenceManifest --app "$projectRoot/build/$Preset/bin/Mlash.exe" --core "$projectRoot/cores/mihomo.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Release qualification gate rejected incomplete/failed evidence' }
}
$stageDirectory = Join-Path $projectRoot "dist/$OutputName"
$buildDirectory = Join-Path $projectRoot "build/$Preset"
$payloadPlan = Join-Path $PSScriptRoot 'package-files.json'
$qtKit = if ($Preset -eq 'windows-mingw') { $env:MLASH_QT } elseif ($Preset -in @('windows-mingw-open','windows-ci')) { "$projectRoot/build/_deps/qt-open" } else { $env:MLASH_QT_MSVC }
if (!$qtKit -or !(Test-Path "$qtKit/bin/windeployqt.exe")) { throw 'Matching Qt kit required' }
if (!(Test-Path "$buildDirectory/bin/Mlash.exe")) { throw 'Build Mlash first' }
if (!(Test-Path "$projectRoot/cores/mihomo.exe") -or !(Test-Path "$projectRoot/cores/manifest.json")) { throw 'Verified/supplied core and manifest required; run bootstrap' }
$manifest = Get-Content "$projectRoot/cores/manifest.json" -Raw | ConvertFrom-Json
$actualHash = (Get-FileHash "$projectRoot/cores/mihomo.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualHash -ne $manifest.executable_sha256) { throw 'Core changed since manifest was written' }
$releaseCore = (Get-Content "$projectRoot/dependencies.lock.json" -Raw | ConvertFrom-Json).'mihomo-release'
if ($null -ne $releaseCore -and ($actualHash -ne $releaseCore.executable_sha256 -or $manifest.version -ne $releaseCore.version -or $manifest.origin -ne $releaseCore.origin -or $manifest.source_commit -ne $releaseCore.source_commit)) { throw 'Package core differs from pinned Mlash release core; run bootstrap' }
if (Test-Path $stageDirectory) {
    $resolvedStage = (Resolve-Path -LiteralPath $stageDirectory).Path
    $resolvedDist = [IO.Path]::GetFullPath((Join-Path $projectRoot 'dist')) + [IO.Path]::DirectorySeparatorChar
    if (!$resolvedStage.StartsWith($resolvedDist, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe staging path' }
    if ((Get-Item -LiteralPath $resolvedStage).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Staging directory cannot be a reparse point' }
    $running = @(Get-Process | Where-Object { $_.Path -and $_.Path.StartsWith($resolvedStage + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) })
    if ($running.Count) { throw 'Target package is running; exit it or use a new OutputName' }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
New-Item -ItemType Directory -Force $stageDirectory | Out-Null
Copy-Item -LiteralPath "$buildDirectory/bin/Mlash.exe" -Destination $stageDirectory
Copy-Item -LiteralPath "$buildDirectory/bin/MlashTunService.exe" -Destination $stageDirectory
if ($Preset -like 'windows-mingw*' -or $Preset -eq 'windows-ci') { $compilerBin=if ($Preset -eq 'windows-ci') { "$env:MLASH_MINGW/bin" } else { "$env:MLASH_MINGW/bin" }; $env:PATH = "$compilerBin;$qtKit/bin;$env:PATH" }
& "$qtKit/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw --skip-plugin-types iconengines,imageformats,generic "$stageDirectory/Mlash.exe"
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }
& python "$PSScriptRoot/package_payload.py" stage --root $stageDirectory --repo $projectRoot --plan $payloadPlan
if ($LASTEXITCODE -ne 0) { throw 'Portable payload allowlist rejected deployment' }
$cliVersion=(& "$stageDirectory/Mlash.exe" --version | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $cliVersion -notmatch '^Mlash ([0-9]+\.[0-9]+\.[0-9]+)$') { throw 'Packaged CLI version could not be verified' }
$expectedVersion=$Matches[1]
foreach ($name in @('Mlash.exe','MlashTunService.exe')) {
    $peVersion=(Get-Item -LiteralPath "$stageDirectory/$name").VersionInfo
    if ($peVersion.FileVersion -ne $expectedVersion -or $peVersion.ProductVersion -ne $expectedVersion) { throw "CLI/PE version mismatch: $name" }
}
$files=@(Get-ChildItem -LiteralPath $stageDirectory -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{path=[IO.Path]::GetRelativePath($stageDirectory,$_.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant();bytes=$_.Length}
})
$runtimeFiles=@($files | Where-Object { $_.path -match '\.(exe|dll)$|^qt.conf$' })
$fingerprintText=($runtimeFiles | ForEach-Object { "$($_.path)=$($_.sha256)" }) -join "`n"
$fingerprint=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($fingerprintText))).ToLowerInvariant()
if ($Release) { $evidence=Get-Content -LiteralPath $EvidenceManifest -Raw | ConvertFrom-Json; if ($evidence.runtime_fingerprint -ne $fingerprint) { throw 'Staged runtime differs from qualified candidate' } }
[ordered]@{schema=1;version=(Get-Item -LiteralPath "$stageDirectory/Mlash.exe").VersionInfo.ProductVersion;release_qualified=[bool]$Release;
    executable_sha256=(Get-FileHash -LiteralPath "$stageDirectory/Mlash.exe").Hash.ToLowerInvariant();core_sha256=$actualHash;runtime_fingerprint=$fingerprint;files=$files} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$stageDirectory/package-manifest.json" -Encoding utf8
& python "$PSScriptRoot/package_payload.py" verify --root $stageDirectory --plan $payloadPlan
if ($LASTEXITCODE -ne 0) { throw 'Portable payload integrity verification failed' }
$zipPath = Join-Path $projectRoot "dist/$OutputName.zip"
Compress-Archive -Path "$stageDirectory/*" -DestinationPath $zipPath -Force -CompressionLevel Optimal
$packageHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath ($zipPath + '.sha256') -Value ($packageHash + '  ' + [IO.Path]::GetFileName($zipPath)) -Encoding utf8
& "$projectRoot/src/platform/windows/RefreshAppIcon.ps1" -Path "$stageDirectory/Mlash.exe"
Get-FileHash -LiteralPath $zipPath -Algorithm SHA256 | Format-List
Write-Output "Portable release candidate: $stageDirectory"
