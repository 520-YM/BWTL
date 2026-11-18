#include <ProSelbuffer.h>

typedef struct {
    ProMdl target,root;
    ProMdlType type;
    int window,root_window;
    ProPath directory;
    ProMdlName name;
    wchar_t occurrence[AX_PATH_DEPTH*12+4];
} AxOpenPending;

static AxOpenPending ax_open_pending;

static void ax_open_cancel(void){
    memset(&ax_open_pending,0,sizeof(ax_open_pending));
}

/* A model-tree selection can describe the owning assembly path or include
   the selected component itself. Only open a real component of this root. */
static ProError ax_open_selected(ProMdl root,wchar_t occurrence[AX_PATH_DEPTH*12+4]){
    ProSelection *choices=NULL;
    ProModelitem item;
    ProAsmcomppath path={0};
    int count=0;
    ProError error=ProSelbufferSelectionsGet(&choices);
    if(!error)error=ProArraySizeGet((ProArray)choices,&count);
    if(!error&&count!=1)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProSelectionModelitemGet(choices[0],&item);
    if(error)goto done;

    error=ProSelectionAsmcomppathGet(choices[0],&path);
    if(error||!path.owner){
        if(item.owner!=root){error=PRO_TK_BAD_CONTEXT;goto done;}
        memset(&path,0,sizeof(path));
        path.owner=(ProSolid)root;
    }
    if(path.owner!=(ProSolid)root||path.table_num<0||path.table_num>AX_PATH_DEPTH){
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    ProMdl leaf=root;
    error=path.table_num?ProAsmcomppathMdlGet(&path,&leaf):PRO_TK_NO_ERROR;
    if(error)goto done;
    if(item.type==PRO_FEATURE){
        ProAsmcomp selected;
        error=ax_component_feature(item.owner,item.id,&selected);
        if(error)goto done;
        if(item.owner==leaf){
            if(path.table_num==AX_PATH_DEPTH){error=PRO_TK_BAD_CONTEXT;goto done;}
            path.comp_id_table[path.table_num++]=item.id;
        }else{
            if(!path.table_num||path.comp_id_table[path.table_num-1]!=item.id){
                error=PRO_TK_BAD_CONTEXT;goto done;
            }
            ProAsmcomppath parent=path;
            ProMdl owner=root;
            if(--parent.table_num)error=ProAsmcomppathMdlGet(&parent,&owner);
            if(error)goto done;
            if(item.owner!=owner){error=PRO_TK_BAD_CONTEXT;goto done;}
        }
    }else if((item.type!=PRO_PART&&item.type!=PRO_ASSEMBLY)||!path.table_num||item.owner!=leaf){
        error=PRO_TK_INVALID_TYPE;goto done;
    }
    if(!ax_path_label(path.comp_id_table,path.table_num,occurrence,AX_PATH_DEPTH*12+4))error=PRO_TK_BAD_CONTEXT;
done:
    if(choices)ProSelectionarrayFree(choices);
    return error;
}

static ProError ax_open_find_name(ProMdl assembly,const wchar_t *name,ProMdlType type,
                                  wchar_t occurrence[AX_PATH_DEPTH*12+4],PartText *out){
    AxScan *scan=NULL;
    ProError error=ax_scan(assembly,0,&scan);
    if(error)return error;
    unsigned matches=0,actual=0;
    for(int i=0;i<scan->count;i++){
        const AxOccurrence *entry=&scan->occurrences[i];
        if((entry->type!=PRO_MDL_PART&&entry->type!=PRO_MDL_ASSEMBLY)||
           (type!=PRO_MDL_UNUSED&&entry->type!=type)||_wcsicmp(entry->name,name))continue;
        wchar_t token[AX_PATH_DEPTH*12+4];
        if(!ax_path_label(entry->ids,entry->depth,token,_countof(token)))continue;
        matches++;
        if(entry->state==PRO_FEAT_ACTIVE&&entry->model){
            actual++;
            if(actual==1)wcscpy_s(occurrence,AX_PATH_DEPTH*12+4,token);
        }
    }
    if(scan->limited||matches>1){
        part_text_add(out,L"按名称无法唯一确认组件。请选中模型树中的目标组件，或指定实例路径 ID。\n");
        unsigned shown=0;
        for(int i=0;i<scan->count&&shown<12;i++){
            const AxOccurrence *entry=&scan->occurrences[i];
            if((entry->type!=PRO_MDL_PART&&entry->type!=PRO_MDL_ASSEMBLY)||
               (type!=PRO_MDL_UNUSED&&entry->type!=type)||_wcsicmp(entry->name,name))continue;
            wchar_t token[AX_PATH_DEPTH*12+4];
            if(ax_path_label(entry->ids,entry->depth,token,_countof(token))){
                part_text_add(out,L"%ls  %ls.%ls  %ls\n",token,entry->name,
                    entry->type==PRO_MDL_ASSEMBLY?L"ASM":L"PRT",ax_state_label(entry->state));
                shown++;
            }
        }
        error=scan->limited?PRO_TK_INCOMPLETE:PRO_TK_E_FOUND;
    }else if(actual!=1){
        part_text_add(out,matches?L"该组件当前被抑制或未加载，Creo 尚无可打开的模型。\n":
                      L"在当前装配树中找不到该组件。请核对名称或指定实例路径 ID。\n");
        error=PRO_TK_E_NOT_FOUND;
    }
    free(scan);
    return error;
}

static ProError ax_open_component(ProMdl assembly,const wchar_t *selector,PartText *out){
    AxComponent component={0};
    AxOpenPending pending={0};
    ProMdl current=NULL,owner=NULL;
    ProMdlType root_type,required=PRO_MDL_UNUSED;
    int previous_window,root_window;
    if(!assembly||!selector||!*selector||!out)return PRO_TK_BAD_INPUTS;
    if(ax_open_pending.target){
        part_text_add(out,L"上一条打开请求正在核对窗口。\n");
        return PRO_TK_E_IN_USE;
    }
    ProError error=ProMdlCurrentGet(&current);
    if(!error)error=ProMdlTypeGet(assembly,&root_type);
    if(!error)error=ProMdlWindowGet(assembly,&root_window);
    if(!error)error=ProWindowCurrentGet(&previous_window);
    if(error||current!=assembly||root_type!=PRO_MDL_ASSEMBLY||previous_window!=root_window){
        part_text_add(out,L"请先激活要打开子件的装配窗口。\n");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    if(!_wcsnicmp(selector,L"ID:",3)){
        if(wcsncpy_s(pending.occurrence,_countof(pending.occurrence),selector,_TRUNCATE))
            return PRO_TK_BAD_INPUTS;
    }else{
        ProMdlName lookup;
        size_t length=wcslen(selector);
        if(length>4){
            const wchar_t *suffix=selector+length-4;
            if(!_wcsicmp(suffix,L".PRT"))required=PRO_MDL_PART;
            if(!_wcsicmp(suffix,L".ASM"))required=PRO_MDL_ASSEMBLY;
            if(required!=PRO_MDL_UNUSED)length-=4;
        }
        if(!length||length>=_countof(lookup)||wcsncpy_s(lookup,_countof(lookup),selector,length)||
           wcspbrk(lookup,L"\\/:")){
            part_text_add(out,L"请指定装配中的组件名称、实例路径 ID 或单个已选中的组件。\n");
            return PRO_TK_BAD_INPUTS;
        }
        if(!_wcsicmp(lookup,L"SELECTED")){
            error=ax_open_selected(assembly,pending.occurrence);
            if(error)part_text_add(out,L"请在当前 Creo 模型树上只选中一个子零件或子装配。\n");
        }else error=ax_open_find_name(assembly,lookup,required,pending.occurrence,out);
        if(error)return error;
    }
    error=ax_component_resolve(assembly,pending.occurrence,&component);
    if(error||!component.model||component.model==assembly){
        part_text_add(out,L"当前装配中该实例不可打开，请核对它是否已加载并处于活动状态。\n");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    pending.target=component.model;
    error=ProMdlTypeGet(pending.target,&pending.type);
    if(error)return error;
    if((pending.type!=PRO_MDL_PART&&pending.type!=PRO_MDL_ASSEMBLY)||
       (required!=PRO_MDL_UNUSED&&pending.type!=required)){
        part_text_add(out,L"所选实例与指定的零件或装配类型不匹配。\n");
        return PRO_TK_INVALID_TYPE;
    }
    error=ProMdlMdlnameGet(pending.target,pending.name);
    if(!error)error=ProDirectoryCurrentGet(pending.directory);
    if(error)return error;
    error=ProMdlWindowGet(pending.target,&pending.window);
    int create=error==PRO_TK_E_NOT_FOUND;
    if(create)error=ProObjectwindowMdlnameCreate(pending.name,
        pending.type==PRO_MDL_ASSEMBLY?PRO_ASSEMBLY:PRO_PART,&pending.window);
    if(!error&&pending.window==root_window)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProWindowCurrentSet(pending.window);
    if(!error&&create)error=ProMdlDisplay(pending.target);
    if(!error)error=ProWindowMdlGet(pending.window,&owner);
    if(!error&&owner!=pending.target)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProWindowActivate(pending.window);
    if(error){
        ProWindowCurrentSet(previous_window);
        part_text_add(out,L"Creo 未能打开指定组件的独立窗口（%d）。\n",error);
        return error;
    }
    pending.root=assembly;
    pending.root_window=root_window;
    ax_open_pending=pending;
    part_text_add(out,L"已请求打开 %ls.%ls（%ls），等待核对 Creo 活动窗口。\n",
        pending.name,pending.type==PRO_MDL_ASSEMBLY?L"ASM":L"PRT",pending.occurrence);
    return PRO_TK_NO_ERROR;
}

static ProError ax_open_readback(wchar_t *report,size_t capacity){
    if(!report||!capacity)return PRO_TK_BAD_INPUTS;
    report[0]=0;
    if(!ax_open_pending.target)return PRO_TK_E_NOT_FOUND;
    const AxOpenPending pending=ax_open_pending;
    ax_open_cancel();

    ProMdl active=NULL,owner=NULL;
    ProPath directory;
    int window;
    ProError error=ProMdlCurrentGet(&active);
    if(!error)error=ProWindowCurrentGet(&window);
    if(!error)error=ProWindowMdlGet(window,&owner);
    if(!error)error=ProDirectoryCurrentGet(directory);
    if(!error&&(active!=pending.target||window!=pending.window||owner!=pending.target||
       _wcsicmp(pending.directory,directory)))error=PRO_TK_BAD_CONTEXT;
    if(error){
        swprintf_s(report,capacity,L"Creo 已收到打开请求，但未核实 %ls（%ls）为当前独立活动模型（%d）。",
            pending.name,pending.occurrence,error);
        return error;
    }
    ax_return_note_open(pending.root,pending.root_window,pending.target,pending.window,pending.occurrence,pending.directory);
    swprintf_s(report,capacity,L"已打开并激活 %ls.%ls 的独立窗口（原装配实例 %ls）。",
        pending.name,pending.type==PRO_MDL_ASSEMBLY?L"ASM":L"PRT",pending.occurrence);
    return PRO_TK_NO_ERROR;
}
