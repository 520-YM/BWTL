#include "camera_bracket.inc"
#include "camera_mount.inc"

typedef struct {
    wchar_t name[PRO_NAME_SIZE];
    wchar_t leaf[MAX_PATH];
    ProMdlType type;
} CameraReference;

static CameraReference camera_references[5];

static ProError camera_reference_settings(const wchar_t *folder){
    static const wchar_t *const keys[]={L"Bracket",L"Electronics",L"Standoff",L"Nut",L"Assembly"};
    ProPath config;
    if(swprintf_s(config,PRO_PATH_SIZE,L"%ls\\camera-models.ini",folder)<0)return PRO_TK_BAD_INPUTS;
    wchar_t verified[MAX_PATH];
    HANDLE file=company_open_path(config,0,verified);
    if(file==INVALID_HANDLE_VALUE)return PRO_TK_E_NOT_FOUND;
    ProError error=0;
    for(size_t i=0;i<_countof(keys);i++){
        CameraReference *item=&camera_references[i];
        DWORD length=GetPrivateProfileStringW(L"Models",keys[i],L"",item->leaf,MAX_PATH,verified);
        const wchar_t *suffix=native_file_suffix(item->leaf);
        AttachmentKind kind=native_file_kind(suffix);
        item->type=i==4?PRO_MDL_ASSEMBLY:PRO_MDL_PART;
        if(!length||length>=MAX_PATH-1||!suffix||kind!=(i==4?ATTACH_ASSEMBLY:ATTACH_PART)||
           !company_fastener_model_name(item->leaf,&(size_t){0},&(unsigned long){0})||
           wcsncpy_s(item->name,_countof(item->name),item->leaf,suffix-item->leaf)||
           !dv_safe_leaf(item->name)){
            error=PRO_TK_BAD_INPUTS;
            break;
        }
    }
    if(!error){
        static const wchar_t *const part_keys[]={L"Standoff1",L"Standoff2",L"Nut1",L"Nut2"};
        cad_electronics.name=camera_references[1].name;
        cad_electronics.id=(int)GetPrivateProfileIntW(L"Instances",L"Electronics",54,verified);
        const int defaults[]={40,41,43,46};
        for(size_t i=0;i<_countof(cad_fasteners);i++){
            cad_fasteners[i].name=camera_references[i<2?2:3].name;
            cad_fasteners[i].id=(int)GetPrivateProfileIntW(L"Instances",part_keys[i],defaults[i],verified);
            if(cad_fasteners[i].id<1||cad_fasteners[i].id==cad_electronics.id)error=PRO_TK_BAD_INPUTS;
            for(size_t j=0;j<i;j++)if(cad_fasteners[i].id==cad_fasteners[j].id)error=PRO_TK_BAD_INPUTS;
        }
        if(cad_electronics.id<1)error=PRO_TK_BAD_INPUTS;
    }
    CloseHandle(file);
    return error;
}

static const wchar_t camera_risk_message[]=
    L"【回溯风险重点提醒】环境摄像头支架包含边法兰。处理不完整附加壁或失败回滚时曾触发 Creo 回溯。当前操作在新建的独立钣金件中按已验收的原步骤重建。";

static ProError camera_reference_folder(ProPath folder){
    ProPath standard;
    ProError error=imitation_standard_directory(standard);
    if(!error&&swprintf_s(folder,PRO_PATH_SIZE,L"%ls\\环境摄像头",standard)<0)error=PRO_TK_BAD_INPUTS;
    if(!error&&(!imitation_library_is_directory(folder)||!storage_directory_on_drive(folder,storage_data_drive())))
        error=PRO_TK_E_NOT_FOUND;
    return error?error:camera_reference_settings(folder);
}

