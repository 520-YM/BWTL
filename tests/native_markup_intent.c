#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static ProMdl model;
static int features[6];
static FILE *receipt;
static ULONGLONG deadline;
static int queued,phase;


static int check_diameter(int feature,double expected){
    ProFeature f;
    MarkupDriver driver={.diameter=expected};
    return !ProFeatureInit((ProSolid)model,feature,&f)&&
        !ProFeatureDimensionVisit(&f,markup_driver,NULL,&driver)&&driver.count==1;
}

static void done(int pass){
    KillTimer(native_backend_window,9008);
    fprintf(receipt,"PASS=%d\n",pass);
    fclose(receipt);
    ProEngineerEnd();
}

static void CALLBACK test_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!queued)queued=ProMacroLoad(L"~ Command " L"\x60" L"BWTLMarkupCheck" L"\x60;")==PRO_TK_NO_ERROR;
}

static void arrow_to(const MarkupImage *image,double px,double py){
    RECT r=markup_image_rect(markup_canvas);
    int x=r.left+(int)lround(px*(r.right-r.left)/image->width);
    int y=r.top+(int)lround(py*(r.bottom-r.top)/image->height);
    SendMessageW(markup_window,WM_COMMAND,MARKUP_TOOL_ARROW,0);
    SendMessageW(markup_canvas,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x-20,y-14));
    SendMessageW(markup_canvas,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x,y));
    SendMessageW(markup_canvas,WM_LBUTTONUP,0,MAKELPARAM(x,y));
}

