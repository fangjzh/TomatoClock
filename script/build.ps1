param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Release",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item $BuildDir -Recurse -Force
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

function Invoke-CMakeConfigure {
    cmake -S $ProjectRoot -B $BuildDir "-DCMAKE_BUILD_TYPE:STRING=$BuildType"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake 配置失败，退出码: $LASTEXITCODE"
    }
}

Invoke-CMakeConfigure

$CacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $CacheFile) {
    $CacheLine = Select-String -Path $CacheFile -Pattern '^CMAKE_BUILD_TYPE:STRING=' | Select-Object -First 1
    if ($CacheLine) {
        $ActualBuildType = $CacheLine.Line.Substring("CMAKE_BUILD_TYPE:STRING=".Length)
        if ($ActualBuildType -ne $BuildType) {
            Write-Warning "检测到构建类型缓存异常（当前: '$ActualBuildType'，期望: '$BuildType'），正在自动重建缓存。"
            Remove-Item $CacheFile -Force -ErrorAction SilentlyContinue
            $CMakeFilesDir = Join-Path $BuildDir "CMakeFiles"
            if (Test-Path $CMakeFilesDir) {
                Remove-Item $CMakeFilesDir -Recurse -Force
            }
            Invoke-CMakeConfigure
        }
    }
}

cmake --build $BuildDir --config $BuildType
if ($LASTEXITCODE -ne 0) {
    throw "CMake 构建失败，退出码: $LASTEXITCODE"
}

Write-Host "Build completed: $BuildType"
