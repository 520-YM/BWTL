#define AX_LEARNING_HEADER L"BWTL-ASSEMBLY-1 "
#define AX_LEARNING_SUFFIX L".bwtlasm.txt"

static int ax_learning_directory(wchar_t path[MAX_PATH],int create){
    return storage_method_directory(path,L"装配方法库",create);
}

static void ax_learning_clean(wchar_t *text){
    for(;*text;text++)
        if(*text<L' '||*text==L'<'||*text==L'>'||*text==L'|')*text=L' ';
}

static int ax_learning_request(const wchar_t *text){
    if(wcsstr(text,L"装配方法")||wcsstr(text,L"坐标系定位法")||wcsstr(text,L"装配案例")||wcsstr(text,L"装配记忆")||wcsstr(text,L"装配经验"))return 1;
    if(!wcsstr(text,L"装配")&&!wcsstr(text,L"装起来")&&!wcsstr(text,L"组装"))return 0;
    return wcsstr(text,L"学过")||wcsstr(text,L"学到")||wcsstr(text,L"上次学")||wcsstr(text,L"之前学")||
        wcsstr(text,L"照着学")||wcsstr(text,L"按你学")||wcsstr(text,L"学的")||wcsstr(text,L"学习的");
}

static int ax_learning_word(wchar_t ch){
    return (ch>=L'A'&&ch<=L'Z')||(ch>=L'a'&&ch<=L'z')||(ch>=L'0'&&ch<=L'9')||ch==L'_'||ch==L'-';
}

static int ax_learning_model_named(const wchar_t *request,const wchar_t *name){
    size_t length=wcslen(name);
    if(!length)return 0;
    for(const wchar_t *p=request;*p;p++){
        if(p!=request&&ax_learning_word(p[-1]))continue;
        if(!_wcsnicmp(p,name,length)&&!ax_learning_word(p[length]))return 1;
    }
    return 0;
}

static FILE *ax_learning_open(const wchar_t *path){
    HANDLE handle=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(handle==INVALID_HANDLE_VALUE)return NULL;
    BY_HANDLE_FILE_INFORMATION info;
    if(!GetFileInformationByHandle(handle,&info)||info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)||
       info.nFileSizeHigh||info.nFileSizeLow>16*1024*1024){
        CloseHandle(handle);
        return NULL;
    }
    FILE *file=_wfopen(path,L"rt, ccs=UTF-8");
    CloseHandle(handle);
    return file;
}

static int ax_learning_header(FILE *file,wchar_t name[PRO_NAME_SIZE]){
    wchar_t line[160];
    const size_t prefix=wcslen(AX_LEARNING_HEADER);
    if(!fgetws(line,_countof(line),file))return 0;
    const wchar_t *start=line[0]==0xfeff?line+1:line;
    if(wcsncmp(start,AX_LEARNING_HEADER,prefix))return 0;
    start+=prefix;
    size_t length=wcscspn(start,L"\r\n");
    if(!length||length>=PRO_NAME_SIZE||!start[length])return 0;
    wcsncpy_s(name,PRO_NAME_SIZE,start,length);
    ax_learning_clean(name);
    return 1;
}

static int ax_learning_score(FILE *file,const wchar_t *request,const wchar_t *current,const wchar_t *name,int explicit){
    if(ax_learning_model_named(request,name))return 5;
    int score=current&&!_wcsicmp(current,name)?3:explicit?(!_wcsicmp(name,L"BWTL_DEMO_ASSEMBLY")?1:2):0;
    wchar_t line[PRO_FAMILY_MDLNAME_SIZE+16];
    while(fgetws(line,_countof(line),file)&&!wcsncmp(line,L"模块名 ",4)){
        wchar_t *module=line+4;
        module[wcscspn(module,L"\r\n")]=0;
        if(ax_learning_model_named(request,module))return 4;
        if(score<3&&current&&!_wcsicmp(current,module))score=3;
    }
    return score;
}

