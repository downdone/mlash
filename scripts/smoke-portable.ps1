#!/usr/bin/env pwsh
# 便携包隔离冒烟：模拟"最终用户只解压 ZIP 后双击"的情形。
#
# 关键点：必须清掉 PATH 里的 Qt/MinGW，否则插件会从开发环境加载，
# 得到"本机看起来能跑、用户机器上崩溃"的假绿。
param(
    [Parameter(Mandatory = $true)][string]$Zip,
    [string]$EvidenceDirectory = ''
)

$ErrorActionPreference = 'Stop'
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("mlash-portable-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $work -Force | Out-Null

try {
    Expand-Archive -Path $Zip -DestinationPath $work -Force

    $exe = Join-Path $work 'Mlash.exe'
    if (-not (Test-Path $exe)) { throw "包内缺少 Mlash.exe" }
    if (-not (Test-Path (Join-Path $work 'platforms/qwindows.dll'))) {
        throw "包内缺少 platforms/qwindows.dll：平台插件目录未随包分发"
    }

    $savedPath = $env:PATH
    $env:PATH = "$env:SystemRoot\system32;$env:SystemRoot"
    try {
        $data = Join-Path $work 'data'
        $shots = Join-Path $work 'shots'
        $process = Start-Process -FilePath $exe `
            -ArgumentList '--smoke-test', '--data-dir', $data, '--screenshots-dir', $shots `
            -WorkingDirectory $work -PassThru -Wait
    }
    finally {
        $env:PATH = $savedPath
    }

    $routes = Join-Path $shots 'page-routes.json'
    $count = if (Test-Path $routes) { (Get-Content $routes -Raw | ConvertFrom-Json).Count } else { 0 }

    if ($process.ExitCode -ne 0) { throw "包内冒烟退出码 $($process.ExitCode)（预期 0）" }
    if ($count -ne 8) { throw "路由清单 $count 条（预期 8）" }

    if ($EvidenceDirectory) {
        New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
        Copy-Item $routes (Join-Path $EvidenceDirectory 'page-routes.json') -Force
        Get-ChildItem $shots -Filter '*.png' | Copy-Item -Destination $EvidenceDirectory -Force
        $hash = (Get-FileHash -Path $Zip -Algorithm SHA256).Hash
        "zip sha256=$hash" | Set-Content (Join-Path $EvidenceDirectory 'package.txt')
    }

    "Portable smoke: OK (exit=0, routes=$count)"
}
finally {
    Remove-Item -Path $work -Recurse -Force -ErrorAction SilentlyContinue
}