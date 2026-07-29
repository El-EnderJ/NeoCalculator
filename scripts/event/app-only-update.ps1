#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$Port,
    [string]$PackageRoot = $PSScriptRoot
)

. (Join-Path $PSScriptRoot 'PackageCommon.ps1')
$package = Get-NumOSPackage -PackageRoot $PackageRoot
Test-NumOSPackageHashes -PackageRoot $package.Root
Show-NumOSBootInstructions

$firmware = $package.Metadata.components | Where-Object { $_.filename -eq 'firmware.bin' }
if (-not $firmware) { throw 'Package metadata has no firmware component.' }
Invoke-NumOSEsptool -Arguments @(
    '--chip','esp32s3','--port',$Port,'--before','default_reset',
    '--after','hard_reset','write_flash',[string]$firmware.offset,
    (Join-Path $package.Root $firmware.filename)
)
Write-Host 'Application-only update complete; persistent state was not erased.' -ForegroundColor Green
