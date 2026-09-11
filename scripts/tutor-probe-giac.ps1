param([string]$ObjectDirectory='out/equations-rebuild/final/giac-host/obj')
$ErrorActionPreference='Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
$probeOutput='out/tutor-engine-01/review-math'
New-Item -ItemType Directory -Force -Path $probeOutput | Out-Null
$flags=@('-std=gnu++17','-O1','-fexceptions','-ffunction-sections','-fdata-sections','-D_USE_MATH_DEFINES','-DHAVE_CONFIG_H','-DIN_GIAC','-DGIAC_KHICAS','-DNO_GUI','-DGIAC_GENERIC','-DEMBEDDED','-DUSE_GMP_REPLACEMENTS','-DUMAP','-DDOUBLEVAL','-D__MINGW_H','-fpermissive','-Ilib/giac','-Ilib/giac/src','-Ilib/libtommath','-Isrc','-w')
& g++ @flags '-c' 'tests/host/tutor_probe_giac.cpp' '-o' "$probeOutput/tutor_probe_giac.o"
if($LASTEXITCODE -ne 0){throw 'probe compilation failed'}
& g++ @flags '-include' 'config.h' '-dM' '-E' '-x' 'c++' 'lib/giac/src/first.h' | Set-Content -Encoding UTF8 -LiteralPath "$probeOutput/compiled-macros.txt"
$sources=@(Get-ChildItem -LiteralPath 'lib/giac/src' -File | Where-Object { $_.Extension -in '.cc','.cpp' -and $_.Name -notmatch '^(kdisplay|k_|fx|ti|Xcas|Fl_|Graph|hist|test\.cpp)' })
$sources+=@(Get-ChildItem -LiteralPath 'lib/libtommath' -Filter '*.c' -File)
$objects=@($sources | ForEach-Object { Join-Path $ObjectDirectory ($_.Name+'.o') })
$objects+=Join-Path $ObjectDirectory 'GiacHostStubs.cpp.o'
foreach($obj in $objects){if(-not (Test-Path -LiteralPath $obj)){throw "Missing cached object: $obj; run scripts/build-giac-host-harness.sh"}}
foreach($source in $sources){
    $cached=Get-Item -LiteralPath (Join-Path $ObjectDirectory ($source.Name+'.o'))
    if($cached.LastWriteTimeUtc -lt $source.LastWriteTimeUtc){throw "Stale vendor object for $($source.FullName); regenerate cache"}
}
$objects+= "$probeOutput/tutor_probe_giac.o"
$response=@($objects | ForEach-Object { '"'+($_ -replace '\\','/')+'"' })
[IO.File]::WriteAllLines((Join-Path (Get-Location) "$probeOutput/link.rsp"),$response)
& g++ "@$probeOutput/link.rsp" '-o' "$probeOutput/tutor_probe_giac.exe" '-Wl,--gc-sections' '-static' '-lpsapi'
if($LASTEXITCODE -ne 0){throw 'probe link failed'}
& "$probeOutput/tutor_probe_giac.exe" | Set-Content -Encoding UTF8 -LiteralPath "$probeOutput/giac-step-probe.jsonl"
if($LASTEXITCODE -ne 0){throw 'probe execution failed'}
$rows=Get-Content -LiteralPath "$probeOutput/giac-step-probe.jsonl" | ConvertFrom-Json
if(@($rows|Where-Object {-not $_.restored}).Count){throw 'probe state restoration failed'}
$rows | Group-Object case | ForEach-Object {[pscustomobject]@{case=$_.Name;runs=$_.Count;answers=@($_.Group.answer|Select-Object -Unique);restored=(@($_.Group|Where-Object {-not $_.restored}).Count -eq 0);events=@($_.Group.events|Where-Object {$_}).Count;latency_us_min=($_.Group.us|Measure-Object -Minimum).Minimum;latency_us_max=($_.Group.us|Measure-Object -Maximum).Maximum}} | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath "$probeOutput/summary.json"
@('lib/giac/library.json','lib/giac/src/config.h','lib/giac/src/first.h','lib/giac/src/kglobal.cc','lib/giac/src/ksolve.cc','lib/giac/src/kvecteur.cc','lib/giac/src/kderive.cc','lib/giac/src/kmisc.cc','lib/giac/src/giacintl.h') | ForEach-Object { $hash=Get-FileHash -LiteralPath $_ -Algorithm SHA256; [pscustomobject]@{path=$_;sha256=$hash.Hash.ToLower()} } | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath "$probeOutput/vendor-hashes.json"
Write-Output "Probe evidence: $probeOutput/giac-step-probe.jsonl"
