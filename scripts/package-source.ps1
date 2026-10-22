param(
    [string]$Ref = 'HEAD',
    [Parameter(Mandatory=$true)][string]$CoreRepository,
    [ValidatePattern('^Mlash-[A-Za-z0-9][A-Za-z0-9._-]*$')][string]$OutputName = 'Mlash-1.17.0-source'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$revision=(& git -C $projectRoot rev-parse --verify ($Ref+'^{commit}') | Out-String).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Mlash source revision is unavailable' }
$lockText=(& git -C $projectRoot show ($revision+':dependencies.lock.json') | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'Source revision has no dependency lock' }
$core=($lockText | ConvertFrom-Json).'mihomo-release'
$coreRevision=(& git -C $CoreRepository rev-parse --verify ($core.source_commit+'^{commit}') | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $coreRevision -ne $core.source_commit) { throw 'Pinned core source commit is unavailable' }
# Archive tracked commits only; never include a working directory, configuration or logs.
$appArchive=Join-Path $projectRoot "dist/$OutputName.zip"
$coreArchive=Join-Path $projectRoot "dist/$OutputName-mihomo.zip"
$recordPath=Join-Path $projectRoot "dist/$OutputName.json"
foreach ($path in @($appArchive,$coreArchive,$recordPath)) { if (Test-Path -LiteralPath $path) { throw 'Source output already exists; choose a new OutputName' } }
New-Item -ItemType Directory -Force (Join-Path $projectRoot 'dist') | Out-Null
& git -C $projectRoot -c core.autocrlf=false -c core.eol=lf archive --format=zip "--output=$appArchive" $revision
if ($LASTEXITCODE -ne 0) { throw 'Mlash source archive failed' }
& python "$PSScriptRoot/check_document_privacy.py" --archive $appArchive
if ($LASTEXITCODE -ne 0) { throw 'Mlash source documentation privacy check failed' }
& git -C $CoreRepository -c core.autocrlf=false -c core.eol=lf archive --format=zip "--output=$coreArchive" $coreRevision
if ($LASTEXITCODE -ne 0) { throw 'Core source archive failed' }
$files=@($appArchive,$coreArchive | ForEach-Object {
    $hash=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath ($_+'.sha256') -Encoding utf8 -Value ($hash+'  '+[IO.Path]::GetFileName($_))
    [ordered]@{file=[IO.Path]::GetFileName($_);sha256=$hash}
})
[ordered]@{schema=1;mlash_commit=$revision;core_commit=$coreRevision;core_version=$core.version;core_executable_sha256=$core.executable_sha256;
    complete_distribution_sources=$false;remaining='Third-party corresponding sources and distribution notices require release review';files=$files} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $recordPath -Encoding utf8
Write-Output "Source archives and provenance: $recordPath"
