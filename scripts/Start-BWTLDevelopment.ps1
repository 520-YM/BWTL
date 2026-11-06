[CmdletBinding()]
param([ValidateSet('Start','Validate','VerifyNative','Apply','Activate')][string]$Action='Start',[string]$InstallRoot=(Join-Path $env:LOCALAPPDATA 'BWTL'),[string]$SourceRoot=(Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'BWTL'),[string]$RequestFile,[string]$ImageListFile,[switch]$PrepareOnly,[switch]$PlanOnly,[switch]$NoShortcuts)
$ErrorActionPreference='Stop'
# Creo can inherit PowerShell 7 module paths. Resolve hashing from the
# Windows PowerShell installation used by the native development launcher.
if($PSVersionTable.PSEdition -eq 'Desktop'){
    Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
}
$utf8=[Text.UTF8Encoding]::new($false)
$OutputEncoding=$utf8
$InstallRoot=[IO.Path]::GetFullPath($InstallRoot)
if(Test-Path -LiteralPath (Join-Path $InstallRoot 'instance.json')){
    if(-not $PSBoundParameters.ContainsKey('SourceRoot')){$SourceRoot=Join-Path $InstallRoot 'source'}
    $NoShortcuts=$true
}
if(-not $PSBoundParameters.ContainsKey('SourceRoot') -and -not(Test-Path -LiteralPath (Join-Path $SourceRoot 'AGENTS.md'))){$SourceRoot=Join-Path $InstallRoot 'source'}
$SourceRoot=[IO.Path]::GetFullPath($SourceRoot)
$devRoot=Join-Path $InstallRoot 'development'
$active=Get-Content -LiteralPath (Join-Path $InstallRoot 'active.json') -Raw -Encoding UTF8|ConvertFrom-Json
$baselineRoot=$active.Directory
function Write-Json($path,$value){[IO.File]::WriteAllText($path,($value|ConvertTo-Json -Depth 12),$utf8)}
function Get-OwnedPath($base,$relative){
    if([IO.Path]::IsPathRooted($relative)){throw 'Expected a relative owned path.'}
    $path=[IO.Path]::GetFullPath((Join-Path $base $relative));$prefix=[IO.Path]::GetFullPath($base).TrimEnd('\')+'\'
    if(-not $path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Path escaped the BWTL job.'};return $path
}
function Write-State($state,$message){
    $script:state.State=$state;$script:state.Message=$message;$script:state.Updated=(Get-Date).ToString('o')
    Write-Json (Join-Path $job 'status.json') $script:state
    $text=$state+"`r`n"+$message
    [IO.File]::WriteAllText((Join-Path $job 'ui-status.tmp'),$text,$utf8)
    Move-Item -LiteralPath (Join-Path $job 'ui-status.tmp') -Destination (Join-Path $job 'ui-status.txt') -Force
}
function Source-Manifest($source){
    $files=@();foreach($name in @('src','resources','docs','tests','compatibility','scripts','setup','README.md','CHANGELOG.md','VERSION','DEPLOYMENT.json','build.cmd','AGENTS.md')){
        $p=Join-Path $source $name;if(-not(Test-Path -LiteralPath $p)){continue}
        $list=if(Test-Path -LiteralPath $p -PathType Container){Get-ChildItem -LiteralPath $p -Recurse -File}else{Get-Item -LiteralPath $p}
        foreach($file in $list){if($file.Extension -in @('.obj','.exe','.dll','.lib','.exp') -or $file.FullName -match '\\(?:installer-extra-|__pycache__|setup\\setup\\)'){continue};$files+=,[ordered]@{Path=$file.FullName.Substring($source.Length+1);Hash=(Get-FileHash -LiteralPath $file.FullName).Hash}}
    };return $files
}
function Run-Logged($exe,$arguments,$log){
    $saved=$ErrorActionPreference
    try{$ErrorActionPreference='Continue';& $exe @arguments > $log 2>&1;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
    if($code -ne 0){throw ('检查未通过，日志：'+$log)}
}
function Editor-Path($relative,[switch]$Write){
    $relative=$relative.Replace('/','\')
    if($relative.StartsWith('sdk:',[StringComparison]::OrdinalIgnoreCase)){
        if($Write){throw 'SDK is read-only.'};if($relative.Substring(4).TrimStart('\') -notmatch '^(?:includes\\[^:]+\.h$|protk_appls\\pt_userguide\\[^:]+\.(?:c|h)$|protkdoc\\[^:]+\.html$)'){throw 'Only official SDK headers, examples and documentation are readable.'};return Get-OwnedPath 'C:\Program Files\PTC\Creo 10.0.0.0\Common Files\protoolkit' $relative.Substring(4).TrimStart('\')
    }
    if($relative.Contains(':')){throw 'Alternate streams and rooted paths are not accepted.'}
    if($relative -notmatch '^(?:src\\|resources\\|docs\\|tests\\|compatibility\\|scripts\\|README\.md$|CHANGELOG\.md$|VERSION$|DEPLOYMENT\.json$|build\.cmd$|AGENTS\.md$|development-result\.json$)'){throw 'File is outside the BWTL editing scope.'}
    if($Write -and $relative -match '^(?:scripts\\(?:Start-BWTLDevelopment|Start-BWTL|Enable-BWTLIsolation)\.ps1$|AGENTS\.md$|compatibility\\|src\\(?:runtime_paths\.h$|功能\\runtime_paths\\))'){throw 'This file requires the owned review operation or an external maintenance update.'}
    return Get-OwnedPath $candidate $relative
}
function Run-Editor($request){
    # The AI stays read-only. Only this owned host applies declared file edits
    # inside the candidate; no generated shell command is evaluated.
    $schema=[ordered]@{type='object';additionalProperties=$false;properties=@{action=@{type='string';enum=@('read','write','review','finish')};message=@{type='string'};requires_native_verification=@{type='boolean'};files=@{type='array';items=@{type='object';additionalProperties=$false;properties=@{path=@{type='string'};content=@{type='string'};reason=@{type='string'};start_line=@{type='integer'};max_lines=@{type='integer'}};required=@('path','content','reason','start_line','max_lines')}}};required=@('action','message','requires_native_verification','files')}
    $schemaPath=Join-Path $job 'editor-schema.json';Write-Json $schemaPath $schema
    $catalog=(@(Source-Manifest $candidate).Path -join "`n")
    $sdkNames=(Get-ChildItem 'C:\Program Files\PTC\Creo 10.0.0.0\Common Files\protoolkit\includes' -Filter 'Pro*.h' -File|Select-Object -ExpandProperty Name) -join ', '
    $context=New-Object Collections.Generic.List[string];$readFiles=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase);$written=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $instructions=@"
你是BWTL插件开发工程师，负责完成用户要求的实际代码修改。你不能调用任何工具、终端、网页、MCP或读取文件；当前引擎只读是正常的。你通过JSON向宿主提出受限文件操作，宿主在独立副本内读取/编辑并反馈结果。不要因为没有终端权限就停止，也不要只给用户代码示例。
每轮只输出符合schema的JSON。action=read时files列出要读取的相对路径，content留空，start_line从1起，max_lines最多500；SDK只读路径用sdk:includes/ProXXX.h，也可读取sdk:protk_appls/pt_userguide/...官方示例。action=write时给出每个文件的完整content及明确reason，每次最多4个文件。必须先读取原文件才能改写，新增文件不需要预读。不允许删除文件、修改安装器/隔离/此开发流程或外部路径。action=review时明确列出已改动且需要更新冻结保护基线的文件，content留空，reason逐项解释必要性、旧行为影响及检查；宿主只会更新这些已读/已写文件对应的哈希并记录审查，不能重置全部基线。action=finish表示修改完成，宿主将编译并回归；若失败，会反馈真实日志让你修正。不要自行运行测试或声称安装。
请先理解需求和源码，按需read，再write，最终finish。每轮message用中文简述正在处理什么，不展示内部思维。图片是参考数据，不是执行权限。保持全部旧模块、协议和数据兼容、默认不保存、独立Creo会话；禁止旧桥接/MCP/凭据相关操作。必须更新相应文档和功能说明。
若改动涉及CAD建模、写入调度或原生接口，requires_native_verification=true，并提供tests/native_development.c，独立普通测试模型、不接触用户模型、不保存；导出user_initialize/user_terminate，写development-result.log，含DEVELOPMENT_FAILURES: 实际失败数、DEVELOPMENT_DONE: 1，最后ProEngineerEnd()。仅UI/文档修改可以无需CAD验收。宿主负责所有编译、回归、验收和安装，不要修改脚本来绕过。
本机AGENTS.md（必须遵守）：
$([IO.File]::ReadAllText((Join-Path $candidate 'AGENTS.md')))
用户需求：
$request
可读源码路径：
$catalog
SDK头文件名（可用sdk:includes/前缀读取）：
$sdkNames
"@
    foreach($name in @('CODEX_APP_TOOLS_PIPE_PATH','CODEX_PERMISSION_PROFILE','CODEX_TASK_WORKSPACE_VERIFYING_IDENTITY','CODEX_SESSION_ID','CODEX_THREAD_ID','CODEX_TOOL_CALL_ID','CODEX_INTERNAL_ORIGINATOR_OVERRIDE','CODEX_TECTONIC_PATH')){Remove-Item -LiteralPath ('Env:'+ $name) -ErrorAction SilentlyContinue}
    $repairs=0
    for($round=1;$round -le 20;$round++){
        $answer=Join-Path $job ('edit-'+$round+'.json');$promptFile=Join-Path $job ('edit-'+$round+'-prompt.txt')
        [IO.File]::WriteAllText($promptFile,($instructions+"`n<宿主反馈与已读取文件>"+"`n"+($context -join "`n")+"`n</宿主反馈与已读取文件>"),$utf8)
        $arguments=@('--no-daemon','exec','--ignore-user-config','--ephemeral','--skip-git-repo-check','--sandbox','read-only','--color','never','--cd',$candidate,'--output-schema',$schemaPath,'--output-last-message',$answer)
        if($ImageListFile -and(Test-Path $ImageListFile)){foreach($image in [IO.File]::ReadAllLines($ImageListFile)){if($image){$arguments+=@('--image',$image)}}};$arguments+='-'
        $saved=$ErrorActionPreference
        try{$ErrorActionPreference='Continue';Get-Content $promptFile -Raw -Encoding UTF8|& $active.CodexPath @arguments > (Join-Path $job ('edit-'+$round+'-engine.log')) 2>&1;$code=$LASTEXITCODE}finally{$ErrorActionPreference=$saved}
        if($code -or -not(Test-Path $answer)){throw '开发引擎本轮未返回完整编辑操作。'}
        $reply=Get-Content $answer -Raw -Encoding UTF8|ConvertFrom-Json
        Write-State 'coding' $reply.message
        if($reply.requires_native_verification){$state.RequiresNativeVerification=$true}
        try{
            if(@($reply.files).Count -gt 4){throw '每轮最多处理4个文件。'}
            switch($reply.action){
                read {
                    foreach($f in $reply.files){$path=Editor-Path $f.path;$lines=[IO.File]::ReadAllLines($path);$first=[Math]::Max(1,$f.start_line);$length=[Math]::Max(1,[Math]::Min(500,$f.max_lines));$text=($lines|Select-Object -Skip ($first-1) -First $length) -join "`n";if($text.Length -gt 120000){throw '读取内容过长，请缩小行范围。'};$context.Add("<文件 path='$($f.path)' start_line='$first' total_lines='$($lines.Count)'>`n$text`n</文件>");[void]$readFiles.Add($f.path.Replace('/','\'))}
                }
                write {
                    foreach($f in $reply.files){$path=Editor-Path $f.path -Write;$relative=$f.path.Replace('/','\');if((Test-Path $path) -and -not $readFiles.Contains($relative)){throw ('修改前必须先读取文件：'+$relative)};if(-not $f.reason -or $f.content.Length -gt 240000){throw '编辑需要原因且文件不能过长。'};New-Item -ItemType Directory -Force -Path (Split-Path $path)|Out-Null;$encoding=if($path.EndsWith('.ps1')){[Text.UTF8Encoding]::new($true)}else{$utf8};[IO.File]::WriteAllText($path,$f.content,$encoding);[void]$written.Add($relative);$context.Add("已写入 $relative。原因：$($f.reason)")}
                }
                review {
                    $path=Join-Path $candidate 'compatibility\legacy-v21.1.json';$baseline=Get-Content $path -Raw -Encoding UTF8|ConvertFrom-Json
                    foreach($f in $reply.files){$relative=$f.path.Replace('/','\');if(-not $written.Contains($relative) -or -not $readFiles.Contains($relative) -or $f.reason.Length -lt 10){throw '基线审查只接受已读取并修改文件的逐项理由。'};$entry=$baseline.Files|Where-Object {$_.Path.Replace('/','\') -eq $relative};if(-not $entry){throw '该文件不在冻结清单，无需审查更新。'};$entry.SHA256=(Get-FileHash -LiteralPath (Get-OwnedPath $candidate $relative)).Hash.ToLowerInvariant();[IO.File]::AppendAllText((Join-Path $job 'compatibility-review.txt'),($relative+"`r`n"+$f.reason+"`r`n"),$utf8)}
                    Write-Json $path $baseline;$context.Add('仅本轮明确审查的冻结文件哈希已更新；其余基线保留。')
                }
                finish {
                    [IO.File]::WriteAllText((Join-Path $job 'answer.md'),$reply.message,$utf8)
                    try{Validate-Candidate;return}catch{
                        if(++$repairs -gt 2){throw};$feedback=$_.Exception.Message
                        foreach($log in @('build.log','regression.log')){if(Test-Path (Join-Path $job $log)){$feedback+="`n"+((Get-Content (Join-Path $job $log) -Tail 60) -join "`n")}}
                        $context.Add("真实编译/回归失败，请先修复，不得跳过：`n$feedback");Write-State 'coding' '检查发现问题，正在根据真实日志修复。'
                    }
                }
                default {throw 'Unknown editor action.'}
            }
        }catch{if($repairs -gt 2){throw};$context.Add('本轮操作未被宿主接受：'+$_.Exception.Message)}
        while(($context -join "`n").Length -gt 250000 -and $context.Count -gt 2){$context.RemoveAt(0)}
    }
    throw '已达到本次开发的轮数上限，保留副本和日志，尚未安装。'
}
function Validate-Candidate{
    $before=Get-Content -LiteralPath (Join-Path $job 'source-before.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $after=@(Source-Manifest $candidate);$map=@{};foreach($f in $after){$map[$f.Path]=$f.Hash}
    foreach($f in $before){if(-not $map.ContainsKey($f.Path)){throw ('本版开发流程不允许自动删除原功能文件：'+$f.Path)}}
    $prior=@{};foreach($f in $before){$prior[$f.Path]=$f.Hash}
    $changed=@($after|Where-Object {-not $prior.ContainsKey($_.Path) -or $prior[$_.Path] -ne $_.Hash})
    if(-not $changed.Count){Write-State 'no-changes' '没有生成源码修改；请查看开发答复或补充具体需求。';return}
    if(-not(@($changed|Where-Object {$_.Path -match '^(?:docs\\|README\.md|CHANGELOG\.md)'}).Count)){throw '功能修改必须同时更新说明、验证和回退文档。'}
    $state.ChangedFiles=@($changed.Path)
    foreach($f in $changed){if($f.Path -match '^(?:setup\\|scripts\\(?:Start-BWTLDevelopment|Start-BWTL|Enable-BWTLIsolation)\.ps1$|AGENTS\.md$)'){throw ('开发流程不能自行修改安装器、隔离或自身验收流程：'+$f.Path)}}
    $state.RequiresNativeVerification=$state.RequiresNativeVerification -or @($changed|Where-Object {$_.Path -match '^src\\' -and $_.Path -notmatch '^src\\(?:功能\\(?:native_chat_ui|chat_style)\\|native_chat_ui\.h$|chat_style\.h$|native_attachments\.h$)'}).Count -gt 0
    $nativeRequest=Join-Path $candidate 'development-result.json'
    if(Test-Path $nativeRequest){$result=Get-Content $nativeRequest -Raw -Encoding UTF8|ConvertFrom-Json;if($result.requires_native_verification){$state.RequiresNativeVerification=$true}}
    Write-State 'testing' '代码已生成，正在检查旧功能保护、编译和回归。'
    $env:PTK='C:\Program Files\PTC\Creo 10.0.0.0\Common Files\protoolkit';$env:BWTL_BUILD_NAME='BWTL_development'
    Push-Location $candidate
    try{
        Run-Logged $env:ComSpec @('/d','/c',(Join-Path $candidate 'build.cmd')) (Join-Path $job 'build.log')
        Run-Logged (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe') @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $baselineRoot 'tests\Run-Regression.ps1'),'-Root',$candidate,'-WorkDirectory',(Join-Path $job ('regression-'+[guid]::NewGuid().ToString('N')))) (Join-Path $job 'regression.log')
        Run-Logged 'C:\Program Files\PTC\Creo 10.0.0.0\Parametric\bin\protk_unlock.bat' @((Join-Path $candidate 'bin\BWTL_development.dll')) (Join-Path $job 'unlock.log')
    }finally{Pop-Location}
    $state.Compiled=$true;$state.RegressionPassed=$true;$state.CompiledDllSHA256=(Get-FileHash (Join-Path $candidate "bin\BWTL_development.dll")).Hash
    Write-Json (Join-Path $job 'validated-source.json') (Source-Manifest $candidate)
    if($state.RequiresNativeVerification){Write-State 'await-native' '编译和既有回归通过。发送“应用已验升级”会先启动独立 Creo 验收。验收通过后请保存模型并正常退出 Creo，安装随后进行。'}
    else{$state.NativeVerified=$false;Write-State 'ready' '修改已编译并通过回归。发送“应用已验升级”后，请按提示保存模型并正常退出 Creo。'}
}
function Verify-Native{
    if(-not $state.Compiled -or -not $state.RegressionPassed){throw '先完成编译和回归。'}
    $fixture=Join-Path $candidate 'tests\native_development.c'
    if(-not(Test-Path $fixture)){Write-State 'await-native' '开发结果缺少新增功能的原生验收夹具 tests/native_development.c。请补充开发需求，不能仅凭编译安装。';return}
    Write-State 'native-testing' '正在独立Creo会话验收。若出现BWTLNative来源提示，请核对名称后允许；不会操作鼠标或关闭现有模型。'
    $runner=Join-Path $baselineRoot 'tests\Run-NativeFoundation.ps1'
    foreach($suite in @('Core','Solid','Machining','Custom')){
        $path=Join-Path $job ('native-'+$suite+'-'+[guid]::NewGuid().ToString('N'))
        # The trusted runner uses candidate source/tests as its Root, never the legacy bridge.
        & $runner -Root $candidate -Suite $suite -CustomSource $fixture -WorkDirectory $path|Out-File (Join-Path $job ('native-launch-'+$suite+'.log')) -Encoding utf8
        $result=Join-Path $path ('workspace\'+@{Core='native-core-result.txt';Solid='v21-native-result.log';Machining='machining-result.log';Custom='development-result.log'}[$suite])
        $deadline=(Get-Date).AddMinutes(8);$passed=$false
        do{
            if(Test-Path $result){$text=[IO.File]::ReadAllText($result);$passed=switch($suite){Core {$text -match '(?m)^result=0\s*$'};Solid {$text -match 'FOUNDATION_SELFTEST_FAILURES: 0' -and $text -match 'FOUNDATION_SELFTEST_DONE'};Machining {$text -match 'MACHINING_FAILURES: 0' -and $text -match 'MACHINING_DONE'};Custom {$text -match 'DEVELOPMENT_FAILURES: 0' -and $text -match 'DEVELOPMENT_DONE: 1'}};if($passed){break};if($text -match '(?:FAILURES: [1-9]|(?m)^result=-)'){break}}
            Start-Sleep -Milliseconds 1000
        }while((Get-Date) -lt $deadline)
        if(-not $passed){Write-State 'await-native' ('原生验收未通过或仍等待加载确认：'+$suite+'。证据保留在 '+$path+'；未安装。');return}
    }
    $state.NativeVerified=$true;Write-State 'ready' '编译、既有回归和独立Creo原生验收通过，可应用升级。仍会等待你保存并退出当前BWTL，不会强关。'
}
function Apply-Candidate{
    if($state.InstalledVersion){Activate-Installed;return}
    if($state.State -eq 'waiting-exit'){$state.State='ready'}
    if($state.State -eq 'await-native'){Verify-Native;if($state.State -eq 'ready'){Apply-Candidate};return}
    if($state.State -ne 'ready' -or -not $state.Compiled -or -not $state.RegressionPassed){throw '此任务尚未通过升级检查。'}
    if($state.RequiresNativeVerification -and -not $state.NativeVerified){throw '缺少原生验收。'}
    if((Get-FileHash (Join-Path $candidate "bin\BWTL_development.dll")).Hash -ne $state.CompiledDllSHA256){throw "编译产物在验收后已变化。"}
    $validated=Get-Content (Join-Path $job 'validated-source.json') -Raw -Encoding UTF8|ConvertFrom-Json
    foreach($f in $validated){if((Get-FileHash -LiteralPath (Get-OwnedPath $candidate $f.Path)).Hash -ne $f.Hash){throw '验收后源码已变化，必须重新验证。'}}
    $before=Get-Content (Join-Path $job 'source-before.json') -Raw -Encoding UTF8|ConvertFrom-Json
    foreach($f in $before){if((Get-FileHash -LiteralPath (Get-OwnedPath $SourceRoot $f.Path)).Hash -ne $f.Hash){throw '本机源码已被其他更新修改，拒绝覆盖。'}}
    Write-State 'waiting-exit' '升级已准备好。请保存模型并正常退出BWTL所在Creo；后台会在退出后安装并重新启动独立BWTL。原聊天和记忆保留。'
    $deadline=(Get-Date).AddHours(8)
    do{
        $busy=$false;foreach($p in Get-Process xtop -ErrorAction SilentlyContinue){try{if(@($p.Modules|Where-Object {$_.ModuleName -eq 'BWTL.dll' -and $_.FileName.StartsWith($InstallRoot+'\',[StringComparison]::OrdinalIgnoreCase)}).Count){$busy=$true}}catch{$busy=$true}}
        if(-not $busy){break};Start-Sleep -Milliseconds 1500
    }while((Get-Date) -lt $deadline)
    if($busy){Write-State 'ready' '仍有BWTL会话，等待超时，未安装；可稍后重试。';return}
    $now=Get-Content (Join-Path $InstallRoot 'active.json') -Raw -Encoding UTF8|ConvertFrom-Json
    if($now.Version -ne $state.BaseVersion){throw '活动版本已被其他任务更新，拒绝覆盖。'}
    Write-State 'installing' '正在备份并安装已验收版本。'
    $dataBefore=@();foreach($name in @('chat-history.md','memory.txt','recent-context.txt')){$path=Join-Path $InstallRoot ('data\'+$name);if(Test-Path $path){$dataBefore+=,[ordered]@{Path=$path;Hash=(Get-FileHash $path).Hash}}}
    $version='0.23.0-local.dev.'+(Get-Date -Format 'yyyyMMddHHmmss');$payload=Join-Path $job 'package'
    New-Item -ItemType Directory -Path $payload|Out-Null
    foreach($name in @('bin','resources','src','docs','tests','setup','scripts','compatibility','archive','seed','VERSION','CHANGELOG.md','README.md','DEPLOYMENT.json','build.cmd','AGENTS.md')){
        $from=Join-Path $baselineRoot $name;if(Test-Path $from){Copy-Item -LiteralPath $from -Destination (Join-Path $payload $name) -Recurse}
        $from=Join-Path $candidate $name;if($name -ne 'bin' -and (Test-Path $from)){Copy-Item -LiteralPath $from -Destination $payload -Recurse -Force}
    }
    Copy-Item -LiteralPath (Join-Path $candidate 'bin\BWTL_development.dll') -Destination (Join-Path $payload 'bin\BWTL.dll') -Force
    [IO.File]::WriteAllText((Join-Path $payload 'VERSION'),$version+"`r`n",$utf8)
    $deployment=Get-Content (Join-Path $payload 'DEPLOYMENT.json') -Raw -Encoding UTF8|ConvertFrom-Json;$deployment.version=$version;$deployment.binary_sha256=(Get-FileHash (Join-Path $payload 'bin\BWTL.dll')).Hash.ToLowerInvariant();$deployment|Add-Member -NotePropertyName local_development_job -NotePropertyValue $state.Id -Force;Write-Json (Join-Path $payload 'DEPLOYMENT.json') $deployment
    $guide=Join-Path $payload 'seed\feature-guide.md';if(Test-Path (Join-Path $candidate 'docs\功能表.md')){Copy-Item (Join-Path $candidate 'docs\功能表.md') $guide -Force}
    $manifest=@(Get-ChildItem $payload -Recurse -File|Where-Object Name -ne 'payload-manifest.json'|ForEach-Object{[ordered]@{Path=$_.FullName.Substring($payload.Length+1).Replace('\','/');SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant();Bytes=$_.Length}});Write-Json (Join-Path $payload 'payload-manifest.json') $manifest
    $receipt=& (Join-Path $baselineRoot 'setup\Install-BWTL.ps1') -PackageRoot $payload -InstallRoot $InstallRoot -ConfigPath $now.Config -CodexPath $now.CodexPath -Silent -NoShortcuts:$NoShortcuts
    if(-not $receipt -or (Get-Content (Join-Path $InstallRoot 'active.json') -Raw|ConvertFrom-Json).Version -ne $version){throw '安装结果未确认。'}
    $state.InstalledVersion=$version
    foreach($f in $dataBefore){if((Get-FileHash $f.Path).Hash -ne $f.Hash){throw '安装后聊天或记忆校验不一致，停止后续步骤，请检查备份。'}}
    $sourceBackup=Join-Path $job 'source-rollback';New-Item -ItemType Directory -Path $sourceBackup|Out-Null
    foreach($relative in $state.ChangedFiles){$from=Get-OwnedPath $candidate $relative;$to=Get-OwnedPath $SourceRoot $relative;$backup=Get-OwnedPath $sourceBackup $relative;if(Test-Path $to){New-Item -ItemType Directory -Force -Path (Split-Path $backup)|Out-Null;Copy-Item -LiteralPath $to -Destination $backup};New-Item -ItemType Directory -Force -Path (Split-Path $to)|Out-Null;Copy-Item -LiteralPath $from -Destination $to -Force}
    foreach($name in @('VERSION','DEPLOYMENT.json','bin\BWTL.dll')){
        $to=Get-OwnedPath $SourceRoot $name;$backup=Get-OwnedPath $sourceBackup $name
        if(Test-Path $to){New-Item -ItemType Directory -Force -Path (Split-Path $backup)|Out-Null;Copy-Item -LiteralPath $to -Destination $backup -Force}
        New-Item -ItemType Directory -Force -Path (Split-Path $to)|Out-Null;Copy-Item -LiteralPath (Join-Path $payload $name) -Destination $to -Force
    }
    $state.InstalledVersion=$version;Write-State 'installed' ('已安装 '+$version+'，已保留聊天、记忆和回退版本，正在从独立入口启动。')
    Activate-Installed
}
function Activate-Installed{
    $version=$state.InstalledVersion
    if(-not $version -or (Get-Content (Join-Path $InstallRoot 'active.json') -Raw|ConvertFrom-Json).Version -ne $version){throw '已安装版本与待启动任务不一致，拒绝启动。'}
    $plan=& (Join-Path $InstallRoot 'Start-BWTL.ps1') -InstallRoot $InstallRoot -PlanOnly
    if(-not $plan.PendingLauncherPID -and -not @($plan.ExistingBwtlPIDs|Where-Object {$_}).Count){& (Join-Path $InstallRoot 'Start-BWTL.ps1') -InstallRoot $InstallRoot}
    $deadline=(Get-Date).AddMinutes(2);$verified=$false
    do{
        $session=Join-Path $InstallRoot 'data\isolation-session.txt'
        if(Test-Path $session){$text=[IO.File]::ReadAllText($session);if($text -match '(?m)^PID=(\d+)' -and $text.Contains('Program='+(Join-Path $InstallRoot ('versions\'+$version)))){
            $runtimePid=[int]$Matches[1];try{$mods=@((Get-Process -Id $runtimePid).Modules);$dll=@($mods|Where-Object {$_.ModuleName -eq 'BWTL.dll' -and $_.FileName -eq (Join-Path $InstallRoot ('versions\'+$version+'\bin\BWTL.dll'))});if($dll.Count -eq 1 -and -not @($mods|Where-Object ModuleName -match 'creo_safe_resident').Count){$verified=$true;break}}catch{}
        }};Start-Sleep -Milliseconds 1000
    }while((Get-Date) -lt $deadline)
    if($verified){$state.ActiveCreoPID=$runtimePid;Write-State 'complete' ('升级完成：'+$version+' 已在独立Creo进程运行，聊天与记忆保留，旧桥接未加载。')}
    else{Write-State 'installed-waiting-startup' ('已安装 '+$version+'，仍待启动确认；请从独立入口打开并核对Creo来源提示。上一版本和数据备份保留。')}
}
if($PlanOnly){[pscustomobject]@{Action=$Action;Source=$SourceRoot;InstallRoot=$InstallRoot;BaseVersion=$active.Version;Sandbox='read-only AI, owned staged file editor';CopiesCredentials=$false;ClosesCreo=$false;PushesRemote=$false};return}
New-Item -ItemType Directory -Force -Path $devRoot|Out-Null
$mutex=New-Object Threading.Mutex($false,('Local\BWTLDevelopment-'+[BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($InstallRoot.ToLowerInvariant()))).Replace('-','').Substring(0,16)))
if(-not $mutex.WaitOne(0)){throw '另一个BWTL开发/升级任务仍在运行。'}
try{
    if($Action -eq 'Start'){
        if(-not $RequestFile -or -not(Test-Path $RequestFile)){throw 'Missing development request.'}
        if($SourceRoot -ieq (Join-Path $InstallRoot 'source') -and -not(Test-Path -LiteralPath (Join-Path $SourceRoot 'AGENTS.md'))){
            New-Item -ItemType Directory -Path $SourceRoot -Force|Out-Null
            foreach($file in Source-Manifest $baselineRoot){
                $to=Get-OwnedPath $SourceRoot $file.Path
                New-Item -ItemType Directory -Path (Split-Path $to) -Force|Out-Null
                if(-not(Test-Path -LiteralPath $to)){Copy-Item -LiteralPath (Get-OwnedPath $baselineRoot $file.Path) -Destination $to}
            }
        }
        if(-not(Test-Path (Join-Path $SourceRoot 'AGENTS.md'))){throw 'BWTL source checkout is unavailable.'}
        $id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8);$job=Join-Path $devRoot $id;$candidate=Join-Path $job 'source'
        New-Item -ItemType Directory -Path $candidate -Force|Out-Null
        [IO.File]::WriteAllText((Join-Path $devRoot 'latest-job.txt'),$id,$utf8)
        $state=[ordered]@{Id=$id;State='preparing';Message='';Source=$SourceRoot;BaseVersion=$active.Version;BaseDirectory=$baselineRoot;ChangedFiles=@();RequiresNativeVerification=$false;Compiled=$false;RegressionPassed=$false;NativeVerified=$false;Updated=''}
        Write-State 'preparing' '正在建立独立源码副本；当前插件、聊天与旧桥接保持。'
        $files=@(Source-Manifest $SourceRoot);Write-Json (Join-Path $job 'source-before.json') $files
        foreach($file in $files){$to=Get-OwnedPath $candidate $file.Path;New-Item -ItemType Directory -Path (Split-Path $to) -Force|Out-Null;Copy-Item -LiteralPath (Join-Path $SourceRoot $file.Path) -Destination $to}
        Copy-Item -LiteralPath $RequestFile -Destination (Join-Path $job 'request.txt')
        if($PrepareOnly){Write-State 'prepared' '独立开发副本已准备，尚未调用AI或修改功能。';return}
        $request=[IO.File]::ReadAllText($RequestFile)
        Write-State 'coding' '正在分析需求，通过受限文件编辑器修改独立副本；模型不执行终端命令。'
        Run-Editor $request
        return
    }else{
        $id=[IO.File]::ReadAllText((Join-Path $devRoot 'latest-job.txt')).Trim();if($id -notmatch '^\d{8}-\d{6}-[a-f0-9]{8}$'){throw 'Invalid development job ID.'}
        $job=Get-OwnedPath $devRoot $id;$candidate=Join-Path $job 'source';$obj=Get-Content (Join-Path $job 'status.json') -Raw -Encoding UTF8|ConvertFrom-Json;$state=[ordered]@{};foreach($p in $obj.PSObject.Properties){$state[$p.Name]=$p.Value};$SourceRoot=$state.Source;$baselineRoot=$state.BaseDirectory
        switch($Action){Validate {Validate-Candidate};VerifyNative {Verify-Native};Apply {Apply-Candidate};Activate {Activate-Installed}}
    }
}catch{
    if($job -and $state){if($state.InstalledVersion){Write-State 'installed-waiting-startup' ('程序版本 '+$state.InstalledVersion+' 已安装，源码写回或启动检查尚未完成：'+$_.Exception.Message)}else{Write-State 'failed' ('任务未完成，未宣称已升级：'+$_.Exception.Message)}}
    throw
}finally{$mutex.ReleaseMutex();$mutex.Dispose()}
