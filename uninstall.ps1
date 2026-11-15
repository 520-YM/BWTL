[CmdletBinding()]
param([string]$InstallRoot,[switch]$Silent,[switch]$PlanOnly,[switch]$DeleteProgramFiles,[switch]$DeleteData)
$ErrorActionPreference='Stop'
if(!$InstallRoot){
    $InstallRoot=[Environment]::GetEnvironmentVariable('BWTL_INSTALL_ROOT','User')
    if(!$InstallRoot){$InstallRoot=Join-Path $env:LOCALAPPDATA 'BWTL'}
}
& (Join-Path $InstallRoot 'Install-BWTL.ps1') -InstallRoot $InstallRoot -Uninstall -Silent:$Silent -PlanOnly:$PlanOnly -DeleteProgramFiles:$DeleteProgramFiles -DeleteData:$DeleteData
