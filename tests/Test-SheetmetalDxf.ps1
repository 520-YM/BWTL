[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
function Assert-True($condition,[string]$message){if(!$condition){throw $message}}
function Near([double]$a,[double]$b){return [Math]::Abs($a-$b) -lt 0.000001}
function Entities([string]$name){
 $lines=[IO.File]::ReadAllLines((Join-Path $WorkDirectory ($name+'.dxf')))
 Assert-True ($lines.Count%2 -eq 0) 'DXF group pairs incomplete'
 $inside=$false;$entity=$null;$result=[Collections.Generic.List[object]]::new()
 for($i=0;$i -lt $lines.Count;$i+=2){
  $code=[int]$lines[$i];$value=$lines[$i+1].Trim()
  if($code -eq 2 -and $value -eq 'ENTITIES'){$inside=$true;continue}
  if(!$inside){continue}
  if($code -eq 0){if($entity){$result.Add($entity)};if($value -eq 'ENDSEC'){break};$entity=@{Type=$value}}
  elseif($entity){$entity[$code]=$value}
 }
 Assert-True ($lines -contains '$INSUNITS') 'Missing millimeter unit marker'
 $units=[Array]::IndexOf($lines,'$INSUNITS');Assert-True ($lines[$units+1] -eq '70' -and $lines[$units+2] -eq '4') 'Wrong DXF length units'
 Assert-True ($lines[-1] -eq 'EOF') 'Missing EOF'
 foreach($e in $result){
  Assert-True ($e.Type -in @('LINE','ARC','CIRCLE')) 'Unexpected approximated entity'
  Assert-True ($e[8] -in @('OUTLINE','HOLES','BEND')) 'Wrong layer'
  foreach($key in @(10,11,20,21,30,31,40,50,51)){if($e.ContainsKey($key)){$v=[double]::Parse($e[$key],[Globalization.CultureInfo]::InvariantCulture);Assert-True (![double]::IsNaN($v) -and ![double]::IsInfinity($v)) 'Non-finite coordinate'}}
 }
 return $result.ToArray()
}
$bent=@(Entities 'BENT_BLANK');$hole=@(Entities 'HOLED_BLANK');$square=@(Entities 'SQUARE_BLANK')
Assert-True (@($bent|Where-Object {$_[8] -eq 'OUTLINE'}).Count -eq 8) 'Bend external contour changed'
$bend=@($bent|Where-Object {$_[8] -eq 'BEND'});Assert-True ($bend.Count -eq 1) 'Missing or duplicated true bend center'
Assert-True ((Near ([double]$bend[0][10]) 0) -and (Near ([double]$bend[0][11]) 80)) 'Bend line length wrong'
$expectedCenter=-40-([Math]::PI/2*(2+0.4*2))/2
Assert-True ((Near ([double]$bend[0][20]) $expectedCenter) -and (Near ([double]$bend[0][21]) $expectedCenter)) 'Bend line is not at developed strip center'
$arcs=@($hole|Where-Object {$_[8] -eq 'HOLES'})
Assert-True ($arcs.Count -eq 2) 'Expected exact semicircular hole edges'
foreach($a in $arcs){Assert-True ($a.Type -eq 'ARC' -and (Near ([double]$a[10]) 20) -and (Near ([double]$a[20]) -10) -and (Near ([double]$a[40]) 3)) 'Wrong hole position or radius'}
Assert-True ((Near ([double]$arcs[0][50]) ([double]$arcs[1][51])) -and (Near ([double]$arcs[0][51]) ([double]$arcs[1][50]))) 'Hole arcs do not close'
Assert-True (@($hole|Where-Object {$_[8] -eq 'BEND'}).Count -eq 1) 'Holed blank bend line absent'
Assert-True ($square.Count -eq 4) 'Square outline changed'
foreach($e in $square){Assert-True ($e.Type -eq 'LINE' -and $e[8] -eq 'OUTLINE') 'Square outline invalid';$dx=[double]$e[11]-[double]$e[10];$dy=[double]$e[21]-[double]$e[20];Assert-True (Near ([Math]::Sqrt($dx*$dx+$dy*$dy)) 40) 'Wrong flat blank dimensions'}
Assert-True (!(Test-Path -LiteralPath (Join-Path $WorkDirectory 'FORMED_REJECT.dxf'))) 'Unsupported form generated output'
Assert-True (@(Get-ChildItem -LiteralPath $WorkDirectory -Filter '.bwtl_*.tmp' -Force).Count -eq 0) 'Export temporary file leaked'
'SHEETMETAL DXF PASS: exact mm units, 40x40 flat contour, hole position/radius/closed arcs, true developed bend center, unsupported-form refusal and no temporary file.'
