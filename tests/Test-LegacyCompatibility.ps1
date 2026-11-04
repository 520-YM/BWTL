[CmdletBinding()]
param([string]$Root)
$ErrorActionPreference='Stop'
if(-not $Root){$Root=Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)}
$Root=[IO.Path]::GetFullPath($Root)
$baseline=Get-Content -LiteralPath (Join-Path $Root 'compatibility/legacy-v21.1.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$failures=@()
foreach($entry in $baseline.Files){
    $path=Join-Path $Root $entry.Path
    if(-not (Test-Path -LiteralPath $path -PathType Leaf)){$failures+='Missing: '+$entry.Path;continue}
    $stream=[IO.File]::OpenRead($path);$sha=[Security.Cryptography.SHA256]::Create()
    try{$hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant()}
    finally{$sha.Dispose();$stream.Dispose()}
    if($hash -ne $entry.SHA256){$failures+='Changed frozen legacy file: '+$entry.Path}
}
if($failures.Count){throw ($failures -join "`n")}
Write-Output "LEGACY COMPATIBILITY PASS: $($baseline.Files.Count) frozen source/contract files; baseline $($baseline.Version)."
