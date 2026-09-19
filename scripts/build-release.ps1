[CmdletBinding()]
param(
    [string]$BuildDirectory = "build/windows-msvc-x86",
    [string]$OutputDirectory = "dist/LibreCreatures",
    [string]$ArchivePath = "dist/LibreCreatures-windows-x86.zip"
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param([string]$Command, [string[]]$Arguments)
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $Command $($Arguments -join ' ')"
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repoRoot

if (Test-Path $OutputDirectory) {
    Remove-Item $OutputDirectory -Recurse -Force
}
$archiveDirectory = Split-Path -Parent $ArchivePath
if ($archiveDirectory -and (Test-Path $archiveDirectory)) {
    Remove-Item $ArchivePath -Force -ErrorAction SilentlyContinue
}

Invoke-Checked "cmake" @(
    "-S", ".",
    "-B", $BuildDirectory,
    "-G", "Visual Studio 17 2022",
    "-A", "Win32"
)
Invoke-Checked "cmake" @(
    "--build", $BuildDirectory,
    "--config", "Release",
    "--parallel"
)
Invoke-Checked "cmake" @(
    "--install", $BuildDirectory,
    "--config", "Release",
    "--prefix", $OutputDirectory
)

$commit = (& git rev-parse HEAD 2>$null).Trim()
if ([string]::IsNullOrWhiteSpace($commit)) {
    $commit = "working tree"
}
@"
LibreCreatures corresponding source

This package was built from:
  https://github.com/h4rm0n1c/LibreCreatures
  commit: $commit

The repository contains the source corresponding to this executable and is
licensed under GPLv3. The original Creatures game data is not part of this
package or repository.
"@ | Set-Content -Path (Join-Path $OutputDirectory "SOURCE-CODE.txt") -Encoding utf8

if ($archiveDirectory -and -not (Test-Path $archiveDirectory)) {
    New-Item -ItemType Directory -Path $archiveDirectory | Out-Null
}
Compress-Archive -Path (Join-Path $OutputDirectory "*") -DestinationPath $ArchivePath -Force
Write-Host "Created $ArchivePath"
