
        ProPath before,after;
        e=ProDirectoryCurrentGet(before);if(!e)e=part_regenerate(model);if(!e)e=ProMdlSave(model);
        if(!e)e=ProDirectoryCurrentGet(after);
        if(!e&&_wcsicmp(before,after))e=PRO_TK_BAD_CONTEXT;
        if(!e)swprintf_s(text,capacity,L"零件 %ls 重生成检查通过，Creo 已报告保存成功；工作目录：%ls。未清理旧版本。",snapshot->name,before);break;
    
