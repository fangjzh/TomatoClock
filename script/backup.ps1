$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BackupDir = Join-Path $ProjectRoot "backup"

if (-not (Test-Path $BackupDir)) {
    New-Item -ItemType Directory -Path $BackupDir | Out-Null
}

$TimeTag = Get-Date -Format "yyyyMMdd-HHmmss"
$TempDir = Join-Path $BackupDir "TomatoClock-src-$TimeTag"
$ZipPath = "$TempDir.zip"

New-Item -ItemType Directory -Path $TempDir | Out-Null

Get-ChildItem -Path $ProjectRoot -Force |
    Where-Object { $_.Name -notin @("build", "backup", "dist") } |
    ForEach-Object {
        Copy-Item $_.FullName (Join-Path $TempDir $_.Name) -Recurse -Force
    }

Compress-Archive -Path (Join-Path $TempDir "*") -DestinationPath $ZipPath -Force
Remove-Item $TempDir -Recurse -Force

Write-Host "Backup created: $ZipPath"