static ProError camera_reference_origin(ProMdl model,const wchar_t *expected){
    ProPath actual;
    ProError error=ProMdlOriginGet(model,actual);
    if(error)return error;
    for(wchar_t *p=actual;*p;p++)if(*p==L'/')*p=L'\\';
    if(!_wcsicmp(actual,expected))return PRO_TK_NO_ERROR;
    if(towupper(actual[0])!=storage_data_drive()||actual[1]!=L':')return PRO_TK_E_IN_USE;
    wchar_t current_hash[65],expected_hash[65];
    ULONGLONG current_size,expected_size;
    const int same=dv_sha256(actual,current_hash,&current_size)&&
        dv_sha256(expected,expected_hash,&expected_size)&&current_size==expected_size&&
        !wcscmp(current_hash,expected_hash);
    return same?PRO_TK_NO_ERROR:PRO_TK_E_IN_USE;
}

static ProError camera_read_source(const wchar_t *reference,ProMdl *assembly,ProMdl *sheet,PartText *out){
    ProPath work,paths[_countof(camera_references)];
    ProMdl models[_countof(camera_references)]={0};
    ProError error=ProDirectoryCurrentGet(work);
    if(error)return error;
    if(!storage_directory_on_drive(reference,storage_data_drive()))return PRO_TK_BAD_CONTEXT;

    /* Finish path and in-session conflict checks before loading any source. */
    for(size_t i=0;i<_countof(camera_references);i++){
        const CameraReference *item=&camera_references[i];
        if(swprintf_s(paths[i],PRO_PATH_SIZE,L"%ls\\%ls",reference,item->leaf)<0)
            return PRO_TK_BAD_INPUTS;
        BY_HANDLE_FILE_INFORMATION file;
        if(!imitation_file_info(paths[i],&file)){
            part_text_add(out,L"公司标准件资料缺失或无法读取：%ls。\n",item->leaf);
            return PRO_TK_E_NOT_FOUND;
        }
        ProMdlName name;
        wcscpy_s(name,_countof(name),item->name);
        error=ProMdlnameInit(name,item->type,&models[i]);
        if(error==PRO_TK_E_NOT_FOUND)continue;
        ProBoolean changed=PRO_B_FALSE;
        if(!error)error=camera_reference_origin(models[i],paths[i]);
        if(!error)error=ProMdlModificationVerify(models[i],&changed);
        if(!error&&changed)error=PRO_TK_E_IN_USE;
        if(error){
            part_text_add(out,L"无法复用 Creo 会话中已打开的 %ls，原因可能是文件来源不同或已有未保存修改。\n",item->leaf);
            return error;
        }
    }

    error=ProDirectoryChange((wchar_t *)reference);
    if(error)return error;
    for(size_t i=0;i<_countof(models)&&!error;i++){
        if(!models[i]){
            ProMdlfileType filetype=camera_references[i].type==PRO_MDL_ASSEMBLY?
                PRO_MDLFILE_ASSEMBLY:PRO_MDLFILE_PART;
            error=ProMdlFiletypeLoad(paths[i],filetype,PRO_B_FALSE,&models[i]);
            if(!error)error=camera_reference_origin(models[i],paths[i]);
            if(error)part_text_add(out,L"无法核对 Creo 读入的公司标准文件 %ls（Creo %d）。\n",camera_references[i].leaf,error);
        }
    }
    const ProError restored=ProDirectoryChange(work);
    if(restored){
        part_text_add(out,L"无法恢复原 Creo 工作目录 %ls（Creo %d）。\n",work,restored);
        return restored;
    }
    if(error)return error;

    *assembly=models[_countof(models)-1];
    ProAsmcomp source_bracket;
    ProMdlsubtype subtype;
    error=ax_component_feature(*assembly,39,&source_bracket);
    if(!error)error=ProAsmcompMdlGet(&source_bracket,sheet);
    if(!error&&*sheet!=models[0])error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProMdlSubtypeGet(*sheet,&subtype);
    if(!error&&subtype!=PROMDLSTYPE_PART_SHEETMETAL)error=PRO_TK_INVALID_TYPE;
    if(!error)part_text_add(out,L"已从公司标准读取环境摄像头 %ls、%ls 及两种紧固件。\n",
        camera_references[_countof(camera_references)-1].leaf,cad_electronics.name);
    return error;
}

