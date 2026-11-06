$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$work=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) ('Temp\BWTL-isolation-test-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $work 'install';$profile=Join-Path $work 'profile';$shared=Join-Path $work 'config.pro';$standards=Join-Path $work 'standards.pro'
$utf8=[Text.UTF8Encoding]::new($false)
New-Item -ItemType Directory -Path $root,(Join-Path $root 'data') -Force|Out-Null
$original="! Existing settings`r`nprotkdat C:\Legacy\CreoSafeResident.dat`r`n! BWTL BEGIN`r`nprotkdat $root\protk.dat`r`n! BWTL END`r`n"
[IO.File]::WriteAllText($shared,$original,$utf8)
[IO.File]::WriteAllText($standards,"template_solidpart C:\Templates\solid.prt`r`nprotkdat C:\Other\Other.dat`r`n",$utf8)
[IO.File]::WriteAllText((Join-Path $root 'data\chat-history.md'),'历史原文不可丢失',$utf8)
[IO.File]::WriteAllText((Join-Path $root 'protk.dat'),'NAME BWTL',$utf8)
[IO.File]::WriteAllText((Join-Path $root 'active.json'),(@{Config=$shared;Version='0.0.1';Directory=(Join-Path $root 'versions\0.0.1');Data=(Join-Path $root 'data')}|ConvertTo-Json),$utf8)
$before=(Get-FileHash -LiteralPath (Join-Path $root 'data\chat-history.md')).Hash
$enable=Join-Path $repo 'scripts\Enable-BWTLIsolation.ps1'
$plan=& $enable -InstallRoot $root -ProfileRoot $profile -StandardsConfig $standards -CreoExecutable 'C:\Missing\creo.exe' -PlanOnly
if(Test-Path -LiteralPath $profile){throw 'PlanOnly changed profile'}
$result=& $enable -InstallRoot $root -ProfileRoot $profile -StandardsConfig $standards
$expected="! Existing settings`r`nprotkdat C:\Legacy\CreoSafeResident.dat`r`n"
if([IO.File]::ReadAllText($shared) -cne $expected){throw 'Legacy config or settings changed'}
if((Get-FileHash -LiteralPath (Join-Path $root 'data\chat-history.md')).Hash -ne $before){throw 'Chat changed'}
$isolated=[IO.File]::ReadAllText($result.Config)
if($isolated -match 'Legacy|Other.dat' -or $isolated -notmatch 'template_solidpart' -or $isolated.IndexOf($root+'\protk.dat') -lt 0){throw 'Isolated config incorrect'}
if([IO.File]::ReadAllText((Join-Path $result.Backup 'shared-config.pro')) -cne $original){throw 'Rollback backup mismatch'}
$again=& $enable -InstallRoot $root -ProfileRoot $profile -StandardsConfig $standards
if($again.Backup -ne $result.Backup){throw 'Idempotent migration changed state'}
$state=Get-Content -LiteralPath (Join-Path $root 'active.json') -Raw|ConvertFrom-Json
if($state.Config -ne $result.Config){throw 'Installer state did not follow isolated config'}
$startPlan=& (Join-Path $repo 'scripts\Start-BWTL.ps1') -InstallRoot $root -PlanOnly
if(-not $startPlan.Isolated -or $startPlan.Home -ne $result.Home){throw 'Launcher profile incorrect'}
[IO.File]::WriteAllText((Join-Path $result.Workspace 'config.pro'),'unexpected',$utf8)
$rejected=$false
try{& (Join-Path $repo 'scripts\Start-BWTL.ps1') -InstallRoot $root -PlanOnly|Out-Null}catch{$rejected=$true}
if(-not $rejected){throw 'Unexpected startup config was accepted'}
'ISOLATION PASS: plan, migration, legacy preservation, chat preservation, backup, repeatability, launch plan and extra-config rejection'
'Test artifacts: '+$work
