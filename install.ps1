[CmdletBinding()]
param([string]$InstallRoot,[string]$ConfigPath,[string]$CodexPath,[string]$CreoExecutable,[switch]$Silent,[switch]$PlanOnly,[switch]$NoShortcuts)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'setup\Install-BWTL.ps1') -PackageRoot $PSScriptRoot @PSBoundParameters
