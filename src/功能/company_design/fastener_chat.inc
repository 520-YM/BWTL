/* The native fastener path uses the same eight directories as the catalog. */
enum { FASTENER_CHAT_FIND=1, FASTENER_CHAT_READ, FASTENER_CHAT_OPEN, FASTENER_CHAT_OPEN_SAVE };

typedef struct {
    int action,scoped;
    wchar_t selector[MAX_PATH];
} FastenerChatRequest;

static struct {
    ProMdl model;
    ProPath directory;
    int window,save_project;
    HANDLE reference;
    wchar_t path[MAX_PATH];
} fastener_open;

static const wchar_t *fastener_chat_separator(const wchar_t *p){
    while(*p&&(iswspace(*p)||wcschr(L"，,：:",*p)))p++;
    return p;
}

static int fastener_chat_end(const wchar_t *p){
    while(*p&&(iswspace(*p)||wcschr(L"。！!吧",*p)))p++;
    return !*p;
}

static int fastener_chat_selector(const wchar_t **input,wchar_t selector[MAX_PATH]){
    const wchar_t *p=fastener_chat_separator(*input);
    wchar_t close=*p==L'「'?L'」':*p==L'“'?L'”':*p==L'"'?L'"':0;
    const wchar_t ascii[]=L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
    size_t length;
    if(close){
        const wchar_t *end=wcschr(++p,close);
        if(!end)return 0;
        length=end-p;
    }else{
        length=wcsspn(p,ascii);
        if(length&&p[length]==L'.'&&
           (!_wcsnicmp(p+length,L".prt",4)||!_wcsnicmp(p+length,L".asm",4))){
            length+=4;
            if(p[length]==L'.'){
                size_t digits=wcsspn(p+length+1,L"0123456789");
                if(digits)length+=digits+1;
            }
        }
    }
    if(!length||length>=MAX_PATH||wcsncpy_s(selector,MAX_PATH,p,length))return 0;
    if(!(length>=4&&p[1]==L':'&&(p[2]==L'\\'||p[2]==L'/'))&&
       wcsspn(selector,L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-.")!=length)return 0;
    *input=p+length+!!close;
    return 1;
}

static int fastener_chat_scope(const wchar_t **input){
    return assembly_take(input,L"标准紧固件模型")||assembly_take(input,L"紧固件模型")||
           assembly_take(input,L"标准紧固件")||assembly_take(input,L"紧固件");
}

static int fastener_chat_parse(const wchar_t *text,FastenerChatRequest *request){
    const wchar_t *p=model_request_action_start(text);
    if(wcspbrk(p,L"?？\r\n"))return 0;
    *request=(FastenerChatRequest){0};
    if(assembly_take(&p,L"打开并保存"))request->action=FASTENER_CHAT_OPEN_SAVE;
    else if(assembly_take(&p,L"打开")||assembly_take(&p,L"调出"))request->action=FASTENER_CHAT_OPEN;
    else if(assembly_take(&p,L"读取"))request->action=FASTENER_CHAT_READ;
    else if(assembly_take(&p,L"查找")||assembly_take(&p,L"搜索"))request->action=FASTENER_CHAT_FIND;
    if(request->action){
        p=fastener_chat_separator(p);
        request->scoped=fastener_chat_scope(&p);
        return fastener_chat_selector(&p,request->selector)&&fastener_chat_end(p)&&
            (request->scoped||request->selector[1]!=L':');
    }
    assembly_take(&p,L"把");
    request->scoped=fastener_chat_scope(&p);
    if(!fastener_chat_selector(&p,request->selector)||request->selector[1]==L':')return 0;
    p=fastener_chat_separator(p);
    if(!assembly_take(&p,L"我要")&&!assembly_take(&p,L"帮我"))assembly_take(&p,L"给我");
    if(!(assembly_take(&p,L"调这个出来")||assembly_take(&p,L"调出来")||
         assembly_take(&p,L"调出")||assembly_take(&p,L"打开"))||!fastener_chat_end(p))return 0;
    request->action=FASTENER_CHAT_OPEN;
    return 1;
}

static int fastener_chat_filename(const CompanyFastenerItem *item,const wchar_t *selector){
    const wchar_t *filename=item->path+item->name_offset;
    const size_t length=wcslen(selector);
    return (length==item->model_chars-4&&!_wcsnicmp(filename,selector,length))||
           (length==item->model_chars&&!_wcsnicmp(filename,selector,length))||!_wcsicmp(filename,selector);
}

static HANDLE fastener_chat_open_source(const wchar_t *input,wchar_t resolved[MAX_PATH]){
    for(UINT i=0;i<_countof(native_company_fasteners);i++){
        wchar_t directory[MAX_PATH];
        if(!company_fastener_resolve(COMPANY_OPEN_FASTENER_FIRST+i,directory)||
           !company_is_under(input,directory))continue;
        HANDLE file=company_open_path(input,0,resolved);
        if(file==INVALID_HANDLE_VALUE)return file;
        if(company_is_under(resolved,directory))return file;
        CloseHandle(file);
        return INVALID_HANDLE_VALUE;
    }
    return INVALID_HANDLE_VALUE;
}

static void fastener_open_cancel(void){
    if(fastener_open.reference)CloseHandle(fastener_open.reference);
    memset(&fastener_open,0,sizeof(fastener_open));
}

static int fastener_open_confirm(uiCmdCmdId id,uiCmdValue *value,void *data){
    if(!fastener_open.reference)return 0;
    if(!smart_active){fastener_open_cancel();return 0;}
    ProMdl active=NULL,owner=NULL;
    ProPath directory,origin;
    int window,save_project;
    ProError error=ProMdlCurrentGet(&active);
    if(!error)error=ProWindowCurrentGet(&window);
    if(!error)error=ProWindowMdlGet(window,&owner);
    if(!error)error=ProDirectoryCurrentGet(directory);
    if(!error)error=ProMdlOriginGet(active,origin);
    ProMdl verified=NULL;
    int matching_source=!error&&(attachment_same_file(origin,fastener_open.reference)||
        (fastener_saved_load(fastener_open.path,fastener_open.reference,&verified)&&verified==active));
    if(!error&&(active!=fastener_open.model||owner!=active||window!=fastener_open.window||
       _wcsicmp(directory,fastener_open.directory)||!matching_source))
        error=PRO_TK_BAD_CONTEXT;
    wchar_t reply[700];
    if(error)swprintf_s(reply,_countof(reply),L"Creo 已接收打开请求，活动窗口或文件版本未能核实（Creo %d）。",error);
    else swprintf_s(reply,_countof(reply),L"已打开并核实紧固件 %ls。\n文件：%ls",wcsrchr(fastener_open.path,L'\\')+1,fastener_open.path);
    if(!error&&fastener_open.save_project){
        wchar_t saved[MAX_PATH],details[600]=L"";
        PartText output={details,_countof(details),0};
        error=fastener_save_project(active,directory,saved,&output);
        if(!error)fastener_saved_record(fastener_open.path,fastener_open.reference,active);
        if(!error)swprintf_s(reply,_countof(reply),L"已在 Creo 中打开紧固件，并保存到当前工作目录，文件读回核对通过。\n文件：%ls",saved);
        else swprintf_s(reply,_countof(reply),L"紧固件已打开，保存未完成（Creo %d）。%ls",error,
            details[0]?details:L"请检查当前工作目录和文件状态，可重新点击继续核对。");
    }
    fastener_open_cancel();
    smart_record_tool(reply);
    smart_reply(reply);
    return 0;
}

static void fastener_open_model(const wchar_t *path,HANDLE reference,int save_project){
    wchar_t reply[1100]=L"";
    PartText output={reply,_countof(reply),0};
    NativeFile source={0};
    ProMdl model=NULL;
    int loaded=0;
    ProMdlName name;
    ProPath directory;
    ProError error=ProDirectoryCurrentGet(directory);
    HANDLE source_lock=INVALID_HANDLE_VALUE;
    if(!error){
        source_lock=company_library_source(path,&source,&output);
        if(source_lock==INVALID_HANDLE_VALUE)error=PRO_TK_CANT_OPEN;
    }
    if(!error&&!fastener_saved_load(path,reference,&model))error=attachment_open_native(&source,&model,&loaded,&output);
    if(source_lock!=INVALID_HANDLE_VALUE)CloseHandle(source_lock);
    if(!error&&model)error=ProMdlMdlnameGet(model,name);
    if(!model&&!error)error=PRO_TK_E_NOT_FOUND;
    if(error&&loaded&&model)ProMdlErase(model);
    if(!error){
        for(int i=0;i<attachment_release_count;i++)if(attachment_release[i]==model){
            attachment_release[i]=attachment_release[--attachment_release_count];
            break;
        }
        error=native_activate_model_window(model,name,source.kind==ATTACH_ASSEMBLY?PRO_ASSEMBLY:PRO_PART,
            &fastener_open.window);
    }
    if(!error){
        fastener_open.save_project=save_project;
        fastener_open.model=model;
        fastener_open.reference=reference;
        wcscpy_s(fastener_open.path,_countof(fastener_open.path),path);
        wcscpy_s(fastener_open.directory,_countof(fastener_open.directory),directory);
        error=ProMacroLoad(L"~ Command \x0060" L"BWTLFastenerOpenConfirm\x0060;");
        if(!error)return;
        fastener_open_cancel();
        reference=NULL;
    }
    if(reference)CloseHandle(reference);
    if(!reply[0])swprintf_s(reply,_countof(reply),L"已找到标准库文件 %ls。Creo 打开或确认窗口时返回 %d，请核对模型版本和引用。",path,error);
    smart_record_tool(reply);
    smart_reply(reply);
}

#include "fastener_bso_family.inc"
#include "fastener_bso_placement.inc"
#include "fastener_bso_chat.inc"

static int fastener_chat(const wchar_t *text){
    if(fastener_bso_chat(text))return 1;
    FastenerChatRequest request;
    if(!fastener_chat_parse(text,&request))return 0;
    if(native_busy||smart_active||ai_process){
        append(L"请等待当前任务完成后再查找或打开紧固件。");
        return 1;
    }
    wchar_t path[MAX_PATH]=L"",reply[1300]=L"";
    size_t matches=0;
    unsigned unavailable=0;
    const int explicit_path=request.selector[1]==L':';
    if(explicit_path){
        wcscpy_s(path,_countof(path),request.selector);
        for(wchar_t *position=path;*position;position++)if(*position==L'/')*position=L'\\';
    }
    else{
        CompanyFastenerCatalog catalog={0};
        company_fastener_catalog_build(&catalog);
        unavailable=catalog.unavailable|catalog.incomplete;
        for(size_t i=0;i<catalog.count;i++){
            const CompanyFastenerItem *item=&catalog.items[i];
            if(!fastener_chat_filename(item,request.selector))continue;
            matches++;
            if(matches==1)wcscpy_s(path,_countof(path),item->path);
        }
        free(catalog.items);
        if(!matches&&!request.scoped)return 0;
    }
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    smart_begin(text);
    smart_local_plan=1;
    if(matches>1)swprintf_s(reply,_countof(reply),L"标准库中有 %zu 个同名的「%ls」。请在「标准文件 → 紧固件」选择具体分类和文件。",matches,request.selector);
    else if(!*path)swprintf_s(reply,_countof(reply),L"在已登记的紧固件库中未找到「%ls」。%ls",request.selector,
        unavailable?L"部分分类暂时无法读取。":L"可在「标准文件 → 紧固件」核对型号。");
    else if(unavailable&&!explicit_path)swprintf_s(reply,_countof(reply),L"部分紧固件分类无法读取，已找到「%ls」。请在「标准文件 → 紧固件」选中确切文件后继续。",request.selector);
    else {
        wchar_t resolved[MAX_PATH];
        size_t name_length;
        unsigned long revision;
        HANDLE file=INVALID_HANDLE_VALUE;
        const wchar_t *leaf=wcsrchr(path,L'\\');
        if(leaf&&company_fastener_model_name(leaf+1,&name_length,&revision))
            file=fastener_chat_open_source(path,resolved);
        if(file==INVALID_HANDLE_VALUE)
            wcscpy_s(reply,_countof(reply),L"这个路径无法通过紧固件库授权范围和文件核对。请从「标准文件 → 紧固件」选中实际型号。");
        else if(request.action==FASTENER_CHAT_OPEN||request.action==FASTENER_CHAT_OPEN_SAVE){fastener_open_model(resolved,file,request.action==FASTENER_CHAT_OPEN_SAVE);return 1;}
        else if(request.action==FASTENER_CHAT_READ){
            static wchar_t report[30000];
            ProError error=company_library_model(resolved,report,_countof(report));
            smart_record_tool(report);
            if(!error){
                wchar_t *details=wcsstr(report,L"\n安装参照");
                if(details)*details=0;
                wcsncpy_s(reply,_countof(reply),report,_TRUNCATE);
            }else wcsncpy_s(reply,_countof(reply),report,_TRUNCATE);
            CloseHandle(file);
        }else{
            CloseHandle(file);
            swprintf_s(reply,_countof(reply),L"已找到紧固件 %ls。\n文件：%ls",wcsrchr(resolved,L'\\')+1,resolved);
        }
    }
    smart_reply(reply);
    return 1;
}
