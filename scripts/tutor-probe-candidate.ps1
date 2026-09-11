param([string]$ObjectDirectory='out/equations-rebuild/final/giac-host/obj',[string]$VendorOverrideDirectory='out/tutor-engine-01/host',[switch]$BuildOnly)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
$probeOutput='out/tutor-engine-01/review-math'
New-Item -ItemType Directory -Force -Path $probeOutput | Out-Null
$sourceFiles=@('src/math/giac/GiacEngine.cpp','src/math/giac/GiacTutor.inc','src/math/tutor/Derivation.h','src/math/tutor/Messages.inc','src/math/tutor/TraceAllocator.h','lib/giac/src/kgen.cc')
$sourceBefore=@($sourceFiles|ForEach-Object {(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash})
$flags=@('-std=gnu++17','-O1','-fexceptions','-ffunction-sections','-fdata-sections','-D_USE_MATH_DEFINES','-DNUMOS_GIAC_HOST_HARNESS=1','-DHAVE_CONFIG_H','-DIN_GIAC','-DGIAC_KHICAS','-DNO_GUI','-DGIAC_GENERIC','-DEMBEDDED','-DUSE_GMP_REPLACEMENTS','-DUMAP','-DDOUBLEVAL','-D__MINGW_H','-fpermissive','-Ilib/giac','-Ilib/giac/src','-Ilib/libtommath','-Isrc','-w')
& g++ @flags '-c' 'src/math/giac/GiacEngine.cpp' '-o' "$probeOutput/candidate-GiacEngine.o"
if($LASTEXITCODE -ne 0){throw 'candidate GiacEngine compilation failed'}
$sourceAfter=@($sourceFiles|ForEach-Object {(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash})
if(($sourceBefore -join ':') -ne ($sourceAfter -join ':')){throw 'Candidate changed during compilation; rerun against stable candidate'}
@($sourceFiles|ForEach-Object {$hash=Get-FileHash -LiteralPath $_ -Algorithm SHA256;[pscustomobject]@{path=$_;sha256=$hash.Hash.ToLower()}})|ConvertTo-Json|Set-Content -Encoding UTF8 -LiteralPath "$probeOutput/candidate-source-hashes.json"
& g++ @flags '-c' 'tests/host/tutor_probe_candidate.cpp' '-o' "$probeOutput/tutor_probe_candidate.o"
if($LASTEXITCODE -ne 0){throw 'candidate probe compilation failed'}
$objects=@(Get-ChildItem -LiteralPath $ObjectDirectory -Filter '*.o' -File | Where-Object {$_.Name -notmatch '_main\.cpp\.o|GiacEngine\.cpp\.o|^tutor_probe'} | ForEach-Object {Join-Path $ObjectDirectory $_.Name})
$replacement=Join-Path $VendorOverrideDirectory 'kgen.cc.o'
if(Test-Path -LiteralPath $replacement){
    if((Get-Item -LiteralPath $replacement).LastWriteTimeUtc -lt (Get-Item -LiteralPath 'lib/giac/src/kgen.cc').LastWriteTimeUtc){throw 'Stale kgen portability-fix object'}
    $objects=@($objects|Where-Object {(Split-Path -Leaf $_) -ne 'kgen.cc.o'})+$replacement
}
$objects+= "$probeOutput/candidate-GiacEngine.o","$probeOutput/tutor_probe_candidate.o"
$response=@($objects | ForEach-Object { '"'+($_ -replace '\\','/')+'"' })
[IO.File]::WriteAllLines((Join-Path (Get-Location) "$probeOutput/candidate-link.rsp"),$response)
& g++ "@$probeOutput/candidate-link.rsp" '-o' "$probeOutput/tutor_probe_candidate.exe" '-Wl,--gc-sections' '-static' '-lpsapi'
if($LASTEXITCODE -ne 0){throw 'candidate probe link failed'}
if($BuildOnly){return}
& python 'scripts/tutor-probe-candidate-run.py'
if($LASTEXITCODE -ne 0){throw 'challenge runner failed'}
Write-Output "Independent challenge traces: $probeOutput/challenge-*.jsonl"
