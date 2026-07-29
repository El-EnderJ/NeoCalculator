#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$Port,
    [Parameter(Mandatory)][switch]$ConfirmFactoryReset,
    [string]$PackageRoot = $PSScriptRoot
)

if (-not $ConfirmFactoryReset) {
    throw 'Recovery overwrites all 16 MB. Re-run with -ConfirmFactoryReset.'
}
. (Join-Path $PSScriptRoot 'PackageCommon.ps1')
$package = Get-NumOSPackage -PackageRoot $PackageRoot
Test-NumOSPackageHashes -PackageRoot $package.Root
Show-NumOSBootInstructions

Invoke-NumOSEsptool -Arguments @(
    '--chip','esp32s3','--port',$Port,'--before','default_reset',
    '--after','hard_reset','write_flash','0x0',
    (Join-Path $package.Root $package.Metadata.factory_image.filename)
)
Write-Host 'Full 16 MB recovery complete; settings/filesystem state was replaced.' -ForegroundColor Green
