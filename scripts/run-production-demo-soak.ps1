#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [ValidateRange(1, 100)][int]$Runs = 10,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
    if (-not $SkipBuild) {
        pio run -e emulator_pc
        if ($LASTEXITCODE -ne 0) { throw 'emulator_pc build failed.' }
    }
    python scripts/production_demo_soak.py --runs $Runs
    if ($LASTEXITCODE -ne 0) { throw 'Production demo soak failed.' }
} finally {
    Pop-Location
}
