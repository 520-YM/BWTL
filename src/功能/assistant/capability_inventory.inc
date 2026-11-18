#ifndef BWTL_BUILD_VERSION
#define BWTL_BUILD_VERSION L"开发测试构建"
#endif

static const wchar_t catalog_read_failed[]=L"当前会话的功能清单暂时无法读取，请稍后重试。";
static const wchar_t catalog_heading[]=L"当前 Creo 会话加载的是 BWTL ";
static const wchar_t catalog_count_label[]=L"个可用功能入口";
static const wchar_t catalog_usage[]=L"按各菜单中可选的操作统计，同一编号下的不同操作分别计入。";

enum { CATALOG_SECONDARY_MAX=64, CATALOG_LABEL_MAX=128 };

typedef struct {
    const wchar_t *group,*label,*draft;
} CatalogEntry;

typedef struct {
    CatalogEntry entries[_countof(native_features)+CATALOG_SECONDARY_MAX+2];
    wchar_t menu_labels[CATALOG_SECONDARY_MAX][CATALOG_LABEL_MAX];
    size_t count,menu_count;
} Catalog;

/* Enumerate the same secondary menus that the chat window displays. */
static int catalog_read_menu(Catalog *list,HMENU menu,const wchar_t *group){
    const int count=GetMenuItemCount(menu);
    if(count<0)return 0;
    for(int i=0;i<count;i++){
        HMENU sub=GetSubMenu(menu,i);
        if(sub){
            if(!catalog_read_menu(list,sub,group))return 0;
            continue;
        }
        MENUITEMINFOW item={sizeof(item)};
        item.fMask=MIIM_FTYPE;
        if(!GetMenuItemInfoW(menu,i,TRUE,&item))return 0;
        if((item.fType&MFT_SEPARATOR)||GetMenuItemID(menu,i)==IMITATION_GO_COMPANY)continue;
        if(list->menu_count==_countof(list->menu_labels))return 0;
        wchar_t *label=list->menu_labels[list->menu_count];
        int n=GetMenuStringW(menu,i,label,CATALOG_LABEL_MAX,MF_BYPOSITION);
        if(n<=0||n>=CATALOG_LABEL_MAX-1)return 0;
        list->menu_count++;
        list->entries[list->count++]=(CatalogEntry){group,label,NULL};
    }
    return 1;
}

static int catalog_collect_feature(size_t section,size_t tab,size_t index,void *data){
    Catalog *list=data;
    const NativeFeature *feature=&native_features[index];
    list->entries[list->count++]=(CatalogEntry){native_feature_sections[section].label,feature->label,feature->draft};
    return 1;
}

static int catalog_collect(Catalog *list){
    if(!native_feature_visit(catalog_collect_feature,list))return 0;
    HMENU workdir=native_workdir_menu_create(),imitation=native_imitation_menu_create();
    HMENU company=native_company_menu_create(NULL);
    int ok=workdir&&imitation&&company&&catalog_read_menu(list,workdir,L"工作目录")&&
        catalog_read_menu(list,imitation,L"仿照模型")&&catalog_read_menu(list,company,L"标准文件");
    if(workdir)DestroyMenu(workdir);
    if(imitation)DestroyMenu(imitation);
    if(company)DestroyMenu(company);
    if(!ok)return 0;
    list->entries[list->count++]=(CatalogEntry){L"截图改图",L"截取 Creo 窗口并按标注改图",NULL};
    return 1;
}

/* Shortcut only for complete and unambiguous questions. Other requests go
   through the conversational intent pass, including mixed requests. */
static int catalog_quick_intent(const wchar_t *text,const wchar_t *recent){
    static const wchar_t *const versions[]={
        L"当前版本号",L"版本号",L"读取当前版本号",L"查看当前版本号",L"读取当前的版本号",
        L"当前BWTL是什么版本",L"BWTL现在是什么版本",L"你现在是什么版本",L"现在插件是什么版本"
    };
    static const wchar_t *const counts[]={
        L"现在有多少项功能",L"现在有多少功能",L"现在有多少个功能",L"插件有多少功能",
        L"现在插件一共有多少功能",L"当前插件有多少功能",L"现在一共有多少功能"
    };
    static const wchar_t *const all[]={
        L"功能列表",L"列出所有功能",L"列出全部功能",L"列出当前插件的所有功能",
        L"全部列出来我看看",L"全部列出来",L"都列出来",L"都列一下"
    };
    wchar_t compact[160];
    size_t used=0;
    for(;*text;text++){
        if(iswspace(*text)||wcschr(L"，,。；;、！!？?",*text))continue;
        if(used+1==_countof(compact))return 0;
        compact[used++]=*text;
    }
    compact[used]=0;
    const wchar_t *request=compact+(compact[0]==L'请');
    for(size_t i=0;i<_countof(versions);i++)if(!_wcsicmp(request,versions[i]))
        return SMART_INTENT_CATALOG_VERSION;
    for(size_t i=0;i<_countof(counts);i++){
        size_t n=wcslen(counts[i]);
        if(wcsncmp(request,counts[i],n))continue;
        if(!request[n]||!wcscmp(request+n,L"告诉我总数就行"))return SMART_INTENT_CATALOG_COUNT;
        for(size_t j=0;j<_countof(all);j++)if(!wcscmp(request+n,all[j]))
            return SMART_INTENT_CATALOG_LIST;
    }
    for(size_t i=0;i<4;i++)if(!wcscmp(request,all[i]))return SMART_INTENT_CATALOG_LIST;
    const wchar_t *answer=NULL;
    for(const wchar_t *turn=recent;(turn=wcsstr(turn,L"\n助手："))!=NULL;turn++)
        answer=turn+4;
    if(answer&&!wcsncmp(answer,catalog_heading,wcslen(catalog_heading))&&wcsstr(answer,catalog_count_label)){
        for(size_t i=4;i<_countof(all);i++)if(!wcscmp(request,all[i]))return SMART_INTENT_CATALOG_LIST;
    }
    return 0;
}

