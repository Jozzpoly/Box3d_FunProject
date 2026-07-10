# gate.ps1 - one-command quality gate for the Jozz Vehicle project.
#
# Encodes README_FOR_AGENTS.md §3 + the per-stage gate so no agent re-derives
# it: build the three targets, run the validator (reading exit code AND its
# printed FAILED/bad lines, since it asserts loosely), run the engine tests,
# and boot-smoke the main lab. Prints ONE summary line; on failure prints the
# first offending line and exits non-zero.
#
# Usage (from anywhere):  .\tools\gate.ps1
#         key numbers:    .\tools\gate.ps1 -Numbers   (also echo key probe lines)
#
# This is the gate the future big-refactor milestone leans on: split a
# 2000-line file, run this, know in one line whether anything moved.

param([switch]$Numbers)

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Fail([string]$stage, [string]$detail) {
    Write-Host ("BRAMKA: FAIL @ {0}" -f $stage) -ForegroundColor Red
    if ($detail) { Write-Host ("  {0}" -f $detail.Trim()) -ForegroundColor Red }
    exit 1
}

$val = "build\bin\Debug\jozz_vehicle_validation.exe"
$test = "build\bin\Debug\test.exe"
$samples = "build\bin\Debug\samples.exe"

# 0. Free samples.exe (an open window holds the linker -> LNK1168).
Get-Process samples -ErrorAction SilentlyContinue | Stop-Process -Force

# 1. Build the three targets. Filter to real compiler/linker errors.
foreach ($target in @("samples", "jozz_vehicle_validation", "test")) {
    $out = cmake --build --preset windows-debug --target $target 2>&1
    $errs = $out | Select-String -Pattern "error C\d|error LNK|fatal error"
    if ($errs) { Fail "build ($target)" $errs[0].Line }
    Write-Host ("  build {0}: OK" -f $target)
}

# 2. Validator. Trust the exit code, but ALSO scan output - it prints "FAILED"
#    / "bad <name>" per probe and asserts loosely (README §3).
$valOut = & $val 2>&1
$valExit = $LASTEXITCODE
$valBad = $valOut | Select-String -Pattern "FAILED|^bad |^\s+bad "
if ($valExit -ne 0 -or $valBad) {
    $detail = if ($valBad) { $valBad[0].Line } else { "validator exit code $valExit" }
    Fail "validator" $detail
}
Write-Host "  validator: OK"

# 3. Engine test suite.
$testOut = & $test 2>&1
if (-not ($testOut | Select-String -Pattern "All Box3D tests passed")) {
    $tf = $testOut | Select-String -Pattern "failed|FAILED" | Select-Object -First 1
    $td = if ($tf) { $tf.Line } else { "test.exe did not report all-passed" }
    Fail "test.exe" $td
}
Write-Host "  test.exe: PASS"

# 4. Boot smoke of the main lab (renders 300 frames, must exit clean).
$smokeOut = & $samples --sample-name "M6 Suspension Rig Lab" --frames 300 2>&1
$smokeErr = $smokeOut | Select-String -Pattern "sokol error" | Where-Object { $_ -notmatch "0 sokol errors" }
if ($smokeErr) { Fail "boot smoke" $smokeErr[0].Line }
if (-not ($smokeOut | Select-String -Pattern "300 frames")) {
    Fail "boot smoke" "lab did not render 300 frames (crash before completion?)"
}
Write-Host "  boot smoke: 0 sokol errors"

# Optional: echo the key probe numbers agents usually read by hand.
if ($Numbers) {
    Write-Host "`n--- key probe numbers ---"
    $valOut | Select-String -Pattern "landing \(3.5|straight-pull heading|p4 steering return probe:|full lock|preset determinism probe:|uruchomiono" |
        ForEach-Object { Write-Host ("  {0}" -f $_.Line.Trim()) }
}

Write-Host "BRAMKA: build 3/3 OK - walidator OK - test PASS - smoke 0 err" -ForegroundColor Green
exit 0
