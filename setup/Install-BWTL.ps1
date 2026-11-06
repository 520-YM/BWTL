[CmdletBinding()]
param([string]$PackageRoot,[string]$InstallRoot,[string]$ConfigPath,[string]$CodexPath,[switch]$Silent,[switch]$PlanOnly,[switch]$Uninstall,[switch]$Rollback,[switch]$NoShortcuts,[switch]$DeleteProgramFiles,[switch]$DeleteData,[switch]$Diagnose,[switch]$PreserveData,[string]$CreoExecutable=(Join-Path $env:ProgramFiles 'PTC\Creo 10.0.0.0\Parametric\bin\parametric.exe'))
$ErrorActionPreference='Stop'
if(-not $PackageRoot){$PackageRoot=Split-Path -Parent $MyInvocation.MyCommand.Path}
$script:utf8=[Text.UTF8Encoding]::new($false)
if(-not $InstallRoot){$InstallRoot=Join-Path $env:LOCALAPPDATA 'BWTL'}
$InstallRoot=[IO.Path]::GetFullPath($InstallRoot)
$PackageRoot=[IO.Path]::GetFullPath($PackageRoot)
if(-not(Test-Path -LiteralPath (Join-Path $PackageRoot 'VERSION')) -and (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) 'VERSION'))){$PackageRoot=Split-Path -Parent $PSScriptRoot}
if(-not(Test-Path -LiteralPath (Join-Path $PackageRoot 'VERSION')) -and (Test-Path -LiteralPath (Join-Path $InstallRoot 'active.json'))){$installed=Get-Content -LiteralPath (Join-Path $InstallRoot 'active.json') -Raw -Encoding UTF8|ConvertFrom-Json;$PackageRoot=$installed.Directory}
. (Join-Path $PSScriptRoot 'Install-Support.ps1')
$script:marker='(?ms)^! BWTL BEGIN\r?\n.*?^! BWTL END(?:\r?\n)?'
function Write-Json($Path,$Value){[IO.File]::WriteAllText($Path,($Value|ConvertTo-Json -Depth 8),$script:utf8)}
function Read-Config($Path){
    if(-not(Test-Path -LiteralPath $Path)){return @{Text='';Encoding=$script:utf8}}
    $bytes=[IO.File]::ReadAllBytes($Path)
    if($bytes.Length -ge 2 -and $bytes[0] -eq 255 -and $bytes[1] -eq 254){return @{Text=[IO.File]::ReadAllText($Path,[Text.Encoding]::Unicode);Encoding=[Text.Encoding]::Unicode}}
    if($bytes.Length -ge 2 -and $bytes[0] -eq 254 -and $bytes[1] -eq 255){return @{Text=[IO.File]::ReadAllText($Path,[Text.Encoding]::BigEndianUnicode);Encoding=[Text.Encoding]::BigEndianUnicode}}
    try{$text=([Text.UTF8Encoding]::new($false,$true)).GetString($bytes);$encoding=$script:utf8;if($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191){$text=$text.TrimStart([char]0xfeff);$encoding=[Text.UTF8Encoding]::new($true)};return @{Text=$text;Encoding=$encoding}}
    catch{return @{Text=[Text.Encoding]::GetEncoding(936).GetString($bytes);Encoding=[Text.Encoding]::GetEncoding(936)}}
}
function Assert-Path($Path){if($Path -match '[\r\n"]' -or $Path.Length -gt 180){throw '路径过长或包含引号/换行，请选择较短的本地目录。'}}
function Native-Path($Path){
    $full=[IO.Path]::GetFullPath($Path)
    if($full -notmatch '[^\x21-\x7e]'){return $full}
    if(-not('BwtlNativePath' -as [type])){Add-Type -TypeDefinition 'using System;using System.Text;using System.Runtime.InteropServices;public static class BwtlNativePath{[DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]public static extern uint GetShortPathName(string path,StringBuilder output,uint size);}'}
    $buffer=[Text.StringBuilder]::new(1024);$n=[BwtlNativePath]::GetShortPathName($full,$buffer,1024)
    if(-not $n -or $buffer.ToString() -match '[^\x21-\x7e]'){throw 'Creo注册不支持此中文/空格路径且系统没有可用短路径。请选择纯英文、无空格的可写安装目录，例如 D:\BWTL。'}
    return $buffer.ToString()
}
function Get-Candidates {
    $items=[Collections.Generic.List[string]]::new()
    foreach($p in @((Join-Path $env:USERPROFILE 'config.pro'),(Join-Path $env:USERPROFILE 'Documents\config.pro'),'C:\PTC\Pro_Stds-C\00_config\config.pro')){if(Test-Path -LiteralPath $p){$items.Add($p)}}
    foreach($base in @('C:\PTC',(Join-Path $env:ProgramFiles 'PTC'))){if(Test-Path -LiteralPath $base){Get-ChildItem -LiteralPath $base -Directory -ErrorAction SilentlyContinue | ForEach-Object {foreach($relative in @('config.pro','00_config\config.pro','Common Files\text\config.pro','Parametric\text\config.pro')){$candidate=Join-Path $_.FullName $relative;if((Test-Path -LiteralPath $candidate) -and -not $items.Contains($candidate)){$items.Add($candidate)}}}}}
    foreach($registry in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall')){
        Get-ItemProperty ($registry+'\*') -ErrorAction SilentlyContinue | Where-Object {$_.DisplayName -match 'Creo.*Parametric' -and $_.InstallLocation} | ForEach-Object {
            foreach($relative in @('config.pro','text\config.pro','Common Files\text\config.pro','Parametric\text\config.pro')){$candidate=Join-Path $_.InstallLocation $relative;if((Test-Path -LiteralPath $candidate) -and -not $items.Contains($candidate)){$items.Add($candidate)}}
        }
    }
    $state=Join-Path $InstallRoot 'active.json';if(Test-Path -LiteralPath $state){$a=Get-Content -LiteralPath $state -Raw -Encoding UTF8|ConvertFrom-Json;if($a.Config -and -not $items.Contains($a.Config)){$items.Insert(0,$a.Config)}}
    return $items.ToArray()
}
function Verify-Payload {
    $manifest=Join-Path $PackageRoot 'payload-manifest.json';if(-not(Test-Path -LiteralPath $manifest)){throw '安装包缺少完整性清单。'}
    $root=$PackageRoot.TrimEnd('\')+'\'
    foreach($entry in (Get-Content -LiteralPath $manifest -Raw -Encoding UTF8|ConvertFrom-Json)){
        $path=[IO.Path]::GetFullPath((Join-Path $PackageRoot $entry.Path));if(-not $path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw '安装包路径越界。'}
        if(-not(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $entry.SHA256){throw ('安装包文件校验失败：'+$entry.Path)}
    }
}
function Add-Seed($Data,$Seed,$Version){
    New-Item -ItemType Directory -Force -Path $Data|Out-Null
    if(Test-Path -LiteralPath (Join-Path $InstallRoot 'instance.json')){
        Get-ChildItem -LiteralPath $Seed -Filter '*.md' -File | Where-Object {$_.Name -notin @('chat-history.md','imported-owner-history.md')} | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $Data $_.Name) -Force
        }
        return
    }
    $history=Join-Path $Seed 'chat-history.md';$dest=Join-Path $Data 'chat-history.md';$previous=Join-Path $Data 'imported-owner-history.md'
    if(Test-Path -LiteralPath $history){
        $source=[IO.File]::ReadAllText($history);$prior=if(Test-Path -LiteralPath $previous){[IO.File]::ReadAllText($previous)}else{''}
        if(-not(Test-Path -LiteralPath $dest)){Copy-Item -LiteralPath $history -Destination $dest}
        elseif($source -ne $prior){$delta=$source;if($prior -and $source.StartsWith($prior,[StringComparison]::Ordinal)){$delta=$source.Substring($prior.Length)};if($delta){[IO.File]::AppendAllText($dest,"`r`n## 导入所有者历史 · $Version`r`n"+$delta,$script:utf8)}}
        [IO.File]::WriteAllText($previous,$source,$script:utf8)
    }
    $memory=Join-Path $Data 'memory.txt';$lines=@();if(Test-Path -LiteralPath $memory){$lines=@([IO.File]::ReadAllLines($memory))}
    $seedMemory=Join-Path $Seed 'memory.txt';if(Test-Path -LiteralPath $seedMemory){foreach($line in [IO.File]::ReadAllLines($seedMemory)){if($line -and $lines -cnotcontains $line){$lines+=,$line}}};[IO.File]::WriteAllLines($memory,[string[]]$lines,$script:utf8)
    foreach($file in Get-ChildItem -LiteralPath $Seed -File){
        if($file.Name -in @('chat-history.md','memory.txt','pending-plan.txt','runtime.log')){continue}
        $target=Join-Path $Data $file.Name;if($file.Name -eq 'feature-guide.md' -or $file.Extension -eq '.md' -or -not(Test-Path -LiteralPath $target)){Copy-Item -LiteralPath $file.FullName -Destination $target -Force}
    }
    if(-not(Test-Path -LiteralPath (Join-Path $Data 'pending-plan.txt'))){[IO.File]::WriteAllText((Join-Path $Data 'pending-plan.txt'),'',$script:utf8)}
    Write-Json (Join-Path $Data 'seed-import.json') @{Version=$Version;Imported=(Get-Date -Format o);Method='Full history retained; local query retrieval; memory merge; no model training'}
}
function Set-Registration($VersionDirectory){
    $registration=Join-Path $InstallRoot 'protk.dat';$native=Native-Path $VersionDirectory;$dll=Join-Path $native 'bin\BWTL.dll';$resources=Join-Path $native 'resources'
    [IO.File]::WriteAllText($registration,"NAME BWTL`r`nSTARTUP dll`r`nEXEC_FILE $dll`r`nTEXT_DIR $resources`r`nALLOW_STOP TRUE`r`nDELAY_START FALSE`r`nEND`r`n",$script:utf8)
}
function Get-Isolation($Root){
    $path=Join-Path $Root 'isolation.json'
    if(Test-Path -LiteralPath $path){return Get-Content -LiteralPath $path -Raw -Encoding UTF8|ConvertFrom-Json}
    if(Test-Path -LiteralPath (Join-Path $Root 'active.json')){throw '此安装使用旧的共享配置。请先按发行说明迁移到 BWTL 专用配置。'}
    $profile=$Root.TrimEnd('\')+'-Profile'
    [pscustomobject]@{
        ProfileRoot=$profile;Home=(Join-Path $profile 'home');Alternate=(Join-Path $profile 'alternate')
        Workspace=(Join-Path $profile 'workspace');Cache=(Join-Path $profile 'cache')
        Temp=(Join-Path $profile 'temp');Logs=(Join-Path $profile 'logs')
        Config=(Join-Path $profile 'home\config.pro');CreoExecutable=[IO.Path]::GetFullPath($CreoExecutable)
    }
}
function Invoke-Install {
    $InstallRoot=[IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
    $isolationPath=Join-Path $InstallRoot 'isolation.json'
    $newProfile=-not(Test-Path -LiteralPath $isolationPath)
    $isolation=Get-Isolation $InstallRoot
    $ConfigPath=if($ConfigPath){[IO.Path]::GetFullPath($ConfigPath)}else{$isolation.Config}
    if($ConfigPath -ine $isolation.Config){throw '本版本只允许更新BWTL专用配置，不修改旧Codex–Creo注册。'}
    [void](Assert-ManagedTree $InstallRoot)
    if($InstallRoot.TrimEnd('\') -ieq $PackageRoot.TrimEnd('\') -or $InstallRoot.StartsWith($PackageRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw '安装目录不能位于安装包临时解压目录内。'}
    if($ConfigPath.StartsWith($InstallRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Creo配置文件不能放在插件安装目录内。'}
    Assert-Path $InstallRoot;Assert-Path $ConfigPath;if($CodexPath){Assert-Path $CodexPath;if(-not(Test-Path -LiteralPath $CodexPath -PathType Leaf) -or [IO.Path]::GetFileName($CodexPath) -ine 'codex.exe'){throw '请选择有效的 codex.exe。'}}
    if([IO.Path]::GetFileName($ConfigPath) -ine 'config.pro' -or (-not $newProfile -and -not(Test-Path -LiteralPath (Split-Path -Parent $ConfigPath)))){throw 'BWTL 专用 config.pro 路径无效。'}
    if($newProfile -and (Test-Path -LiteralPath $isolation.ProfileRoot)){throw 'BWTL 专用配置目录已存在。请选择另一安装目录或恢复原 isolation.json。'}
    if(-not(Test-Path -LiteralPath $isolation.CreoExecutable -PathType Leaf)){throw '未找到 Creo Parametric，请填写 parametric.exe 的完整路径。'}
    if(-not [Environment]::Is64BitOperatingSystem){throw '本安装包需要64位Windows。'}
    Verify-Payload
    foreach($required in @('resources/ribbon/toolkitribbonui.rbn','resources/text/ribbon/toolkitribbonui.rbn','bin/BWTL.dll')){if(-not(Test-Path -LiteralPath (Join-Path $PackageRoot $required))){throw ('安装包缺少必要组件：'+$required)}}
    $version=([IO.File]::ReadAllText((Join-Path $PackageRoot 'VERSION'))).Trim();if($version -notmatch '^\d+\.\d+\.\d+(?:-[\w.]+)?$'){throw '版本号无效。'}
    $versionDirectory=Join-Path $InstallRoot ('versions\'+$version);$data=Join-Path $InstallRoot 'data';$activePath=Join-Path $InstallRoot 'active.json'
    if($PlanOnly){return [pscustomobject]@{Version=$version;InstallRoot=$InstallRoot;Config=$ConfigPath;Data=$data;VersionDirectory=$versionDirectory}}
    if($newProfile){
        New-Item -ItemType Directory -Force -Path $InstallRoot,$isolation.Home,$isolation.Alternate,$isolation.Workspace,$isolation.Cache,$isolation.Temp,$isolation.Logs|Out-Null
        $logs=Native-Path $isolation.Logs
        [IO.File]::WriteAllText($ConfigPath,"trail_dir $logs`r`n",$script:utf8)
        Write-Json $isolationPath $isolation
    }
    $stamp=Get-Date -Format 'yyyyMMdd-HHmmss-ffff';$backup=Join-Path $InstallRoot ('backups\'+$stamp)
    New-Item -ItemType Directory -Force -Path $backup|Out-Null
    $prior=$null;if(Test-Path -LiteralPath $activePath){$prior=Get-Content -LiteralPath $activePath -Raw -Encoding UTF8|ConvertFrom-Json;Copy-Item -LiteralPath $activePath -Destination (Join-Path $backup 'active.json')}
    if($prior -and $prior.Config -ine $ConfigPath){throw '升级必须使用原配置文件，或先卸载原注册后重新选择。'}
    $old=Read-Config $ConfigPath;$existed=Test-Path -LiteralPath $ConfigPath
    if($existed){Copy-Item -LiteralPath $ConfigPath -Destination (Join-Path $backup 'config.pro')}
    foreach($name in @('protk.dat','active.json')){if(Test-Path -LiteralPath (Join-Path $InstallRoot $name)){Copy-Item -LiteralPath (Join-Path $InstallRoot $name) -Destination (Join-Path $backup $name) -Force}}
    if(Test-Path -LiteralPath $data){Copy-Item -LiteralPath $data -Destination (Join-Path $backup 'data') -Recurse}
    $createdVersion=$false
    try{
        if(Test-Path -LiteralPath $versionDirectory){
            if((Get-FileHash -LiteralPath (Join-Path $versionDirectory 'payload-manifest.json')).Hash -ne (Get-FileHash -LiteralPath (Join-Path $PackageRoot 'payload-manifest.json')).Hash){throw '相同版本号已有不同内容，拒绝覆盖；请使用新版本号。'}
        }else{New-Item -ItemType Directory -Force -Path $versionDirectory|Out-Null;$createdVersion=$true;foreach($name in @('bin','resources','src','docs','tests','setup','scripts','compatibility','archive','seed','VERSION','CHANGELOG.md','README.md','DEPLOYMENT.json','payload-manifest.json','build.cmd','AGENTS.md')){$path=Join-Path $PackageRoot $name;if(Test-Path -LiteralPath $path){Copy-Item -LiteralPath $path -Destination (Join-Path $versionDirectory $name) -Recurse}}}
        if(-not $PreserveData -or -not $prior){Add-Seed $data (Join-Path $PackageRoot 'seed') $version}
        if(-not $CodexPath -and $prior -and $prior.CodexPath){$CodexPath=$prior.CodexPath}
        [IO.File]::WriteAllText((Join-Path $versionDirectory 'settings.ini'),"[Runtime]`r`nCodexPath=$CodexPath`r`nDataDirectory=$data`r`n",[Text.Encoding]::Unicode)
        Set-Registration $versionDirectory
        $registration=Join-Path (Native-Path $InstallRoot) 'protk.dat';$block="! BWTL BEGIN`r`nprotkdat $registration`r`n! BWTL END`r`n"
        $new=if($old.Text -match $script:marker){[regex]::Replace($old.Text,$script:marker,[Text.RegularExpressions.MatchEvaluator]{param($m)$block})}else{
            $legacy='(?im)^([ \t]*protkdat[ \t]+[^\r\n]*creo-native-window[\\/]protk\.dat[^\r\n]*)\r?$'
            [regex]::Replace($old.Text,$legacy,'! BWTL migrated legacy registration: $1').TrimEnd()+"`r`n"+$block
        }
        [IO.File]::WriteAllText($ConfigPath,$new,$old.Encoding)
        $state=[ordered]@{Version=$version;Directory=$versionDirectory;Config=$ConfigPath;CodexPath=$CodexPath;Data=$data;PreviousVersion=if($prior -and $prior.Version -ne $version){$prior.Version}elseif($prior){$prior.PreviousVersion}else{''};PreviousDirectory=if($prior -and $prior.Version -ne $version){$prior.Directory}elseif($prior){$prior.PreviousDirectory}else{''};Installed=(Get-Date -Format o);Backup=$backup}
        Write-Json $activePath $state
        foreach($file in @('Install-BWTL.ps1','Install-Support.ps1','Manage.cmd','BWTL-Manage.exe','BWTL-Uninstall.exe')){Copy-Item -LiteralPath (Join-Path $PackageRoot ('setup\'+$file)) -Destination (Join-Path $InstallRoot $file) -Force}
        foreach($file in @('Start-BWTL.ps1','Enable-BWTLIsolation.ps1')){Copy-Item -LiteralPath (Join-Path $PackageRoot ('scripts\'+$file)) -Destination (Join-Path $InstallRoot $file) -Force}
        if(-not $NoShortcuts){
            $menu=Join-Path ([Environment]::GetFolderPath('Programs')) '霸威天龙';New-Item -ItemType Directory -Force -Path $menu|Out-Null;$shell=New-Object -ComObject WScript.Shell
            foreach($item in @(@('安装管理与升级',''),@('卸载霸威天龙','-Uninstall'),@('回退上一版本','-Rollback'),@('导出安装诊断','-Diagnose'))){$shortcut=$shell.CreateShortcut((Join-Path $menu ($item[0]+'.lnk')));$shortcut.TargetPath=Join-Path $InstallRoot 'BWTL-Manage.exe';$shortcut.Arguments=$item[1];$shortcut.WindowStyle=7;$shortcut.Save()}
            $launcher=Join-Path $InstallRoot 'Start-BWTL.ps1'
            $shortcut=$shell.CreateShortcut((Join-Path $menu 'Creo（BWTL）.lnk'))
            $shortcut.TargetPath=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
            $shortcut.Arguments='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+$launcher+'" -InstallRoot "'+$InstallRoot+'" -NextAvailable'
            $shortcut.IconLocation=$isolation.CreoExecutable+',0';$shortcut.Save()
        }
        return $state
    }catch{
        if($existed){Copy-Item -LiteralPath (Join-Path $backup 'config.pro') -Destination $ConfigPath -Force}
        foreach($name in @('protk.dat','active.json')){if(Test-Path -LiteralPath (Join-Path $backup $name)){Copy-Item -LiteralPath (Join-Path $backup $name) -Destination (Join-Path $InstallRoot $name) -Force}}
        # Retain the failed version and all data for diagnosis; never delete chats.
        throw ('安装未完成，已尝试恢复启动配置；备份：'+$backup+'。'+$_.Exception.Message)
    }
}
function Assert-ManagedTree($Path){
    $root=[IO.Path]::GetFullPath($InstallRoot).TrimEnd('\');$full=[IO.Path]::GetFullPath($Path).TrimEnd('\')
    if($root -eq [IO.Path]::GetPathRoot($root).TrimEnd('\') -or $root -eq $env:USERPROFILE -or ($full -ne $root -and -not $full.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase))){throw '卸载路径不在已验证的插件安装目录内。'}
    $ancestor=$full
    while($ancestor){if(Test-Path -LiteralPath $ancestor){if((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw '安装路径含链接或联接点，未执行删除。'}};$parent=Split-Path -Parent $ancestor;if($parent -eq $ancestor){break};$ancestor=$parent}
    if(Test-Path -LiteralPath $full -PathType Container){
        $stack=[Collections.Generic.Stack[string]]::new();$stack.Push($full)
        while($stack.Count){foreach($item in Get-ChildItem -LiteralPath $stack.Pop() -Force){if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw '待清理目录含链接或联接点，未执行删除。'};if($item.PSIsContainer){$stack.Push($item.FullName)}}}
    }
    return $full
}
function Remove-ManagedPath($Path){
    $checked=Assert-ManagedTree $Path
    if(Test-Path -LiteralPath $checked){Remove-Item -LiteralPath $checked -Recurse -Force}
}
function Remove-OwnedShortcuts {
    if($NoShortcuts){return}
    $menu=Join-Path ([Environment]::GetFolderPath('Programs')) '霸威天龙';if(-not(Test-Path -LiteralPath $menu)){return}
    $shell=New-Object -ComObject WScript.Shell
    foreach($name in @('安装管理与升级','卸载（保留聊天）','卸载霸威天龙','回退上一版本','导出安装诊断','Creo（BWTL）')){
        $path=Join-Path $menu ($name+'.lnk');if(Test-Path -LiteralPath $path){$link=$shell.CreateShortcut($path);if($link.TargetPath.StartsWith($InstallRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or $link.Arguments.Contains('"'+(Join-Path $InstallRoot 'Install-BWTL.ps1')+'"') -or $link.Arguments.Contains('"'+(Join-Path $InstallRoot 'Start-BWTL.ps1')+'"')){Remove-Item -LiteralPath $path -Force}}
    }
    if(@(Get-ChildItem -LiteralPath $menu -Force).Count -eq 0){Remove-Item -LiteralPath $menu}
}
function New-UninstallDialog {
    Add-Type -AssemblyName System.Windows.Forms;Add-Type -AssemblyName System.Drawing
    [Windows.Forms.Application]::EnableVisualStyles()
    $dialog=New-Object Windows.Forms.Form;$dialog.Text='霸威天龙 · 一键卸载';$dialog.Size=New-Object Drawing.Size(660,355);$dialog.StartPosition='CenterScreen';$dialog.Font=New-Object Drawing.Font('Microsoft YaHei UI',10);$dialog.FormBorderStyle='FixedDialog';$dialog.MaximizeBox=$false
    $label=New-Object Windows.Forms.Label;$label.Text="卸载将移除本插件的 Creo 启动注册。请选择需要清理的内容：`n安装目录：$InstallRoot";$label.SetBounds(20,18,610,65);$dialog.Controls.Add($label)
    $program=New-Object Windows.Forms.CheckBox;$program.Name='DeleteProgramFiles';$program.Text='删除插件程序文件和历史版本';$program.Checked=$true;$program.SetBounds(20,90,600,28);$dialog.Controls.Add($program)
    $data=New-Object Windows.Forms.CheckBox;$data.Name='DeleteData';$data.Text='删除聊天记录、记忆、数据备份和内置历史副本';$data.Checked=$false;$data.SetBounds(20,132,610,28);$dialog.Controls.Add($data)
    $hint=New-Object Windows.Forms.Label;$hint.Text="默认保留数据，方便以后重新安装。勾选删除数据后无法通过插件恢复。`n仅清理此安装目录；下载的安装包、Creo、模型文件不在清理范围内。";$hint.SetBounds(20,178,610,58);$dialog.Controls.Add($hint)
    $ok=New-Object Windows.Forms.Button;$ok.Text='执行卸载';$ok.SetBounds(355,260,125,35);$ok.DialogResult=[Windows.Forms.DialogResult]::OK;$dialog.Controls.Add($ok)
    $cancel=New-Object Windows.Forms.Button;$cancel.Text='取消';$cancel.SetBounds(495,260,125,35);$cancel.DialogResult=[Windows.Forms.DialogResult]::Cancel;$dialog.Controls.Add($cancel);$dialog.CancelButton=$cancel
    return $dialog
}
function Invoke-Manage {
    $statePath=Join-Path $InstallRoot 'active.json';if(-not(Test-Path -LiteralPath $statePath)){throw '未找到本安装器管理的安装。'};$state=Get-Content -LiteralPath $statePath -Raw -Encoding UTF8|ConvertFrom-Json
    if($Rollback -and $state.Uninstalled){throw '当前已卸载启动注册，请先重新安装后再回退。'}
    if($Rollback){if(-not $state.PreviousDirectory -or -not(Test-Path -LiteralPath (Join-Path $state.PreviousDirectory 'bin\BWTL.dll'))){throw '没有可回退的上一版本。'};Set-Registration $state.PreviousDirectory;$a=$state.Version;$b=$state.Directory;$state.Version=$state.PreviousVersion;$state.Directory=$state.PreviousDirectory;$state.PreviousVersion=$a;$state.PreviousDirectory=$b;Write-Json $statePath $state;return '已切换上一版本；数据保留，手动重启Creo后生效。'}
    $validatedRoot=Assert-ManagedTree $InstallRoot
    $expectedData=Join-Path $validatedRoot 'data';$versions=Join-Path $validatedRoot 'versions'
    if([IO.Path]::GetFullPath($state.Data).TrimEnd('\') -ine $expectedData -or -not [IO.Path]::GetFullPath($state.Directory).StartsWith($versions+'\',[StringComparison]::OrdinalIgnoreCase)){throw '安装状态中的数据或版本路径异常，未执行卸载。'}
    $targets=[Collections.Generic.List[string]]::new()
    if($DeleteProgramFiles){$targets.Add($versions)}
    if($DeleteData){$targets.Add((Join-Path $validatedRoot 'diagnostics'))}
    if($DeleteData){
        $targets.Add($expectedData)
        if(Test-Path -LiteralPath (Join-Path $validatedRoot 'backups')){foreach($backup in Get-ChildItem -LiteralPath (Join-Path $validatedRoot 'backups') -Directory){$targets.Add((Join-Path $backup.FullName 'data'))}}
        if(-not $DeleteProgramFiles -and (Test-Path -LiteralPath $versions)){foreach($version in Get-ChildItem -LiteralPath $versions -Directory){$targets.Add((Join-Path $version.FullName 'seed'));$targets.Add((Join-Path $version.FullName 'archive'))}}
    }
    foreach($target in $targets){[void](Assert-ManagedTree $target)}
    if(($DeleteProgramFiles -or $DeleteData) -and (Test-Path -LiteralPath $versions)){
        foreach($dll in Get-ChildItem -LiteralPath $versions -Filter '*.dll' -Recurse -File){try{$probe=[IO.File]::Open($dll.FullName,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);$probe.Dispose()}catch{throw '插件文件正在使用或不可写，请保存工作并退出使用此插件的Creo后，再执行卸载。'}}
    }
    if($PlanOnly){return [pscustomobject]@{InstallRoot=$validatedRoot;DeleteProgramFiles=[bool]$DeleteProgramFiles;DeleteData=[bool]$DeleteData;Targets=$targets.ToArray()}}
    if(Test-Path -LiteralPath $state.Config){$old=Read-Config $state.Config;$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-ffff';Copy-Item -LiteralPath $state.Config -Destination ($state.Config+'.bwtl-uninstall-'+$stamp);[IO.File]::WriteAllText($state.Config,[regex]::Replace($old.Text,$script:marker,''),$old.Encoding)}
    $state|Add-Member -NotePropertyName Uninstalled -NotePropertyValue (Get-Date -Format o) -Force;Write-Json $statePath $state
    foreach($target in $targets){Remove-ManagedPath $target}
    if($DeleteProgramFiles){
        Remove-OwnedShortcuts
        foreach($name in @('protk.dat','Manage.cmd','BWTL-Manage.exe')){Remove-ManagedPath (Join-Path $validatedRoot $name)}
        # Keep the small cleanup entry and receipt while data remains, so the
        # user can subsequently choose to delete it without reinstalling.
        if($DeleteData){foreach($name in @('backups','BWTL-Uninstall.exe','Install-BWTL.ps1','Install-Support.ps1','active.json','diagnostics')){Remove-ManagedPath (Join-Path $validatedRoot $name)};if(@(Get-ChildItem -LiteralPath $validatedRoot -Force).Count -eq 0){Remove-Item -LiteralPath $validatedRoot}}
    }
    $programText=if($DeleteProgramFiles){'程序和历史版本已删除。'}else{'程序文件已保留。'};$dataText=if($DeleteData){'聊天、记忆、备份数据及内置历史副本已删除。'}else{'聊天、记忆及数据备份已保留。'}
    return ('卸载完成：已移除Creo启动注册。'+$programText+$dataText+'下载的安装包和其他软件未删除。')
}
if($Diagnose){$report=Export-BwtlDiagnostics $InstallRoot;if($Silent){$report}else{Add-Type -AssemblyName System.Windows.Forms;[Windows.Forms.MessageBox]::Show(('诊断报告已保存，请把该文件发给维护人员：'+[Environment]::NewLine+$report),'霸威天龙诊断')|Out-Null};return}
if($Uninstall -or $Rollback){
    if($Silent -or $PlanOnly){Invoke-Manage;return}
    Add-Type -AssemblyName System.Windows.Forms
    if($Uninstall){$dialog=New-UninstallDialog;try{if($dialog.ShowDialog() -ne 'OK'){return};$DeleteProgramFiles=$dialog.Controls['DeleteProgramFiles'].Checked;$DeleteData=$dialog.Controls['DeleteData'].Checked}finally{$dialog.Dispose()}}
    try{$result=Invoke-Manage;[Windows.Forms.MessageBox]::Show($result,'霸威天龙')|Out-Null}catch{[Windows.Forms.MessageBox]::Show(('卸载/回退未完成：'+$_.Exception.Message),'霸威天龙')|Out-Null};return
}
if($Silent -or $PlanOnly){Invoke-Install;return}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$form=New-Object Windows.Forms.Form;$form.Text='霸威天龙 · 安装与升级';$form.Size=New-Object Drawing.Size(690,535);$form.StartPosition='CenterScreen';$form.Font=New-Object Drawing.Font('Microsoft YaHei UI',10);$form.FormBorderStyle='FixedDialog';$form.MaximizeBox=$false
function Label($Text,$X,$Y,$W=600){$l=New-Object Windows.Forms.Label;$l.Text=$Text;$l.SetBounds($X,$Y,$W,30);$form.Controls.Add($l)}
Label '霸威天龙：原生Creo助手 · 完整工具与聊天记忆' 20 18
Label 'Creo 启动程序 parametric.exe（首次安装时使用）' 20 62
$creoBox=New-Object Windows.Forms.TextBox;$creoBox.SetBounds(20,95,630,30);$creoBox.Text=$CreoExecutable;$form.Controls.Add($creoBox)
Label '安装目录（按用户安装，默认无需管理员）' 20 137
$rootBox=New-Object Windows.Forms.TextBox;$rootBox.Text=$InstallRoot;$rootBox.SetBounds(20,168,630,30);$form.Controls.Add($rootBox)
Label 'Codex引擎路径（可留空自动检测；AI使用者需在本机登录）' 20 207
$codexBox=New-Object Windows.Forms.TextBox;$codexBox.Text=$CodexPath;$codexBox.SetBounds(20,238,630,30);$form.Controls.Add($codexBox)
Label '新安装创建专用配置；升级保留已有聊天和记忆。' 20 281
$status=New-Object Windows.Forms.Label;$status.SetBounds(20,435,630,50);$form.Controls.Add($status)
$install=New-Object Windows.Forms.Button;$install.Text='一键安装 / 升级';$install.SetBounds(490,375,160,40);$form.Controls.Add($install)
$uninstallButton=New-Object Windows.Forms.Button;$uninstallButton.Text='卸载 / 数据清理';$uninstallButton.SetBounds(20,375,150,40);$form.Controls.Add($uninstallButton)
$uninstallButton.Add_Click({try{Start-BwtlManagement '-Uninstall' $rootBox.Text}catch{[Windows.Forms.MessageBox]::Show($_.Exception.Message,'霸威天龙')|Out-Null}})
$diagnoseButton=New-Object Windows.Forms.Button;$diagnoseButton.Text='导出诊断';$diagnoseButton.SetBounds(180,375,130,40);$form.Controls.Add($diagnoseButton)
$diagnoseButton.Add_Click({try{$report=Export-BwtlDiagnostics $rootBox.Text;[Windows.Forms.MessageBox]::Show(('报告已保存：'+[Environment]::NewLine+$report),'霸威天龙')|Out-Null}catch{[Windows.Forms.MessageBox]::Show($_.Exception.Message,'霸威天龙')|Out-Null}})
$rollbackButton=New-Object Windows.Forms.Button;$rollbackButton.Text='回退上一版';$rollbackButton.SetBounds(320,375,140,40);$form.Controls.Add($rollbackButton)
$rollbackButton.Add_Click({try{Start-BwtlManagement '-Rollback' $rootBox.Text}catch{[Windows.Forms.MessageBox]::Show($_.Exception.Message,'霸威天龙')|Out-Null}})
$install.Add_Click({
    $install.Enabled=$false;try{$script:InstallRoot=$rootBox.Text;$script:CreoExecutable=$creoBox.Text;$script:CodexPath=$codexBox.Text;$status.Text='正在校验并安装…';$form.Refresh();$result=Invoke-Install;$status.Text='安装完成：'+$result.Version;[Windows.Forms.MessageBox]::Show("从开始菜单打开「Creo（BWTL）」，然后点击 Creo 顶部的 BWTL。AI 使用本机已登录的 Codex。",'BWTL')|Out-Null}catch{$status.Text='安装未完成';[Windows.Forms.MessageBox]::Show($_.Exception.Message,'BWTL')|Out-Null}finally{$install.Enabled=$true}
})
[void]$form.ShowDialog()
