param([Parameter(Mandatory)][string]$PackageDirectory,[Parameter(Mandatory)][string]$SystemTests,[switch]$SystemProxyTest)
$ErrorActionPreference='Stop'
$machine=Get-CimInstance Win32_ComputerSystem
$descriptor="$($machine.Manufacturer) $($machine.Model)"
if (!$SystemProxyTest -or $descriptor -notmatch '(?i)VMware|VirtualBox|Virtual Machine|QEMU|KVM|Parallels') { throw 'Explicit -SystemProxyTest and controlled VM required; no OS writes performed' }
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$tests=(Resolve-Path -LiteralPath $SystemTests).Path
$resultRoot=Join-Path $env:TEMP ('Mlash-acceptance-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $resultRoot | Out-Null
$env:MLASH_TEST_APP=Join-Path $package 'Mlash.exe'; $env:MLASH_TEST_CORE=Join-Path $package 'cores/mihomo.exe'
$savedPath=$env:PATH
try {
    $env:PATH="$package;$env:SystemRoot\System32;$env:SystemRoot"
    & $tests --system-proxy-test -o (Join-Path $resultRoot 'system-tests.xml,xml')
    if ($LASTEXITCODE -ne 0) { throw "VM tests failed/skipped: $LASTEXITCODE; recovery artifacts must be preserved" }
    $report=Join-Path $resultRoot 'runtime.json'; $data=Join-Path $resultRoot 'smoke-data'
    $process=Start-Process -FilePath $env:MLASH_TEST_APP -ArgumentList '--smoke-test','--data-dir',('"'+$data+'"'),'--runtime-report',('"'+$report+'"') -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(15000) -or $process.ExitCode -ne 0) { throw 'VM portable smoke failed' }
    if (!(Get-Content $report -Raw | ConvertFrom-Json).tls_available) { throw 'VM TLS unavailable' }
    $os=Get-CimInstance Win32_OperatingSystem
    [ordered]@{ schema=1; result='pass'; controlled_vm=$true; os=$os.Caption; build=[int]$os.BuildNumber; windows_major=if ([int]$os.BuildNumber -ge 22000) {11} else {10};
        executable_sha256=(Get-FileHash $env:MLASH_TEST_APP).Hash.ToLowerInvariant(); core_sha256=(Get-FileHash $env:MLASH_TEST_CORE).Hash.ToLowerInvariant();
        system_test_sha256=(Get-FileHash $tests).Hash.ToLowerInvariant(); cases=@('A10','A11','A12','A13','A14','A20-Run-readback');
        clean_machine=$false; manual_pending=@('A19-Explorer','A20-login','A23-sleep','clean-Windows'); artifacts=@('system-tests.xml','runtime.json'); ended_utc=[DateTime]::UtcNow.ToString('o') } |
        ConvertTo-Json -Depth 5 | Set-Content (Join-Path $resultRoot 'vm-results.json') -Encoding utf8
    Write-Output "Controlled VM results: $resultRoot"
} finally { $env:PATH=$savedPath; Remove-Item Env:MLASH_TEST_APP,Env:MLASH_TEST_CORE -ErrorAction SilentlyContinue }
