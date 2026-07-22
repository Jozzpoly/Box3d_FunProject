[CmdletBinding()]
param(
    [string]$SourceRoot,
    [string]$PipelineRoot = "build/scan_pipeline",
    [switch]$NoLaunch
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$python = (Get-Command python -ErrorAction Stop).Source
$runner = Join-Path $PSScriptRoot "scan_real_terrain_flow.py"

if (-not (Test-Path -LiteralPath $runner -PathType Leaf)) {
    throw "Real terrain flow runner is missing from this checkout"
}

$arguments = @(
    $runner,
    "--pipeline-root",
    $PipelineRoot,
    "continue"
)

if (-not [string]::IsNullOrWhiteSpace($SourceRoot)) {
    $resolvedSource = Resolve-Path -LiteralPath $SourceRoot -ErrorAction Stop
    if (-not (Test-Path -LiteralPath $resolvedSource.Path -PathType Container)) {
        throw "Private scan source root must be a directory"
    }
    $arguments += @("--source-root", $resolvedSource.Path)
}

if (-not $NoLaunch) {
    $arguments += "--launch"
}

Push-Location $repoRoot
try {
    Write-Host "Real terrain preview flow"
    Write-Host "Repository: $repoRoot"
    Write-Host "Mode: resumable / fail-closed"
    & $python @arguments
    $code = $LASTEXITCODE
    if ($null -eq $code) {
        $code = 0
    }
    if ($code -ne 0) {
        throw "Real terrain preview flow failed with exit code $code"
    }
}
finally {
    Pop-Location
}
