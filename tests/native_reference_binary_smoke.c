#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <ProCore.h>
#include <ProMenu.h>
#include <ProUICmd.h>

static int verify_chat_binary(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    uiCmdCmdId open;
    FILE *log=fopen("imitation-binary-smoke.log","w");
    if(log){
        wchar_t path[MAX_PATH]=L"";
        HMODULE module=GetModuleHandleW(L"BWTL.dll");
        if(module)GetModuleFileNameW(module,path,_countof(path));
        fprintf(log,"INSTALLED_BINARY=%d\n",wcsstr(path,L"versions\\0.23.0-local.chatresize.1\\bin\\BWTL.dll")!=NULL);
        fprintf(log,"IMITATION_REGISTERED=%d\n",ProCmdCmdIdFind("BWTLImitateModel",&open)==PRO_TK_NO_ERROR);
        fprintf(log,"BWTL_BINARY=%d\n",GetModuleHandleW(L"BWTL.dll")!=NULL);
        fprintf(log,"CHAT_REGISTERED=%d\n",ProCmdCmdIdFind("CodexNativeOpen",&open)==PRO_TK_NO_ERROR);
        fclose(log);
    }
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId cmd;
    ProError e=ProCmdActionAdd("BWTLBinarySmoke",verify_chat_binary,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    return e?e:ProMacroLoad(L"~ Command `BWTLBinarySmoke`;");
}

__declspec(dllexport) void user_terminate(void){}
