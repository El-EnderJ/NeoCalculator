Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:ExpectedBoard = 'numos-esp32-s3-wroom-1u-n16r8'
$script:AllowedEnvironments = @(
    'numos-esp32-s3-wroom-1u-n16r8',
    'numos-esp32-s3-wroom-1u-n16r8-bringup',
    'numos-esp32-s3-wroom-1u-n16r8-demo'
)

function Get-NumOSPackage {
    param([Parameter(Mandatory)][string]$PackageRoot)

    $root = (Resolve-Path -LiteralPath $PackageRoot).Path
    $metadataPath = Join-Path $root 'build-metadata.json'
    if (-not (Test-Path -LiteralPath $metadataPath -PathType Leaf)) {
        throw "Missing build-metadata.json in $root"
    }
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    if ($metadata.board_identifier -ne $script:ExpectedBoard) {
        throw "Wrong board identifier '$($metadata.board_identifier)'; expected '$script:ExpectedBoard'."
    }
    if ($metadata.environment -notin $script:AllowedEnvironments) {
        throw "Environment '$($metadata.environment)' is not a production package."
    }
    if ([int64]$metadata.flash_bytes -ne 16777216) {
        throw "Package is not a 16 MB production image."
    }
    Write-Host "Selected environment: $($metadata.environment)" -ForegroundColor Cyan
    Write-Host "Board identifier:     $($metadata.board_identifier)"
    Write-Host "Source commit:        $($metadata.source.commit)"
    if ($metadata.source.tree_dirty) {
        Write-Warning "Package was produced from a dirty source tree; source-state hash $($metadata.source.source_state_sha256)"
    }
    return [pscustomobject]@{ Root = $root; Metadata = $metadata }
}

function Test-NumOSPackageHashes {
    param([Parameter(Mandatory)][string]$PackageRoot)

    $sumsPath = Join-Path $PackageRoot 'SHA256SUMS'
    if (-not (Test-Path -LiteralPath $sumsPath -PathType Leaf)) {
        throw "Missing SHA256SUMS in $PackageRoot"
    }
    foreach ($line in Get-Content -LiteralPath $sumsPath) {
        if (-not $line.Trim()) { continue }
        if ($line -notmatch '^([0-9a-f]{64})  ([^\\/]+)$') {
            throw "Malformed SHA256SUMS line: $line"
        }
        $expected = $Matches[1]
        $name = $Matches[2]
        $path = Join-Path $PackageRoot $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Hash manifest references missing file: $name"
        }
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $expected) {
            throw "SHA256 mismatch for $name"
        }
    }
    Write-Host 'Package hashes: PASS' -ForegroundColor Green
}

function Show-NumOSBootInstructions {
    Write-Host ''
    Write-Host 'If the ROM bootloader is not detected:' -ForegroundColor Yellow
    Write-Host '  1. Hold BOOT.'
    Write-Host '  2. Tap RESET.'
    Write-Host '  3. Release BOOT when flashing begins.'
    Write-Host 'After flashing, tap RESET once.'
}

function Invoke-NumOSEsptool {
    param([Parameter(Mandatory)][string[]]$Arguments)
    & python -m esptool @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "esptool failed with exit code $LASTEXITCODE"
    }
}