static int check(uiCmdCmdId id,uiCmdValue *value,void *data){
    queued=0;
    if(GetTickCount64()>deadline){fprintf(receipt,"TIMEOUT_PHASE=%d\n",phase);done(0);return 0;}
    if(phase==1&&markup_window&&markup_capture.image){
        MarkupImage *image=markup_capture.image;
        RECT full;
        HWND creo=markup_creo_window();
        int complete=creo&&GetWindowRect(creo,&full)&&image->width==full.right-full.left&&image->height==full.bottom-full.top;
        fprintf(receipt,"FULL_WINDOW=%d WIDTH=%d HEIGHT=%d HOLES=%d\n",complete,image->width,image->height,image->count);
        if(!complete||image->count!=6){done(0);return 0;}

        MoveWindow(markup_window,0,0,1210,690,TRUE);
        MoveWindow(markup_window,0,0,520,365,TRUE);
        MoveWindow(markup_window,0,0,890,620,TRUE);
        RECT canvas,child;
        GetClientRect(markup_window,&canvas);
        GetWindowRect(markup_canvas,&child);
        int resized=child.right-child.left==canvas.right-24&&child.bottom-child.top==canvas.bottom-155;
        fprintf(receipt,"RESIZED=%d\n",resized);
        if(!resized){done(0);return 0;}
        for(int i=0;i<5;i++){
            for(int j=0;j<image->count;j++)if(image->holes[j].feature==features[i])
                arrow_to(image,image->holes[j].x,image->holes[j].y);
        }
        fprintf(receipt,"MARKS=%d\n",markup_count);
        if(markup_count!=5){done(0);return 0;}
        SetWindowTextW(markup_input,L"所有标注的孔径改到8mm");
        phase=2;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==2&&!markup_waiting&&markup_retake){
        int pass=check_diameter(features[5],4);
        for(int i=0;i<5;i++)pass=pass&&check_diameter(features[i],8);
        wchar_t status[1000];
        GetWindowTextW(markup_status,status,_countof(status));
        pass=pass&&wcsstr(status,L"已修改 5 处");
        fprintf(receipt,"FIVE_WRITES_AND_STATUS=%d\n",pass);
        if(!pass){done(0);return 0;}
        phase=3;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==3&&markup_window&&markup_capture.image&&!markup_retake){
        MarkupImage *image=markup_capture.image;
        arrow_to(image,image->holes[0].x,image->holes[0].y);
        arrow_to(image,110,110);
        SetWindowTextW(markup_input,L"1号 Ø7；2号 Ø9");
        phase=4;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==4&&!markup_waiting){
        wchar_t status[1000];
        GetWindowTextW(markup_status,status,_countof(status));
        if(!wcsstr(status,L"2 号标注"))return 0;
        int pass=check_diameter(features[5],4);
        for(int i=0;i<5;i++)pass=pass&&check_diameter(features[i],8);
        fprintf(receipt,"FAILED_TARGET_VISIBLE_NO_WRITES=%d\n",pass);
        if(!pass){done(0);return 0;}
        SendMessageW(markup_window,WM_COMMAND,MARKUP_CLEAR,0);
        MarkupImage *image=markup_capture.image;
        for(int i=0;i<2;i++)for(int j=0;j<image->count;j++)if(image->holes[j].feature==features[i])
            arrow_to(image,image->holes[j].x,image->holes[j].y);
        if(markup_count!=2){done(0);return 0;}
        SetWindowTextW(markup_input,L"1号的孔径再放大2毫米，2号先保留原样。");
        phase=5;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==5&&!markup_waiting){
        wchar_t status[1000];
        GetWindowTextW(markup_status,status,_countof(status));
        if(!wcsstr(status,L"已修改 1 处")){
            if(wcsstr(status,L"已标注"))return 0;
            fprintf(receipt,"SEMANTIC_STATUS=%ls\n",status);
            done(0);
            return 0;
        }
        int pass=check_diameter(features[0],10)&&check_diameter(features[1],8)&&check_diameter(features[5],4);
        fprintf(receipt,"CODEX_RELATIVE_SELECTED_ONLY=%d\n",pass);
        if(!pass){done(0);return 0;}
        phase=6;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==6&&markup_capture.image&&markup_display&&markup_window&&!markup_retake&&markup_display->ticket==markup_capture.image->ticket){
        MarkupImage *image=markup_capture.image;
        for(int i=0;i<image->count;i++)if(image->holes[i].feature==features[0])
            arrow_to(image,image->holes[i].x,image->holes[i].y);
        int matched=markup_count==1?markup_match(image,&markup_marks[0]):-1;
        int target=matched>=0&&image->holes[matched].feature==features[0];
        fprintf(receipt,"FOLLOWUP_MARKS=%d VISIBLE=%d MATCH=%d TARGET=%d\n",markup_count,image->count,matched,target);
        if(!target){done(0);return 0;}
        SetWindowTextW(markup_input,L"这个孔再大一点，先别自行定尺寸。");
        phase=7;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==7&&!markup_waiting&&markup_previous.ticket==markup_request.ticket){
        int pass=!markup_retake&&check_diameter(features[0],10)&&check_diameter(features[1],8);
        fprintf(receipt,"CODEX_CLARIFIED_WITHOUT_WRITE=%d\n",pass);
        if(!pass){done(0);return 0;}
        SetWindowTextW(markup_input,L"12毫米");
        phase=8;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==8&&!markup_waiting&&markup_retake){
        int pass=check_diameter(features[0],12)&&check_diameter(features[1],8);
        fprintf(receipt,"CODEX_CONTEXTUAL_FOLLOWUP=%d\n",pass);
        if(!pass){done(0);return 0;}
        phase=9;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==9&&markup_capture.image&&markup_display&&markup_window&&!markup_retake&&markup_display->ticket==markup_capture.image->ticket){
        const MarkupImage *image=markup_capture.image;
        for(int i=0;i<image->count;i++)if(image->holes[i].feature==features[0])
            arrow_to(image,image->holes[i].x,image->holes[i].y);
        SetWindowTextW(markup_input,L"这个孔稍微放大一点，尺寸你帮我选一个，先给我建议。");
        phase=10;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
        if(phase==10&&!markup_waiting&&markup_display&&markup_previous.ticket==markup_display->ticket&&
       !wcscmp(markup_previous.text,L"这个孔稍微放大一点，尺寸你帮我选一个，先给我建议。")){
        int pass=markup_proposal.count==1&&markup_proposal.edits[0].value>12&&
            check_diameter(features[0],12)&&check_diameter(features[1],8);
        fprintf(receipt,"CODEX_PROPOSED_WITHOUT_WRITE=%d\n",pass);
        if(!pass){done(0);return 0;}
        SetWindowTextW(markup_input,L"按建议改");
        phase=11;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==11&&!markup_waiting&&markup_retake){
        int pass=check_diameter(features[0],markup_intent.edits[0].value)&&check_diameter(features[1],8);
        fprintf(receipt,"CONFIRMED_RECOMMENDATION=%d\n",pass);
        done(pass);
    }
    return 0;
}

static int open_markup_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    int win;
    if(!ProWindowCurrentGet(&win))ProWindowBrowserSizeSet(win,0);
    HWND owner=GetAncestor(GetActiveWindow(),GA_ROOT);
    int opened=native_ui_open_threaded(owner);
    wchar_t label[30]=L"";
    if(opened)GetWindowTextW(native_top_buttons[4],label,_countof(label));
    int menu=opened&&!wcscmp(label,L"截图改图")&&IsWindowEnabled(native_top_buttons[4]);
    fprintf(receipt,"MENU=%d\n",menu);
    if(!menu||!SetTimer(native_backend_window,9008,250,test_timer)){done(0);return 0;}
    phase=1;
    deadline=GetTickCount64()+190000;
    PostMessageW(native_window,WM_COMMAND,CHAT_RESERVED_3,0);
    return 0;
}

static int prepare_markup_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    receipt=fopen("screenshot-markup.log","w");
    if(!receipt){ProEngineerEnd();return 0;}
    setvbuf(receipt,NULL,_IONBF,0);
    Fixture stock;ProMdlName name;
    model_background_mode=1;
    ProError error=fixture(&model,name,&stock);
    wchar_t command[512],label[16];
    for(int i=0;!error&&i<6;i++){
        swprintf_s(label,_countof(label),L"H%d",i+1);
        swprintf_s(command,_countof(command),L"CREO_OP HOLE %ls %ls SURFACE:%d SURFACE:%d %d SURFACE:%d %d 4 THROUGH 0",
            name,label,stock.top,stock.x,16+24*(i%3),stock.y,16+24*(i/3));
        error=expansion_build(model,command,receipt);
        ProModelitem item;
        if(!error)error=ProModelitemByNameInit(model,PRO_FEATURE,label,&item);
        if(!error)features[i]=item.id;
    }
    if(!error)error=dv_display_new(model);
    if(!error)error=ProMacroLoad(L"~ Command " L"\x60" L"BWTLMarkupOpenTest" L"\x60;");
    if(error)done(0);
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLMarkupPrepare",prepare_markup_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLMarkupOpenTest",open_markup_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLMarkupCheck",check,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command " L"\x60" L"BWTLMarkupPrepare" L"\x60;");
}
