/* Retain only verified models belonging to an unfinished local workflow.
   A continuation rechecks the same handles, component ID and directory. */
enum { DEFAULT_PART_IDLE, DEFAULT_PART_WAIT_PARENT, DEFAULT_PART_WAIT_CHILD, DEFAULT_PART_WAIT_SINGLE };
static struct {
    int phase,scope,kind,component_id,window,window_retry;
    ULONGLONG turn_started;
    ProMdl parent,part;
    ProMdlName parent_name,part_name,requested_child;
    ProPath directory,staging,parent_origin,part_origin;
} default_part;

static void default_part_end_transition(void){
    if(default_part.phase==DEFAULT_PART_WAIT_CHILD&&ax_open_pending.target==default_part.part)ax_open_cancel();
    default_part.phase=DEFAULT_PART_IDLE;
}

static void default_part_cancel(void){
    default_part_end_transition();
    memset(&default_part,0,sizeof(default_part));
}

static int default_part_stop(void){
    if(default_part.phase==DEFAULT_PART_IDLE||!smart_active)return 0;
    default_part_end_transition();
    const wchar_t *message=L"已停止当前零件创建。已经完成的建模步骤留在此 Creo 会话，可发送「继续上次创建零件」。";
    smart_record_tool(message);
    smart_reply(message);
    return 1;
}

static void default_part_fail(int step,ProError error,const wchar_t *reason){
    wchar_t message[1850];
    const int retained=default_part.parent||default_part.part;
    if(default_part.scope==MODELING_PART_CURRENT&&step>1)step--;
    default_part_end_transition();
    swprintf_s(message,_countof(message),L"创建零件停在第 %d 步（Creo %d）。\n%ls%ls",
        step,error,reason,retained?L"\n已完成的步骤已保留，可在同一 Creo 会话发送「继续上次创建零件」。":L"");
    smart_record_tool(message);
    smart_reply(message);
}

static size_t default_part_version_at(const wchar_t *name,size_t length){
    size_t first=length;
    while(first&&name[first-1]>=L'0'&&name[first-1]<=L'9')first--;
    return first>1&&first<length&&(name[first-1]==L'V'||name[first-1]==L'v')?first-1:length;
}

static int default_part_format_name(const ModelingPartIntent *request,const wchar_t *configured,ProMdlName name){
    if(!request->name[0])return 1;
    size_t length=wcslen(request->name);
    if(length>=PRO_NAME_SIZE||wcscpy_s(name,PRO_NAME_SIZE,request->name))return 0;
    size_t version=default_part_version_at(name,length);
    if(version<length){
        if(request->revision[0]&&wcstol(name+version+1,NULL,10)!=wcstol(request->revision+1,NULL,10))return 0;
    }else if(wcscat_s(name,PRO_NAME_SIZE,request->revision[0]?request->revision:configured))return 0;
    return model_assembly_name_valid(name)&&dv_safe_leaf(name);
}

static ProError default_part_name_available(const wchar_t *name,int paired){
    ProError error=paired?ax_target_collision(name,PRO_MDL_ASSEMBLY):PRO_TK_NO_ERROR;
    return error?error:ax_target_collision(name,PRO_MDL_PART);
}

static ProError default_part_auto_name(ProMdlName name,int paired,int kind,const wchar_t *revision){
    SYSTEMTIME now;
    static unsigned serial;
    GetLocalTime(&now);
    const wchar_t *prefix=paired?L"asm":kind==2?L"smt":L"prt";
    for(int i=0;i<16;i++){
        if(swprintf_s(name,PRO_NAME_SIZE,L"ai_%ls_%02u%02u%02u_%02u%02u%02u_%u%ls",
            prefix,now.wYear%100,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,++serial,revision)<0||
           !model_assembly_name_valid(name))return PRO_TK_INVALID_NAME;
        ProError error=default_part_name_available(name,paired);
        if(error!=PRO_TK_E_FOUND)return error;
    }
    return PRO_TK_E_FOUND;
}

