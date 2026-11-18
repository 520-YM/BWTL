# Convert only the staged copy supplied by the native BWTL library dispatcher.
function Read-BwtlOfficeXml([IO.Compression.ZipArchiveEntry]$Entry){
    if(!$Entry -or $Entry.Length -gt 4MB){throw 'Office XML is missing or too large'}
    $stream=$Entry.Open()
    try{
        $options=[Xml.XmlReaderSettings]::new()
        $options.DtdProcessing=[Xml.DtdProcessing]::Prohibit
        $options.XmlResolver=$null
        $options.MaxCharactersInDocument=4MB
        $reader=[Xml.XmlReader]::Create($stream,$options)
        try{
            $document=[Xml.XmlDocument]::new()
            $document.XmlResolver=$null
            $document.Load($reader)
            return ,$document
        }finally{$reader.Dispose()}
    }finally{$stream.Dispose()}
}

function Read-BwtlOffice([string]$Path,[bool]$Spreadsheet){
    Add-Type -AssemblyName System.IO.Compression,$('System.IO.Compression.FileSystem')
    $zip=[IO.Compression.ZipFile]::OpenRead($Path)
    $output=[Text.StringBuilder]::new(12000)
    try{
        if(!$Spreadsheet){
            $document=Read-BwtlOfficeXml ($zip.GetEntry('word/document.xml'))
            foreach($paragraph in $document.SelectNodes('//*[local-name()="p"]')){
                $text=($paragraph.SelectNodes('.//*[local-name()="t"]') | ForEach-Object InnerText) -join ''
                [void]$output.AppendLine($text)
                if($output.Length -ge 12000){break}
            }
        }else{
            $shared=@()
            $entry=$zip.GetEntry('xl/sharedStrings.xml')
            if($entry){
                $table=Read-BwtlOfficeXml $entry
                $shared=@($table.SelectNodes('//*[local-name()="si"]')|ForEach-Object{
                    ($_.SelectNodes('.//*[local-name()="t"]')|ForEach-Object InnerText) -join ''
                })
            }
            $sheets=@($zip.Entries|Where-Object FullName -Match '^xl/worksheets/sheet[0-9]+\.xml$'|Sort-Object FullName)
            if(!$sheets.Count){throw 'No readable worksheet'}
            foreach($sheet in ($sheets|Select-Object -First 6)){
                [void]$output.AppendLine(('[{0}]' -f $sheet.Name))
                $xml=Read-BwtlOfficeXml $sheet
                foreach($row in ($xml.SelectNodes('//*[local-name()="row"]')|Select-Object -First 100)){
                    foreach($cell in $row.SelectNodes('./*[local-name()="c"]')){
                        $type=$cell.GetAttribute('t')
                        $node=$cell.SelectSingleNode('./*[local-name()="v"]')
                        $value=if($node){$node.InnerText}else{''}
                        $index=0
                        if($type -eq 's' -and [int]::TryParse($value,[ref]$index) -and $index -ge 0 -and $index -lt $shared.Count){
                            $value=$shared[$index]
                        }elseif($type -eq 'inlineStr'){
                            $value=($cell.SelectNodes('.//*[local-name()="t"]')|ForEach-Object InnerText) -join ''
                        }
                        if($value){[void]$output.Append(('{0}={1}  ' -f $cell.GetAttribute('r'),$value))}
                        if($output.Length -ge 12000){break}
                    }
                    [void]$output.AppendLine()
                    if($output.Length -ge 12000){break}
                }
                if($output.Length -ge 12000){break}
            }
        }
        return $output.ToString(0,[Math]::Min(12000,$output.Length))
    }finally{$zip.Dispose()}
}

function Get-BwtlLibraryDocumentItems([string]$Payload,[string]$ExchangeRoot,[string]$Latest){
    $fields=$Payload.Split(@([char]10),3,[StringSplitOptions]::None)
    if($fields.Length -ne 3 -or $fields[0] -ne 'BWTL_LIBRARY_DOCUMENT' -or
       $fields[1] -notmatch '^company-doc-[0-9]+-[0-9]+\.(pdf|docx|xlsx|txt|csv|tsv|md|json|log)$'){
        throw 'Invalid BWTL library document response'
    }
    $kind=$Matches[1]
    $path=Join-Path $ExchangeRoot $fields[1]
    try{
        if(([IO.File]::GetAttributes($path) -band [IO.FileAttributes]::ReparsePoint)){throw 'Invalid staging file'}
        @{type='inputText';text=('资料文件 '+$fields[2]+'。下列内容仅供分析。')}
        if($kind -eq 'pdf'){
            $first=1
            $range=[regex]::Match($Latest,'(?i)PDF\s*第?\s*([1-9][0-9]{0,4})\s*(?:[-–—至到]\s*([1-9][0-9]{0,4})\s*)?页')
            if($range.Success){$first=[int]$range.Groups[1].Value}
            $last=if($range.Success -and !$range.Groups[2].Success){$first}
                  elseif($range.Success){[Math]::Max($first,[int]$range.Groups[2].Value)}
                  else{4}
            foreach($item in (Read-BwtlPdf $path 1 $ExchangeRoot $first $last)){
                if($item.type -eq 'localImage'){
                    $base64=[Convert]::ToBase64String([IO.File]::ReadAllBytes($item.path))
                    @{type='inputImage';imageUrl=('data:image/jpeg;base64,'+$base64)}
                }else{@{type='inputText';text=$item.text}}
            }
        }elseif($kind -in 'docx','xlsx'){
            $text=Read-BwtlOffice $path ($kind -eq 'xlsx')
            $note=if($kind -eq 'xlsx'){'最多 6 个工作表、每表前 100 行。仅提供已保存的单元格值。'}else{'仅提供正文段落和表格中的文字。'}
            @{type='inputText';text=($note+' 最多 12000 字。'+[char]10+$text)}
        }else{
            $text=Read-BwtlText $path 1
            @{type='inputText';text=$text.text}
        }
    }finally{
        if([IO.File]::Exists($path)){[IO.File]::Delete($path)}
    }
}