static ProError camera_display_assembly(ProMdl assembly){
    ProMdlName name;
    int window;
    ProError error=ProMdlMdlnameGet(assembly,name);
    if(!error)error=ProObjectwindowMdlnameCreate(name,PRO_ASSEMBLY,&window);
    if(!error)error=ProWindowCurrentSet(window);
    if(!error)error=ProMdlDisplay(assembly);
    if(!error)error=ProWindowActivate(window);
    return error;
}

static ProError camera_prepare_dependency_files(const wchar_t *reference,const wchar_t *work,PartText *out){
    ProPath sources[3],targets[3];
    int needed[3]={0};
    wchar_t digest[65];
    for(int i=0;i<3;i++){
        const CameraReference *item=&camera_references[i+1];
        ProPath leaf=L"";
        if(swprintf_s(sources[i],PRO_PATH_SIZE,L"%ls\\%ls",reference,item->leaf)<0)
            return PRO_TK_BAD_INPUTS;
        ProError error=dv_native_revision(work,item->name,leaf);
        if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
        needed[i]=error==PRO_TK_E_NOT_FOUND;
        if(needed[i]){
            if(swprintf_s(targets[i],PRO_PATH_SIZE,L"%ls\\%ls",work,item->leaf)<0)
                return PRO_TK_BAD_INPUTS;
            continue;
        }
        if(swprintf_s(targets[i],PRO_PATH_SIZE,L"%ls\\%ls",work,leaf)<0)
            return PRO_TK_BAD_INPUTS;
        ULONGLONG a_size,b_size;
        wchar_t present[65];
        if(!dv_sha256(sources[i],digest,&a_size)||!dv_sha256(targets[i],present,&b_size)||
           a_size!=b_size||wcscmp(digest,present)){
            part_text_add(out,L"工作目录已有同名但内容不同的标准件 %ls。请核对版本后再保存装配。\n",targets[i]);
            return PRO_TK_E_IN_USE;
        }
    }
    for(int i=0;i<3;i++){
        if(!needed[i])continue;
        ProError error=ax_pack_copy(sources[i],camera_references[i+1].leaf,work,digest);
        if(error){
            part_text_add(out,L"无法复制并校验标准件 %ls（Creo %d）。\n",targets[i],error);
            return error;
        }
    }
    part_text_add(out,L"已核对工作目录内的标准电子件和两类紧固件文件，装配重开可读取。\n");
    return PRO_TK_NO_ERROR;
}
static ProError camera_standard_build(const wchar_t *reference,const wchar_t *requested,int save,int display,
                                      ProMdl *assembly,PartText *out){
    ProPath workspace;
    ProMdl source_asm=NULL,source_sheet=NULL,sheet=NULL;
    ProAsmcomp bracket={0},electronics={0};
    ProMdlName name=L"";
    ImitationReplay replay={0};
    const char *native_stage=NULL;
    const wchar_t *stage=L"核对工作目录";
    int components=0;
    *assembly=NULL;
    ProError error=ProDirectoryCurrentGet(workspace);
    if(!error&&(!storage_directory_on_drive(workspace,storage_data_drive())||!storage_prepare_output())){
        part_text_add(out,L"%ls\n",storage_output_message);
        error=PRO_TK_BAD_CONTEXT;
    }
    if(!error){
        stage=L"读取公司标准源模型";
        error=camera_read_source(reference,&source_asm,&source_sheet,out);
    }
    if(!error){
        stage=L"按指定图号新建空总装";
        const int previous_background=model_background_mode;
        model_background_mode=1;
        error=model_create_empty_named(3,requested,name,&native_stage);
        model_background_mode=previous_background;
        if(!error)error=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,assembly);
        if(error&&native_stage)part_text_add(out,L"Creo 创建回执阶段 %hs。\n",native_stage);
        if(!error)part_text_add(out,L"第 1 步 直接按图号建立空总装 %ls.ASM。\n",name);
    }
    if(!error){
        stage=L"在总装内建立同图号空钣金件";
        error=ax_new_part_in(*assembly,name,1,&sheet,&bracket,out);
        if(!error)part_text_add(out,L"第 2 步 同图号原生空钣金件 %ls.PRT 已默认定位，组件 ID %d。\n",
            name,bracket.id);
    }
    if(!error){
        stage=L"预定位实际标准电子件";
        error=cad_place_electronics(source_asm,*assembly,&electronics,out);
    }
    if(!error){
        stage=L"从空白钣金重画板壁和安装孔";
        part_text_add(out,L"第 4 步 按源特征树重画钣金及安装孔，再补正式定位约束。\n");
        error=cad_rebuild_bracket(source_sheet,sheet,&replay,out);
    }
    if(!error){
        stage=L"按实际孔位约束电子件并安装螺母柱和压铆螺母";
        error=cad_finish_mounts(source_asm,*assembly,&bracket,&electronics,&replay,out);
    }
    if(!error){
        stage=L"核对总装的组件树和再生成";
        AxScan *scan=NULL;
        error=part_regenerate(*assembly);
        if(!error)error=part_health(*assembly);
        if(!error)error=ax_scan(*assembly,1,&scan);
        if(!error){
            components=scan->count;
            if(components!=6)error=PRO_TK_INCOMPLETE;
            for(int i=0;!error&&i<components;i++)
                if(scan->occurrences[i].state!=PRO_FEAT_ACTIVE||scan->occurrences[i].depth!=1)
                    error=PRO_TK_INCOMPLETE;
        }
        free(scan);
        if(!error)part_text_add(out,L"回读通过：%d 个活动组件，%d 道钣金原生特征。\n",
            components,replay.steps);
    }
    if(!error&&save){
        stage=L"核对保存到工作目录的标准件依赖";
        error=camera_prepare_dependency_files(reference,workspace,out);
        if(!error){stage=L"保存新装配和钣金件";error=ProMdlSave(sheet);}
        if(!error)error=ProMdlSave(*assembly);
        if(!error)part_text_add(out,L"已保存两份同号模型至 %ls。\n",workspace);
    }
    if(error)part_text_add(out,L"未完成阶段「%ls」，Creo %d。请按上述原生回执核对。\n",stage,error);
    if(*assembly&&display&&!ax_recovery_uncertain&&!sh_recovery_uncertain&&!cm_recovery_uncertain){
        ProError shown=camera_display_assembly(*assembly);
        if(shown)part_text_add(out,L"新装配已在 Creo 会话建立，窗口激活返回 Creo %d。\n",shown);
    }
    if(*assembly&&(!save||error))part_text_add(out,L"新装配及已完成步骤保留在当前 Creo 会话。\n");
    return error;
}

