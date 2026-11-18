#include <ProSelbuffer.h>

/* Creo may give the component path to the selected feature's owner or include
   the selected occurrence itself. Verify the owner/ID against both forms. */
static ProError assembly_selection_path(ProMdl root,ProSelection selection,wchar_t key[AX_PATH_DEPTH*12+4]){
    ProModelitem item;
    ProAsmcomppath path={0};
    ProError error=ProSelectionModelitemGet(selection,&item);
    if(error)return error;
    error=ProSelectionAsmcomppathGet(selection,&path);
    if(error||!path.owner||path.table_num<0){
        if(item.owner!=root)return PRO_TK_BAD_CONTEXT;
        memset(&path,0,sizeof(path));
        path.owner=(ProSolid)root;
    }
    if(path.owner!=(ProSolid)root||path.table_num>AX_PATH_DEPTH)return PRO_TK_BAD_CONTEXT;
    ProMdl leaf=root;
    if(path.table_num&&(error=ProAsmcomppathMdlGet(&path,&leaf)))return error;
    if(item.type==PRO_FEATURE){
        ProAsmcomp selected;
        error=ax_component_feature(item.owner,item.id,&selected);
        if(error)return error;
        if(item.owner==leaf){
            if(path.table_num>=AX_PATH_DEPTH)return PRO_TK_BAD_CONTEXT;
            path.comp_id_table[path.table_num++]=item.id;
        }else{
            if(!path.table_num||path.comp_id_table[path.table_num-1]!=item.id)return PRO_TK_BAD_CONTEXT;
            ProAsmcomppath parent=path;
            ProMdl owner=root;
            if(--parent.table_num&&(error=ProAsmcomppathMdlGet(&parent,&owner)))return error;
            if(item.owner!=owner)return PRO_TK_BAD_CONTEXT;
        }
    }else if(item.type!=PRO_ASSEMBLY||item.owner!=leaf||!path.table_num){
        return PRO_TK_INVALID_TYPE;
    }
    if(!ax_path_label(path.comp_id_table,path.table_num,key,AX_PATH_DEPTH*12+4))
        return PRO_TK_BAD_CONTEXT;
    AxComponent selected={0};
    ProMdlType type;
    error=ax_component_resolve(root,key,&selected);
    if(!error)error=ProMdlTypeGet(selected.model,&type);
    return !error&&type!=PRO_MDL_ASSEMBLY?PRO_TK_INVALID_TYPE:error;
}

static ProError assembly_selected_parent_path(ProMdl root,wchar_t key[AX_PATH_DEPTH*12+4]){
    ProSelection *choices=NULL;
    int count=0;
    ProError error=ProSelbufferSelectionsGet(&choices);
    if(!error)error=ProArraySizeGet((ProArray)choices,&count);
    if(!error&&count!=1)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=assembly_selection_path(root,choices[0],key);
    if(choices)ProSelectionarrayFree(choices);
    return error;
}

enum {ASSEMBLY_SELECT_ASSEMBLY=1,ASSEMBLY_SELECT_SOLID,ASSEMBLY_SELECT_SHEET,ASSEMBLY_SELECT_SAME};