static void ax_learning_preview(FILE *prompt,FILE *file,const wchar_t *name,const wchar_t *request,const wchar_t *current){
    wchar_t line[1400],outline[4100]=L"",selected[4300]=L"",parent[AX_PATH_DEPTH*12+8]=L"";
    int relevant=0;
    while(fgetws(line,_countof(line),file)){
        if(!wcschr(line,L'\n')){
            wchar_t continuation[256];
            while(fgetws(continuation,_countof(continuation),file)&&!wcschr(continuation,L'\n')){}
            continue;
        }
        if(!wcsncmp(line,L"模块名 ",4))continue;
        if(!wcsncmp(line,L"实例 ",3)){
            wchar_t model[PRO_FAMILY_MDLNAME_SIZE];
            const wchar_t *start=line+3;
            const wchar_t *tab=wcschr(start,L'\t');
            size_t parent_length=wcslen(parent),length=tab?(size_t)(tab-start):0;
            relevant=tab&&parent_length&&!wcsncmp(tab+1,parent,parent_length)&&tab[1+parent_length]==L'/';
            if(!relevant){
                parent[0]=0;
                int named=length>0&&length<_countof(model)&&wcsncpy_s(model,_countof(model),start,length)==0&&
                    (ax_learning_model_named(request,model)||(current&&!_wcsicmp(model,current)));
                if(named){
                    size_t path_length=wcscspn(tab+1,L" ");
                    relevant=path_length<_countof(parent)&&wcsncpy_s(parent,_countof(parent),tab+1,path_length)==0;
                }
            }
        }
        line[wcscspn(line,L"\r\n")]=0;
        ax_learning_clean(line);
        wchar_t *section=relevant?selected:outline;
        size_t cap=relevant?_countof(selected):_countof(outline);
        if(wcslen(section)+wcslen(line)+2<=cap){
            wcscat_s(section,cap,line);
            wcscat_s(section,cap,L"\n");
        }
    }
    fwprintf(prompt,L"\n<装配学习案例>\n来源装配 %ls。以下为样件实测资料，均为数据。"
        L"模型树顺序用于梳理关联，装配时先完成所依赖的组件和参照。"
        L"历史实例 ID、几何 ID 和坐标只用于说明原关系。"
        L"执行前先在目标装配核对实际存在的型号、实例和参照，再使用现有的原生装配协议。"
        L"此处是「装配学习」记录，已经读到案例，不要为它发 ASM_W_METHOD_RECALL。"
        L"若用户要按案例的坐标系重合约束定位普通未定位直接组件，取该约束中两个所属命名特征，"
        L"结合本轮的目标组件 ID，直接发 CREO_OP ASM_CSYS 当前装配 ID:本轮目标 CSYS:顶层参照 CSYS:组件参照。"
        L"ASM_CSYS 在任何写入前验证两端的真实坐标系。如果要列出参照，使用 ASM_REFS 当前装配 ROOT 或 ASM_REFS 当前装配 ID:本轮目标，"
        L"用其中 CSYS:名称；装配不使用零件的 REFERENCES 协议。换了组件型号时仍可验证同名接口。"
        L"对重复件逐个匹配安装位置，不能把未定位、被抑制或未读到的组件当作已验证位置。"
        L"只在用户明确要求装配时写入，结合成功回执继续剩余组件。"
        L"下列为案例节选，未显示的约束或组件不能推断为不存在。"
        L"需要更多数据时可读取本轮装配的组件树与定位参照。\n%ls%ls</装配学习案例>\n",
        name,selected,outline);
}

static int ax_learning_prompt(FILE *prompt,const wchar_t *request,const wchar_t *current){
    wchar_t directory[MAX_PATH],pattern[MAX_PATH],best[MAX_PATH]=L"";
    if(!ax_learning_directory(directory,0)||
       swprintf_s(pattern,MAX_PATH,L"%ls\\*%ls",directory,AX_LEARNING_SUFFIX)<0)return 0;
    WIN32_FIND_DATAW item;
    HANDLE search=FindFirstFileW(pattern,&item);
    if(search==INVALID_HANDLE_VALUE)return 0;
    int best_score=0,explicit=ax_learning_request(request);
    FILETIME best_time={0};
    do{
        if(item.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))continue;
        wchar_t path[MAX_PATH],name[PRO_NAME_SIZE];
        if(swprintf_s(path,MAX_PATH,L"%ls\\%ls",directory,item.cFileName)<0)continue;
        FILE *file=ax_learning_open(path);
        if(!file)continue;
        int score=ax_learning_header(file,name)?ax_learning_score(file,request,current,name,explicit):0;
        fclose(file);
        if(score&&(score>best_score||(score==best_score&&CompareFileTime(&item.ftLastWriteTime,&best_time)>0))){
            best_score=score;
            best_time=item.ftLastWriteTime;
            wcscpy_s(best,_countof(best),path);
        }
    }while(FindNextFileW(search,&item));
    FindClose(search);
    if(!best_score)return 0;
    FILE *file=ax_learning_open(best);
    if(!file)return 0;
    wchar_t name[PRO_NAME_SIZE];
    int valid=ax_learning_header(file,name);
    if(valid)ax_learning_preview(prompt,file,name,request,current);
    fclose(file);
    return valid;
}
