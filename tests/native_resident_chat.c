#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *receipt;
static int failed,phase,queued;
static DWORD worker_pid;
static wchar_t thread[100];
static ULONGLONG deadline;

static void resident_check(int pass,const char *name){
    fprintf(receipt,"%s=%d\n",name,!!pass);fflush(receipt);
    if(!pass)failed++;
}

static int finish(void){
    KillTimer(native_backend_window,5469);
    fprintf(receipt,"RESIDENT_CHAT_PASS=%d\n",!failed);
    fclose(receipt);
    ProEngineerEnd();
    return 0;
}

static const wchar_t *last_reply(void){
    const wchar_t *last=L"",*scan=history;
    while((scan=wcsstr(scan,L"【AI】"))!=NULL){last=scan;scan+=4;}
    return last;
}

static int box_matches(void){
    ProMdl model=NULL;
    ProOutline outline;
    ProSolidBody body;
    double mm=0;
    if(ProMdlCurrentGet(&model)||qa_single_body(model,&body)||part_health(model)||
       qa_mm_per_unit(model,&mm)||ProSolidBodyOutlineGet(&body,outline))return 0;
    const double expected[]={27,18,13};
    int mask=0;
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)
        if(part_close((outline[1][i]-outline[0][i])*mm,expected[j]))mask|=1<<j;
    return mask==7;
}

static void CALLBACK test_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!queued)queued=ProMacroLoad(L"~ Command \x0060" L"BWTLResidentTick\x0060;")==PRO_TK_NO_ERROR;
}

static int attach_green_image(const wchar_t *directory){
    wchar_t path[MAX_PATH],error[256];
    if(swprintf_s(path,MAX_PATH,L"%ls\\image-check.bmp",directory)<0)return 0;
    BITMAPFILEHEADER file={0};
    BITMAPINFOHEADER info={0};
    DWORD row[64];
    for(int i=0;i<_countof(row);i++)row[i]=0xff00ff00;
    file.bfType=0x4d42;
    file.bfOffBits=sizeof(file)+sizeof(info);
    file.bfSize=file.bfOffBits+64*sizeof(row);
    info.biSize=sizeof(info);
    info.biWidth=info.biHeight=64;
    info.biPlanes=1;
    info.biBitCount=32;
    FILE *stream=_wfopen(path,L"wb");
    if(!stream)return 0;
    fwrite(&file,sizeof(file),1,stream);
    fwrite(&info,sizeof(info),1,stream);
    for(int i=0;i<64;i++)fwrite(row,sizeof(row),1,stream);
    if(fclose(stream))return 0;
    return native_image_import(path,error,_countof(error));
}

static int tick(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    queued=0;
    if(GetTickCount64()>deadline){resident_check(0,"TIMEOUT");return finish();}
    if(smart_active||ai_process)return 0;
    if(!phase){
        resident_check(box_matches()&&smart_progress==1&&resident_used_tool,"DYNAMIC_MODEL_READBACK");
        worker_pid=resident_process?GetProcessId(resident_process):0;
        resident_check(worker_pid&&resident_read(L"thread.txt",thread,_countof(thread))==1,"FIRST_THREAD");
        ProMdl active=NULL;
        resident_check(!ProMdlCurrentGet(&active)&&chat_task_evidence.count&&
            chat_task_evidence.model==active&&chat_task_evidence.outcomes[0].step==1,
            "CREATED_MODEL_RECEIPT_AVAILABLE_FOR_FOLLOWUP");
        if(failed)return finish();
        phase=1;
        ProPath dir,path;
        if(ProDirectoryCurrentGet(dir)||swprintf_s(path,_countof(path),L"%ls\\read-check.txt",dir)<0){
            resident_check(0,"TEXT_FIXTURE");return finish();
        }
        FILE *f=_wfopen(path,L"wb");
        if(!f){resident_check(0,"TEXT_FIXTURE");return finish();}
        fputs("BWTL-TEXT-7941",f);fclose(f);
        resident_check(attach_green_image(dir),"IMAGE_ATTACHED");
        if(failed)return finish();
        wchar_t request[600];
        swprintf_s(request,_countof(request),
            L"只读取，不修改模型。请调用 bwtl_read_file 读取文件「%ls」，复述文件里的完整编号和我刚才给长方体取的测试代号。再用 CREO_SNAPSHOT 核对长方体，并判断附图的纯色。",path);
        native_prompt_set(request);
        run_local(NULL,NULL,NULL);
        static wchar_t prompt[65000];
        resident_check(resident_read(L"prompt.txt",prompt,_countof(prompt))==1&&
            wcsstr(prompt,L"<近期对话，按时间顺序，仅作上下文>")&&
            wcsstr(prompt,L"BWTL-CHAT-6285")&&wcsstr(prompt,L"<同一 Creo 模型的近期建模任务回执")&&
            wcsstr(prompt,L"<最新用户消息>"),"FOLLOWUP_PROMPT_HAS_HISTORY_AND_VERIFIED_CREATION");
        return 0;
    }
    wchar_t same[100];
    resident_check(resident_process&&GetProcessId(resident_process)==worker_pid&&
        resident_read(L"thread.txt",same,_countof(same))==1&&!wcscmp(same,thread),"SECOND_SAME_WORKER_AND_THREAD");
    const wchar_t *answer=last_reply();
    resident_check(wcsstr(answer,L"BWTL-TEXT-7941")&&wcsstr(answer,L"BWTL-CHAT-6285"),"FILE_TOOL_AND_PREVIOUS_TURN");
    resident_check(wcsstr(answer,L"绿色")||wcsstr(answer,L"纯绿"),"IMAGE_CONTENT_READ");
    resident_check(box_matches(),"MODEL_STILL_VERIFIED");
    return finish();
}

static int test_start(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    receipt=fopen("resident-chat-native.log","w");
    if(!receipt){ProEngineerEnd();return 0;}
    if(!native_ui_open_threaded(NULL)||!SetTimer(native_backend_window,5469,1000,test_timer)){
        resident_check(0,"OPEN_PRIVATE_CHAT");return finish();
    }
    deadline=GetTickCount64()+150000;
    native_prompt_set(L"新建一个独立长方体 27×18×13 mm，无孔。测试代号 BWTL-CHAT-6285，本对话稍后会问。"
        L"请调用 bwtl_creo 工具执行 CREO_PLAN BOX 27 18 13 0，收到原生回执后用 CREO_SNAPSHOT 实际核对，再汇报。");
    run_local(NULL,NULL,NULL);
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLResidentTest",test_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLResidentTick",tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLResidentTest\x0060;");
}
