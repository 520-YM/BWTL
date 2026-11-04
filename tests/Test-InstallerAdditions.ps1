param([Parameter(Mandatory=$true)][string]$Payload,[Parameter(Mandatory=$true)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
$base=Join-Path $PSScriptRoot ('installer-extra-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $base|Out-Null
$installer=Join-Path $Payload 'setup/Install-BWTL.ps1';$checks=0
function Check($ok,$name){if(-not $ok){throw ('FAILED: '+$name)};$script:checks++;Write-Output ('PASS: '+$name)}
$root=Join-Path $base 'app';$cfg=Join-Path $base 'config.pro'
$legacy="protkdat C:\Old\CreoSafeResident.dat`r`nprotkdat C:\Other\keep.dat`r`n"
[IO.File]::WriteAllText($cfg,$legacy)
$state=& $installer -PackageRoot $Payload -InstallRoot $root -ConfigPath $cfg -Silent -NoShortcuts
Check ([IO.File]::ReadAllText($cfg).StartsWith($legacy)) 'legacy bridge unchanged without opt-in'
foreach($rel in @('resources/ribbon/toolkitribbonui.rbn','resources/text/ribbon/toolkitribbonui.rbn')){Check (Test-Path (Join-Path $state.Directory $rel)) ('installed ribbon '+$rel)}
$state=& $installer -PackageRoot $Payload -InstallRoot $root -ConfigPath $cfg -Silent -NoShortcuts -DisableLegacyBridge
$text=[IO.File]::ReadAllText($cfg)
Check ($text -match '(?m)^! BWTL disabled legacy bridge: protkdat C:\\Old\\CreoSafeResident.dat') 'opt-in disables only old bridge registration'
Check ($text -match '(?m)^protkdat C:\\Other\\keep.dat') 'other plugin left enabled'
Check ([IO.File]::ReadAllText((Join-Path $state.Backup 'config.pro')).Contains($legacy.TrimEnd())) 'pre-change configuration backed up'
$report=& (Join-Path $root 'Install-BWTL.ps1') -InstallRoot $root -Diagnose -Silent
$diag=[IO.File]::ReadAllText($report)
Check ($diag.Contains('NO runtime.log')) 'missing runtime log does not claim startup success'
Check ($diag.Contains('DLL matches released SHA256: True')) 'installed DLL checksum checked'
Check (-not $diag.Contains('用户：')) 'diagnostic does not dump chat history'
[IO.File]::WriteAllText((Join-Path $root 'data/runtime.log'),"user_initialize: 0`r`nRegister native shortcut: 0`r`n")
$report=& (Join-Path $root 'Install-BWTL.ps1') -InstallRoot $root -Diagnose -Silent
Check ([IO.File]::ReadAllText($report).Contains('Register native shortcut: 0')) 'diagnostic includes startup status'
Check ([IO.File]::ReadAllText($report).Contains('Runtime log newer than install: True')) 'diagnostic distinguishes stale startup log'
$rejected=$false;try{& $installer -PackageRoot $Payload -InstallRoot (Join-Path $Payload 'nested-install') -ConfigPath $cfg -Silent -NoShortcuts}catch{$rejected=$true}
Check $rejected 'installation inside temporary payload rejected'
$rejected=$false;try{& $installer -PackageRoot $Payload -InstallRoot $root -ConfigPath (Join-Path $root 'config.pro') -Silent -NoShortcuts}catch{$rejected=$true}
Check $rejected 'Creo config inside removable installation rejected'
& (Join-Path $root 'Install-BWTL.ps1') -InstallRoot $root -Uninstall -Silent -NoShortcuts -DeleteProgramFiles -DeleteData | Out-Null
Check (-not(Test-Path $root)) 'full cleanup removes helpers and diagnostic data'
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($installer,[ref]$tokens,[ref]$errors)
Check ($errors.Count -eq 0) 'installer syntax valid'
$source=[IO.File]::ReadAllText($installer)
Check ($source.Contains("Start-BwtlManagement '-Uninstall'")) 'installer has direct uninstall action'
Check ($source.Contains("Start-BwtlManagement '-Rollback'")) 'installer has direct rollback action'
Check ($source.Contains('Export-BwtlDiagnostics $rootBox.Text')) 'installer has direct diagnostic action'
Write-Output "ADDITIONAL INSTALLER TESTS PASSED: $checks"
