param([int]$Samples = 5)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$programPath = Join-Path $projectRoot 'dist/Mlash-1.11.0-windows-x64/Mlash.exe'
if (!(Test-Path $programPath)) { throw 'Package Mlash first' }
$measurementRoot = Join-Path $projectRoot 'build/measurements'
New-Item -ItemType Directory -Force $measurementRoot | Out-Null
$savedPath = $env:PATH
$env:PATH = "$env:WINDIR\System32;$env:WINDIR"
$results = @()
try {
    for ($sampleIndex = 0; $sampleIndex -lt $Samples; $sampleIndex++) {
        $dataPath = Join-Path $measurementRoot "data-$sampleIndex"
        $reportPath = Join-Path $measurementRoot "runtime-$sampleIndex.json"
        $process = Start-Process -FilePath $programPath -ArgumentList '--smoke-test','--data-dir',('"' + $dataPath + '"'),'--runtime-report',('"' + $reportPath + '"') -PassThru -WindowStyle Hidden
        if ($process.WaitForExit(650)) { throw 'Process exited before sampling' }
        $process.Refresh()
        $privateBytes = $process.PrivateMemorySize64
        $workingSet = $process.WorkingSet64
        $modules = @($process.Modules | Where-Object { $_.ModuleName -like 'Qt6*' } | ForEach-Object FileName)
        if (!$process.WaitForExit(10000)) { $process.Kill(); throw 'Smoke exit timed out' }
        if ($process.ExitCode -ne 0) { throw 'Smoke process failed' }
        $runtime = Get-Content $reportPath -Raw | ConvertFrom-Json
        if (!$runtime.tls_available) { throw 'Portable TLS backend unavailable' }
        $results += [ordered]@{ sample=$sampleIndex; private_bytes=$privateBytes; working_set=$workingSet; runtime=$runtime; qt_modules=$modules }
    }
} finally { $env:PATH = $savedPath }
[ordered]@{ scope='Empty-profile GUI sampled at 650ms; no core; not a 10-minute idle or P95 budget qualification';
    executable_sha256=(Get-FileHash -LiteralPath $programPath -Algorithm SHA256).Hash.ToLowerInvariant(); samples=$results } |
    ConvertTo-Json -Depth 8 | Set-Content (Join-Path $measurementRoot 'results.json') -Encoding utf8
$results | ForEach-Object { [pscustomobject]@{Sample=$_.sample; PrivateMiB=[math]::Round($_.private_bytes/1MB,2); WorkingSetMiB=[math]::Round($_.working_set/1MB,2); ReadyMs=$_.runtime.ready_ms; TLS=$_.runtime.tls_backend} } | Format-Table