static int assembly_selected_request(const wchar_t *text,ProMdlName name){
    wchar_t compact[240];
    size_t length=0;
    name[0]=0;
    for(;*text;text++){
        if(iswspace(*text))continue;
        if(length+1==_countof(compact))return 0;
        compact[length++]=*text;
    }
    while(length&&wcschr(L"。！!.",compact[length-1]))--length;
    compact[length]=0;
    const wchar_t *p=compact;
    assembly_take(&p,L"请");
    if(!assembly_take(&p,L"在"))return 0;
    if(!assembly_take(&p,L"我选中的")&&!assembly_take(&p,L"当前选中的")&&
       !assembly_take(&p,L"已选中的")&&!assembly_take(&p,L"选中的"))return 0;
    if(!assembly_take(&p,L"子装配"))return 0;
    if(!assembly_take(&p,L"里面")&&!assembly_take(&p,L"内部")&&
       !assembly_take(&p,L"里")&&!assembly_take(&p,L"中")&&!assembly_take(&p,L"内"))return 0;
    if(!assembly_take(&p,L"新建")&&!assembly_take(&p,L"创建")&&!assembly_take(&p,L"建"))return 0;
    if(!assembly_take(&p,L"一个"))assembly_take(&p,L"1个");
    int kind=0;
    if(assembly_take(&p,L"同图号的")||assembly_take(&p,L"同图号")||
       assembly_take(&p,L"同号的")||assembly_take(&p,L"同号")){
        assembly_take(&p,L"空");
        return !wcscmp(p,L"钣金子零件")||!wcscmp(p,L"钣金零件")||!wcscmp(p,L"钣金件")?ASSEMBLY_SELECT_SAME:0;
    }
    assembly_take(&p,L"空");
    if(assembly_take(&p,L"子装配"))kind=ASSEMBLY_SELECT_ASSEMBLY;
    else if(assembly_take(&p,L"钣金子零件")||assembly_take(&p,L"钣金零件")||assembly_take(&p,L"钣金件"))kind=ASSEMBLY_SELECT_SHEET;
    else if(assembly_take(&p,L"实体子零件")||assembly_take(&p,L"实体零件")||
            assembly_take(&p,L"子零件")||assembly_take(&p,L"零件"))kind=ASSEMBLY_SELECT_SOLID;
    else return 0;
    while(*p&&wcschr(L"，,：:",*p))++p;
    if(!assembly_take(&p,L"名为")&&!assembly_take(&p,L"名称")&&!assembly_take(&p,L"图号"))assembly_take(&p,L"叫");
    while(*p&&wcschr(L"：:",*p))++p;
    if(*p&&!dv_safe_leaf(p))return -kind;
    if(*p)wcscpy_s(name,PRO_NAME_SIZE,p);
    return kind;
}

static ProError assembly_selected_create(ProMdl root,const wchar_t *parent,int kind,const wchar_t *name,
    wchar_t *reply,size_t capacity){
    ProMdlName root_name;
    PartSnapshot snapshot={0};
    PartCommand command;
    wchar_t protocol[640];
    ProError error=ProMdlMdlnameGet(root,root_name);
    if(error)return error;
    int len=kind==ASSEMBLY_SELECT_ASSEMBLY?
        swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_NEW_ASM %ls %ls DEFAULT IN %ls",root_name,name,parent):
        swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_NEW_PART %ls %ls %ls IN %ls",root_name,
            kind==ASSEMBLY_SELECT_SAME?L"SAME":name[0]?name:L"AUTO",
            kind==ASSEMBLY_SELECT_SOLID?L"SOLID":L"SHEETMETAL",parent);
    if(len<0||part_command_parse(protocol,&command)!=1)return PRO_TK_BAD_INPUTS;
    error=part_snapshot_take_mode(root,&snapshot,0);
    if(!error)error=part_execute(&command,&snapshot,reply,capacity);
    part_snapshot_clear(&snapshot);
    return error;
}

static int assembly_selected_chat(const wchar_t *text){
    ProMdlName name;
    int kind=assembly_selected_request(text,name);
    if(!kind)return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    wchar_t reply[2400]=L"",parent[AX_PATH_DEPTH*12+4];
    ProMdl root=NULL;
    ProMdlType type;
    if(kind<0||(kind==ASSEMBLY_SELECT_ASSEMBLY&&!name[0]))
        wcscpy_s(reply,_countof(reply),L"请在新建指令里填写有效的 Creo 图号，例如「在选中的子装配里新建子装配 M26A0163001V00」。");
    else if(ProMdlCurrentGet(&root)||ProMdlTypeGet(root,&type)||type!=PRO_MDL_ASSEMBLY||
            assembly_selected_parent_path(root,parent))
        wcscpy_s(reply,_countof(reply),L"请在当前 Creo 装配的模型树里只选中一个实际子装配，再发送这条指令。");
    else if(!storage_prepare_output())
        wcscpy_s(reply,_countof(reply),storage_output_message);
    else{
        native_set_busy(3);
        ProError error=assembly_selected_create(root,parent,kind,name,reply,_countof(reply));
        native_set_busy(0);
        if(error&&!reply[0])swprintf_s(reply,_countof(reply),L"选中父装配 %ls 内的建件未完成（Creo %d）。",parent,error);
    }
    append_card(L"AI",reply);
    save_turn(L"助手",reply);
    return 1;
}