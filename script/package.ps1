param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$DistDir = Join-Path $ProjectRoot "dist"

Write-Host "开始构建: $BuildType"
& (Join-Path $PSScriptRoot "build.ps1") -BuildType $BuildType
Write-Host "构建完成，开始打包。"

function Resolve-WindeployqtPath {
    if ($env:WINDEPLOYQT_PATH -and (Test-Path $env:WINDEPLOYQT_PATH)) {
        return $env:WINDEPLOYQT_PATH
    }

    $cmd = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $possiblePaths = @(
        "C:/Qt/6.10.1/mingw_64/bin/windeployqt.exe",
        "C:/Qt/6.10.1/msvc2022_64/bin/windeployqt.exe",
        "C:/Qt/6.8.0/mingw_64/bin/windeployqt.exe",
        "C:/Qt/6.8.0/msvc2022_64/bin/windeployqt.exe"
    )

    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            return $path
        }
    }

    return $null
}

$ExeCandidates = @(
    (Join-Path (Join-Path $BuildDir $BuildType) "TomatoClock.exe"),
    (Join-Path $BuildDir "TomatoClock.exe")
)

$ExePath = $ExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $ExePath) {
    throw "未找到 TomatoClock.exe（构建后），请检查构建输出目录。"
}

if (-not (Test-Path $DistDir)) {
    New-Item -ItemType Directory -Path $DistDir | Out-Null
}

$PackageName = "TomatoClock-$BuildType-$(Get-Date -Format 'yyyyMMdd-HHmmss').zip"
$PackagePath = Join-Path $DistDir $PackageName

$tempDir = Join-Path $DistDir "_package_temp"
if (Test-Path $tempDir) {
    Remove-Item $tempDir -Recurse -Force
}
New-Item -ItemType Directory -Path $tempDir | Out-Null

Copy-Item $ExePath (Join-Path $tempDir "TomatoClock.exe")

$projectRootFiles = @(
    "LICENSE",
    "NOTICE",
    "COPYRIGHT"
)

foreach ($fileName in $projectRootFiles) {
    $sourcePath = Join-Path $ProjectRoot $fileName
    if (Test-Path $sourcePath) {
        Copy-Item $sourcePath (Join-Path $tempDir $fileName) -Force
    }
}

$licensesDir = Join-Path $ProjectRoot "licenses"
if (Test-Path $licensesDir) {
    Copy-Item $licensesDir (Join-Path $tempDir "licenses") -Recurse -Force
}

$windeployqtPath = Resolve-WindeployqtPath
if (-not $windeployqtPath) {
    throw "未找到 windeployqt.exe，请安装 Qt 并将其加入 PATH，或设置环境变量 WINDEPLOYQT_PATH。"
}

Write-Host "Using windeployqt: $windeployqtPath"
& $windeployqtPath --compiler-runtime --dir $tempDir (Join-Path $tempDir "TomatoClock.exe")

if (Test-Path (Join-Path $ProjectRoot "doc")) {
    $docOutputDir = Join-Path $tempDir "doc"
    New-Item -ItemType Directory -Path $docOutputDir -Force | Out-Null

    $userDocs = @(
        "RUN.md",
        "USER_MANUAL.md"
    )

    foreach ($docFile in $userDocs) {
        $sourceDoc = Join-Path (Join-Path $ProjectRoot "doc") $docFile
        if (Test-Path $sourceDoc) {
            Copy-Item $sourceDoc (Join-Path $docOutputDir $docFile) -Force
        }
    }
}

if (Test-Path (Join-Path $ProjectRoot "resource")) {
    Copy-Item (Join-Path $ProjectRoot "resource") (Join-Path $tempDir "resource") -Recurse
}

Compress-Archive -Path (Join-Path $tempDir "*") -DestinationPath $PackagePath -Force
Remove-Item $tempDir -Recurse -Force

Write-Host "Package created: $PackagePath"
