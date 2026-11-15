[CmdletBinding()]
param([Parameter(Mandatory)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($WorkDirectory)
$journal=Join-Path $root 'data/imitation-native-diagnostics.log'
$traceback=Join-Path $root 'workspace/traceback.log'
if(!(Test-Path -LiteralPath $journal -PathType Leaf)){throw 'Native imitation journal not found.'}

$pending=[ordered]@{}
$context=@{}
$failures=0
$steps=0
$active=''
foreach($line in Get-Content -LiteralPath $journal -Encoding UTF8){
    if($line -match '^(?<phase>BEGIN|END) pid=(?<pid>\d+) target=(?<target>\S+).*? step=(?<step>\d+) feature=(?<feature>\d+) '){
        $key=@($Matches.pid,$Matches.target,$Matches.step,$Matches.feature) -join '/'
        if($Matches.phase -eq 'BEGIN'){
            $active=$key
            $pending[$key]=$line
        }elseif($pending.Contains($key)){
            $pending.Remove($key)
            $context.Remove($key)
            $active=''
            $steps++
            if($line -notmatch ' creo=0$'){$failures++}
        }
    }elseif($active -and $line.StartsWith('CONTEXT ')){
        $context[$active]=$line
    }
}
$hasTraceback=Test-Path -LiteralPath $traceback -PathType Leaf
$incomplete=@(foreach($key in $pending.Keys){
    [pscustomobject]@{Step=$pending[$key];Context=$context[$key]}
})
[pscustomobject]@{
    CompletedSteps=$steps
    FailedSteps=$failures
    Incomplete=$incomplete
    Traceback=if($hasTraceback){$traceback}else{$null}
}