static wchar_t *catalog_message(int mode){
    if(mode==SMART_INTENT_CATALOG_VERSION){
        wchar_t result[160];
        swprintf_s(result,_countof(result),L"%ls%ls。",catalog_heading,BWTL_BUILD_VERSION);
        return _wcsdup(result);
    }
    Catalog *list=calloc(1,sizeof(*list));
    if(!list)return NULL;
    if(!catalog_collect(list)){free(list);return NULL;}
    size_t capacity=512;
    if(mode==SMART_INTENT_CATALOG_LIST){
        for(size_t i=0;i<list->count;i++)capacity+=wcslen(list->entries[i].group)+wcslen(list->entries[i].label)+12;
    }
    wchar_t *result=calloc(capacity,sizeof(*result));
    if(!result){free(list);return NULL;}
    PartText out={result,capacity,0};
    part_text_add(&out,L"%ls%ls，共 %zu %ls。%ls",catalog_heading,BWTL_BUILD_VERSION,list->count,catalog_count_label,catalog_usage);
    if(mode==SMART_INTENT_CATALOG_LIST){
        part_text_add(&out,L"\n「插件功能 → 功能列表」可用记事本查看同一份清单和用法。标有「回溯风险」的项目请先查看清单中的 Creo 回溯说明。\n");
        const wchar_t *group=L"";
        for(size_t i=0;i<list->count;i++){
            const CatalogEntry *item=&list->entries[i];
            if(wcscmp(group,item->group)){
                group=item->group;
                part_text_add(&out,L"\n%ls\n",group);
            }
            part_text_add(&out,L"• %ls\n",item->label);
        }
    }
    free(list);
    if(out.truncated){free(result);return NULL;}
    return result;
}

static void catalog_prompt_write(FILE *prompt,const wchar_t *query){
    Catalog *list=calloc(1,sizeof(*list));
    fwprintf(prompt,L"\n<本次运行的插件数据>\n当前加载的 BWTL 版本 %ls。",BWTL_BUILD_VERSION);
    if(list&&catalog_collect(list)){
        fwprintf(prompt,L"本机当前有 %zu 个菜单功能入口。%ls\n",list->count,catalog_usage);
        static const wchar_t *const questions[]={L"功能",L"能力",L"支持",L"能不能",L"有没有",L"怎么用",L"可以做",L"会不会",L"能否"};
        if(wcslen(query)<512&&smart_has_text(query,questions,_countof(questions))){
            struct {int score;const CatalogEntry *item;} matches[16]={0};
            for(size_t i=0;i<list->count;i++){
                const CatalogEntry *item=&list->entries[i];
                int score=2*history_score(item->label,query)+history_score(item->group,query);
                if(score<=matches[_countof(matches)-1].score)continue;
                for(size_t j=0;j<_countof(matches);j++){
                    if(score<=matches[j].score)continue;
                    memmove(matches+j+1,matches+j,(_countof(matches)-j-1)*sizeof(matches[0]));
                    matches[j].score=score;
                    matches[j].item=item;
                    break;
                }
            }
            fwprintf(prompt,L"与最新问题相关的本机菜单入口（节选）\n");
            for(size_t i=0;i<_countof(matches)&&matches[i].item;i++)
                fwprintf(prompt,L"- %ls → %ls\n",matches[i].item->group,matches[i].item->label);
        }
    }
    free(list);
    fwprintf(prompt,L"</本次运行的插件数据>\n报插件版本或功能总数以本次数据为准。结合用户具体想做的事和已提供的工具协议判断能力。上面的菜单节选用于查找入口，覆盖范围由相应的实际工具协议决定。\n");
}

static int catalog_write_document(wchar_t *file,size_t capacity){
    Catalog *list=calloc(1,sizeof(*list));
    wchar_t temporary[MAX_PATH];
    if(!list)return 0;
    int ok=catalog_collect(list)&&runtime_data[0]&&
        swprintf_s(file,capacity,L"%ls\\BWTL 功能列表.txt",runtime_data)>=0&&
        swprintf_s(temporary,_countof(temporary),L"%ls.pending-%lu",file,GetCurrentThreadId())>=0;
    if(!ok){free(list);return 0;}
    FILE *out=_wfopen(temporary,L"wt, ccs=UTF-8");
    if(!out){free(list);return 0;}
    fwprintf(out,L"BWTL 当前功能列表\n%ls%ls，共 %zu %ls。\n%ls\n\n",
        catalog_heading,BWTL_BUILD_VERSION,list->count,catalog_count_label,catalog_usage);
    fwprintf(out,L"Creo 回溯风险\n标记项目在特定参照、附加壁未完整生成或原生清理阶段有过 Creo 回溯。逐项触发条件、已验收范围及替代用法记录在下方文件。\n%ls\\docs\\Creo回溯风险重点提醒.md\n\n",
        runtime_root);
    const wchar_t *group=L"";
    for(size_t i=0;i<list->count;i++){
        const CatalogEntry *item=&list->entries[i];
        if(wcscmp(group,item->group)){
            group=item->group;
            fwprintf(out,L"\n%ls\n",group);
        }
        fwprintf(out,L"• %ls\n",item->label);
        if(item->draft&&item->draft[0]&&!wcsstr(item->label,L"【回溯风险】"))
            fwprintf(out,L"  %ls\n",item->draft);
    }
    ok=!ferror(out);
    if(fclose(out))ok=0;
    if(ok)ok=MoveFileExW(temporary,file,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temporary);
    free(list);
    return ok;
}
