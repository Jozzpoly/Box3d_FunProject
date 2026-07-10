# doc_drift_check.ps1 - catch the doc<->code drift class (README A2).
#
# NOT a framework: a small curated list of terms that MUST appear in both the
# code and the docs. When a UI label / physics-model field / env hook is
# renamed in code, this fires if the docs still say the old thing (or never
# said the new thing). Extend $checks when you add a drift-prone term; keep it
# short - the primary defense is the "grep docs after a rename" rule in
# README §4, this is just a tripwire.
#
# Usage:  .\tools\doc_drift_check.ps1      (exit 0 = clean, 1 = drift found)

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

# Each check: a term that, if present in <code>, must also be present in <doc>.
$checks = @(
    @{ term = "[ARCADE]";              code = "samples\jozz_vehicle_m6_rig_lab_ui_tabs.cpp"; doc = "README_FOR_AGENTS.md" },
    @{ term = "rackFrictionBase";      code = "samples\jozz_vehicle_m6_suspension_rig.h";   doc = "README_FOR_AGENTS.md" },
    @{ term = "rackFrictionLoadCoeff"; code = "samples\jozz_vehicle_m6_suspension_rig.h";   doc = "README_FOR_AGENTS.md" },
    @{ term = "LoadJozzVehicleM6PresetConfig"; code = "samples\jozz_vehicle_m6_config_io.h"; doc = "docs\SUBSYSTEM_UI_PRESETS_PL.md" }
)

$drift = 0
foreach ($c in $checks) {
    # -SimpleMatch = literal, so pass the raw term (no regex escaping).
    $inCode = Select-String -Path $c.code -Pattern $c.term -SimpleMatch -Quiet
    $inDoc  = Select-String -Path $c.doc  -Pattern $c.term -SimpleMatch -Quiet
    if ($inCode -and -not $inDoc) {
        Write-Host ("DRIFT: '{0}' is in {1} but NOT in {2}" -f $c.term, $c.code, $c.doc) -ForegroundColor Red
        $drift++
    }
    elseif (-not $inCode) {
        Write-Host ("STALE CHECK: '{0}' no longer in {1} - update or drop this check" -f $c.term, $c.code) -ForegroundColor Yellow
    }
}

if ($drift -gt 0) {
    Write-Host ("doc-drift: {0} term(s) out of sync" -f $drift) -ForegroundColor Red
    exit 1
}
Write-Host "doc-drift: docs and code agree on the checked terms" -ForegroundColor Green
exit 0