static ProError default_part_directory_current(void){
    ProPath directory;
    ProError error=ProDirectoryCurrentGet(directory);
    if(!error&&(_wcsicmp(directory,default_part.directory)||!storage_output_directory(directory)))
        error=PRO_TK_BAD_CONTEXT;
    return error;
}

/* Creo's model-copy API is Save As. Keep those initial files in this
   process's private E-drive data area and restore the project's work dir
   before display, callbacks or any user-requested save. */
static ProError default_part_stage_enter(void){
    ProError error=default_part_directory_current();
    if(error)return error;
    if(!default_part.staging[0]){
        static unsigned serial;
        ProPath candidate;
        if(!storage_output_directory(ai_dir)||
           swprintf_s(candidate,_countof(candidate),L"%ls\\part-draft-%u",ai_dir,++serial)<0||
           !CreateDirectoryW(candidate,NULL))return PRO_TK_CANT_WRITE;
        wcscpy_s(default_part.staging,_countof(default_part.staging),candidate);
    }
    DWORD attributes=GetFileAttributesW(default_part.staging);
    if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)||
       (attributes&FILE_ATTRIBUTE_REPARSE_POINT)||!storage_output_directory(default_part.staging))
        return PRO_TK_CANT_WRITE;
    return ProDirectoryChange(default_part.staging);
}

static ProError default_part_stage_leave(void){
    ProError error=ProDirectoryChange(default_part.directory);
    if(!error)error=default_part_directory_current();
    trace("Restore creation project working directory",error);
    return error;
}

static ProError default_part_model_matches(ProMdl model,const wchar_t *name,ProMdlType kind,const wchar_t *origin){
    ProMdl found=NULL;
    ProPath actual;
    if(!model)return PRO_TK_E_NOT_FOUND;
    ProError error=ProMdlnameInit((wchar_t *)name,kind,&found);
    if(!error&&found!=model)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProMdlOriginGet(found,actual);
    if(!error&&_wcsicmp(actual,origin))error=PRO_TK_BAD_CONTEXT;
    return error;
}

static ProError default_part_active(ProMdl model,int expected_window,int *actual_window){
    ProMdl active=NULL,owner=NULL;
    int window;
    ProError error=ProMdlCurrentGet(&active);
    if(!error)error=ProWindowCurrentGet(&window);
    if(!error)error=ProWindowMdlGet(window,&owner);
    if(!error&&(active!=model||owner!=model||(expected_window>=0&&expected_window!=window)))
        error=PRO_TK_BAD_CONTEXT;
    if(!error&&actual_window)*actual_window=window;
    return error;
}

static ProError default_part_queue(int phase){
    default_part.phase=phase;
    return ProMacroLoad(L"~ Command \x0060" L"BWTLDefaultPartContinue\x0060;");
}

static ProError default_part_verify_empty(void){
    ProMdlsubtype subtype;
    const ProMdlsubtype expected=default_part.kind==2?PROMDLSTYPE_PART_SHEETMETAL:PROMDLSTYPE_PART_SOLID;
    ProError error=default_part_model_matches(default_part.part,default_part.part_name,PRO_MDL_PART,default_part.part_origin);
    if(!error)error=ProMdlSubtypeGet(default_part.part,&subtype);
    if(!error&&subtype!=expected)error=PRO_TK_INVALID_TYPE;
    if(!error)error=template_empty((ProSolid)default_part.part);
    if(!error)error=part_health(default_part.part);
    return error;
}

static ProError default_part_verify_component(void){
    ProMdl part=NULL;
    ProAsmcomp feature;
    double scale;
    ProMatrix actual,identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProError error=default_part_verify_empty();
    if(!error)error=ax_component_feature(default_part.parent,default_part.component_id,&feature);
    if(!error)error=ProAsmcompMdlGet(&feature,&part);
    if(!error&&part!=default_part.part)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ax_default_verify(&feature);
    if(!error)error=qa_mm_per_unit(default_part.parent,&scale);
    if(!error)error=ax_actual_pose(default_part.parent,&feature,actual);
    if(!error&&!ax_same_pose(actual,identity,scale))error=PRO_TK_BAD_CONTEXT;
    return error;
}