static int camera_drawing_number(const wchar_t *text,ProMdlName name){
    name[0]=0;
    for(const wchar_t *p=text;*p;p++){
        if(*p!=L'M'&&*p!=L'm')continue;
        const wchar_t *end=p;
        while((*end>=L'A'&&*end<=L'Z')||(*end>=L'a'&&*end<=L'z')||(*end>=L'0'&&*end<=L'9'))end++;
        if(end-p<4||!iswdigit(p[1])||!iswdigit(p[2])||towupper(p[3])!=L'A')continue;
        if(end-p!=14||towupper(p[11])!=L'V')return -1;
        for(int i=4;i<14;i++)if(i!=11&&!iswdigit(p[i]))return -1;
        if(name[0])return -1;
        for(int i=0;i<14;i++)name[i]=towupper(p[i]);
        name[14]=0;
        p=end-1;
    }
    if(name[0])return 1;
    const wchar_t *short_number=wcsstr(text,L"图号");
    if(!short_number)return 0;
    short_number+=2;
    while(*short_number&&(iswspace(*short_number)||wcschr(L"，,：:为",*short_number)))short_number++;
    if(short_number[0]>=L'0'&&short_number[0]<=L'9'&&short_number[1]>=L'0'&&short_number[1]<=L'9'&&
       (!short_number[2]||iswspace(short_number[2])||wcschr(L"，。！,;；",short_number[2]))){
        const wchar_t code[3]={short_number[0],short_number[1],0};
        if(assembly_naming_target(code,name))return 1;
    }
    return -1;
}

