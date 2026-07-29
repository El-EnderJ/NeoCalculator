#!/usr/bin/env pwsh
[CmdletBinding()]
param([switch]$SkipBuild)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
    python tests/host/run_production_demo_tests.py
    if ($LASTEXITCODE -ne 0) { throw 'Production demo host suite failed.' }
    python tests/host/run_production_keypad_tests.py
    if ($LASTEXITCODE -ne 0) { throw 'Production keypad suite failed.' }
    python tests/host/run_production_display_tests.py
    if ($LASTEXITCODE -ne 0) { throw 'Production display suite failed.' }
    python scripts/check-production-target.py
    if ($LASTEXITCODE -ne 0) { throw 'Production target validation failed.' }

    if (-not $SkipBuild) {
        pio run -e emulator_pc
        if ($LASTEXITCODE -ne 0) { throw 'emulator_pc build failed.' }
    }
    & scripts/run-emulator-windows.ps1 `
        --headless --deterministic --quiet --frames 5000 `
        --script tests/emulator/scripts/production_demo_acceptance.numos `
        --fs-sandbox
    if ($LASTEXITCODE -ne 0) { throw 'Production demo emulator flow failed.' }
    Write-Host 'PROD-DEMO-HARDEN-01 scripted acceptance: PASS' -ForegroundColor Green
} finally {
    Pop-Location
}
