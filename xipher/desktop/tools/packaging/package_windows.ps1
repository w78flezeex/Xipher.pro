param(
    [string]$BuildDir = "xipher/desktop/out/build/ci",
    [string]$OutDir = "xipher/desktop/out/artifacts/windows"
)

$ErrorActionPreference = "Stop"

$workspace = Resolve-Path -Path "$PSScriptRoot\..\..\.."
$buildPath = Join-Path $workspace $BuildDir
$outPath = Join-Path $workspace $OutDir
$staging = Join-Path $outPath "xipher_desktop"

New-Item -ItemType Directory -Force -Path $staging | Out-Null

$exe = Get-ChildItem -Path $buildPath -Recurse -Filter "xipher_desktop.exe" | Select-Object -First 1
if (-not $exe) {
    throw "xipher_desktop.exe not found in $buildPath"
}

Copy-Item $exe.FullName $staging -Force

$qtDeploy = Get-Command windeployqt -ErrorAction SilentlyContinue
if ($qtDeploy) {
    & windeployqt $exe.FullName --qmldir (Join-Path $workspace "xipher/desktop/qml") --dir $staging
}

$zipPath = Join-Path $outPath "xipher_desktop_windows.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $zipPath

Write-Host "Artifact: $zipPath"
