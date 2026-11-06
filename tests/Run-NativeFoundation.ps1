[CmdletBinding()]
param([ValidateSet('Core','Solid','Machining','Custom')][string]$Suite='Core',[string]$Root,[string]$CustomSource,[Parameter(Mandatory=$true)][string]$WorkDirectory,[string]$StandardsConfig='C:\PTC\Pro_Stds-C\00_config\config.pro',[string]$CreoBase='C:\Program Files\PTC\Creo 10.0.0.0')
$ErrorActionPreference='Stop'
$repo=if($Root){[IO.Path]::GetFullPath($Root)}else{Split-Path -Parent $PSScriptRoot};$testRoot=Join-Path $repo "tests";$WorkDirectory=[IO.Path]::GetFullPath($WorkDirectory)
if(Test-Path -LiteralPath $WorkDirectory){throw 'Native test work directory must be new.'}
foreach($name in @('home','alternate','workspace','cache','temp')){New-Item -ItemType Directory -Path (Join-Path $WorkDirectory $name) -Force|Out-Null}
$source=if($Suite -eq 'Core'){'native_core_selfcheck.c'}elseif($Suite -eq 'Solid'){'native_solid.c'}elseif($Suite -eq 'Machining'){'native_machining.c'}else{'native_development.c'}
$sourcePath=Join-Path $testRoot $source
if($Suite -eq 'Custom' -and $CustomSource){$sourcePath=[IO.Path]::GetFullPath($CustomSource);if(-not $sourcePath.StartsWith($testRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Custom native fixture must be inside BWTL tests.'}}
$ptk=Join-Path $CreoBase 'Common Files\protoolkit'
$vs=(& (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe') -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath|Select-Object -First 1)
if(-not $vs){throw 'Visual C++ tools not found.'}
$libraries=@('protk_dllmd_NU.lib','ucore.lib','udata.lib')|ForEach-Object {'"'+(Join-Path $ptk ('x86e_win64\obj\'+$_))+'"'}
$command='cl /nologo /MD /W3 /utf-8 /LD /DPRO_MACHINE=36 /DPRO_OS=4 /I"'+(Join-Path $ptk 'includes')+'" "'+$sourcePath+'" /Fo"native-test.obj" /link /OUT:"native-test.dll" '+($libraries -join ' ')+' kernel32.lib user32.lib gdi32.lib psapi.lib advapi32.lib ws2_32.lib shell32.lib wsock32.lib mpr.lib winspool.lib netapi32.lib comdlg32.lib ole32.lib windowscodecs.lib uuid.lib'
$runner=@('@echo off',('call "'+(Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat')+'" >nul'),'if errorlevel 1 exit /b 1',('cd /d "'+$WorkDirectory+'"'),$command,'exit /b %ERRORLEVEL%')
[IO.File]::WriteAllLines((Join-Path $WorkDirectory 'build.cmd'),$runner,[Text.Encoding]::Default)
& $env:ComSpec /d /c (Join-Path $WorkDirectory 'build.cmd') > (Join-Path $WorkDirectory 'build.log') 2>&1
if($LASTEXITCODE){throw ('Native compile failed: '+(Join-Path $WorkDirectory 'build.log'))}
& (Join-Path $CreoBase 'Parametric\bin\protk_unlock.bat') (Join-Path $WorkDirectory 'native-test.dll')
if($LASTEXITCODE){throw 'Official Toolkit unlock failed.'}
$utf8=[Text.UTF8Encoding]::new($false)
$registration="NAME BWTLNative$Suite`r`nSTARTUP dll`r`nEXEC_FILE $(Join-Path $WorkDirectory 'native-test.dll')`r`nTEXT_DIR $(Join-Path $repo 'resources')`r`nALLOW_STOP TRUE`r`nDELAY_START FALSE`r`nEND`r`n"
[IO.File]::WriteAllText((Join-Path $WorkDirectory 'native-test.dat'),$registration,$utf8)
$config=if($StandardsConfig -and (Test-Path -LiteralPath $StandardsConfig -PathType Leaf)){[IO.File]::ReadAllText($StandardsConfig)}else{''}
$config=[regex]::Replace($config,'(?ms)^! BWTL BEGIN\r?\n.*?^! BWTL END(?:\r?\n)?','')
$config=[regex]::Replace($config,'(?im)^\s*(?:protkdat|toolkit_registry_file|prodevdat|creotkdat)\s+[^\r\n]*','')
$config+="`r`nprotkdat $(Join-Path $WorkDirectory 'native-test.dat')`r`ntrail_dir $(Join-Path $WorkDirectory 'workspace')`r`n"
[IO.File]::WriteAllText((Join-Path $WorkDirectory 'home\config.pro'),$config,$utf8)
$start=[Diagnostics.ProcessStartInfo]::new((Join-Path $CreoBase 'Parametric\bin\parametric.exe'));$start.UseShellExecute=$false;$start.WorkingDirectory=Join-Path $WorkDirectory 'workspace'
$start.EnvironmentVariables['PTC_WF_ROOT']=Join-Path $WorkDirectory 'cache';$start.EnvironmentVariables['TEMP']=Join-Path $WorkDirectory 'temp';$start.EnvironmentVariables['TMP']=Join-Path $WorkDirectory 'temp'
$start.EnvironmentVariables['HOME']=Join-Path $WorkDirectory 'home';$start.EnvironmentVariables['PTC_CREO_ALT_SETTINGS_PATH']=Join-Path $WorkDirectory 'alternate'
$start.EnvironmentVariables['BWTL_ISOLATED_SESSION']='1';$start.EnvironmentVariables['BWTL_PROFILE_ROOT']=$WorkDirectory
$start.EnvironmentVariables['BWTL_NATIVE_CORE_TEST']='yes';$start.EnvironmentVariables['BWTL_FOUNDATION_SELFTEST']='yes'
$start.EnvironmentVariables['PRO_LANG']='chs';$start.EnvironmentVariables['LANG']='chs'
$process=[Diagnostics.Process]::Start($start)
[pscustomobject]@{Suite=$Suite;WorkDirectory=$WorkDirectory;LauncherPID=$process.Id;Status='Started; verify native logs after Creo trust confirmation if requested.'}
