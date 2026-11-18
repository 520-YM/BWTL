/* Return only to an open assembly proven to contain the current model. */
enum { AX_RETURN_HISTORY=16, AX_RETURN_MATCHES=8 };

typedef struct {
    ProMdl root,child;
    int root_window,child_window;
    ProPath directory;
    wchar_t occurrence[AX_PATH_DEPTH*12+4];
} AxReturnLink;

typedef struct {
    AxReturnLink link;
    ProMdlName name;
} AxReturnPending;

static AxReturnLink ax_return_history[AX_RETURN_HISTORY];
static unsigned ax_return_history_next;
static AxReturnPending ax_return_pending;

static void ax_return_cancel(void){
    memset(&ax_return_pending,0,sizeof(ax_return_pending));
}

static void ax_return_note_open(ProMdl root,int root_window,ProMdl child,int child_window,
                               const wchar_t *occurrence,const wchar_t *directory){
    if(!root||!child||root==child||root_window==child_window||!occurrence||!directory)return;
    AxReturnLink link={.root=root,.child=child,.root_window=root_window,.child_window=child_window};
    if(wcscpy_s(link.occurrence,_countof(link.occurrence),occurrence)||
       wcscpy_s(link.directory,_countof(link.directory),directory))return;
    for(unsigned i=0;i<_countof(ax_return_history);i++){
        AxReturnLink *entry=&ax_return_history[i];
        if(entry->child==child&&entry->child_window==child_window){
            *entry=link;
            return;
        }
    }
    ax_return_history[ax_return_history_next]=link;
    ax_return_history_next=(ax_return_history_next+1)%_countof(ax_return_history);
}

/* Window navigation does not need the master-representation or dynamic
   positioning restrictions used by component editing. Verify actual live
   references and active feature states, including each nested path segment. */
static int ax_return_link_live(const AxReturnLink *link){
    ProMdl root=NULL,child=NULL,owner;
    ProIdTable ids={0};
    ProPath directory;
    const int depth=ax_path_parse(link->occurrence,ids);
    if(!depth||ProWindowMdlGet(link->root_window,&root)||root!=link->root||
       ProWindowMdlGet(link->child_window,&child)||child!=link->child||
       ProDirectoryCurrentGet(directory)||_wcsicmp(directory,link->directory))return 0;
    owner=root;
    for(int i=0;i<depth;i++){
        ProAsmcomp feature;
        ProMdlType type;
        ProFeatStatus state;
        if(!owner||ProMdlTypeGet(owner,&type)||type!=PRO_MDL_ASSEMBLY||
           ax_component_feature(owner,ids[i],&feature)||ProFeatureStatusGet(&feature,&state)||
           state!=PRO_FEAT_ACTIVE||ProAsmcompMdlGet(&feature,&owner))return 0;
    }
    return owner==child;
}

typedef struct {
    ProMdl child;
    int child_window;
    ProPath directory;
    const wchar_t *requested;
    int depth,count,incomplete;
    AxReturnPending matches[AX_RETURN_MATCHES];
} AxReturnSearch;

static ProError ax_return_scan_window(int window,ProAppData data){
    AxReturnSearch *search=data;
    ProMdl root=NULL;
    ProMdlType type;
    ProMdlName name;
    if(window==search->child_window||ProWindowMdlGet(window,&root)||!root||root==search->child||
       ProMdlTypeGet(root,&type)||type!=PRO_MDL_ASSEMBLY||ProMdlMdlnameGet(root,name)||
       (search->requested&&_wcsicmp(search->requested,name)))return PRO_TK_NO_ERROR;
    AxScan *scan=NULL;
    ProError error=ax_scan(root,0,&scan);
    if(error){
        search->incomplete=1;
        return PRO_TK_NO_ERROR;
    }

    AxReturnPending candidate={0};
    int depth=0;
    for(int i=0;i<scan->count;i++){
        const AxOccurrence *entry=&scan->occurrences[i];
        if(entry->state==PRO_FEAT_ACTIVE&&entry->model==search->child&&
           (!depth||entry->depth<depth)&&
           ax_path_label(entry->ids,entry->depth,candidate.link.occurrence,
                         _countof(candidate.link.occurrence))){
            depth=entry->depth;
            if(depth==1)break;
        }
    }
    if(scan->limited&&depth!=1&&!search->requested)search->incomplete=1;
    free(scan);
    if(!depth||(search->depth&&depth>search->depth))return PRO_TK_NO_ERROR;
    if(depth!=search->depth){
        search->depth=depth;
        search->count=0;
    }
    for(int i=0;i<search->count;i++)
        if(search->matches[i].link.root==root)return PRO_TK_NO_ERROR;
    if(search->count>=_countof(search->matches))return PRO_TK_NO_ERROR;

    candidate.link.root=root;
    candidate.link.child=search->child;
    candidate.link.root_window=window;
    candidate.link.child_window=search->child_window;
    wcscpy_s(candidate.link.directory,_countof(candidate.link.directory),search->directory);
    wcscpy_s(candidate.name,_countof(candidate.name),name);
    search->matches[search->count++]=candidate;
    return PRO_TK_NO_ERROR;
}

