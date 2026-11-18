#define user_initialize project_probe_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *project_probe_log;
static int project_probe_failures;
static const wchar_t project_probe_template[]=L"（数量）（项目名称）（国家）（订单号）";
static const wchar_t project_probe_drawings[]=L"V1.0\\1. 3D模型";

static void project_probe_check(int pass,const char *name){
    fprintf(project_probe_log,"%s=%d\n",name,pass!=0);
    if(!pass)project_probe_failures++;
}

static int project_probe_join(wchar_t path[MAX_PATH],const wchar_t *dir,const wchar_t *leaf){
    return swprintf_s(path,MAX_PATH,L"%ls\\%ls",dir,leaf)>=0;
}

static void project_probe_trim(wchar_t *path){
    size_t length=wcslen(path);
    while(length>3&&path[length-1]==L'\\')path[--length]=0;
}

static int project_probe_at(const wchar_t *path){
    ProPath current;
    if(ProDirectoryCurrentGet(current))return 0;
    project_probe_trim(current);
    return !_wcsicmp(current,path);
}

static int project_probe_folder(const wchar_t *path){
    DWORD attributes=GetFileAttributesW(path);
    return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY);
}

static int project_probe_put(const wchar_t *path,const char *data){
    FILE *file=_wfopen(path,L"wb");
    if(!file)return 0;
    int success=fputs(data,file)>=0;
    return fclose(file)==0&&success;
}

static int project_probe_has(const wchar_t *dir,const wchar_t *leaf,const char *expected){
    wchar_t path[MAX_PATH];
    if(!project_probe_join(path,dir,leaf))return 0;
    FILE *file=_wfopen(path,L"rb");
    if(!file)return 0;
    char content[128]={0};
    size_t size=fread(content,1,sizeof(content),file);
    int success=!ferror(file)&&size==strlen(expected)&&!memcmp(content,expected,size);
    fclose(file);
    return success;
}

static int project_probe_root(const wchar_t *root,int complete){
    wchar_t source[MAX_PATH],version[MAX_PATH],drawing[MAX_PATH],path[MAX_PATH];
    if(!CreateDirectoryW(root,NULL)||
       !project_probe_join(source,root,project_probe_template)||!CreateDirectoryW(source,NULL)||
       !project_probe_join(version,source,L"V1.0")||!CreateDirectoryW(version,NULL))return 0;
    if(!complete)return 1;
    return project_probe_join(drawing,source,project_probe_drawings)&&CreateDirectoryW(drawing,NULL)&&
        project_probe_join(path,drawing,L"空文件夹")&&CreateDirectoryW(path,NULL)&&
        project_probe_join(path,drawing,L"样件.txt")&&project_probe_put(path,"model data")&&
        project_probe_join(path,source,L"项目记录.txt")&&project_probe_put(path,"template record");
}

static int project_probe_timestamp(const wchar_t *root,const wchar_t *current){
    size_t root_length=wcslen(root);
    const wchar_t *prefix=L"\\（数量）（项目名称";
    const wchar_t *suffix=L"）（国家）（订单号）\\V1.0\\1. 3D模型";
    size_t prefix_length=wcslen(prefix);
    if(_wcsnicmp(root,current,root_length)||
       wcsncmp(current+root_length,prefix,prefix_length))return 0;
    const wchar_t *stamp=current+root_length+prefix_length;
    if(wcslen(stamp)!=15+wcslen(suffix)||wcscmp(stamp+15,suffix))return 0;
    for(int i=0;i<15;i++){
        if(i==8 ? stamp[i]!=L'-' : stamp[i]<L'0'||stamp[i]>L'9')return 0;
    }
    return 1;
}

static int project_probe_finish(const wchar_t *directory){
    if(directory&&*directory)ProDirectoryChange((wchar_t *)directory);
    fprintf(project_probe_log,"PROJECT_CREATE_PASS=%d\n",project_probe_failures==0);
    fclose(project_probe_log);
    ProEngineerEnd();
    return 0;
}

