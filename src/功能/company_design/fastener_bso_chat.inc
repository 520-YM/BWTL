enum { BSO_FIND=1,BSO_SAVE,BSO_PLACE };
static int fastener_bso_spec(const wchar_t **input){
    wchar_t key[32];
    size_t n=0;
    const wchar_t *p=fastener_chat_separator(*input);
    wchar_t close=*p==L'「'?L'」':*p==L'“'?L'”':*p==L'"'?L'"':0;
    if(close)p++;
    for(;*p;p++){
        wchar_t c=towupper(*p);
        if(c==L' '||c==L'\t'||c==L'-'||c==L'_'||c==L'.')continue;
        if(c==L'×')c=L'X';
        if(!wcschr(L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",c))break;
        if(n+1>=_countof(key))return 0;
        key[n++]=c;
    }
    if(close&&*p++!=close)return 0;
    key[n]=0;
    if(wcscmp(key,L"BSOM3X6")&&wcscmp(key,L"BSO35M3X6"))return 0;
    *input=p;
    return 1;
}

static int fastener_bso_request_mode(const wchar_t *text,int permit_reference){
    const wchar_t *p=model_request_action_start(text);
    if(wcspbrk(p,L"?？\r\n"))return 0;
    assembly_take(&p,L"你先");
    int action=BSO_FIND;
    if(assembly_take(&p,L"装配")||assembly_take(&p,L"安装"))action=BSO_PLACE;
    else if(assembly_take(&p,L"打开并保存")||assembly_take(&p,L"读取并另存")||assembly_take(&p,L"我要")||
        assembly_take(&p,L"调出")||assembly_take(&p,L"打开")||assembly_take(&p,L"另存"))action=BSO_SAVE;
    else if(!assembly_take(&p,L"查找")&&!assembly_take(&p,L"搜索"))assembly_take(&p,L"读取");
    int object_first=assembly_take(&p,L"把");
    fastener_chat_scope(&p);
    int explicit=fastener_bso_spec(&p);
    if(!explicit){
        if(!permit_reference||!object_first||!assembly_take(&p,L"它"))return 0;
        const wchar_t *prior=NULL;
        for(const wchar_t *line=conversation;(line=wcsstr(line,L"\n用户："))!=NULL;line+=4)prior=line+4;
        if(!prior)return 0;
        size_t length=wcscspn(prior,L"\r\n");
        wchar_t last[256];
        if(!length||length>=_countof(last)||wcsncpy_s(last,_countof(last),prior,length)||
            !fastener_bso_request_mode(last,0))return 0;
    }
    p=fastener_chat_separator(p);
    if(assembly_take(&p,L"装配到")||assembly_take(&p,L"装到")||assembly_take(&p,L"安装到"))action=BSO_PLACE;
    else if(assembly_take(&p,L"调出来")||assembly_take(&p,L"调出")||assembly_take(&p,L"另存到")||
        assembly_take(&p,L"并另存到")||assembly_take(&p,L"并保存到"))action=BSO_SAVE;
    if(action==BSO_PLACE){
        assembly_take(&p,L"到");
        if(!assembly_take(&p,L"选中的孔位")&&!assembly_take(&p,L"选中的孔")&&
            !assembly_take(&p,L"对应位置上")&&!assembly_take(&p,L"对应位置"))assembly_take(&p,L"这几个孔");
    }
    if(action==BSO_SAVE){
        if(!assembly_take(&p,L"我的工作目录")&&!assembly_take(&p,L"当前工作目录"))assembly_take(&p,L"工作目录");
        assembly_take(&p,L"里面");
    }
    return fastener_chat_end(p)?action:0;
}

static int fastener_bso_request(const wchar_t *text){
    return fastener_bso_request_mode(text,1);
}

static int fastener_bso_chat(const wchar_t *text){
    int action=fastener_bso_request(text);
    if(!action)return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    smart_begin(text);
    smart_local_plan=1;
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    wchar_t report[3500]=L"",library[MAX_PATH]=L"",saved[MAX_PATH]=L"";
    PartText out={report,_countof(report),0};
    ProMdl assembly=NULL,source=NULL,project=NULL;
    AxHardwareSite sites[12];
    int count=0;
    double mm=0;
    ProWVerstamp version=NULL,after=NULL;
    ProPath directory,current_directory;
    int window=0,current_window=0;
    ProError error=0;
    if(action==BSO_PLACE){
        ProMdlType type;
        if(ProMdlCurrentGet(&assembly)||ProMdlTypeGet(assembly,&type)||type!=PRO_MDL_ASSEMBLY){
            part_text_add(&out,L"请先打开准备安装螺母柱的装配。");
            error=PRO_TK_BAD_CONTEXT;
        }
        if(!error)error=ProMdlVerstampGet(assembly,&version);
        if(!error)error=ProWindowCurrentGet(&window);
        if(!error)error=ProDirectoryCurrentGet(directory);
        if(!error)error=qa_mm_per_unit(assembly,&mm);
        if(!error)error=fastener_bso_selection(assembly,mm,sites,&count,&out);
    }
    if(!error){
        wchar_t result[1800]=L"";
        PartText detail={result,_countof(result),0};
        error=fastener_bso_read(&source,library,&detail);
        if(result[0])part_text_add(&out,L"%ls\n",result);
    }
    if(!error&&action!=BSO_FIND){
        wchar_t result[1800]=L"";
        PartText detail={result,_countof(result),0};
        error=fastener_bso_copy(source,&project,saved,&detail);
        if(!error)part_text_add(&out,L"工作目录中的原生零件：%ls\n",saved);
        else if(result[0])part_text_add(&out,L"%ls\n",result);
    }
    if(!error&&action==BSO_PLACE){
        ProMdl active=NULL;
        if(ProMdlCurrentGet(&active)||active!=assembly||ProWindowCurrentGet(&current_window)||current_window!=window||
           ProDirectoryCurrentGet(current_directory)||_wcsicmp(current_directory,directory)||
           ProMdlVerstampGet(assembly,&after)||!ProVerstampEqual(version,after)){
            part_text_add(&out,L"装配、窗口、版本或工作目录发生变化，请重新选定实际孔位。");
            error=PRO_TK_BAD_CONTEXT;
        }else error=fastener_bso_place(assembly,project,sites,count,mm,&out);
    }
    if(!error&&action==BSO_FIND)part_text_add(&out,L"族表来源：%ls\n",library);
    if(error)part_text_add(&out,L"\n%ls未完成（Creo %d）。",action==BSO_PLACE?L"装配":action==BSO_SAVE?L"查找或另存":L"读取",error);
    if(after)ProVerstampFree(&after);
    if(version)ProVerstampFree(&version);
    smart_record_tool(report);
    smart_reply(report);
    return 1;
}
