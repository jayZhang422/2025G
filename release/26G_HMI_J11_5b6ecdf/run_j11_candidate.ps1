[CmdletBinding()]
param(
    [string]$XsctPath = 'D:\Vivado_install\Vitis\2020.2\bin\xsct.bat'
)

$ErrorActionPreference = 'Stop'
$releaseRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path

& (Join-Path $releaseRoot 'verify_release.ps1')
if (-not $?) {
    throw 'Release hash verification did not complete successfully'
}
if (-not (Test-Path -LiteralPath $XsctPath -PathType Leaf)) {
    throw "Vitis 2020.2 XSCT not found: $XsctPath"
}

$env:PS_HMI_CANDIDATE_BIT = (Resolve-Path -LiteralPath (
    Join-Path $releaseRoot 'artifacts\25g_2026g_hmi_j11.bit')).Path
$env:PS_HMI_CANDIDATE_XSA = (Resolve-Path -LiteralPath (
    Join-Path $releaseRoot 'artifacts\25g_2026g_hmi_j11.xsa')).Path
$env:PS_HMI_CANDIDATE_PS7_INIT = (Resolve-Path -LiteralPath (
    Join-Path $releaseRoot 'artifacts\ps7_init.tcl')).Path
$env:PS_HMI_CANDIDATE_ELF = (Resolve-Path -LiteralPath (
    Join-Path $releaseRoot 'artifacts\hmi_candidate_app.elf')).Path
$runScript = (Resolve-Path -LiteralPath (
    Join-Path $releaseRoot 'source_tree\25G_PS\script\ps_run_hmi_candidate.tcl')).Path

Write-Host 'Programming the exact J11 candidate without FSBL or Bootgen...'
& $XsctPath $runScript
if ($LASTEXITCODE -ne 0) {
    throw "XSCT candidate run failed with exit code $LASTEXITCODE"
}
