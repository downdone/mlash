param([string]$PackageDirectory, [string]$EvidenceDirectory)
$ErrorActionPreference='Stop'
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
if ((Test-Path -LiteralPath $EvidenceDirectory) -and @(Get-ChildItem -LiteralPath $EvidenceDirectory -Force).Count -gt 0) {
    throw 'Smoke evidence directory must be empty to prevent stale screenshots from passing'
}
New-Item -ItemType Directory -Force $EvidenceDirectory | Out-Null
$evidence=(Resolve-Path -LiteralPath $EvidenceDirectory).Path
$savedPath=$env:PATH; $savedPlugin=$env:QT_PLUGIN_PATH; $savedPlatform=$env:QT_QPA_PLATFORM
try {
    $env:PATH="$env:SystemRoot/System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH=$null; $env:QT_QPA_PLATFORM=$null
    & "$package/Mlash.exe" --version | Set-Content "$evidence/version.txt" -Encoding utf8
    if ($LASTEXITCODE -ne 0) { throw 'Packaged version command failed' }
    $process=Start-Process -FilePath "$package/Mlash.exe" -ArgumentList @('--smoke-test','--data-dir',('"'+$evidence+'/data"'),'--runtime-report',('"'+$evidence+'/runtime.json"'),'--screenshots-dir',('"'+$evidence+'/screenshots"')) -PassThru -WindowStyle Hidden
    if (!$process.WaitForExit(30000)) {
        if ((Get-Process -Id $process.Id).Path -eq "$package\Mlash.exe") { Stop-Process -Id $process.Id }
        throw 'Isolated package smoke timed out'
    }
    if ($process.ExitCode -ne 0) { throw 'Isolated package smoke failed' }
    $runtime=Get-Content "$evidence/runtime.json" -Raw | ConvertFrom-Json
    if (!$runtime.tls_available -or $runtime.tls_backend -ne 'schannel') { throw 'Packaged TLS unavailable' }
    if (@(Get-ChildItem "$evidence/screenshots/page-*.png").Count -ne 8) { throw 'Expected all eight page screenshots' }
    $routes=@(Get-Content "$evidence/screenshots/page-routes.json" -Raw | ConvertFrom-Json)
    $expectedViews=@(0,1,0,2,3,3,4,3)
    $expectedTabs=@(-1,-1,-1,-1,1,3,-1,2)
    if ($routes.Count -ne 8) { throw 'Expected eight verified screenshot routes' }
    for ($i=0; $i -lt 8; $i++) {
        if ($routes[$i].page -ne $i -or $routes[$i].view -ne $expectedViews[$i] -or $routes[$i].tab -ne $expectedTabs[$i]) {
            throw "Screenshot page-$i route mismatch"
        }
    }
    foreach ($name in @('settings-search.png','settings-return.png')) {
        if (!(Test-Path -LiteralPath "$evidence/screenshots/$name")) { throw "Missing packaged settings interaction screenshot: $name" }
    }
} finally { $env:PATH=$savedPath; $env:QT_PLUGIN_PATH=$savedPlugin; $env:QT_QPA_PLATFORM=$savedPlatform }
