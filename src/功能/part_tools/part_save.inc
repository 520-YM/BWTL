
        ProPath before,after;
        e=ProDirectoryCurrentGet(before);if(!e)e=part_regenerate(model);if(!e)e=ProMdlSave(model);
        if(!e)e=ProDirectoryCurrentGet(after);
        if(!e&&_wcsicmp(before,after))e=PRO_TK_BAD_CONTEXT;
        if(!e){
            ProPath delivered;
            ProError checked=ax_native_source(before,snapshot->name,snapshot->type,delivered);
            if(!checked&&dv_file_nonempty(delivered))
                swprintf_s(text,capacity,L"%ls 已重生成，Creo 已报告保存成功。已核对当前项目目录中的模型文件：%ls。未清理旧版本。",snapshot->name,delivered);
            else swprintf_s(text,capacity,L"%ls 重生成通过，Creo 已报告保存成功；工作目录：%ls。未能核实此目录中的保存文件。",snapshot->name,before);
        }
        break;
    
