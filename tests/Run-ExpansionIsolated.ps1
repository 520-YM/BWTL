[CmdletBinding()]
param([ValidateSet('Core','Solid','Machining','Custom')][string]$Suite='Core',[string]$Root,[string]$CustomSource,[string]$FixtureProfileRoot,[switch]$ReuseFixture,[Parameter(Mandatory=$true)][string]$WorkDirectory,[string]$StandardsConfig='C:\PTC\Pro_Stds-C\00_config\config.pro',[string]$CreoBase='C:\Program Files\PTC\Creo 10.0.0.0')
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
$config=[IO.File]::ReadAllText($StandardsConfig)
$config=[regex]::Replace($config,'(?ms)^! BWTL BEGIN\r?\n.*?^! BWTL END(?:\r?\n)?','')
$config=[regex]::Replace($config,'(?im)^\s*(?:protkdat|toolkit_registry_file|prodevdat|creotkdat)\s+[^\r\n]*','')
$config+="`r`nprotkdat $(Join-Path $WorkDirectory 'native-test.dat')`r`ntrail_dir $(Join-Path $WorkDirectory 'workspace')`r`n"
$config += [Environment]::NewLine + 'open_protk_unsigned_apps always' + [Environment]::NewLine
[IO.File]::WriteAllText((Join-Path $WorkDirectory 'home\config.pro'),$config,$utf8)
$start=[Diagnostics.ProcessStartInfo]::new((Join-Path $CreoBase 'Parametric\bin\parametric.exe'));$start.UseShellExecute=$false;$start.WorkingDirectory=Join-Path $WorkDirectory 'workspace'
$start.EnvironmentVariables['PTC_WF_ROOT']=Join-Path $WorkDirectory 'cache';$start.EnvironmentVariables['TEMP']=Join-Path $WorkDirectory 'temp';$start.EnvironmentVariables['TMP']=Join-Path $WorkDirectory 'temp'
$start.EnvironmentVariables['HOME']=Join-Path $WorkDirectory 'home';$start.EnvironmentVariables['PTC_CREO_ALT_SETTINGS_PATH']=Join-Path $WorkDirectory 'alternate'
$start.EnvironmentVariables['BWTL_ISOLATED_SESSION']='1';$start.EnvironmentVariables['BWTL_PROFILE_ROOT']=$WorkDirectory
if($FixtureProfileRoot){
    if($Suite -ne 'Custom' -or !(Test-Path -LiteralPath (Join-Path $FixtureProfileRoot 'data/isolation-session.txt'))){throw 'Select a completed isolated fixture profile.'}
    $profile=(Resolve-Path -LiteralPath $FixtureProfileRoot).Path
    $start.EnvironmentVariables['BWTL_PROFILE_ROOT']=$profile
    $fixtureData=Join-Path $profile 'data'
    [IO.File]::WriteAllText((Join-Path $WorkDirectory 'settings.ini'),"[Runtime]`r`nDataDirectory=$fixtureData`r`n",[Text.Encoding]::Unicode)
}
if($ReuseFixture){$start.EnvironmentVariables['BWTL_TEST_REUSE']='1'}
$start.EnvironmentVariables['BWTL_OUTPUT_DRIVE']=[IO.Path]::GetPathRoot($WorkDirectory).Substring(0,1)
$start.EnvironmentVariables['BWTL_NATIVE_CORE_TEST']='yes';$start.EnvironmentVariables['BWTL_FOUNDATION_SELFTEST']='yes'
$start.EnvironmentVariables['PRO_LANG']='chs';$start.EnvironmentVariables['LANG']='chs'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.ComponentModel;
public static class PrivateProbeDesktop {
 [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Startup {
  public int cb; public string reserved, desktop, title; public int x,y,width,height,xCount,yCount,fill,flags;
  public short show, reserved2; public IntPtr reserved3, input,output,error;
 }
 [StructLayout(LayoutKind.Sequential)] struct Proc { public IntPtr process,thread; public int pid,tid; }
 [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 static extern IntPtr CreateDesktop(string name, IntPtr device, IntPtr mode, int flags, int access, IntPtr attributes);
 [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desk);
 static IntPtr liveDesktop;
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 static extern bool CreateProcess(string application, System.Text.StringBuilder command, IntPtr processAttr, IntPtr threadAttr,
   bool inherit, int flags, IntPtr environment, string cwd, ref Startup start, out Proc proc);
 public static int Start(string executable,string cwd,string name,string environment) {
  var desktop=CreateDesktop(name,IntPtr.Zero,IntPtr.Zero,0,0x1ff,IntPtr.Zero);
  if(desktop==IntPtr.Zero)throw new Win32Exception();
  var startup=new Startup(); startup.cb=Marshal.SizeOf(typeof(Startup)); startup.desktop=name;
  IntPtr env=Marshal.StringToHGlobalUni(environment);
  try {
    Proc result;
    if(!CreateProcess(executable,new System.Text.StringBuilder("\""+executable+"\""),IntPtr.Zero,IntPtr.Zero,
        false,0x400,env,cwd,ref startup,out result))throw new Win32Exception();
    CloseHandle(result.process); CloseHandle(result.thread); liveDesktop=desktop;
    return result.pid;
  } finally { Marshal.FreeHGlobal(env); if(liveDesktop==IntPtr.Zero)CloseDesktop(desktop); }
 }
}
"@
$values=foreach($name in ($start.EnvironmentVariables.Keys | Sort-Object)){ '{0}={1}' -f $name,$start.EnvironmentVariables[$name] }
$environment=($values -join [char]0)+[char]0+[char]0
$desktopName='BWTL-Probe-'+[Guid]::NewGuid().ToString('N').Substring(0,10)
$probePid=[PrivateProbeDesktop]::Start($start.FileName,$start.WorkingDirectory,$desktopName,$environment)
[pscustomobject]@{Suite=$Suite;WorkDirectory=$WorkDirectory;LauncherPID=$probePid;Status='Started on private desktop'}
