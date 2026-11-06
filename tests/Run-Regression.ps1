[CmdletBinding()]
param([string]$Root,[string]$WorkDirectory)
$ErrorActionPreference='Stop'
if(-not $Root){$Root=Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)}
$Root=[IO.Path]::GetFullPath($Root)
& (Join-Path $PSScriptRoot 'Test-LegacyCompatibility.ps1') -Root $Root
if(-not $WorkDirectory){$WorkDirectory=Join-Path $Root ('build/regression-'+[Guid]::NewGuid().ToString('N'))}
$WorkDirectory=[IO.Path]::GetFullPath($WorkDirectory)
New-Item -ItemType Directory -Path $WorkDirectory -Force|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if(-not $vs){throw 'Visual Studio C++ Build Tools required.'}
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
$names=@('test_model_plan','test_part_command','test_foundation_protocol','test_solid_protocol','test_machining_protocol','test_smart_policy','test_smart_session','test_attachments','test_prompt_encoding','test_history_context','test_chat_store')
$lines=@('@echo off',('call "'+$vcvars+'" >nul'),'if errorlevel 1 exit /b 1',('cd /d "'+$WorkDirectory+'"'))
foreach($name in $names){
    $lines+=('cl /nologo /MD /W3 /utf-8 "'+(Join-Path $Root ('tests/'+$name+'.c'))+'" /Fo"'+$name+'.obj" /Fe"'+$name+'.exe"')
    if($name -eq 'test_attachments'){$lines[$lines.Count-1]+=' ole32.lib windowscodecs.lib uuid.lib gdi32.lib user32.lib shell32.lib'}
    $lines+='if errorlevel 1 exit /b 1'
    if($name -eq 'test_chat_store'){
        $lines+=('"'+$name+'.exe" write "'+(Join-Path $WorkDirectory 'isolated-data')+'"')
        $lines+='if errorlevel 1 exit /b 1'
        $lines+=('"'+$name+'.exe" read "'+(Join-Path $WorkDirectory 'isolated-data')+'"')
    }else{$lines+=('"'+$name+'.exe"')}
    $lines+='if errorlevel 1 exit /b 1'
}
$lines+='exit /b 0'
$runner=Join-Path $WorkDirectory 'run.cmd'
[IO.File]::WriteAllLines($runner,$lines,[Text.Encoding]::Default)
$savedPreference=$ErrorActionPreference
try{
    # Windows PowerShell wraps native stderr as ErrorRecord even for warnings.
    # Preserve diagnostics and decide success using the batch exit code.
    $ErrorActionPreference='Continue'
    $output=& $env:ComSpec /d /c ('"'+$runner+'"') 2>&1
    $exit=$LASTEXITCODE
}finally{$ErrorActionPreference=$savedPreference}
$output | ForEach-Object {Write-Output $_}
[IO.File]::WriteAllLines((Join-Path $WorkDirectory 'regression.log'),[string[]]$output,[Text.UTF8Encoding]::new($false))
if($exit -ne 0){throw "Regression failed (exit $exit); release blocked."}
Write-Output 'REGRESSION PASS: 11 suites, including two-process history persistence. No Creo GUI interaction.'
