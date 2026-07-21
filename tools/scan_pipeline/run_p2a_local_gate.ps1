[CmdletBinding()]
param(
    [switch]$SkipVehicleGate
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$python = (Get-Command python -ErrorAction Stop).Source
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$git = (Get-Command git -ErrorAction Stop).Source

function Invoke-HardGate {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Command
    )

    Write-Host ""
    Write-Host "==> $Label"
    & $Command
    $code = $LASTEXITCODE
    if ($null -eq $code) {
        $code = 0
    }
    if ($code -ne 0) {
        throw "$Label failed with exit code $code"
    }
    Write-Host "    PASS: $Label"
}

Push-Location $repoRoot
try {
    $head = (& $git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($head)) {
        throw "Cannot resolve the current Git HEAD"
    }

    Write-Host "P2A local gate"
    Write-Host "Repository: $repoRoot"
    Write-Host "HEAD:       $head"

    Invoke-HardGate "Canonical P1/P1B/P2A contracts" {
        & $python ".\tools\scan_pipeline\run_p1_contracts.py"
    }

    Invoke-HardGate "Configure Windows build" {
        & $cmake --preset windows
    }

    Invoke-HardGate "Build native samples target" {
        & $cmake --build --preset windows-debug --target samples
    }

    $samples = Join-Path $repoRoot "build\bin\Debug\samples.exe"
    if (-not (Test-Path -LiteralPath $samples -PathType Leaf)) {
        throw "Native samples build did not produce: $samples"
    }
    Write-Host "    PASS: samples.exe exists"

    if (-not $SkipVehicleGate) {
        Invoke-HardGate "Existing vehicle/build/smoke gate" {
            & ".\tools\gate.ps1"
        }
    }

    Write-Host ""
    Write-Host "P2A_LOCAL_CODE_GATE_PASS"
    Write-Host "HEAD: $head"
    Write-Host "Next boundary: create and verify the private real bundle/preview; this code gate does not assert TERRAIN_VISIBLE_PASS."
}
finally {
    Pop-Location
}
