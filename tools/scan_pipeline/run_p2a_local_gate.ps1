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
$powerShellHost = (Get-Process -Id $PID -ErrorAction Stop).Path

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
        Write-Host ""
        Write-Host "==> Existing vehicle/build/smoke gate"

        # tools/gate.ps1 intentionally captures native stderr because samples.exe
        # writes its successful smoke summary there. Run the legacy gate in a
        # child PowerShell process so this script's ErrorActionPreference=Stop
        # cannot reinterpret that successful stderr line as a terminating error.
        # The child process exit code remains the hard pass/fail boundary.
        $vehicleGate = Join-Path $repoRoot "tools\gate.ps1"
        $vehicleGateArgument = '"' + $vehicleGate + '"'
        $process = Start-Process `
            -FilePath $powerShellHost `
            -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $vehicleGateArgument) `
            -Wait `
            -PassThru `
            -NoNewWindow
        if ($process.ExitCode -ne 0) {
            throw "Existing vehicle/build/smoke gate failed with exit code $($process.ExitCode)"
        }
        Write-Host "    PASS: Existing vehicle/build/smoke gate"
    }

    Write-Host ""
    Write-Host "P2A_LOCAL_CODE_GATE_PASS"
    Write-Host "HEAD: $head"
    Write-Host "Next boundary: create and verify the private real bundle/preview; this code gate does not assert TERRAIN_VISIBLE_PASS."
}
finally {
    Pop-Location
}