static ProError ax_return_parent(const wchar_t *optional_name,PartText *out){
    if(!out)return PRO_TK_BAD_INPUTS;
    if(ax_return_pending.link.root||ax_open_pending.target){
        part_text_add(out,L"上一条窗口切换正在核对，请稍等。\n");
        return PRO_TK_E_IN_USE;
    }
    ProMdlName requested=L"",active_name=L"";
    if(optional_name&&*optional_name){
        size_t n=wcslen(optional_name);
        if(n>4&&!_wcsicmp(optional_name+n-4,L".ASM"))n-=4;
        if(!n||n>=_countof(requested)||wcsncpy_s(requested,_countof(requested),optional_name,n)||
           !part_name(requested)){
            part_text_add(out,L"请使用 Creo 装配名，例如「回到装配 FRAME.ASM」。\n");
            return PRO_TK_BAD_INPUTS;
        }
    }

    AxReturnSearch search={0};
    ProMdlType type;
    ProMdl owner=NULL;
    ProError error=ProWindowCurrentGet(&search.child_window);
    if(!error)error=ProMdlCurrentGet(&search.child);
    if(!error)error=ProWindowMdlGet(search.child_window,&owner);
    if(!error&&(!owner||owner!=search.child))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProMdlTypeGet(search.child,&type);
    if(!error)error=ProMdlMdlnameGet(search.child,active_name);
    if(!error)error=ProDirectoryCurrentGet(search.directory);
    if(error||(type!=PRO_MDL_PART&&type!=PRO_MDL_ASSEMBLY)){
        part_text_add(out,L"请先激活已打开的 Creo 零件或子装配窗口。\n");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    if(type==PRO_MDL_ASSEMBLY&&requested[0]&&!_wcsicmp(requested,active_name)){
        part_text_add(out,L"当前已经在装配 %ls.ASM 窗口。\n",active_name);
        return PRO_TK_BAD_CONTEXT;
    }

    AxReturnPending target={0};
    for(unsigned i=0;i<_countof(ax_return_history);i++){
        const AxReturnLink *entry=&ax_return_history[i];
        if(entry->child==search.child&&entry->child_window==search.child_window&&
           ax_return_link_live(entry)&&!ProMdlMdlnameGet(entry->root,target.name)&&
           (!requested[0]||!_wcsicmp(requested,target.name))){
            target.link=*entry;
            break;
        }
    }
    if(!target.link.root){
        search.requested=requested[0]?requested:NULL;
        error=ProWindowsVisit(ax_return_scan_window,&search);
        if(error||search.incomplete){
            part_text_add(out,L"Creo 未能完整核对已打开的装配关系。可以指定装配名后重试。\n");
            return error?error:PRO_TK_INCOMPLETE;
        }
        if(search.count>1){
            part_text_add(out,L"多个已打开的装配包含当前模型。请指定要回到的装配名。\n");
            for(int i=0;i<search.count;i++)
                part_text_add(out,L"  %ls.ASM（%ls）\n",search.matches[i].name,
                              search.matches[i].link.occurrence);
            return PRO_TK_E_FOUND;
        }
        if(!search.count){
            if(requested[0])part_text_add(out,L"未找到包含当前模型的 %ls.ASM 窗口。请确认该装配已打开。\n",requested);
            else if(type==PRO_MDL_ASSEMBLY)
                part_text_add(out,L"当前已经在装配 %ls.ASM 窗口，暂未找到上一级装配窗口。\n",active_name);
            else part_text_add(out,L"当前没有已打开且实际包含 %ls.PRT 的装配窗口。\n",active_name);
            return PRO_TK_E_NOT_FOUND;
        }
        target=search.matches[0];
    }
    if(!ax_return_link_live(&target.link)){
        part_text_add(out,L"原装配窗口或组件关系已变化，请核对 Creo 的装配树。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    error=ProWindowCurrentSet(target.link.root_window);
    if(!error)error=ProWindowActivate(target.link.root_window);
    if(error){
        ProWindowCurrentSet(target.link.child_window);
        part_text_add(out,L"Creo 未能切换到装配 %ls.ASM（%d）。\n",target.name,error);
        return error;
    }

    ax_return_pending=target;
    return PRO_TK_NO_ERROR;
}

static ProError ax_return_readback(wchar_t *report,size_t capacity){
    if(!report||!capacity)return PRO_TK_BAD_INPUTS;
    report[0]=0;
    if(!ax_return_pending.link.root)return PRO_TK_E_NOT_FOUND;
    const AxReturnPending pending=ax_return_pending;
    ax_return_cancel();

    ProMdl active=NULL;
    int window;
    ProError error=ProWindowCurrentGet(&window);
    if(!error)error=ProMdlCurrentGet(&active);
    if(!error&&(window!=pending.link.root_window||active!=pending.link.root||
       !ax_return_link_live(&pending.link)))error=PRO_TK_BAD_CONTEXT;
    if(error){
        swprintf_s(report,capacity,L"Creo 尚未确认 %ls.ASM 已成为活动装配窗口（%d）。",pending.name,error);
        return error;
    }
    swprintf_s(report,capacity,L"已回到装配 %ls.ASM。",pending.name);
    return PRO_TK_NO_ERROR;
}
