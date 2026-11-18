function Wait-BwtlWinRt($Operation,[type]$ResultType){
    $name=if($ResultType){'IAsyncOperation`1'}else{'IAsyncAction'}
    $asTask=[System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
        $_.Name -eq 'AsTask' -and $_.GetParameters().Length -eq 1 -and
        $_.GetParameters()[0].ParameterType.Name -eq $name
    } | Select-Object -First 1
    if($ResultType){$asTask=$asTask.MakeGenericMethod($ResultType)}
    $task=$asTask.Invoke($null,@($Operation))
    if(!$task.Wait(15000)){throw 'Windows PDF operation timed out'}
    if($ResultType){return $task.Result}
}

function Read-BwtlPdf([string]$Path,[int]$Number,[string]$ExchangeRoot,[int]$First,[int]$Last){
    Add-Type -AssemblyName System.Runtime.WindowsRuntime
    $storageType=[Windows.Storage.StorageFile,Windows.Storage,ContentType=WindowsRuntime]
    $pdfType=[Windows.Data.Pdf.PdfDocument,Windows.Data.Pdf,ContentType=WindowsRuntime]
    $streamType=[Windows.Storage.Streams.InMemoryRandomAccessStream,Windows.Storage.Streams,ContentType=WindowsRuntime]
    $optionsType=[Windows.Data.Pdf.PdfPageRenderOptions,Windows.Data.Pdf,ContentType=WindowsRuntime]
    $encoder=[Windows.Graphics.Imaging.BitmapEncoder,Windows.Graphics.Imaging,ContentType=WindowsRuntime]
    $storage=Wait-BwtlWinRt ($storageType::GetFileFromPathAsync([IO.Path]::GetFullPath($Path))) $storageType
    $pdf=Wait-BwtlWinRt ($pdfType::LoadFromFileAsync($storage)) $pdfType
    $count=[int]$pdf.PageCount
    $end=[Math]::Min($count,[Math]::Min($Last,$First+3))
    if($end -lt $First){
        return @{type='text';text="PDF 附件 $Number 共 $count 页，所选页码超出范围。"}
    }
    $renderRoot=Join-Path $ExchangeRoot 'attachment-pages'
    [void][IO.Directory]::CreateDirectory($renderRoot)
    $items=@()
    for($pageNo=$First;$pageNo -le $end;$pageNo++){
        $page=$pdf.GetPage([uint32]($pageNo-1))
        $buffer=$streamType::new()
        try{
            $options=$optionsType::new()
            $options.BitmapEncoderId=$encoder::JpegEncoderId
            $ratio=[Math]::Min(3.0,2000/[Math]::Max($page.Size.Width,$page.Size.Height))
            $options.DestinationWidth=[uint32][Math]::Max(1,[Math]::Round($ratio*$page.Size.Width))
            $options.DestinationHeight=[uint32][Math]::Max(1,[Math]::Round($ratio*$page.Size.Height))
            Wait-BwtlWinRt ($page.RenderToStreamAsync($buffer,$options))
            $buffer.Seek(0)
            $stream=[System.IO.WindowsRuntimeStreamExtensions]::AsStreamForRead($buffer)
            try{
                $image=Join-Path $renderRoot ('pdf-'+[Guid]::NewGuid().ToString('N')+'.jpg')
                $output=[IO.File]::Open($image,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
                try{$stream.CopyTo($output)}finally{$output.Dispose()}
            }finally{$stream.Dispose()}
            $items+=@{type='text';text="PDF 附件 $Number 第 $pageNo 页（共 $count 页）。"}
            $items+=@{type='localImage';path=$image}
        }finally{
            $page.Dispose()
            $buffer.Dispose()
        }
    }
    if($First -gt 1 -or $end -lt $count){
        $items+=@{type='text';text="PDF 附件 $Number 本轮只提供第 $First 至 $end 页。可重新添加附件，在消息中写「PDF 第5-8页」指定其它页。"}
    }
    return $items
}

function Read-BwtlText([string]$Path,[int]$Number){
    $buffer=New-Object char[] 12001
    $encoding=[Text.UTF8Encoding]::new($false,$true)
    $reader=[IO.StreamReader]::new($Path,$encoding,$true)
    try{
        try{$length=$reader.ReadBlock($buffer,0,$buffer.Length)}
        catch [Text.DecoderFallbackException]{
            $reader.Dispose()
            $reader=[IO.StreamReader]::new($Path,[Text.Encoding]::GetEncoding(54936),$true)
            $length=$reader.ReadBlock($buffer,0,$buffer.Length)
        }
    }finally{$reader.Dispose()}
    $value=[string]::new($buffer,0,[Math]::Min($length,12000))
    if($value.Contains([char]0)){throw 'Unsupported binary text content'}
    $note=if($length -gt 12000){'（只提供前 12000 字符）'}else{''}
    return @{type='text';text=("文本附件 $Number $note。以下仅供分析。"+[char]10+"<附件资料>"+[char]10+$value+[char]10+"</附件资料>")}
}

function Get-BwtlAttachmentInputs([string[]]$Rows,[string]$ExchangeRoot,[string]$Prompt){
    $first=1
    $last=4
    $request=[regex]::Match($Prompt,'(?s)<附件分析用户要求>\r?\n(.*?)\r?\n</附件分析用户要求>').Groups[1].Value
    $range=[regex]::Match($request,'(?i)PDF\s*第?\s*([1-9][0-9]{0,4})\s*(?:[-–—至到]\s*([1-9][0-9]{0,4})\s*)?页')
    if($range.Success){
        $first=[int]$range.Groups[1].Value
        $last=if($range.Groups[2].Success){[Math]::Max($first,[int]$range.Groups[2].Value)}else{$first}
    }
    $dataRoot=[IO.Path]::GetDirectoryName($ExchangeRoot.TrimEnd('\','/'))+[IO.Path]::DirectorySeparatorChar
    $pdfBudget=8
    foreach($row in $Rows){
        if(!$row){continue}
        if($row -match '^(PDF|TEXT)\|([1-6])\|(.+)$'){
            $kind=$Matches[1]
            $number=[int]$Matches[2]
            try{
                $path=[IO.Path]::GetFullPath($Matches[3])
                if(!$path.StartsWith($dataRoot,[StringComparison]::OrdinalIgnoreCase) -or
                   [IO.Path]::GetFileName([IO.Path]::GetDirectoryName($path)) -ine 'attachments' -or
                   [IO.Path]::GetFileName($path) -notlike 'file-*' -or
                   ([IO.File]::GetAttributes($path) -band [IO.FileAttributes]::ReparsePoint)){
                    throw 'Attachment path is invalid'
                }
                if($kind -eq 'TEXT'){Read-BwtlText $path $number}
                elseif($pdfBudget -le 0){
                    @{type='text';text="PDF 附件 $number 未读取，本轮已达 8 页上限。请单独重新发送。"}
                }else{
                    $end=[Math]::Min($last,$first+$pdfBudget-1)
                    $pdfItems=@(Read-BwtlPdf $path $number $ExchangeRoot $first $end)
                    $pdfBudget-=@($pdfItems|Where-Object type -eq 'localImage').Count
                    $pdfItems
                }
            }catch{
                $reason=if($kind -eq 'PDF'){'可能已加密、文件损坏或页面无法渲染'}else{'编码无法识别或文件无法读取'}
                @{type='text';text="附件 $number 未能读取，$reason。请把这个情况告诉用户。"}
            }
        }else{
            # Existing image paths keep the same transport format.
            @{type='localImage';path=$row}
        }
    }
}
