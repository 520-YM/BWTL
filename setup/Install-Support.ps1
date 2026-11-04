# Installer-only helpers. Never load or execute a Creo model.
function Get-FileHash {
    param([Parameter(Position=0)][string]$LiteralPath,[string]$Algorithm='SHA256')
    $stream=[IO.File]::OpenRead($LiteralPath);$sha=[Security.Cryptography.SHA256]::Create()
    try{[pscustomobject]@{Hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','');Path=$LiteralPath}}
    finally{$stream.Dispose();$sha.Dispose()}
}
function Export-BwtlDiagnostics {
    param([string]$Root)
    $Root=[IO.Path]::GetFullPath($Root)
    $lines=[Collections.Generic.List[string]]::new()
    $lines.Add('BWTL installation diagnostics '+(Get-Date -Format o))
    $lines.Add('Root: '+$Root)
    $lines.Add('OS64: '+[Environment]::Is64BitOperatingSystem)
    $statePath=Join-Path $Root 'active.json'
    if(Test-Path -LiteralPath $statePath){
        $a=Get-Content -LiteralPath $statePath -Raw -Encoding UTF8|ConvertFrom-Json
        $lines.Add('Version: '+$a.Version);$lines.Add('Installed: '+$a.Installed)
        $engine=$a.CodexPath
        if(-not $engine){$found=Get-Command codex.exe -ErrorAction SilentlyContinue;if($found){$engine=$found.Source}}
        if(-not $engine){$bin=Join-Path $env:LOCALAPPDATA 'OpenAI/Codex/bin';if(Test-Path -LiteralPath $bin){$found=Get-ChildItem -LiteralPath $bin -Filter codex.exe -Recurse -File -ErrorAction SilentlyContinue|Sort-Object LastWriteTime -Descending|Select-Object -First 1;if($found){$engine=$found.FullName}}}
        $lines.Add('Codex executable discovered: '+[bool]($engine -and (Test-Path -LiteralPath $engine)))
        if($engine){$lines.Add('Codex path: '+$engine)}
        $lines.Add('Codex authentication was not inspected or copied.')
        $lines.Add('Config: '+$a.Config);$lines.Add('Config exists: '+(Test-Path -LiteralPath $a.Config))
        if(Test-Path -LiteralPath $a.Config){
            $cfg=Read-Config $a.Config
            foreach($line in ($cfg.Text -split '\r?\n')){if($line -match '^\s*(protkdat|toolkit_registry_file|load_ui_customization_run_dir)\s'){$lines.Add('Config registration: '+$line)}}
        }
        foreach($rel in @('bin/BWTL.dll','resources/ribbon/toolkitribbonui.rbn','resources/text/ribbon/toolkitribbonui.rbn','settings.ini')){
            $p=Join-Path $a.Directory $rel;$lines.Add($rel+' exists: '+(Test-Path -LiteralPath $p))
        }
        $dll=Join-Path $a.Directory 'bin/BWTL.dll';$dep=Join-Path $a.Directory 'DEPLOYMENT.json'
        if((Test-Path -LiteralPath $dll) -and (Test-Path -LiteralPath $dep)){
            $expected=(Get-Content -LiteralPath $dep -Raw -Encoding UTF8|ConvertFrom-Json).binary_sha256
            $lines.Add('DLL matches released SHA256: '+((Get-FileHash $dll).Hash -ieq $expected))
        }
        $log=Join-Path $a.Data 'runtime.log'
        if(Test-Path -LiteralPath $log){
            $info=Get-Item -LiteralPath $log;$lines.Add('Runtime log modified: '+$info.LastWriteTime.ToString('o'))
            $lines.Add('Runtime log newer than install: '+($info.LastWriteTime -ge [datetime]$a.Installed))
            $lines.Add('--- Last 60 runtime diagnostic lines (not chat history) ---')
            foreach($line in Get-Content -LiteralPath $log -Tail 60){$lines.Add($line)}
        }else{$lines.Add('NO runtime.log: plugin startup is not confirmed. Check actual loaded config, trust prompt and DLL dependencies.')}
        if($a.Uninstalled){$lines.Add('Registration removed at: '+$a.Uninstalled)}
    }else{$lines.Add('NO active.json: no managed installation at this location.')}
    $registration=Join-Path $Root 'protk.dat'
    if(Test-Path -LiteralPath $registration){$lines.Add('--- protk.dat ---');$lines.Add([IO.File]::ReadAllText($registration))}
    foreach($dll in @('vcruntime140.dll','ucrtbase.dll')){$lines.Add('System runtime '+$dll+': '+(Test-Path -LiteralPath (Join-Path $env:SystemRoot ('System32/'+$dll))))}
    $lines.Add('Registration present does NOT prove Creo has loaded this config or the DLL.')
    $lines.Add('Codex login is needed for AI, not for the native ribbon entry. No credentials or chat contents collected.')
    $dir=Join-Path $Root 'diagnostics';New-Item -ItemType Directory -Path $dir -Force|Out-Null
    $path=Join-Path $dir ('BWTL-diagnostics-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.txt')
    [IO.File]::WriteAllLines($path,$lines,[Text.UTF8Encoding]::new($true))
    return $path
}
function Start-BwtlManagement([string]$Mode,[string]$Root){
    $script=Join-Path $Root 'Install-BWTL.ps1'
    if(-not(Test-Path -LiteralPath $script)){throw '此目录尚未安装，或管理文件缺失。请先安装/修复。'}
    $p=[Diagnostics.ProcessStartInfo]::new((Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'))
    $p.Arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$script+'" -InstallRoot "'+$Root+'" '+$Mode
    $p.UseShellExecute=$false;$p.CreateNoWindow=$true;$p.WindowStyle='Hidden'
    [Diagnostics.Process]::Start($p).Dispose()
}
