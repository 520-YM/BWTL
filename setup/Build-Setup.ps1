[CmdletBinding()]
param(
    [string]$SourceRoot=(Split-Path -Parent $PSScriptRoot),
    [Parameter(Mandatory=$true)][string]$WorkDirectory,
    [string]$OutputDirectory
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$SourceRoot=[IO.Path]::GetFullPath($SourceRoot).TrimEnd('\')
$WorkDirectory=[IO.Path]::GetFullPath($WorkDirectory)
if($WorkDirectory -eq $SourceRoot -or $WorkDirectory.StartsWith($SourceRoot+'\',[StringComparison]::OrdinalIgnoreCase)){
    throw 'Use a work directory outside the source checkout.'
}
if(-not $OutputDirectory){$OutputDirectory=Join-Path $SourceRoot 'dist'}
$version=[IO.File]::ReadAllText((Join-Path $SourceRoot 'VERSION')).Trim()
if($version -notmatch '^\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?$'){throw 'Invalid release version.'}
$exe=Join-Path $OutputDirectory "BWTL-Setup-$version.exe"
$zip=Join-Path $OutputDirectory "BWTL-$version-windows-x64.zip"
$checksums=Join-Path $OutputDirectory "SHA256SUMS-$version.txt"
foreach($file in @($exe,$zip,$checksums)){if(Test-Path -LiteralPath $file){throw "Release artifact already exists: $file"}}
$metadata=Get-Content -LiteralPath (Join-Path $SourceRoot 'DEPLOYMENT.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$dll=Join-Path $SourceRoot 'bin\BWTL.dll'
if($metadata.version -ne $version -or $metadata.binary_sha256 -ine (Get-FileHash -LiteralPath $dll).Hash -or -not $metadata.unlocked){throw 'DLL release metadata mismatch.'}
& (Join-Path $SourceRoot 'tests\Run-Regression.ps1') -Root $SourceRoot -WorkDirectory (Join-Path $WorkDirectory 'regression')
$stage=Join-Path $WorkDirectory ('payload-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $stage,(Join-Path $stage 'bin'),(Join-Path $stage 'seed'),(Join-Path $stage 'tests'),$OutputDirectory|Out-Null
foreach($name in @('resources','src','docs','setup','compatibility','scripts','VERSION','CHANGELOG.md','README.md','DEPLOYMENT.json','build.cmd','AGENTS.md','install.cmd','install.ps1','uninstall.ps1')){
    Copy-Item -LiteralPath (Join-Path $SourceRoot $name) -Destination $stage -Recurse
}
Get-ChildItem -LiteralPath (Join-Path $SourceRoot 'tests') -File | Copy-Item -Destination (Join-Path $stage 'tests')
Copy-Item -LiteralPath $dll -Destination (Join-Path $stage 'bin\BWTL.dll')
$featureList=Get-ChildItem -LiteralPath (Join-Path $SourceRoot 'docs') -Filter 'BWTL *.txt' | Select-Object -First 1
Copy-Item -LiteralPath $featureList.FullName -Destination (Join-Path $stage 'seed\feature-guide.md')
$manifest=@(Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {
    [ordered]@{Path=$_.FullName.Substring($stage.Length+1).Replace('\','/');SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant();Bytes=$_.Length}
})
$utf8=[Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $stage 'payload-manifest.json'),($manifest|ConvertTo-Json -Depth 4),$utf8)
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$csc=Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $csc /nologo /target:winexe /platform:x64 /optimize+ /codepage:65001 /reference:System.Windows.Forms.dll /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll ("/resource:$zip,BWTLPayload") ("/out:$exe") (Join-Path $SourceRoot 'setup\Bootstrap.cs')
if($LASTEXITCODE -ne 0){throw 'Installer compiler failed.'}
$lines=@($exe,$zip|ForEach-Object {(Get-FileHash -LiteralPath $_).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($_)})
[IO.File]::WriteAllLines($checksums,$lines,$utf8)
[pscustomobject]@{Version=$version;Files=$manifest.Count;Stage=$stage;Setup=$exe;Zip=$zip;Checksums=$checksums}
