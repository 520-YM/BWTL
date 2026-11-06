[CmdletBinding()]
param([string]$InstallRoot=(Join-Path $env:LOCALAPPDATA 'BWTL'),[switch]$Silent,[switch]$PlanOnly,[switch]$DeleteProgramFiles,[switch]$DeleteData)
$ErrorActionPreference='Stop'
& (Join-Path $InstallRoot 'Install-BWTL.ps1') -InstallRoot $InstallRoot -Uninstall -Silent:$Silent -PlanOnly:$PlanOnly -DeleteProgramFiles:$DeleteProgramFiles -DeleteData:$DeleteData