static void default_part_success(void){
    wchar_t reply[800];
    const wchar_t *kind=default_part.kind==2?L"钣金":L"实体";
    if(default_part.scope==MODELING_PART_SINGLE)
        swprintf_s(reply,_countof(reply),L"已单独创建空%ls零件 %ls.PRT，并打开该零件窗口。尚未保存到项目目录。",kind,default_part.part_name);
    else if(default_part.scope==MODELING_PART_CURRENT)
        swprintf_s(reply,_countof(reply),L"已在当前装配 %ls.ASM 中以 Creo 默认定位新建空%ls零件 %ls.PRT（组件 ID:%d），并打开该零件窗口。尚未保存到项目目录。",
            default_part.parent_name,kind,default_part.part_name,default_part.component_id);
    else
        swprintf_s(reply,_countof(reply),L"已创建装配 %ls.ASM，以 Creo 默认定位装入同名空钣金件 %ls.PRT（组件 ID:%d），并打开钣金件窗口。尚未保存到项目目录。",
            default_part.parent_name,default_part.part_name,default_part.component_id);
    smart_steps=default_part.scope==MODELING_PART_NEW?3:default_part.scope==MODELING_PART_CURRENT?2:1;
    smart_progress=default_part.scope==MODELING_PART_NEW?2:1;
    smart_record_tool(reply);
    default_part_cancel();
    smart_reply(reply);
}

static void default_part_open_child(void){
    ProError error=default_part_verify_component();
    if(error){
        default_part_fail(3,error,L"Creo 未能确认原来的子零件仍在同一组件位置，或零件已被修改。请核对装配树。");
        return;
    }
    wchar_t selector[32],details[700]=L"";
    PartText out={details,_countof(details),0};
    swprintf_s(selector,_countof(selector),L"ID:%d",default_part.component_id);
    error=ax_open_component(default_part.parent,selector,&out);
    if(error){
        default_part_fail(3,error,L"子零件已经装好，Creo 未能打开它的窗口。");
        return;
    }
    smart_record_tool(details);
    error=default_part_queue(DEFAULT_PART_WAIT_CHILD);
    if(error)default_part_fail(3,error,L"子零件已经装好，Creo 未能调度窗口核对。");
}

static void default_part_build_child(void){
    wchar_t details[1000]=L"";
    PartText out={details,_countof(details),0};
    ProAsmcomp occurrence={0};
    if(!default_part.part){
        ProError error=ax_recovery_uncertain?PRO_TK_BAD_CONTEXT:ax_master(default_part.parent);
        if(error){
            default_part_fail(2,error,L"当前装配状态无法安全定位子件。请核对主表示、动态定位或之前失败的装配回退。");
            return;
        }
        if(default_part.scope==MODELING_PART_NEW){
            TemplateDatums state={0};
            error=ProSolidFeatVisit((ProSolid)default_part.parent,template_feature,NULL,&state);
            if(error||state.components){
                default_part_fail(2,error?error:PRO_TK_BAD_CONTEXT,L"这个新装配已不再是可核对的空装配，请检查其组件树。");
                return;
            }
        }
        ProMdlName name;
        error=ax_new_part_name(default_part.parent,default_part.requested_child,name);
        if(!error)error=ax_target_collision(name,PRO_MDL_PART);
        if(error){
            default_part_fail(2,error,L"项目目录或 Creo 会话中已有这个子零件名，或无法核对图号。请检查后继续。");
            return;
        }
        error=default_part_stage_enter();
        if(error){
            default_part_fail(2,error,L"E 盘的私人暂存目录不可写，未开始复制钣金模板。");
            return;
        }
        ProMdl part=NULL;
        error=ax_new_part_in(default_part.parent,name,default_part.kind==2,&part,&occurrence,&out);
        ProError restored=default_part_stage_leave();
        if(error){
            if(restored){
                default_part_fail(2,restored,L"子零件未完成。Creo 未能切回项目工作目录，请先切回原项目目录。");
                return;
            }
            default_part_fail(2,error,details[0]?details:L"Creo 未能在空件模板、定位约束或组件回读中通过核对。");
            return;
        }
        default_part.part=part;
        default_part.component_id=occurrence.id;
        error=ProMdlMdlnameGet(part,default_part.part_name);
        if(!error)error=ProMdlOriginGet(part,default_part.part_origin);
        if(error){
            default_part_fail(2,error,L"子件已经创建，但 Creo 未能读回它的身份。请检查该装配的模型树。");
            return;
        }
        if(restored){
            default_part_fail(3,restored,L"子零件已装入。Creo 未能切回项目工作目录，请先切回原目录再继续打开。");
            return;
        }
        smart_record_tool(details);
    }
    default_part_open_child();
}

