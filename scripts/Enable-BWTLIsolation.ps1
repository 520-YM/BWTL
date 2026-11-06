[CmdletBinding()]
param([string]$InstallRoot=(Join-Path $env:LOCALAPPDATA 'BWTL'),[string]$ProfileRoot=(Join-Path $env:LOCALAPPDATA 'BWTL-Profile'),[string]$StandardsConfig='C:\PTC\Pro_Stds-C\00_config\config.pro',[string]$CreoExecutable='C:\Program Files\PTC\Creo 10.0.0.0\Parametric\bin\parametric.exe',[switch]$PlanOnly)
$ErrorActionPreference='Stop'
$InstallRoot=[IO.Path]::GetFullPath($InstallRoot);$ProfileRoot=[IO.Path]::GetFullPath($ProfileRoot)
$statePath=Join-Path $InstallRoot 'active.json';$state=Get-Content -LiteralPath $statePath -Raw -Encoding UTF8|ConvertFrom-Json
$isolationPath=Join-Path $InstallRoot 'isolation.json';$newConfig=Join-Path $ProfileRoot 'home\config.pro'
if(Test-Path -LiteralPath $isolationPath){
    $existing=Get-Content -LiteralPath $isolationPath -Raw -Encoding UTF8|ConvertFrom-Json
    if($state.Config -ine $existing.Config -or $existing.ProfileRoot -ine $ProfileRoot){throw '已有隔离状态与本次目录不一致。'}
    return $existing
}
if(Test-Path -LiteralPath $ProfileRoot){throw '请选择尚未使用的BWTL专用配置目录。'}
$sharedPath=[IO.Path]::GetFullPath($state.Config);$original=[IO.File]::ReadAllText($sharedPath)
$pattern='(?ms)^! BWTL BEGIN\r?\n.*?^! BWTL END(?:\r?\n)?'
$blocks=[regex]::Matches($original,$pattern)
if($blocks.Count -ne 1 -or $blocks[0].Value.IndexOf((Join-Path $InstallRoot 'protk.dat'),[StringComparison]::OrdinalIgnoreCase) -lt 0){throw '原配置未找到唯一的本安装BWTL注册块，未执行迁移。'}
$without=[regex]::Replace($original,$pattern,'')
if($without -cne $original.Remove($blocks[0].Index,$blocks[0].Length)){throw '配置差异超出BWTL注册块。'}
$settings=if($StandardsConfig -and (Test-Path -LiteralPath $StandardsConfig -PathType Leaf)){[IO.File]::ReadAllText($StandardsConfig)}else{''}
$settings=[regex]::Replace($settings,'(?ms)^! BWTL BEGIN\r?\n.*?^! BWTL END(?:\r?\n)?','')
$settings=[regex]::Replace($settings,'(?im)^\s*(?:protkdat|toolkit_registry_file|prodevdat|creotkdat)\s+[^\r\n]*','')
$backup=Join-Path $InstallRoot ('backups\isolation-'+(Get-Date -Format 'yyyyMMdd-HHmmss-ffff'))
$record=[ordered]@{ProfileRoot=$ProfileRoot;Home=(Join-Path $ProfileRoot 'home');Alternate=(Join-Path $ProfileRoot 'alternate');Workspace=(Join-Path $ProfileRoot 'workspace');Cache=(Join-Path $ProfileRoot 'cache');Temp=(Join-Path $ProfileRoot 'temp');Logs=(Join-Path $ProfileRoot 'logs');Config=$newConfig;OriginalSharedConfig=$sharedPath;Backup=$backup;CreoExecutable=$CreoExecutable;Created=(Get-Date -Format o)}
if($PlanOnly){return $record}
$utf8=[Text.UTF8Encoding]::new($false)
New-Item -ItemType Directory -Path $backup -Force|Out-Null
Copy-Item -LiteralPath $sharedPath -Destination (Join-Path $backup 'shared-config.pro')
Copy-Item -LiteralPath $statePath -Destination (Join-Path $backup 'active.json')
Copy-Item -LiteralPath (Join-Path $InstallRoot 'data') -Destination (Join-Path $backup 'data') -Recurse
try{
    foreach($path in @($record.Home,$record.Alternate,$record.Workspace,$record.Cache,$record.Temp,$record.Logs)){New-Item -ItemType Directory -Path $path -Force|Out-Null}
    $settings+="`r`ntrail_dir $($record.Logs)`r`n! BWTL BEGIN`r`nprotkdat $(Join-Path $InstallRoot 'protk.dat')`r`n! BWTL END`r`n"
    [IO.File]::WriteAllText($newConfig,$settings,$utf8)
    # Remove only BWTL's owned block. The legacy registration remains byte-for-byte.
    $originalBytes=[IO.File]::ReadAllBytes($sharedPath)
    $strict=[Text.UTF8Encoding]::new($false,$true);[void]$strict.GetString($originalBytes)
    $encoding=if($originalBytes.Length -ge 3 -and $originalBytes[0] -eq 239 -and $originalBytes[1] -eq 187 -and $originalBytes[2] -eq 191){[Text.UTF8Encoding]::new($true)}else{$utf8}
    [IO.File]::WriteAllText($sharedPath,$without.TrimStart([char]0xfeff),$encoding)
    $state.Config=$newConfig
    [IO.File]::WriteAllText($statePath,($state|ConvertTo-Json -Depth 8),$utf8)
    [IO.File]::WriteAllText($isolationPath,($record|ConvertTo-Json -Depth 8),$utf8)
    return [pscustomobject]$record
}catch{
    Copy-Item -LiteralPath (Join-Path $backup 'shared-config.pro') -Destination $sharedPath -Force
    Copy-Item -LiteralPath (Join-Path $backup 'active.json') -Destination $statePath -Force
    throw
}
