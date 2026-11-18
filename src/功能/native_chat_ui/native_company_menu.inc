enum {
    COMPANY_OPEN_STANDARDS=1001,COMPANY_OPEN_TECHNICAL,COMPANY_OPEN_ROOT,
    COMPANY_OPEN_FASTENER_FIRST,
    COMPANY_RULE_VIEW=1101,COMPANY_RULE_EDIT,COMPANY_RULE_RESTORE,
    COMPANY_FASTENER_MODEL_FIRST=2000
};

typedef struct {
    wchar_t path[MAX_PATH];
    wchar_t caption[128];
    size_t name_offset,model_chars;
    unsigned long revision;
    UINT category;
} CompanyFastenerItem;

typedef struct {
    CompanyFastenerItem *items;
    size_t count,capacity;
    unsigned unavailable,incomplete;
} CompanyFastenerCatalog;

static const struct {const wchar_t *label,*folder,*catalog_file;} native_company_fasteners[]={
    {L"螺丝装配",L"Assembly螺丝（沉头十字；扁圆头十字；内、外六角等）垫片弹片等",L"Assembly.mnu"},
    {L"螺栓",L"Bolts异性螺丝（螺栓）（马车螺丝等）",L"bolts.mnu"},
    {L"螺母",L"Nuts螺母（六角、方形、法兰、蝶形、圆头、滚花、滚花台阶等）",L"nuts.mnu"},
    {L"销",L"Pins（销）",L"Pins.mnu"},
    {L"压铆件",L"Riveting_parts（压铆螺丝、盲孔通孔压铆螺母大小、防水压铆螺母、压花螺母、铜柱、铜鼻子）",L"Riveting_parts.mnu"},
    {L"铆钉",L"Rivets(铆钉)",L"rivets.mnu"},
    {L"自攻螺丝",L"Screws（螺丝）自攻螺、大扁圆头、异性螺丝",L"screws.mnu"},
    {L"垫圈",L"Washers（垫圈）",L"washers.mnu"}
};

static HMENU native_company_menu_create(const CompanyFastenerCatalog *catalog){
    HMENU menu=CreatePopupMenu();
    if(!menu)return NULL;
    const UINT standard_flags=MF_STRING|(native_busy?MF_GRAYED:MF_ENABLED);
    if(!AppendMenuW(menu,standard_flags,IMITATION_OPEN_STANDARD,L"打开已验收标准件…"))goto fail;
    if(catalog){
        HMENU fasteners=native_feature_add_submenu(menu,L"紧固件");
        if(!fasteners)goto fail;
        size_t model=0;
        for(UINT i=0;i<_countof(native_company_fasteners);i++){
            HMENU category=native_feature_add_submenu(fasteners,native_company_fasteners[i].label);
            if(!category)goto fail;
            size_t first=model;
            for(;model<catalog->count&&catalog->items[model].category==i;model++){
                const CompanyFastenerItem *item=&catalog->items[model];
                wchar_t label[MAX_PATH*2];
                if(swprintf_s(label,_countof(label),L"%.*ls%ls%ls",(int)item->model_chars,
                    item->path+item->name_offset,item->caption[0]?L"  ·  ":L"",item->caption)<0||
                    !AppendMenuW(category,standard_flags,COMPANY_FASTENER_MODEL_FIRST+(UINT)model,label))goto fail;
            }
            const wchar_t *status=NULL;
            if(catalog->unavailable&(1u<<i))status=L"目录不可用";
            else if(catalog->incomplete&(1u<<i))status=L"部分目录未能读取";
            else if(model==first)status=L"暂无型号";
            if(status&&!AppendMenuW(category,MF_STRING|MF_GRAYED,0,status))goto fail;
        }
    }else if(!AppendMenuW(menu,MF_STRING,COMPANY_OPEN_FASTENER_FIRST,L"紧固件"))goto fail;
    HMENU rules=native_feature_add_submenu(menu,L"建模规则");
    if(!rules||
       !AppendMenuW(rules,MF_STRING,COMPANY_RULE_VIEW,L"查看生效规则")||
       !AppendMenuW(rules,standard_flags,COMPANY_RULE_EDIT,L"修改规则…")||
       !AppendMenuW(rules,standard_flags,COMPANY_RULE_RESTORE,L"恢复上一版…"))goto fail;
    HMENU documents=native_feature_add_submenu(menu,L"资料");
    if(!documents||
       !AppendMenuW(documents,MF_STRING,COMPANY_OPEN_STANDARDS,L"打开公司标准")||
       !AppendMenuW(documents,MF_STRING,COMPANY_OPEN_TECHNICAL,L"打开技术文件")||
       !AppendMenuW(documents,MF_STRING,COMPANY_OPEN_ROOT,L"打开公司资料目录"))goto fail;
    HMENU archive=native_feature_add_submenu(menu,L"归档");
    if(!archive||
       !AppendMenuW(archive,standard_flags,IMITATION_LIBRARY,L"将上次结果存为公司标准件")||
       !AppendMenuW(archive,standard_flags,IMITATION_BATCH_LIBRARY,L"批量归档已通过的结果"))goto fail;
    return menu;
fail:
    DestroyMenu(menu);
    return NULL;
}
