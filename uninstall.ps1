param([Parameter(Mandatory=$true)][string]$ConfigPath)
$ErrorActionPreference='Stop'
$ConfigPath=[IO.Path]::GetFullPath($ConfigPath)
$text=[IO.File]::ReadAllText($ConfigPath)
$pattern='(?ms)^! BWTL BEGIN\r?\n.*?^! BWTL END(?:\r?\n)?'
if($text -notmatch $pattern){Write-Output 'No BWTL managed registration found; no changes made.';return}
Copy-Item -LiteralPath $ConfigPath -Destination ($ConfigPath+'.bwtl-uninstall-backup-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-ffff'))
[IO.File]::WriteAllText($ConfigPath,[regex]::Replace($text,$pattern,''),[Text.UTF8Encoding]::new($false))
Write-Output 'BWTL registration removed. Data and program files were retained. Takes effect on next Creo startup.'