static int camera_standard_requested(const wchar_t *text){
    const wchar_t *object=wcsstr(text,L"环境摄像头");
    if(!object||smart_explicit_readonly(text)||smart_readonly_override(text)||
       wcsstr(text,L"通用摄像头")||wcsstr(text,L"不要画")||wcsstr(text,L"别画")||
       wcsstr(text,L"不要创建")||wcsstr(text,L"不要新建")||wcsstr(text,L"别创建")||wcsstr(text,L"别新建")||
       wcsstr(text,L"要不要")||wcsstr(text,L"怎么画")||wcsstr(text,L"如何画")||
       wcsstr(text,L"怎么建")||wcsstr(text,L"如何建"))return 0;
    if(!wcsncmp(object+5,L"支架",2)&&!wcsstr(text,L"总装")&&!wcsstr(text,L"装配体"))return 0;
    static const wchar_t *actions[]={L"创建",L"新建",L"重建",L"照着做",L"做一个",L"做一套",L"画一个",L"画一套",L"画个",L"帮我画",L"重新画",L"试做"};
    for(size_t i=0;i<_countof(actions);i++)if(wcsstr(text,actions[i]))return 1;
    return 0;
}

static int camera_save_requested(const wchar_t *text){
    return wcsstr(text,L"保存")&&!wcsstr(text,L"不保存")&&!wcsstr(text,L"不要保存")&&
        !wcsstr(text,L"别保存")&&!wcsstr(text,L"不自动保存")&&
        !wcsstr(text,L"不要自动保存")&&!wcsstr(text,L"别自动保存");
}
static int camera_standard_chat(const wchar_t *text){
    if(!camera_standard_requested(text))return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    wchar_t detail[PART_TEXT_MAX]=L"";
    PartText out={detail,_countof(detail),0};
    ProMdlName name;
    ProPath reference;
    ProMdl assembly;
    const int exact=camera_drawing_number(text,name);
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    if(exact<0){
        part_text_add(&out,L"没有读到唯一的完整公司图号。请给出一个格式为 M26A0100000V00 的图号。\n");
    }else if(!exact&&(wcsstr(text,L"正式")||wcsstr(text,L"出图"))){
        part_text_add(&out,L"正式图号需要工程已分配的年度流水。请提供完整图号，随后按标准件流程创建。\n");
    }else if(camera_reference_folder(reference)){
        part_text_add(&out,L"当前 BWTL 的「公司标准件」目录中缺少「环境摄像头」资料，请核对标准源模型已登记。\n");
    }else{
        if(!exact){
            swprintf_s(name,_countof(name),L"CAMERA_%08lX_%08lX",GetCurrentProcessId(),GetTickCount());
            part_text_add(&out,L"本次试建暂用 %ls。正式建档可在命令中提供公司的完整图号。\n",name);
        }
        append_card(L"提示",camera_risk_message);
        save_turn(L"回溯风险",camera_risk_message);
        native_set_busy(2);
        camera_standard_build(reference,name,camera_save_requested(text),1,&assembly,&out);
        native_set_busy(0);
    }
    append_card(L"AI",detail);
    save_turn(L"助手",detail);
    return 1;
}