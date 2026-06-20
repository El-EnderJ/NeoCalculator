#!/usr/bin/env pwsh
# ============================================================================
# run-emulator-windows.ps1 — Launch the NumOS SDL2 desktop emulator on Windows.
#
# Locates the built emulator executable and resolves SDL2.dll at runtime,
# without mutating the machine's global PATH. It only extends PATH for the
# child process it spawns.
#
# Build first:
#     pio run -e emulator_pc
# Then run:
#     ./scripts/run-emulator-windows.ps1
#
# Any extra arguments are forwarded to the emulator:
#     ./scripts/run-emulator-windows.ps1 --some-flag
#
# SDL2.dll resolution order:
#   1. next to the emulator .exe (portable bundle)
#   2. <NUMOS_SDL2_ROOT|SDL2_DIR|SDL2_ROOT>\bin\SDL2.dll
#   3. C:\SDL2\x86_64-w64-mingw32\bin\SDL2.dll  (historical project default)
#   4. already on PATH (where.exe SDL2.dll)
#
# Build-dir resolution order (to find program.exe):
#   1. $env:PLATFORMIO_BUILD_DIR
#   2. `build_dir` from platformio.ini  (this repo sets C:/.piobuild/numOS)
#   3. .pio/build  (PlatformIO default)
# ============================================================================
[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $EmulatorArgs
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir

function Resolve-BuildDir {
    if ($env:PLATFORMIO_BUILD_DIR) { return $env:PLATFORMIO_BUILD_DIR }
    $ini = Join-Path $RepoRoot 'platformio.ini'
    if (Test-Path $ini) {
        $line = Select-String -Path $ini -Pattern '^\s*build_dir\s*=\s*(.+?)\s*$' |
                Select-Object -First 1
        if ($line) { return $line.Matches[0].Groups[1].Value }
    }
    return (Join-Path $RepoRoot '.pio\build')
}

# ── 1. Locate the emulator executable ───────────────────────────────────────
$buildDir = Resolve-BuildDir
$exe = Join-Path $buildDir 'emulator_pc\program.exe'

if (-not (Test-Path $exe)) {
    # Fall back to the PlatformIO default location too.
    $fallback = Join-Path $RepoRoot '.pio\build\emulator_pc\program.exe'
    if (Test-Path $fallback) {
        $exe = $fallback
    } else {
        Write-Error @"
Emulator executable not found.
  Looked for: $exe
          and: $fallback
Build it first:
  pio run -e emulator_pc
"@
        exit 1
    }
}
Write-Host "[run-emulator] Executable: $exe" -ForegroundColor Cyan

# ── 2. Resolve SDL2.dll ─────────────────────────────────────────────────────
$exeDir = Split-Path -Parent $exe
$dllDir = $null

# (1) next to the exe
if (Test-Path (Join-Path $exeDir 'SDL2.dll')) {
    $dllDir = $exeDir
    Write-Host "[run-emulator] SDL2.dll found next to the executable." -ForegroundColor Green
}

# (2) configured SDL2 root(s)
if (-not $dllDir) {
    $roots = @($env:NUMOS_SDL2_ROOT, $env:SDL2_DIR, $env:SDL2_ROOT,
               'C:\SDL2\x86_64-w64-mingw32') | Where-Object { $_ }
    foreach ($r in $roots) {
        $cand = Join-Path $r 'bin\SDL2.dll'
        if (Test-Path $cand) {
            $dllDir = Split-Path -Parent $cand
            Write-Host "[run-emulator] SDL2.dll found in SDL2 root: $dllDir" -ForegroundColor Green
            break
        }
    }
}

# (3) already on PATH?
if (-not $dllDir) {
    $onPath = (& where.exe SDL2.dll 2>$null | Select-Object -First 1)
    if ($onPath) {
        $dllDir = Split-Path -Parent $onPath
        Write-Host "[run-emulator] SDL2.dll already on PATH: $onPath" -ForegroundColor Green
    }
}

if (-not $dllDir) {
    Write-Error @"
SDL2.dll could not be located.
Provide it via any one of:
  * copy SDL2.dll next to: $exeDir
  * set NUMOS_SDL2_ROOT to your SDL2 dev folder (contains bin\SDL2.dll), e.g.:
        `$env:NUMOS_SDL2_ROOT = 'C:\SDL2\x86_64-w64-mingw32'
  * add the SDL2 bin folder to PATH for this shell.
See docs/emulator-sdl2-quickstart.md (Troubleshooting: missing SDL2.dll).
"@
    exit 1
}

# Extend PATH for THIS process only (inherited by the child; not persisted).
if ($dllDir -ne $exeDir) {
    $env:PATH = "$dllDir;$env:PATH"
}

# ── 3. Run ──────────────────────────────────────────────────────────────────
Write-Host "[run-emulator] Launching NumOS emulator... (close the window to exit)" -ForegroundColor Cyan
& $exe @EmulatorArgs
exit $LASTEXITCODE
