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

$flashArgs = @('--chip','esp32s3','--port',$Port,'--before','default_reset',
               '--after','hard_reset','write_flash')
foreach ($component in $package.Metadata.components) {
    $flashArgs += @([string]$component.offset, (Join-Path $package.Root $component.filename))
}
if ($package.Metadata.filesystem_image) {
    $flashArgs += @(
        [string]$package.Metadata.filesystem_image.offset,
        (Join-Path $package.Root $package.Metadata.filesystem_image.filename)
    )
}
Invoke-NumOSEsptool -Arguments $flashArgs
Write-Host 'First flash complete. Run the board-day validation checklist.' -ForegroundColor Green
