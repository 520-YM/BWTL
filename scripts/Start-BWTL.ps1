[CmdletBinding()]
param([string]$InstallRoot=(Join-Path $env:LOCALAPPDATA 'BWTL'),[switch]$PlanOnly,[switch]$NextAvailable)
$ErrorActionPreference='Stop'
$InstallRoot=[IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
# No argument means the normal desktop entry. Updates always pass an exact root.
$auto=($NextAvailable -or -not $PSBoundParameters.ContainsKey('InstallRoot')) -and -not $PlanOnly
$utf8=[Text.UTF8Encoding]::new($false)

function Read-Json($path){Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json}
function Write-Json($path,$value){[IO.File]::WriteAllText($path,($value | ConvertTo-Json -Depth 6),$utf8)}
function Get-LoadedBwtl {
    foreach($process in Get-Process -Name xtop -ErrorAction SilentlyContinue){
        try{
            foreach($module in $process.Modules | Where-Object ModuleName -ieq 'BWTL.dll'){
                [pscustomobject]@{Id=$process.Id;Path=$module.FileName}
            }
        }catch{
            if(-not $process.HasExited){throw '无法检查现有 Creo 会话的插件路径。'}
        }
    }
}
function Get-LaunchPid($root){
    $path=Join-Path $root 'launch.json'
    if(-not (Test-Path -LiteralPath $path)){return}
    $lease=Read-Json $path
    $process=Get-Process -Id $lease.PID -ErrorAction SilentlyContinue
    if($process -and $process.ProcessName -ieq 'parametric' -and $process.StartTime.ToUniversalTime().Ticks -eq [long]$lease.StartTicks){$process.Id}
}
function Assert-LocalPath($path){
    for($parent=$path;$parent;$parent=Split-Path -Parent $parent){
        if((Test-Path -LiteralPath $parent) -and ((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){
            throw "BWTL 会话路径含链接或联接点：$parent"
        }
    }
}
function New-Instance($root,$number){
    $primary=Read-Json (Join-Path $InstallRoot 'active.json')
    $base=Read-Json (Join-Path $InstallRoot 'isolation.json')
    if($primary.Config -ine $base.Config){throw '主会话的启动配置与隔离配置不一致。'}
    $profile=Join-Path ($InstallRoot+'-Instances') ('profiles\'+$number)
    $isolation=[ordered]@{ProfileRoot=$profile;Config=(Join-Path $profile 'home\config.pro');CreoExecutable=$base.CreoExecutable}
    Assert-LocalPath $root
    foreach($name in @('Home','Alternate','Workspace','Cache','Temp','Logs')){
        $directory=Join-Path $profile $name.ToLowerInvariant()
        Assert-LocalPath $directory
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
        $isolation[$name]=$directory
    }
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    $config=[IO.File]::ReadAllText($base.Config)
    $config=[regex]::Replace($config,'(?im)^[ \t]*(?:protkdat|creotkdat|prodevdat|toolkit_registry_file|trail_dir|last_session_directory_path|pro_spool_dir)\b[^\r\n]*(?:\r?\n|$)','')
    $config=[regex]::Replace($config,'(?im)^! BWTL (?:BEGIN|END|migrated legacy registration:)[^\r\n]*(?:\r?\n|$)','')
    if($config -match '(?i)creo_safe_resident|CreoSafeResident'){throw '额外会话配置含旧桥接注册。'}
    if(-not (Test-Path -LiteralPath $isolation.Config)){
        $lines=@($config.TrimEnd(),"trail_dir $($isolation.Logs)","pro_spool_dir $($isolation.Temp)","last_session_directory_path $($isolation.Cache)",'')
        [IO.File]::WriteAllText($isolation.Config,($lines -join [Environment]::NewLine),$utf8)
    }
    Write-Json (Join-Path $root 'isolation.json') $isolation
    Write-Json (Join-Path $root 'instance.json') @{Number=$number}
    $source=Join-Path $root 'source'
    New-Item -ItemType Directory -Force -Path $source | Out-Null
    foreach($name in @('bin','resources','src','docs','tests','setup','scripts','compatibility','archive','VERSION','CHANGELOG.md','README.md','DEPLOYMENT.json','build.cmd','AGENTS.md')){
        $entry=Join-Path $primary.Directory $name
        if(Test-Path -LiteralPath $entry){Copy-Item -LiteralPath $entry -Destination $source -Recurse -Force}
    }
    & (Join-Path $primary.Directory 'setup\Install-BWTL.ps1') -PackageRoot $primary.Directory -InstallRoot $root -ConfigPath $isolation.Config -CodexPath $primary.CodexPath -Silent -NoShortcuts | Out-Null
}

$mutex=[Threading.Mutex]::new($false,'Local\BWTL-Creo-Launch')
$held=$false
try{
    try{$held=$mutex.WaitOne([TimeSpan]::FromMinutes(3))}
    catch [Threading.AbandonedMutexException]{$held=$true}
    if(-not $held){throw '等待 BWTL 启动分配超时。'}
    $allLoaded=@(Get-LoadedBwtl)
    for($number=1;;$number++){
        $root=$InstallRoot
        if($auto -and $number -gt 1){$root=Join-Path ($InstallRoot+'-Instances') ([string]$number)}
        $loaded=@($allLoaded | Where-Object {$_.Path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)})
        $pending=Get-LaunchPid $root
        if(-not $auto -or (-not $loaded.Count -and -not $pending)){break}
    }
    if($auto -and $number -gt 1 -and -not (Test-Path -LiteralPath (Join-Path $root 'active.json'))){New-Instance $root $number}
    $instance=Join-Path $root 'instance.json'
    if(Test-Path -LiteralPath $instance){$number=(Read-Json $instance).Number}
    $state=Read-Json (Join-Path $root 'active.json')
    $isolation=Read-Json (Join-Path $root 'isolation.json')
    if($state.Config -ine $isolation.Config){throw 'BWTL 启动配置与隔离配置不一致，请修复安装。'}
    foreach($path in @($isolation.Home,$isolation.Alternate,$isolation.Workspace)){
        if(-not(Test-Path -LiteralPath $path -PathType Container)){throw 'BWTL 隔离目录缺失。'}
    }
    $config=[IO.File]::ReadAllText($state.Config)
    if($config -match '(?i)creo_safe_resident|CreoSafeResident'){throw 'BWTL 专用配置含旧桥接注册，已拒绝启动。'}
    $registration=Join-Path $root 'protk.dat'
    if($config.IndexOf($registration,[StringComparison]::OrdinalIgnoreCase) -lt 0){throw 'BWTL 专用注册缺失。'}
    foreach($directory in @($isolation.Alternate,$isolation.Workspace)){
        foreach($name in @('config.pro','config.sup','protk.dat','creotk.dat','prodev.dat')){
            if(Test-Path -LiteralPath (Join-Path $directory $name)){throw ('隔离启动目录出现额外配置，请检查：'+$name)}
        }
    }
    $exe=$isolation.CreoExecutable
    if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Creo 启动程序不存在。'}
    if($PlanOnly){
        return [pscustomobject]@{Executable=$exe;Instance=$number;Home=$isolation.Home;Workspace=$isolation.Workspace
            Cache=$isolation.Cache;Config=$state.Config;Data=$state.Data;Isolated=$true;ExistingBwtlPIDs=@($loaded | ForEach-Object Id);PendingLauncherPID=$pending}
    }
    if($loaded.Count -or $pending){throw "BWTL 会话 $number 已在运行或正在启动。"}
    $start=[Diagnostics.ProcessStartInfo]::new($exe)
    $start.UseShellExecute=$false
    $start.WorkingDirectory=$isolation.Workspace
    $start.EnvironmentVariables['HOME']=$isolation.Home
    $start.EnvironmentVariables['PTC_CREO_ALT_SETTINGS_PATH']=$isolation.Alternate
    $start.EnvironmentVariables['PTC_WF_ROOT']=$isolation.Cache
    $start.EnvironmentVariables['BWTL_PROFILE_ROOT']=$isolation.ProfileRoot
    $start.EnvironmentVariables['BWTL_ISOLATED_SESSION']='1'
    $start.EnvironmentVariables['BWTL_INSTANCE_NUMBER']=[string]$number
    $start.EnvironmentVariables['PRO_LANG']='chs'
    $start.EnvironmentVariables['LANG']='chs'
    $start.EnvironmentVariables['TEMP']=$isolation.Temp
    $start.EnvironmentVariables['TMP']=$isolation.Temp
    $process=[Diagnostics.Process]::Start($start)
    try{
        Write-Json (Join-Path $root 'launch.json') @{PID=$process.Id;StartTicks=$process.StartTime.ToUniversalTime().Ticks.ToString()}
    }finally{$process.Dispose()}
}finally{
    if($held){$mutex.ReleaseMutex()}
    $mutex.Dispose()
}
