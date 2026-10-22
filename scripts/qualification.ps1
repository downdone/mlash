param(
    [string]$Program = '',
    [ValidateRange(1,604800)][int]$Seconds = 600,
    [switch]$RetainGui,
    [switch]$Background
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if (!$Program) { $Program=Join-Path $projectRoot 'dist/Mlash-1.17.0-windows-x64-portable/Mlash.exe' }
$Program=(Resolve-Path -LiteralPath $Program).Path
$runRoot=Join-Path $projectRoot ('build/qualification/'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runRoot | Out-Null
$originalProgram=$Program
$packageDirectory=Split-Path $Program -Parent
$manifestPath=Join-Path $packageDirectory 'package-manifest.json'
if (!(Test-Path -LiteralPath $manifestPath)) { throw 'Qualification requires a portable package manifest' }
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.executable_sha256 -ne (Get-FileHash -LiteralPath $Program).Hash.ToLowerInvariant()) { throw 'Package EXE differs from manifest' }
$sut=Join-Path $runRoot 'sut'; New-Item -ItemType Directory -Path $sut | Out-Null
$packagePrefix=[IO.Path]::GetFullPath($packageDirectory)+[IO.Path]::DirectorySeparatorChar
foreach ($file in $manifest.files) {
    $sourcePath=[IO.Path]::GetFullPath((Join-Path $packageDirectory $file.path))
    if (!$sourcePath.StartsWith($packagePrefix,[StringComparison]::OrdinalIgnoreCase) -or ((Get-Item -LiteralPath $sourcePath).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Unsafe qualification package entry' }
    $destination=Join-Path $sut $file.path
    New-Item -ItemType Directory -Force (Split-Path $destination -Parent) | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destination
    if ((Get-FileHash -LiteralPath $destination).Hash.ToLowerInvariant() -ne $file.sha256) { throw 'Qualification snapshot checksum mismatch' }
}
Copy-Item -LiteralPath $manifestPath -Destination $sut
$Program=Join-Path $sut 'Mlash.exe'
$report=Join-Path $runRoot 'report.json'
$arguments=@('--qualification-seconds',"$Seconds",'--data-dir',('"'+(Join-Path $runRoot 'data')+'"'),'--qualification-report',('"'+$report+'"'))
if ($RetainGui) { $arguments += '--qualification-gui' }
$savedPath=$env:PATH
try {
    $env:PATH="$env:SystemRoot\System32;$env:SystemRoot"
    $process=Start-Process -FilePath $Program -ArgumentList $arguments -PassThru -WindowStyle Hidden
} finally { $env:PATH=$savedPath }
$record=[ordered]@{ pid=$process.Id; program=$Program; original_program=$originalProgram; runtime_fingerprint=$manifest.runtime_fingerprint; executable_sha256=(Get-FileHash -LiteralPath $Program).Hash.ToLowerInvariant();
    report=$report; started_utc=[DateTime]::UtcNow.ToString('o'); requested_seconds=$Seconds; background=[bool]$Background; retain_gui=[bool]$RetainGui }
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runRoot 'launch.json') -Encoding utf8
Write-Output "Isolated qualification PID=$($process.Id), report=$report"
if ($Background) { return }
while (!$process.WaitForExit(30000)) { Write-Output "Qualification still running: $report" }
if ($process.ExitCode -ne 0) { throw "Qualification failed: exit $($process.ExitCode), inspect $report" }
$result=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if ($result.status -ne 'completed') { throw "Qualification incomplete: $($result.failure)" }
python "$PSScriptRoot/qualification_summary.py" $report --output (Join-Path $runRoot 'summary.json')
if ($LASTEXITCODE -ne 0) { throw 'Qualification summary failed' }