static void default_part_start_or_resume(void);

static int default_part_retry_window(void){
    if(default_part.window_retry)return 0;
    default_part.window_retry=1;
    default_part_end_transition();
    default_part_start_or_resume();
    return 1;
}

static int default_part_continue(uiCmdCmdId id,uiCmdValue *value,void *data){
    if(default_part.phase==DEFAULT_PART_IDLE)return 0;
    if(!smart_active){default_part_cancel();return 0;}
    const int phase=default_part.phase;
    const int step=phase==DEFAULT_PART_WAIT_CHILD?3:1;
    ProError error=default_part_directory_current();
    if(error){
        default_part_fail(step,error,L"Creo 工作目录已切换。请回到创建时的工作目录再继续。");
        return 0;
    }
    if(phase==DEFAULT_PART_WAIT_SINGLE){
        error=default_part_active(default_part.part,default_part.window,NULL);
        if(!error)error=default_part_verify_empty();
        if(error){
            if(!default_part_retry_window())default_part_fail(1,error,L"独立零件已建好，Creo 未能确认它是当前活动窗口中的空零件。");
        }else default_part_success();
        return 0;
    }
    error=default_part_model_matches(default_part.parent,default_part.parent_name,PRO_MDL_ASSEMBLY,default_part.parent_origin);
    if(error){
        default_part_fail(step,error,L"上一次任务的装配已关闭、改名或更换。请确认 Creo 中仍打开原装配。");
        return 0;
    }
    if(phase==DEFAULT_PART_WAIT_PARENT){
        error=default_part_active(default_part.parent,default_part.window,NULL);
        if(error){
            if(!default_part_retry_window())default_part_fail(1,error,L"Creo 尚未确认指定的装配窗口已激活。");
        }else{
            default_part.phase=DEFAULT_PART_IDLE;
            default_part_build_child();
        }
    }else{
        wchar_t receipt[700]=L"";
        error=ax_open_readback(receipt,_countof(receipt));
        if(!error)error=default_part_verify_component();
        if(error){
            if(!default_part_retry_window())default_part_fail(3,error,L"子零件已装好，Creo 尚未确认它是当前活动窗口中的目标零件。");
        }else{
            smart_record_tool(receipt);
            default_part_success();
        }
    }
    return 0;
}

static void default_part_begin_turn(const wchar_t *text){
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    smart_begin(text);
    smart_local_plan=1;
    smart_readonly=0;
}

