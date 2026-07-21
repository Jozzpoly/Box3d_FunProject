# SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
# SPDX-License-Identifier: MIT
#
# Windows entry point for the Photogrammetry Import V2 P0 baseline.
# The Python implementation owns the guards and report format so those parts can
# be unit-tested without weakening the existing tools/gate.ps1 contract.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ScanArchive,

    [string]$ExpectedSha256 = "",

    [string]$BaseBranch = "Photogrametry_Import_experiment",

    [string]$BaseCommit = "f1c4919e501721749084210aea9b571e96b69bed",

    [string]$OutputRoot = "build\scan_pipeline\p0_baseline"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $repoRoot

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
}
if (-not $python) {
    Write-Error "Python was not found. P0 cannot produce a validated baseline report."
    exit 1
}

$script = Join-Path $PSScriptRoot "scan_p0_baseline.py"
$argsList = @(
    $script,
    "--scan-archive", $ScanArchive,
    "--base-branch", $BaseBranch,
    "--base-commit", $BaseCommit,
    "--output-root", $OutputRoot
)
if ($ExpectedSha256) {
    $argsList += @("--expected-sha256", $ExpectedSha256)
}

if ($python.Name -eq "py.exe" -or $python.Name -eq "py") {
    & $python.Source -3 @argsList
}
else {
    & $python.Source @argsList
}
exit $LASTEXITCODE
