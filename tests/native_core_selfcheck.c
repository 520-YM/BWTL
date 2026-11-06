#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <psapi.h>
#include <ProToolkit.h>
#include <ProCore.h>
#include <ProUtil.h>
#include <ProUICmd.h>
#include <ProMenu.h>
static FILE *runtime_open(const wchar_t *name,const wchar_t *mode){return _wfopen(name,mode);}
static void trace(const char *label,int value){FILE *f=fopen("native-core.log","a");if(f){fprintf(f,"%s: %d\n",label,value);fclose(f);}}
#include "../src/native_model.h"
#include "../src/part_tools.h"
#include "../src/part_selfcheck.h"
static int finish_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    static wchar_t result[PART_TEXT_MAX];
    ProError error=part_selfcheck(result,PART_TEXT_MAX);
    FILE *f=_wfopen(L"native-core-result.txt",L"wt, ccs=UTF-8");
    if(f){fwprintf(f,L"result=%d\n%ls\n",error,result);fclose(f);}
    trace("CORE_TEST_RESULT",error);ProEngineerEnd();return 0;
}
static int begin_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdlName name;const char *stage="create baseline";ProError error;
    model_background_mode=0;error=model_create_empty(1,name,&stage);trace(stage,error);
    if(error){trace("CORE_TEST_RESULT",error);ProEngineerEnd();return 0;}
    trace("queue core test after window activation",ProMacroLoad(L"~ Command `BWTLCoreFinish`;"));return 0;
}
static uiCmdAccessState access_test(uiCmdAccessMode mode){return ACCESS_AVAILABLE;}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[8];HMODULE modules[2048];DWORD bytes=0,i;uiCmdCmdId command;ProError error;int legacy=0;
    if(!GetEnvironmentVariableA("BWTL_NATIVE_CORE_TEST",enabled,8)||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes))return PRO_TK_GENERAL_ERROR;
    for(i=0;i<bytes/sizeof(HMODULE)&&i<2048;i++){
        wchar_t path[MAX_PATH];if(GetModuleFileNameW(modules[i],path,MAX_PATH)){
            CharLowerBuffW(path,(DWORD)wcslen(path));if(wcsstr(path,L"creo_safe_resident"))legacy=1;
        }
    }
    trace("legacy_bridge_loaded",legacy);
    if(legacy){trace("CORE_TEST_RESULT",PRO_TK_BAD_CONTEXT);ProEngineerEnd();return PRO_TK_BAD_CONTEXT;}
    error=ProCmdActionAdd("BWTLCoreBegin",begin_test,uiProeImmediate,access_test,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    error=ProCmdActionAdd("BWTLCoreFinish",finish_test,uiProeImmediate,access_test,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    trace("queue baseline",ProMacroLoad(L"~ Command `BWTLCoreBegin`;"));return 0;
}
__declspec(dllexport) void user_terminate(void){trace("TEST_TERMINATED",0);}
