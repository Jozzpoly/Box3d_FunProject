# Compatibility wrapper for the old Windows entry point.
# The canonical cross-platform gate and its single term registry live in
# tools/docs_audit.py; do not duplicate the checks here.

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot
python tools/docs_audit.py
exit $LASTEXITCODE