static int project_probe_run(uiCmdCmdId command,uiCmdValue *value,void *data){
    static const wchar_t full_name[]=L"1台测试终端（示例地区）（TEST-0001）";
    ProPath initial,current;
    wchar_t root[MAX_PATH],missing[MAX_PATH],source[MAX_PATH],target[MAX_PATH],drawing[MAX_PATH],path[MAX_PATH];
    wchar_t message[PRO_PATH_SIZE+100]=L"",label[64]=L"";
    project_probe_log=fopen("project-create.log","w");
    if(!project_probe_log){ProEngineerEnd();return 0;}
    HMENU menu=native_workdir_menu_create();
    project_probe_check(menu&&GetMenuStringW(menu,WORKDIR_CREATE,label,_countof(label),MF_BYCOMMAND)&&
                        !wcscmp(label,L"创建新项目"),"MENU_CREATE");
    if(menu)DestroyMenu(menu);

    if(ProDirectoryCurrentGet(initial)){project_probe_check(0,"INITIAL_DIRECTORY");return project_probe_finish(NULL);}
    project_probe_trim(initial);
    int fixtures=project_probe_join(root,initial,L"fixture-main")&&
        project_probe_join(missing,initial,L"fixture-incomplete")&&
        project_probe_root(root,1)&&project_probe_root(missing,0)&&
        project_probe_join(source,root,project_probe_template)&&
        project_probe_join(target,root,full_name)&&
        project_probe_join(drawing,target,project_probe_drawings);
    project_probe_check(fixtures,"FIXTURES");
    if(!fixtures)return project_probe_finish(initial);

    pending_plan.ready=1;
    smart_waiting_for_user=1;
    project_create_workdir(root,full_name,message);
    project_probe_check(project_probe_at(drawing)&&wcsstr(message,drawing)!=NULL,"FULL_NAME_DIRECTORY");
    project_probe_check(project_probe_has(target,L"项目记录.txt","template record")&&
        project_probe_has(drawing,L"样件.txt","model data")&&
        project_probe_join(path,drawing,L"空文件夹")&&project_probe_folder(path),"CONTENTS");
    project_probe_check(project_probe_has(source,L"项目记录.txt","template record"),"TEMPLATE_PRESERVED");
    project_probe_check(!pending_plan.ready&&!smart_waiting_for_user,"OLD_BINDINGS_CLEARED");

    int sentinel=project_probe_join(path,target,L"项目记录.txt")&&project_probe_put(path,"destination record");
    message[0]=0;
    project_create_workdir(root,full_name,message);
    project_probe_check(sentinel&&project_probe_has(target,L"项目记录.txt","destination record")&&
        project_probe_at(drawing)&&message[0],"DUPLICATE_PRESERVED");

    message[0]=0;
    project_create_workdir(root,L"不合法/名字",message);
    project_probe_check(project_probe_at(drawing)&&message[0],"INVALID_NAME_NO_SWITCH");

    message[0]=0;
    project_create_workdir(root,L"带尾部句点.",message);
    project_probe_check(project_probe_at(drawing)&&message[0]&&
        project_probe_join(path,root,L"带尾部句点")&&
        GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES,"TRAILING_DOT_NO_SWITCH");

    message[0]=0;
    project_create_workdir(root,L"带尾部空格 ",message);
    project_probe_check(project_probe_at(drawing)&&message[0]&&
        project_probe_join(path,root,L"带尾部空格")&&
        GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES,"TRAILING_SPACE_NO_SWITCH");

    message[0]=0;
    project_create_workdir(root,L"COM¹.txt",message);
    project_probe_check(project_probe_at(drawing)&&wcsstr(message,L"保留名称")!=NULL,"RESERVED_NAME_NO_SWITCH");

    message[0]=0;
    project_create_workdir(root,project_probe_template,message);
    project_probe_check(project_probe_at(drawing)&&wcsstr(message,L"同名")!=NULL&&
        project_probe_has(source,L"项目记录.txt","template record"),"TEMPLATE_NAME_NO_SWITCH");

    message[0]=0;
    project_create_workdir(missing,L"缺失模板目录",message);
    project_probe_check(project_probe_at(drawing)&&message[0]&&
        project_probe_join(path,missing,L"缺失模板目录")&&
        GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES,"MISSING_MODEL_DIRECTORY_NO_SWITCH");

    message[0]=0;
    project_create_workdir(root,L"",message);
    int readback=!ProDirectoryCurrentGet(current);
    if(readback)project_probe_trim(current);
    project_probe_check(readback&&project_probe_timestamp(root,current)&&
        project_probe_has(current,L"样件.txt","model data")&&wcsstr(message,current)!=NULL,"EMPTY_NAME_TIMESTAMP");
    return project_probe_finish(initial);
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||
       strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    uiCmdCmdId action;
    ProError error=project_probe_production_initialize(argc,argv,version,build,error_buffer);
    if(!error)error=ProCmdActionAdd("BWTLProjectCreateProbe",project_probe_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&action);
    return error?error:ProMacroLoad(L"~ Command " L"\x0060" L"BWTLProjectCreateProbe" L"\x0060;");
}