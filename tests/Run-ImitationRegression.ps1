[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$WorkDirectory,
    [ValidateRange(20,180)][int]$TimeoutSeconds=45,
    [string]$CreoBase='C:\Program Files\PTC\Creo 10.0.0.0',
    [string]$StandardsConfig='C:\PTC\Pro_Stds-C\00_config\config.pro'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$WorkDirectory=[IO.Path]::GetFullPath($WorkDirectory)
if([IO.Path]::GetPathRoot($WorkDirectory) -ine 'E:\' -or
   [IO.Path]::GetPathRoot($root) -ine 'E:\'){
    throw 'BWTL source and native regression output must be on E:.'
}
if(Test-Path -LiteralPath $WorkDirectory){throw 'Choose a new native regression directory.'}
$null=New-Item -ItemType Directory -Path $WorkDirectory
$previousTemp=$env:TEMP
$previousTmp=$env:TMP
$env:TEMP=(New-Item -ItemType Directory -Path (Join-Path $WorkDirectory 'temp')).FullName
$env:TMP=$env:TEMP
$cases=@(
    @{Group='Sketch';Name='section-basics';Pattern='^BASICS=1 JOURNAL=1 CLOSED_LOOPS=0 OPEN_LOOP=0 PASS=1$'},
    @{Group='Sketch';Name='sketch-curves';Pattern='^PASS=1 ERROR=0$'},
    @{Group='Sketch';Name='sketch-constraints';Pattern='^ORIGINAL=1b7 REBUILT=1b7 PASS=1 ERROR=0$'},
    @{Group='Sketch';Name='sketch-design-constraints';Pattern='^ORIGINAL=ff7 REBUILT=ff7 LOCKED_POINT=1 PASS=1 ERROR=0$'},
    @{Group='Solid';Name='extrusions';Pattern='^SAME=0 SEPARATE=0 PASS=1$'},
    @{Group='Solid';Name='boolean';Pattern='^MERGE=0 SUBTRACT=0 INTERSECT=0 SPLIT=0 PASS=1$'},
    @{Group='Solid';Name='stepped-hole';Pattern='^PASS=1 ERROR=0$'},
    @{Group='Pattern';Name='dimension-pattern';Pattern='^DIMENSION_ONE=0 DIMENSION_BOTH=0 DIRECTION=0 DIRECTION_BOTH=0 PASS=1$'},
    @{Group='Pattern';Name='mirror';Pattern='^SINGLE=0 MULTIPLE=0 PASS=1$'},
    @{Group='Quilt';Name='quilt-edits';Pattern='^OFFSET=0 EXTEND=0 TRIM=0 MERGE=0 PASS=1$'},
    @{Group='Quilt';Name='quilt-solid';Pattern='^POS=0 NEG=0 SYM=0 SOLID=0 THICKEN_CUT=0 SOLIDIFY_CUT=0 PASS=1$'},
    @{Group='Quilt';Name='quilt-check';Pattern='^JOINED=0 LOCAL=0 PASS=1$'},
    @{Group='Tree';Name='tree-verification';Pattern='^GROUPS=0 SKETCHES=0 PASS=1$'},
    @{Group='Tree';Name='tree-structure';Pattern='^PASS=1 ERROR=0$'},
    @{Group='SheetMetal';Name='first-wall';Pattern='^POSITIVE=0 NEGATIVE=0 PASS=1$'},
    @{Group='SheetMetalAndSolid';Name='batch-shapes';Pattern='^CASES=12 PASS=1$'},
    @{Group='SheetMetalAndSolid';Name='batch-design';Pattern='^DESIGN CASES=10 PASS=1$'},
    @{Group='Workflow';Name='standard-size';Pattern='^PASS=1 ERROR=0$'},
    @{Group='External';Name='external-paths';Pattern='^PASS=1 ERROR=0$'},
    @{Group='External';Name='family';Pattern='^PASS=1 ERROR=0$'},
    @{Group='ImportedGeometry';Name='inferred';Pattern='^STEP=0 IGES=0 PASS=1$'}
)
$receipt=Join-Path $WorkDirectory 'imitation-regression.json'
$launcher=Join-Path $PSScriptRoot 'Run-ExpansionIsolated.ps1'
$results=[Collections.Generic.List[object]]::new()
try{
    foreach($case in $cases){
        $name=$case.Name
        $caseRoot=Join-Path $WorkDirectory $name
        $nativeLog=Join-Path $caseRoot "workspace/replay-$name.log"
        $traceback=Join-Path $caseRoot 'workspace/traceback.log'
        $isolation=Join-Path $caseRoot 'data/isolation-session.txt'
        $source=Join-Path $PSScriptRoot ('native_replay_'+$name.Replace('-','_')+'.c')
        $passed=$false
        try{
            $launch=& $launcher -Root $root -Suite Custom -CustomSource $source -WorkDirectory $caseRoot -CreoBase $CreoBase -StandardsConfig $StandardsConfig
            $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            do{
                if(Test-Path -LiteralPath $traceback){throw "Creo traceback at $traceback"}
                if((Test-Path -LiteralPath $nativeLog) -and (Select-String -LiteralPath $nativeLog -Pattern 'PASS=[01]' -Quiet)){break}
                Start-Sleep -Milliseconds 400
            }while([DateTime]::UtcNow -lt $deadline)
            if(!(Test-Path -LiteralPath $nativeLog)){throw "Creo test timed out, launcher PID $($launch.LauncherPID)"}
            if(!(Select-String -LiteralPath $nativeLog -Pattern $case.Pattern -Quiet)){throw 'Native result missing or failed.'}
            if(!(Test-Path -LiteralPath $isolation) -or !(Select-String -LiteralPath $isolation -Pattern '^LegacyBridgeLoaded=0$' -Quiet)){
                throw 'Isolation record missing or legacy bridge was loaded.'
            }
            $passed=$true
            $detail='Passed'
        }catch{
            $detail=$_.Exception.Message
        }
        $result=[pscustomobject]@{Group=$case.Group;Name=$name;Passed=$passed;Detail=$detail;Log=$nativeLog}
        $results.Add($result)
        $result
        ConvertTo-Json -InputObject $results.ToArray() -Depth 4 | Set-Content -LiteralPath $receipt -Encoding utf8
    }
}finally{
    $env:TEMP=$previousTemp
    $env:TMP=$previousTmp
}
if($results.Where({!$_.Passed}).Count){throw "Native imitation regression failed. See $receipt"}
"Native imitation regression passed $($results.Count) suites."
