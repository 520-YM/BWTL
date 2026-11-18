#define user_initialize menu_production_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static HMENU organized_find_group(HMENU menu,const wchar_t *label){
    int count=GetMenuItemCount(menu);
    for(int i=0;i<count;i++){
        wchar_t title[128];
        if(GetMenuStringW(menu,(UINT)i,title,_countof(title),MF_BYPOSITION)&&!wcscmp(title,label))
            return GetSubMenu(menu,i);
    }
    return NULL;
}

static int organized_find_id(HMENU menu,UINT action){
    int result=0,count=GetMenuItemCount(menu);
    for(int i=0;i<count;i++){
        HMENU sub=GetSubMenu(menu,i);
        if(sub)result+=organized_find_id(sub,action);
        else if(GetMenuItemID(menu,i)==action)result++;
    }
    return result;
}

static int organized_feature_coverage(HMENU menu,size_t *total){
    static const wchar_t *const categories[]={
        L"零件建模",L"钣金与机箱",L"装配与定位",L"修改与复用",
        L"检查与交付",L"公司标准与订单",L"对话与任务"
    };
    if(!menu||GetMenuItemID(menu,0)!=FEATURE_LIST_COMMAND)return 0;
    int group_count=0,root_count=GetMenuItemCount(menu);
    for(int i=0;i<root_count;i++)if(GetSubMenu(menu,i))group_count++;
    if(group_count!=_countof(categories))return 0;
    for(size_t i=0;i<_countof(categories);i++)if(!organized_find_group(menu,categories[i]))return 0;
    for(size_t i=0;i<_countof(native_features);i++){
        if(native_features[i].draft){
            if(organized_find_id(menu,(UINT)i+1)!=1)return 0;
            ++*total;
        }
    }
    return *total>0;
}

static int organized_imitation_coverage(HMENU menu,HMENU company){
    if(!menu||!company||GetMenuItemID(menu,0)!=IMITATION_CURRENT||GetMenuItemID(menu,1)!=IMITATION_FILE)return 0;
    if(!organized_find_group(menu,L"重建与试改")||!organized_find_group(menu,L"检查与比对")||
       !organized_find_group(menu,L"批量处理"))return 0;
    HMENU learn=organized_find_group(menu,L"学习与复用");
    if(!learn||!organized_find_group(learn,L"已验证的做法"))return 0;
    for(UINT id=IMITATION_CURRENT;id<IMITATION_GO_COMPANY;id++){
        int moved=id==IMITATION_LIBRARY||id==IMITATION_BATCH_LIBRARY||id==IMITATION_OPEN_STANDARD;
        if(organized_find_id(moved?company:menu,id)!=1||organized_find_id(moved?menu:company,id)!=0)return 0;
    }
    return organized_find_id(menu,IMITATION_GO_COMPANY)==1;
}

static int organized_workdir_coverage(HMENU menu){
    if(!menu||GetMenuItemID(menu,0)!=WORKDIR_CREATE||GetMenuItemID(menu,1)!=WORKDIR_PICK)return 0;
    for(UINT id=WORKDIR_VIEW;id<=WORKDIR_CREATE;id++)if(organized_find_id(menu,id)!=1)return 0;
    return 1;
}

static int organized_trial(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=fopen("organized-menus.log","w");
    if(!log){ProEngineerEnd();return 0;}
    HMENU features=native_feature_menu_create(),imitation=native_imitation_menu_create();
    HMENU workdir=native_workdir_menu_create(),company=native_company_menu_create();
    size_t count=0;
    int have_features=organized_feature_coverage(features,&count);
    int have_imitation=organized_imitation_coverage(imitation,company);
    int have_workdir=organized_workdir_coverage(workdir);
    Catalog *catalog=calloc(1,sizeof(*catalog));
    int have_catalog=catalog&&catalog_collect(catalog)&&catalog->count>count;
    uiCmdCmdId markup;
    int have_markup=ProCmdCmdIdFind("BWTLScreenshotMarkup",&markup)==PRO_TK_NO_ERROR;
    fprintf(log,"FEATURE_GROUPS_AND_IDS=%d\nFEATURE_COMMANDS=%zu\nIMITATION_COMPANY_IDS=%d\nWORKDIR_ORDER=%d\nCATALOG=%d\nSCREENSHOT_ROUTE=%d\nORGANIZED_MENUS_PASS=%d\n",
        have_features,count,have_imitation,have_workdir,have_catalog,have_markup,
        have_features&&have_imitation&&have_workdir&&have_catalog&&have_markup);
    fclose(log);
    free(catalog);
    if(features)DestroyMenu(features);
    if(imitation)DestroyMenu(imitation);
    if(workdir)DestroyMenu(workdir);
    if(company)DestroyMenu(company);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=menu_production_init(argc,argv,version,build,buffer);
    uiCmdCmdId action;
    if(!error)error=ProCmdActionAdd("BWTLOrganizedMenus",organized_trial,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&action);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLOrganizedMenus\x0060;");
}