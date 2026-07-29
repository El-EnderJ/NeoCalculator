#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\out\PROD-DEMO-HARDEN-01')
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$output = if ([IO.Path]::IsPathRooted($OutputRoot)) {
    [IO.Path]::GetFullPath($OutputRoot)
} else {
    [IO.Path]::GetFullPath((Join-Path $repo $OutputRoot))
}
$environments = @(
    'numos-esp32-s3-wroom-1u-n16r8',
    'numos-esp32-s3-wroom-1u-n16r8-bringup',
    'numos-esp32-s3-wroom-1u-n16r8-demo'
)

New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach ($environment in $environments) {
    Write-Host "Building production package: $environment" -ForegroundColor Cyan
    & pio run -e $environment -t factory_image
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $environment" }

    $buildRoot = if ($env:PLATFORMIO_BUILD_DIR) {
        $env:PLATFORMIO_BUILD_DIR
    } else {
        'C:/.piobuild/numOS'
    }
    $source = Join-Path $buildRoot "$environment\factory-package"
    $destination = Join-Path $output $environment
    if (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }
    Copy-Item -LiteralPath $source -Destination $destination -Recurse
    & (Join-Path $destination 'verify-package.ps1') -PackageRoot $destination
    if ($LASTEXITCODE -ne 0) { throw "Package verification failed: $environment" }
}
Write-Host "All production packages ready: $output" -ForegroundColor Green
