[CmdletBinding()]
param([string]$ConfigPath,[string]$CodexPath,[string]$DataDirectory,[switch]$ImportOwnerData,[switch]$PlanOnly)
$ErrorActionPreference='Stop'
$packageRoot=[IO.Path]::GetFullPath($PSScriptRoot)
if($packageRoot.Length -gt 180){throw 'Please extract to a shorter writable path, such as D:\BWTL.'}
$dll=Join-Path $packageRoot 'bin\BWTL.dll'
$resources=Join-Path $packageRoot 'resources'
if(-not(Test-Path -LiteralPath $dll)){throw 'bin\BWTL.dll is missing. Download the binary package or build first.'}
if(-not(Test-Path -LiteralPath (Join-Path $resources 'text\codex_native_messages.txt'))){throw 'Resource files missing.'}
if(-not $ConfigPath){$ConfigPath=Read-Host 'Enter the full path of config.pro read by your Creo startup'}
$ConfigPath=[IO.Path]::GetFullPath($ConfigPath)
if([IO.Path]::GetFileName($ConfigPath) -ine 'config.pro'){throw 'ConfigPath must point to config.pro.'}
if(-not(Test-Path -LiteralPath (Split-Path -Parent $ConfigPath))){throw 'The config.pro parent directory must already exist.'}
if($CodexPath){$CodexPath=[IO.Path]::GetFullPath($CodexPath);if(-not(Test-Path -LiteralPath $CodexPath -PathType Leaf) -or [IO.Path]::GetExtension($CodexPath) -ine '.exe'){throw 'CodexPath must point to an existing codex.exe.'}}
if(-not $DataDirectory){$DataDirectory=Join-Path $packageRoot 'data'}
$DataDirectory=[IO.Path]::GetFullPath($DataDirectory)
foreach($value in @($ConfigPath,$CodexPath,$DataDirectory,$packageRoot)){if($value -match '[\r\n\"]'){throw 'Paths must not contain quotes or line breaks.'}}
$registration=Join-Path $packageRoot 'protk.dat'
$settings=Join-Path $packageRoot 'settings.ini'
$old=if(Test-Path -LiteralPath $ConfigPath){[IO.File]::ReadAllText($ConfigPath)}else{''}
$pattern='(?ms)^! BWTL BEGIN\r?\n.*?^! BWTL END(?:\r?\n)?'
$block="! BWTL BEGIN`r`nprotkdat `"$registration`"`r`n! BWTL END`r`n"
$new=if($old -match $pattern){[regex]::Replace($old,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$block})}else{$old.TrimEnd()+"`r`n"+$block}
if($ImportOwnerData){
    $backupSource=Join-Path $packageRoot 'owner-backup\data'
    if(-not(Test-Path -LiteralPath $backupSource)){throw 'Owner backup missing. Download it separately from the repository.'}
    foreach($entry in Get-ChildItem -LiteralPath $backupSource -File){if(Test-Path -LiteralPath (Join-Path $DataDirectory $entry.Name)){throw 'Import would overwrite existing data. Choose a new empty DataDirectory.'}}
}
if($PlanOnly){[pscustomobject]@{Package=$packageRoot;Config=$ConfigPath;Registration=$registration;Data=$DataDirectory;ImportOwnerData=[bool]$ImportOwnerData};return}
$encoding=[Text.UTF8Encoding]::new($false)
$stamp=[DateTime]::Now.ToString('yyyyMMdd-HHmmss-ffff')
if(Test-Path -LiteralPath $ConfigPath){Copy-Item -LiteralPath $ConfigPath -Destination "$ConfigPath.bwtl-backup-$stamp"}
if(Test-Path -LiteralPath $settings){Copy-Item -LiteralPath $settings -Destination "$settings.backup-$stamp"}
New-Item -ItemType Directory -Path $DataDirectory -Force | Out-Null
[IO.File]::WriteAllText($settings,"[Runtime]`r`nCodexPath=$CodexPath`r`nDataDirectory=$DataDirectory`r`n",[Text.Encoding]::Unicode)
[IO.File]::WriteAllText($registration,"NAME BWTL`r`nSTARTUP dll`r`nEXEC_FILE `"$dll`"`r`nTEXT_DIR `"$resources`"`r`nALLOW_STOP TRUE`r`nDELAY_START FALSE`r`nEND`r`n",$encoding)
if($ImportOwnerData){Get-ChildItem -LiteralPath $backupSource -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $DataDirectory $_.Name)}}
[IO.File]::WriteAllText($ConfigPath,$new,$encoding)
Write-Output 'BWTL registered. Restart Creo when your work is saved, then use the cdx mapkey to open the assistant.'
Write-Output "Data directory: $DataDirectory"
Write-Output 'Creo and Codex licenses/logins are not included. See docs/INSTALL.zh-CN.md for deployment status.'
