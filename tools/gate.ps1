# gate.ps1 - one-command quality gate for the Jozz Vehicle project.
#
# Encodes README_FOR_AGENTS.md §3 + the per-stage gate so no agent re-derives
# it: build the three targets, run the validator (reading exit code AND its
# printed FAILED/bad lines, since it asserts loosely), run the engine tests,
# and boot-smoke the main lab. Prints ONE summary line; on failure prints the
# first offending line and exits non-zero.
#
# Usage (from anywhere):  .\tools\gate.ps1
#         key numbers:    .\tools\gate.ps1 -Numbers       (also echo key probe lines)
#         save baseline:  .\tools\gate.ps1 -SaveBaseline  (snapshot validator + quad)
#         diff baseline:  .\tools\gate.ps1 -DiffBaseline  (fail on ANY changed line)
#                add:     -Shots  (also compare the render quad; use for R3/R4)
#
# This is the gate the big-refactor milestone (R0-R7) leans on. -DiffBaseline is
# the R0 bar: "green" is not enough for a move-only refactor - the validator's
# printed numbers must be IDENTICAL to the pre-refactor baseline, byte for byte
# (both validator stdout and, with -Shots, the render). Measured facts this
# rests on: the validator is deterministic run-to-run (349 lines identical over
# 3 runs) and the render is deterministic too (same PNG hash). So ANY diff means
# behaviour moved -> STOP, not "explain it away".

param(
    [switch]$Numbers,
    [switch]$SaveBaseline,
    [switch]$DiffBaseline,
    [switch]$Shots
)

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Fail([string]$stage, [string]$detail) {
    Write-Host ("BRAMKA: FAIL @ {0}" -f $stage) -ForegroundColor Red
    if ($detail) { Write-Host ("  {0}" -f $detail.Trim()) -ForegroundColor Red }
    exit 1
}

$val = "build\bin\Debug\jozz_vehicle_validation.exe"
$test = "build\bin\Debug\box3d_unit_tests.exe"
$samples = "build\bin\Debug\samples.exe"

# --- R0 baseline helpers -----------------------------------------------------
# Baseline lives under build\ (gitignored): it is a LOCAL before/after snapshot
# for one refactor stage on one machine, not a committed artifact.
$baselineTxt = "build\gate_baseline.txt"
$baselineShot = "build\gate_baseline_shots\quad.png"
$currentShot = "build\gate_current_shots\quad.png"

# Defensive time-line filter, applied identically on save AND diff so it can
# never skew the comparison. The validator today prints NO time lines (verified:
# 3 runs byte-identical), but if someone adds a "duration"/"elapsed" line later
# it must not turn a clean refactor red. Deliberately narrow - keyed on the
# words only, so it can never eat a physics number line.
function Select-StableLines([string[]]$lines) {
    return @($lines | Where-Object { $_ -notmatch '(?i)\b(duration|elapsed)\b' })
}

# Render the baseline quad to $outPng from a NEUTRAL, deterministic pose: strip
# every JOZZ_M6_* diagnostic env var first so the snapshot does not depend on
# whatever the agent had exported. quad_shot sets JOZZ_M6_CAM per view itself.
function New-GateQuad([string]$outPng) {
    $dir = Split-Path -Parent $outPng
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    foreach ($n in @("JOZZ_M6_DIAG", "JOZZ_M6_WHEEL", "JOZZ_M6_DUMPER", "JOZZ_M6_MOUNT",
            "JOZZ_M6_ARMTINT", "JOZZ_M6_HERTZ", "JOZZ_M6_DAMP", "JOZZ_M6_PRELOAD",
            "JOZZ_M6_DROOP", "JOZZ_M6_PRESET", "JOZZ_M6_TAB", "JOZZ_M6_DUMP", "JOZZ_M6_CAM")) {
        Remove-Item ("env:{0}" -f $n) -ErrorAction SilentlyContinue
    }
    & "$repoRoot\tools\quad_shot.ps1" -Out $outPng | Out-Null
    if (-not (Test-Path $outPng)) { Fail "shots" "quad_shot did not produce $outPng" }
}

# 0. Free samples.exe (an open window holds the linker -> LNK1168).
Get-Process samples -ErrorAction SilentlyContinue | Stop-Process -Force

# 1. Build the three targets. Filter to real compiler/linker errors.
foreach ($target in @("samples", "jozz_vehicle_validation", "box3d_unit_tests")) {
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
    $valOut | Select-String -Pattern "landing \(3.5|straight-pull heading|p4 steering return probe:|full lock|preset determinism probe:|ran \d+ probes" |
        ForEach-Object { Write-Host ("  {0}" -f $_.Line.Trim()) }
}

# 5. R0 baseline: save a snapshot, or diff the current run against it.
#    Both reuse $valOut captured above (same bytes the gate already vetted).
$valLines = @($valOut | ForEach-Object { [string]$_ })

if ($SaveBaseline) {
    $stable = Select-StableLines $valLines
    Set-Content -Path $baselineTxt -Value $stable -Encoding UTF8
    Write-Host ("  baseline: {0} validator lines -> {1}" -f $stable.Count, $baselineTxt)
    New-GateQuad $baselineShot
    $bh = (Get-FileHash $baselineShot).Hash
    Write-Host ("  baseline: quad -> {0} ({1} B, {2})" -f $baselineShot, (Get-Item $baselineShot).Length, $bh.Substring(0, 12))
}

if ($DiffBaseline) {
    if (-not (Test-Path $baselineTxt)) { Fail "diff-baseline" "no $baselineTxt - run -SaveBaseline first" }
    $cur = Select-StableLines $valLines
    $base = @(Get-Content -Path $baselineTxt -Encoding UTF8)
    $max = [Math]::Max($base.Count, $cur.Count)
    for ($i = 0; $i -lt $max; $i++) {
        $bl = if ($i -lt $base.Count) { $base[$i] } else { "<no such line>" }
        $cl = if ($i -lt $cur.Count) { $cur[$i] } else { "<no such line>" }
        if ($bl -cne $cl) {
            Fail "diff-baseline" ("line {0}: baseline [{1}] vs now [{2}]" -f ($i + 1), $bl, $cl)
        }
    }
    Write-Host ("  diff-baseline: validator IDENTICAL ({0} lines)" -f $base.Count)
    if ($Shots) {
        if (-not (Test-Path $baselineShot)) { Fail "diff-baseline" "no $baselineShot - run -SaveBaseline first" }
        New-GateQuad $currentShot
        $hb = (Get-FileHash $baselineShot).Hash
        $hc = (Get-FileHash $currentShot).Hash
        if ($hb -ne $hc) {
            Write-Host "  QUAD RENDER DIFFERS from baseline:" -ForegroundColor Yellow
            Write-Host ("    baseline: {0} ({1} B, {2})" -f $baselineShot, (Get-Item $baselineShot).Length, $hb.Substring(0, 12)) -ForegroundColor Yellow
            Write-Host ("    now:      {0} ({1} B, {2})" -f $currentShot, (Get-Item $currentShot).Length, $hc.Substring(0, 12)) -ForegroundColor Yellow
            Fail "diff-baseline shots" "render changed - OPEN BOTH files above (render is the gate); a move-only refactor must not move a pixel"
        }
        Write-Host "  diff-baseline shots: render IDENTICAL (hash match)"
    }
    else {
        Write-Host "  (shots skipped - add -Shots for the visual stages R3/R4)"
    }
}

Write-Host "BRAMKA: build 3/3 OK - walidator OK - test PASS - smoke 0 err" -ForegroundColor Green
exit 0
