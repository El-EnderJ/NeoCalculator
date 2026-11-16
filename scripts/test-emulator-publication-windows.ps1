#!/usr/bin/env pwsh
# Real Windows file-sharing and process checks, in an isolated release folder.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $BuiltExecutable,
    [string] $TestRoot = ('C:\.codex-cache\emulator-availability-01\' + [Guid]::NewGuid().ToString('N'))
)
$ErrorActionPreference = 'Stop'
$publish = Join-Path $PSScriptRoot 'publish-emulator-windows.ps1'
$candidate = (Resolve-Path -LiteralPath $BuiltExecutable).Path
$release = Join-Path $TestRoot 'release'
$live = Join-Path $release 'program.exe'
New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
function Assert-True($condition, $message) {
    if (-not $condition) { throw $message }
    Write-Host "PASS $message"
}
$ini = Join-Path (Split-Path -Parent $PSScriptRoot) 'platformio.ini'
$configured = (Select-String -LiteralPath $ini -Pattern '^build_dir\s*=\s*(.+)$' | Select-Object -First 1).Matches[0].Groups[1].Value.Trim()
# build_dir is derived from ${platformio.core_dir}; expand it like PlatformIO.
$token = '${platformio.core_dir}'
if ($configured.StartsWith($token)) {
    $core = $env:PLATFORMIO_CORE_DIR
    if (-not $core) {
        $short = (Split-Path -Qualifier $HOME) + '\.platformio'
        $core = if (Test-Path -LiteralPath $short) { $short } else { Join-Path $HOME '.platformio' }
    }
    $configured = $core + $configured.Substring($token.Length)
}
$scratch = [IO.Path]::GetFullPath($configured).TrimEnd('\') + '\'
$stable = [IO.Path]::GetFullPath('C:\.piobuild\numOS\emulator_pc\program.exe')
Assert-True (-not $stable.StartsWith($scratch, [StringComparison]::OrdinalIgnoreCase)) 'PlatformIO build root cannot contain the stable release'
& $publish -BuiltExecutable $candidate -DestinationDirectory $release
$initial = (Get-FileHash -LiteralPath $live).Hash
Assert-True ($initial -eq (Get-FileHash -LiteralPath $candidate).Hash) 'first publication with complete runtime'

$invalid = Join-Path $TestRoot 'invalid.exe'
[IO.File]::WriteAllText($invalid, 'not an executable')
$failed = $false
try { & $publish -BuiltExecutable $invalid -DestinationDirectory $release } catch { $failed = $true }
Assert-True ($failed -and (Get-FileHash -LiteralPath $live).Hash -eq $initial) 'failed candidate preserves active executable'

# A PE overlay changes the hash without changing the executable instructions.
$next = Join-Path $TestRoot 'next.exe'
Copy-Item -LiteralPath $candidate -Destination $next
$stream = [IO.File]::OpenWrite($next)
try {
    $stream.Seek(0, [IO.SeekOrigin]::End) | Out-Null
    $bytes = [Text.Encoding]::ASCII.GetBytes('NumOS atomic publication test')
    $stream.Write($bytes, 0, $bytes.Length)
} finally { $stream.Dispose() }
$lock = [IO.File]::Open($live, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
$failed = $false
try {
    try { & $publish -BuiltExecutable $next -DestinationDirectory $release } catch { $failed = $true }
    Assert-True ($failed -and (Get-FileHash -LiteralPath $live).Hash -eq $initial) 'locked executable stays available without closing the user session'
} finally { $lock.Dispose() }

& $publish -BuiltExecutable $next -DestinationDirectory $release
Assert-True ((Get-FileHash -LiteralPath $live).Hash -eq (Get-FileHash -LiteralPath $next).Hash) 'validated replacement is installed'
Assert-True ((Get-FileHash -LiteralPath (Join-Path $release 'previous-program.exe')).Hash -eq $initial) 'previous executable is preserved'

$runtime = Join-Path $TestRoot 'different-runtime'
New-Item -ItemType Directory -Path $runtime | Out-Null
[IO.File]::WriteAllText((Join-Path $runtime 'SDL2.dll'), 'incompatible runtime')
$current = (Get-FileHash -LiteralPath $live).Hash
$failed = $false
try { & $publish -BuiltExecutable $next -DestinationDirectory $release -RuntimeDirectory $runtime } catch { $failed = $true }
Assert-True ($failed -and (Get-FileHash -LiteralPath $live).Hash -eq $current) 'runtime mismatch preserves the working bundle'
& $publish -BuiltExecutable $candidate -DestinationDirectory $release
Assert-True ((Get-FileHash -LiteralPath $live).Hash -eq $initial -and
    (Get-FileHash -LiteralPath (Join-Path $release 'previous-program.exe')).Hash -eq $current) 'repeated publication safely rotates the previous executable'
Assert-True (@(Get-ChildItem -LiteralPath $release -Directory -Filter '.publish-*').Count -eq 0) 'private staging is cleaned after success and failure'
Write-Host "Publication evidence: $TestRoot"