static void default_part_start_or_resume(void){
    ProError error=default_part_directory_current();
    if(error){
        default_part_fail(default_part.part&&default_part.parent?3:1,error,L"原工作目录已切换或不可读。请回到原工作目录。");
        return;
    }
    if(default_part.scope==MODELING_PART_SINGLE){
        error=default_part_verify_empty();
        if(!error)error=native_activate_model_window(default_part.part,default_part.part_name,PRO_PART,&default_part.window);
        if(!error)error=default_part_queue(DEFAULT_PART_WAIT_SINGLE);
        if(error)default_part_fail(1,error,L"独立零件已创建，Creo 未能确认或打开它的窗口。");
        return;
    }
    error=default_part_model_matches(default_part.parent,default_part.parent_name,PRO_MDL_ASSEMBLY,default_part.parent_origin);
    if(error){
        default_part_fail(1,error,L"Creo 已找不到原来的装配。请核实之前创建的模型。");
        return;
    }
    int child_window,parent_window;
    if(default_part.part&&!default_part_active(default_part.part,-1,&child_window)&&
       !ProMdlWindowGet(default_part.parent,&parent_window)&&parent_window!=child_window&&
       !default_part_verify_component()){
        wchar_t occurrence[32];
        swprintf_s(occurrence,_countof(occurrence),L"ID:%d",default_part.component_id);
        ax_return_note_open(default_part.parent,parent_window,default_part.part,child_window,occurrence,default_part.directory);
        default_part_success();
        return;
    }
    if(!default_part_active(default_part.parent,-1,NULL)){
        default_part_build_child();
        return;
    }
    error=native_activate_model_window(default_part.parent,default_part.parent_name,PRO_ASSEMBLY,&default_part.window);
    if(!error)error=default_part_queue(DEFAULT_PART_WAIT_PARENT);
    if(error)default_part_fail(1,error,L"Creo 未能打开或激活之前的装配窗口。");
}

static int default_part_resume_chat(const wchar_t *text){
    if(!default_part.parent&&!default_part.part)return 0;
    const wchar_t *request=model_request_action_start(text);
    if(assembly_take(&request,L"继续上次创建零件")||assembly_take(&request,L"继续创建零件")){
        if(*modeling_part_skip(request))return 0;
    }else if(!assembly_take(&request,L"继续上次任务")||*modeling_part_skip(request)||
             smart_paused||default_part.turn_started!=smart_started)return 0;
    if(native_busy||smart_active||ai_process){append(L"当前任务仍在运行。");return 1;}
    default_part_begin_turn(text);
    default_part.turn_started=smart_started;
    default_part.window_retry=0;
    default_part_start_or_resume();
    return 1;
}

/* Existing models are never used as a substitute for a fresh drawing number. */
static void default_part_create_model(ProMdlName name,int paired){
    const char *stage="Create the named empty template";
    int background=model_background_mode;
    ProMdlName actual=L"";
    ProMdl model=NULL;
    ProMdlType type=paired?PRO_MDL_ASSEMBLY:PRO_MDL_PART;
    ProError error=default_part_stage_enter();
    if(error){
        default_part_fail(1,error,L"E 盘的私人暂存目录不可写，未开始复制模型模板。");
        return;
    }
    model_background_mode=1;
    error=model_create_empty_named(paired?3:default_part.kind,name,actual,&stage);
    model_background_mode=background;
    trace(stage,error);
    ProError restored=default_part_stage_leave();
    if(!error)error=ProMdlnameInit(actual,type,&model);
    ProPath origin;
    if(!error)error=ProMdlOriginGet(model,origin);
    if(error&&restored){
        default_part_fail(1,restored,L"模型未完成。Creo 未能切回项目工作目录，请先切回原项目目录。");
        return;
    }
    if(error){
        default_part_fail(1,error,paired?L"Creo 无法完成新装配，请检查默认装配模板和工作目录。":
            L"Creo 无法完成独立空零件，请检查对应的 Creo 模板和工作目录。");
        return;
    }
    if(paired){
        default_part.parent=model;
        wcscpy_s(default_part.parent_name,PRO_NAME_SIZE,actual);
        wcscpy_s(default_part.parent_origin,PRO_PATH_SIZE,origin);
        wcscpy_s(default_part.requested_child,PRO_NAME_SIZE,L"SAME");
        if(!restored)model_assembly_creation_record(model,actual);
        smart_record_tool(L"已从默认装配模板新建空装配。");
    }else{
        default_part.part=model;
        wcscpy_s(default_part.part_name,PRO_NAME_SIZE,actual);
        wcscpy_s(default_part.part_origin,PRO_PATH_SIZE,origin);
    }
    if(restored){
        default_part_fail(1,restored,L"模型已创建。Creo 未能切回项目工作目录，请先切回原目录再继续。");
        return;
    }
    default_part_start_or_resume();
}

