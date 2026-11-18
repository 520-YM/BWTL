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

static void mark_at(const MarkupImage *image,double px,double py,int tool){
    RECT r=markup_image_rect(markup_canvas);
    int x=r.left+(int)lround(px*(r.right-r.left)/image->width);
    int y=r.top+(int)lround(py*(r.bottom-r.top)/image->height);
    int end_x=tool==MARKUP_TOOL_ARROW?x:x+12;
    int end_y=tool==MARKUP_TOOL_ARROW?y:y+12;
    SendMessageW(markup_window,WM_COMMAND,tool,0);
    SendMessageW(markup_canvas,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x-12,y-12));
    SendMessageW(markup_canvas,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(end_x,end_y));
    SendMessageW(markup_canvas,WM_LBUTTONUP,0,MAKELPARAM(end_x,end_y));
}

static void export_bitmap(const MarkupImage *image){
    DIBSECTION dib;
    if(GetObjectW(image->bitmap,sizeof(dib),&dib)!=sizeof(dib)||!dib.dsBm.bmBits)return;
    FILE *out=fopen("creo-full-window.bmp","wb");
    if(!out)return;
    DWORD bytes=dib.dsBm.bmWidthBytes*image->height;
    BITMAPFILEHEADER header={0x4d42,sizeof(header)+sizeof(dib.dsBmih)+bytes,0,0,sizeof(header)+sizeof(dib.dsBmih)};
    fwrite(&header,sizeof(header),1,out);
    fwrite(&dib.dsBmih,sizeof(dib.dsBmih),1,out);
    fwrite(dib.dsBm.bmBits,bytes,1,out);
    fclose(out);
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
        export_bitmap(image);
        const SIZE sizes[]={{1210,690},{520,365},{890,620}};
        int resized=1,grouped=1;
        for(int i=0;i<_countof(sizes);i++){
            if(!MoveWindow(markup_window,0,0,MulDiv(sizes[i].cx,native_dpi,96),MulDiv(sizes[i].cy,native_dpi,96),TRUE)){
                done(0);return 0;
            }
            RECT client,image_rect,tools,management,submit,circle,box,undo,send,input;
            GetClientRect(markup_window,&client);
            GetWindowRect(markup_canvas,&image_rect);
            int padding=MulDiv(12,native_dpi,96);
            resized=resized&&image_rect.right-image_rect.left==client.right-2*padding&&
                image_rect.bottom-image_rect.top==client.bottom-MulDiv(175,native_dpi,96);
            grouped=grouped&&GetWindowRect(GetDlgItem(markup_window,MARKUP_TOOLS_LABEL),&tools)&&
                GetWindowRect(GetDlgItem(markup_window,MARKUP_MANAGEMENT_LABEL),&management)&&
                GetWindowRect(GetDlgItem(markup_window,MARKUP_SUBMIT_LABEL),&submit)&&
                GetWindowRect(markup_tools[0],&circle)&&GetWindowRect(markup_tools[2],&box)&&
                GetWindowRect(GetDlgItem(markup_window,MARKUP_UNDO),&undo)&&
                GetWindowRect(markup_send,&send)&&GetWindowRect(markup_input,&input)&&
                tools.bottom<=circle.top&&tools.left==circle.left&&
                management.bottom<=undo.top&&management.left==undo.left&&box.right<undo.left&&
                undo.bottom<image_rect.top&&image_rect.bottom<input.top&&input.bottom<submit.top&&
                submit.bottom<=send.top&&submit.left==send.left;
        }
        fprintf(receipt,"RESIZED=%d GROUPED=%d\n",resized,grouped);
        if(!resized||!grouped){done(0);return 0;}
        mark_at(image,image->holes[0].x,image->holes[0].y,MARKUP_TOOL_CIRCLE);
        SetWindowTextW(markup_input,L"这个孔改 Ø5");
        wchar_t feedback[256];
        GetWindowTextW(markup_status,feedback,_countof(feedback));
        int circle=markup_count==1&&wcsstr(feedback,L"Ø4 → Ø5 mm")&&check_diameter(image->holes[0].feature,4);
        SendMessageW(markup_window,WM_COMMAND,MARKUP_UNDO,0);
        mark_at(image,image->holes[0].x,image->holes[0].y,MARKUP_TOOL_BOX);
        GetWindowTextW(markup_status,feedback,_countof(feedback));
        int box=markup_count==1&&wcsstr(feedback,L"Ø4 → Ø5 mm");
        SendMessageW(markup_window,WM_COMMAND,MARKUP_CLEAR,0);
        fprintf(receipt,"CIRCLE_PREVIEW=%d BOX_PREVIEW=%d CLEAR=%d\n",circle,box,markup_count==0);
        if(!circle||!box||markup_count){done(0);return 0;}
        for(int i=0;i<5;i++){
            for(int j=0;j<image->count;j++)if(image->holes[j].feature==features[i])
                mark_at(image,image->holes[j].x,image->holes[j].y,MARKUP_TOOL_ARROW);
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
        pass=pass&&wcsstr(status,L"已修改 5 处")&&wcsstr(status,L"Ø4 → 8 mm");
        fprintf(receipt,"FIVE_WRITES_AND_STATUS=%d\n",pass);
        if(!pass){done(0);return 0;}
        phase=3;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        return 0;
    }
    if(phase==3&&markup_window&&markup_display&&markup_capture.image&&!markup_retake&&markup_display->ticket==markup_capture.image->ticket){
        MarkupImage *image=markup_capture.image;
        mark_at(image,image->holes[0].x,image->holes[0].y,MARKUP_TOOL_ARROW);
        mark_at(image,110,110,MARKUP_TOOL_ARROW);
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
        for(int i=0;i<image->count;i++)if(image->holes[i].feature==features[5])
            mark_at(image,image->holes[i].x,image->holes[i].y,MARKUP_TOOL_CIRCLE);
        SetWindowTextW(markup_input,L"这个孔改 Ø5");
        GetWindowTextW(markup_status,status,_countof(status));
        int located=markup_count==1&&wcsstr(status,L"Ø4 → Ø5 mm")!=NULL;
        fprintf(receipt,"SINGLE_CIRCLE_TARGET=%d\n",located);
        if(!located){done(0);return 0;}
        phase=5;
        SendMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
        if(!markup_waiting){fprintf(receipt,"SINGLE_CIRCLE_SUBMITTED=0\n");done(0);}
        return 0;
    }
    if(phase==5&&!markup_waiting){
        wchar_t status[1000];
        GetWindowTextW(markup_status,status,_countof(status));
        if(wcsstr(status,L"正在处理你的要求"))return 0;
        int pass=markup_retake&&check_diameter(features[5],5)&&wcsstr(status,L"Ø4 → 5 mm");
        for(int i=0;i<5;i++)pass=pass&&check_diameter(features[i],8);
        fprintf(receipt,"SINGLE_CIRCLE_EDIT_AND_READBACK=%d\n",pass);
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
    deadline=GetTickCount64()+30000;
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