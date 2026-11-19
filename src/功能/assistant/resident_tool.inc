/* All operations here run on Creo's registered command thread. */
static int resident_named_file(const wchar_t *path){
    const wchar_t *position=smart_latest_request;
    size_t length=wcslen(path);
    while((position=wcsstr(position,path))!=NULL){
        wchar_t end=position[length];
        if(!end||iswspace(end)||wcschr(L"。，；、！？\"”’」』",end))return 1;
        position+=length;
    }
    return 0;
}

static void resident_file(const wchar_t *path){
    static const wchar_t *const supported[]={L".txt",L".md",L".csv",L".tsv",L".json",L".log"};
    static wchar_t text[32000];
    wchar_t base[MAX_PATH],resolved[MAX_PATH];
    const wchar_t *extension=wcsrchr(path,L'.');
    wchar_t drive=storage_output_drive();
    int kind=0;
    if(extension)for(int i=0;i<_countof(supported);i++)if(!_wcsicmp(extension,supported[i])){kind=1;break;}
    if(wcslen(path)<4||wcslen(path)>=MAX_PATH||!kind||path[1]!=L':'||
       (path[2]!=L'\\'&&path[2]!=L'/')||!resident_named_file(path)||
       !GetFullPathNameW(path,MAX_PATH,base,NULL)){
        resident_tool_result(-1,L"请在本条消息写出要读取的完整 E 盘文本文件路径。支持 TXT、MD、CSV、TSV、JSON 和 LOG。");
        return;
    }
    HANDLE file=CreateFileW(base,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(file==INVALID_HANDLE_VALUE){resident_tool_result(-1,L"打不开用户指定的文件。");return;}
    DWORD size,length=GetFinalPathNameByHandleW(file,resolved,_countof(resolved),VOLUME_NAME_DOS),read=0;
    LARGE_INTEGER bytes;
    static char buffer[128000];
    int ok=length>=7&&length<_countof(resolved)&&!wcsncmp(resolved,L"\\\\?\\",4)&&resolved[5]==L':'&&
        towupper(resolved[4])==(drive?drive:towupper(runtime_data[0]))&&GetFileType(file)==FILE_TYPE_DISK&&
        GetFileSizeEx(file,&bytes)&&bytes.QuadPart>=0&&bytes.QuadPart<(LONGLONG)sizeof(buffer);
    size=ok?(DWORD)bytes.QuadPart:0;
    if(ok)ok=ReadFile(file,buffer,size,&read,NULL)&&read==size;
    CloseHandle(file);
    if(!ok){resident_tool_result(-1,L"只读取数据盘上小于 128 KB 的普通文本文件。");return;}
    DWORD bom=read>=3&&!memcmp(buffer,"\xef\xbb\xbf",3)?3:0;
    int chars=0;
    if(read>=2&&(unsigned char)buffer[0]==0xff&&(unsigned char)buffer[1]==0xfe&&!(read%2)){
        chars=(int)(read/2-1);
        if(chars<_countof(text))memcpy(text,buffer+2,(size_t)chars*sizeof(wchar_t));
        else chars=0;
    }else if(read>bom)chars=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,buffer+bom,(int)(read-bom),text,_countof(text)-1);
    if((read>bom&&!chars)||wmemchr(text,0,chars)){
        resident_tool_result(-1,L"文件不是可读取的 UTF-8 或 UTF-16 文本，或超过单次长度。");
        return;
    }
    text[chars]=0;
    resident_tool_result(0,chars?text:L"该文件为空。");
}

static const wchar_t *resident_refresh_model(void){
    if(smart_steps&&smart_target_model){
        const wchar_t *problem=smart_continue_binding();
        if(problem)return problem;
        ProMdl target=smart_target_model;
        if(part_snapshot_take(target,&request_snapshot))return L"无法读取本轮已绑定的新模型版本。";
    }
    return NULL;
}

static void resident_snapshot(void){
    static wchar_t text[PART_TEXT_MAX];
    PartSnapshot captured={0};
    ProMdl model=smart_target_model;
    ProError error=model?0:part_request_target_get(smart_goal,&model);
    if(!error)error=part_snapshot_take(model,&captured);
    if(error)resident_tool_result(error,L"当前没有可读取的 Creo 模型。");
    else{
        part_snapshot_format_selected(&captured,text,_countof(text),15);
        resident_tool_result(0,text);
    }
    part_snapshot_clear(&captured);
}

static void return_assembly_start_task(void);

static int resident_tool_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    if(!resident_tool_queued)return 0;
    resident_tool_queued=0;
    if(!ai_process||!smart_active)return 0;
    resident_tool_inflight=1;
    if(request_attachment_turn&&smart_intent==SMART_INTENT_CHAT){
        resident_tool_result(-1,L"本轮为聊天分析。请使用宿主随本轮提供的资料。");
        return 0;
    }
    if(!wcsncmp(resident_operation,L"LIBRARY ",8)){
        company_library_dispatch(resident_operation+8);
        return 0;
    }
    if(!wcsncmp(resident_operation,L"PROJECT ",8)){
        company_project_dispatch(resident_operation+8);
        return 0;
    }
    if(!wcsncmp(resident_operation,L"FILE ",5)){
        resident_file(resident_operation+5);
        return 0;
    }
    if(smart_intent==SMART_INTENT_PENDING||smart_intent==SMART_INTENT_CHAT||smart_intent==SMART_INTENT_CLARIFY){
        resident_tool_result(-1,L"本轮先完成意图判断。");
        return 0;
    }
    const wchar_t *problem=resident_refresh_model();
    if(problem){resident_tool_result(-1,problem);return 0;}
    if(!wcscmp(resident_operation,L"CREO_RETURN_PARENT")){
        resident_used_tool=1;
        return_assembly_start_task();
        return 0;
    }
    if(!wcscmp(resident_operation,L"CREO_SNAPSHOT")){
        resident_snapshot();
        return 0;
    }
    PartCommand action={0};
    ModelPlan plan={0};
    int parsed=part_command_parse(resident_operation,&action);
    if(parsed==1){
        if(!smart_protocol_allowed(&action)){
            resident_tool_result(-1,smart_operation_block());
            return 0;
        }
        if((smart_intent==SMART_INTENT_NEW&&smart_progress&&!smart_target_model&&!smart_read_command(&action))||
           smart_protocol_seen(&action)){
            resident_tool_result(-1,L"请根据本轮 Creo 回执选择新目标或继续剩余操作，不能写入源件或重复执行。");
            return 0;
        }
        resident_used_tool=1;
        pending_part=action;
        execute_part_command();
    }else if(!parsed&&model_plan_parse(resident_operation,&plan)){
        if(!smart_new_plan_allowed()||quick_intent(smart_goal,quick_topic(smart_goal))){
            resident_tool_result(-1,L"当前任务目标已绑定或需要使用对应的原生整件工具。");
            return 0;
        }
        resident_used_tool=1;
        pending_plan=plan;
        execute_model_plan();
    }else resident_tool_result(-1,L"请按本轮列出的格式提交一条受支持的 Creo 协议。");
    return 0;
}
