#!/usr/bin/env pwsh
[CmdletBinding()]
param([string]$PackageRoot = $PSScriptRoot)

. (Join-Path $PSScriptRoot 'PackageCommon.ps1')
$package = Get-NumOSPackage -PackageRoot $PackageRoot
Test-NumOSPackageHashes -PackageRoot $package.Root
Write-Host "Offline package verification: PASS ($($package.Metadata.profile))" -ForegroundColor Green
