[CmdletBinding()]
param([string]$SourceRoot=(Split-Path -Parent $PSScriptRoot),[Parameter(Mandatory=$true)][string]$DataSource,[string]$LegacyRoot,[Parameter(Mandatory=$true)][string]$WorkDirectory,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$SourceRoot=[IO.Path]::GetFullPath($SourceRoot);$WorkDirectory=[IO.Path]::GetFullPath($WorkDirectory)
& (Join-Path $SourceRoot 'tests/Run-Regression.ps1') -Root $SourceRoot -WorkDirectory (Join-Path $WorkDirectory 'regression')
if($WorkDirectory.StartsWith($SourceRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'WorkDirectory must be outside SourceRoot to avoid recursive archives.'}
if(-not $OutputDirectory){$OutputDirectory=Join-Path $SourceRoot 'dist'}
$version=([IO.File]::ReadAllText((Join-Path $SourceRoot 'VERSION'))).Trim();if($version -notmatch '^\d+\.\d+\.\d+(?:-[\w.]+)?$'){throw 'Invalid version'}
foreach($name in @(('BWTL-Setup-'+$version+'.exe'),('BWTL-'+$version+'-windows-x64.zip'),('BWTL-owner-data-'+$version+'.zip'))){if(Test-Path -LiteralPath (Join-Path $OutputDirectory $name)){throw 'Version artifacts already exist. Increment VERSION or choose a new output directory.'}}
$stage=Join-Path $WorkDirectory ('payload-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Force -Path $stage,(Join-Path $stage 'seed'),(Join-Path $stage 'archive'),$OutputDirectory|Out-Null
$csc=Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $csc /nologo /target:winexe /platform:x64 /optimize+ /codepage:65001 /reference:System.Windows.Forms.dll (('/out:{0}' -f (Join-Path $SourceRoot 'setup\BWTL-Manage.exe'))) (Join-Path $SourceRoot 'setup\Manager.cs')
if($LASTEXITCODE -ne 0){throw 'Manager compiler failed'}
Copy-Item -LiteralPath (Join-Path $SourceRoot 'setup\BWTL-Manage.exe') -Destination (Join-Path $SourceRoot 'setup\BWTL-Uninstall.exe') -Force
$sourceHashes=@(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File -Force | ForEach-Object {
    $relative=$_.FullName.Substring($SourceRoot.Length+1).Replace('\','/')
    if($relative -notmatch '^(dist|build)/' -and $relative -ne 'SHA256SUMS.txt'){
        (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$relative
    }
})
[IO.File]::WriteAllText((Join-Path $SourceRoot 'SHA256SUMS.txt'),($sourceHashes -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
foreach($name in @('bin','resources','src','docs','tests','setup','compatibility','VERSION','CHANGELOG.md','README.md','DEPLOYMENT.json','build.cmd')){Copy-Item -LiteralPath (Join-Path $SourceRoot $name) -Destination (Join-Path $stage $name) -Recurse}
Get-ChildItem -LiteralPath $DataSource -Recurse -File|ForEach-Object {
    $relative=$_.FullName.Substring(([IO.Path]::GetFullPath($DataSource)).TrimEnd('\').Length+1);$target=Join-Path $stage ('seed\'+$relative);New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target)|Out-Null;Copy-Item -LiteralPath $_.FullName -Destination $target
}
$guide="# 霸威天龙 $version 内置功能索引`n本版保留全部现有工具；历史只供参考，实际执行以编译协议和本次模型快照为准。`n`n"+[IO.File]::ReadAllText((Join-Path $SourceRoot 'docs\功能表.md'))
[IO.File]::WriteAllText((Join-Path $stage 'seed\feature-guide.md'),$guide,[Text.UTF8Encoding]::new($false))
$sourceCount=0
$sourceZip=[IO.Compression.ZipFile]::Open((Join-Path $stage ('archive\owner-project-complete-'+$version+'.zip')),[IO.Compression.ZipArchiveMode]::Create)
try{
    foreach($file in Get-ChildItem -LiteralPath $SourceRoot -Recurse -File -Force){
        $relative=$file.FullName.Substring($SourceRoot.Length+1).Replace('\','/')
        # Do not embed previous installers recursively. All application data,
        # source, resources, DLLs and historical native-project files remain.
        if($relative -match '^(dist|build)/'){continue}
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($sourceZip,$file.FullName,$relative,[IO.Compression.CompressionLevel]::Optimal)|Out-Null;$sourceCount++
    }
}finally{$sourceZip.Dispose()}
if($LegacyRoot){[IO.Compression.ZipFile]::CreateFromDirectory([IO.Path]::GetFullPath($LegacyRoot),(Join-Path $stage ('archive\owner-native-complete-'+$version+'.zip')),[IO.Compression.CompressionLevel]::Optimal,$false)}
$manifest=@(Get-ChildItem -LiteralPath $stage -Recurse -File|ForEach-Object {[ordered]@{Path=$_.FullName.Substring($stage.Length+1).Replace('\','/');SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant();Bytes=$_.Length}})
[IO.File]::WriteAllText((Join-Path $stage 'payload-manifest.json'),($manifest|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
$payload=Join-Path $WorkDirectory ('payload-'+[Guid]::NewGuid().ToString('N')+'.zip');[IO.Compression.ZipFile]::CreateFromDirectory($stage,$payload,[IO.Compression.CompressionLevel]::Optimal,$false)
$exe=Join-Path $OutputDirectory ('BWTL-Setup-'+$version+'.exe');$csc=Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $csc /nologo /target:winexe /platform:x64 /optimize+ /codepage:65001 /reference:System.Windows.Forms.dll /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll (('/resource:{0},BWTLPayload' -f $payload)) (('/out:{0}' -f $exe)) (Join-Path $SourceRoot 'setup\Bootstrap.cs')
if($LASTEXITCODE -ne 0){throw 'Installer compiler failed'}
$portable=Join-Path $OutputDirectory ('BWTL-'+$version+'-windows-x64.zip');Copy-Item -LiteralPath $payload -Destination $portable -Force
$owner=Join-Path $OutputDirectory ('BWTL-owner-data-'+$version+'.zip');$ownerTemp=Join-Path $WorkDirectory ('owner-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Force -Path (Join-Path $ownerTemp 'owner-backup\data')|Out-Null
Get-ChildItem -LiteralPath (Join-Path $stage 'seed') -File|ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $ownerTemp 'owner-backup\data') -Force}
if(Test-Path -LiteralPath $owner){throw 'Owner data version already exists; increment VERSION rather than overwrite an archive.'}
[IO.Compression.ZipFile]::CreateFromDirectory($ownerTemp,$owner,[IO.Compression.CompressionLevel]::Optimal,$false)
$artifacts=@($exe,$portable,$owner);$lines=@($artifacts|ForEach-Object {((Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($_))})
[IO.File]::WriteAllText((Join-Path $OutputDirectory ('SHA256SUMS-'+$version+'.txt')),($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
$report=[ordered]@{Version=$version;Stage=$stage;PayloadFiles=$manifest.Count;SeedFiles=@(Get-ChildItem -LiteralPath (Join-Path $stage 'seed') -Recurse -File).Count;FullProjectFiles=$sourceCount;ExcludedSourceDirectories=@('dist','build');LegacyFiles=if($LegacyRoot){@(Get-ChildItem -LiteralPath $LegacyRoot -Recurse -File).Count}else{0};Artifacts=$artifacts;Created=(Get-Date -Format o)}
[IO.File]::WriteAllText((Join-Path $WorkDirectory 'build-result.json'),($report|ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false));$report