static int default_part_chat(const wchar_t *text){
    ModelingPartIntent request={0};
    if(!modeling_part_parse(text,&request))return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    default_part_begin_turn(text);
    if(request.invalid){
        smart_reply(L"请核对图号和版本号。图号含版本最多 31 字符，可用英文字母、数字、短横线和下划线。例如「创建新零件，图号 M26A0181001，版本号 V03」。");
        return 1;
    }
    ModelingRuleState rules;
    if(modeling_rule_read(&rules)!=MODELING_RULE_OK){
        smart_reply(L"BWTL 无法读取建模规则，请先在「标准文件 → 建模规则」核对规则文件。");
        return 1;
    }
    int scope=request.scope;
    if(scope==MODELING_PART_NEW&&!request.require_company&&rules.current.mode==MODELING_RULE_INDEPENDENT)
        scope=MODELING_PART_SINGLE;
    if(scope==MODELING_PART_NEW&&request.kind==1){
        smart_reply(L"当前规则会先建装配再建同号钣金件。若你这次要的是独立实体零件，请说「单独创建实体零件」。");
        return 1;
    }
    if(scope!=MODELING_PART_SINGLE&&ax_recovery_uncertain){
        smart_reply(L"上一次装配操作的回退尚未核实。请在新的 Creo 会话再创建零件。");
        return 1;
    }

    ProMdlName name=L"";
    if(!default_part_format_name(&request,rules.current.revision,name)){
        smart_reply(L"图号和版本号无法一起使用，请核对两处版本是否一致，以及总长度是否超过 31 字符。");
        return 1;
    }
    ProPath directory;
    if(ProDirectoryCurrentGet(directory)||!storage_prepare_output()){
        smart_reply(storage_output_message);
        return 1;
    }
    ProMdl current=NULL;
    ProPath origin;
    ProMdlName current_name;
    if(scope==MODELING_PART_CURRENT){
        ProMdlType type;
        if(ProMdlCurrentGet(&current)||ProMdlTypeGet(current,&type)||type!=PRO_MDL_ASSEMBLY||
           default_part_active(current,-1,NULL)||ProMdlMdlnameGet(current,current_name)||ProMdlOriginGet(current,origin)){
            smart_reply(L"先激活要放入新子零件的 Creo 装配窗口，然后再执行。");
            return 1;
        }
    }
    const int paired=scope==MODELING_PART_NEW;
    const int kind=request.kind?request.kind:scope==MODELING_PART_SINGLE?1:2;
    if(scope!=MODELING_PART_CURRENT||name[0]||request.revision[0]){
        const wchar_t *revision=request.revision[0]?request.revision:rules.current.revision;
        ProError error=name[0]?default_part_name_available(name,paired):default_part_auto_name(name,paired,kind,revision);
        if(error){
            wchar_t message[380];
            if(error==PRO_TK_E_FOUND)
                swprintf_s(message,_countof(message),L"当前会话或工作目录中已有 %ls，换一个图号或版本即可创建。",name);
            else swprintf_s(message,_countof(message),L"无法核对新图号的可用性（Creo %d），请检查工作目录及图号。",error);
            smart_reply(message);
            return 1;
        }
    }
    default_part_cancel();
    default_part.turn_started=smart_started;
    default_part.scope=scope;
    default_part.kind=kind;
    wcscpy_s(default_part.directory,PRO_PATH_SIZE,directory);
    if(scope==MODELING_PART_CURRENT){
        default_part.parent=current;
        wcscpy_s(default_part.parent_name,PRO_NAME_SIZE,current_name);
        wcscpy_s(default_part.parent_origin,PRO_PATH_SIZE,origin);
        wcscpy_s(default_part.requested_child,PRO_NAME_SIZE,request.same_name?L"SAME":name[0]?name:L"AUTO");
        default_part_build_child();
    }else default_part_create_model(name,paired);
    return 1;
}
