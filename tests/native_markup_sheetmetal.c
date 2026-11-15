#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static FILE *logfile;
static ProMdl sheet;
static ProMdlName name;
static int cut_id,opening_id,phase,queued;
static ULONGLONG deadline;

static ProError run(const wchar_t *protocol){
    PartCommand command={0};
    wchar_t message[2000]=L"";
    ProError e=part_command_parse(protocol,&command)==1?
        foundation_execute(&command,sheet,message,_countof(message)):PRO_TK_BAD_INPUTS;
    fprintf(logfile,"command=%ls code=%d\n",protocol,e);
    return e;
}
static int finish(int pass){
    if(native_backend_window)KillTimer(native_backend_window,9010);
    fprintf(logfile,"PASS=%d\n",pass);
    fclose(logfile);
    ProEngineerEnd();
    return 0;
}
static void CALLBACK opening_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!queued)queued=!ProMacroLoad(L"~ Command " L"\x60" L"BWTLMarkupOpeningChat" L"\x60;");
}
static int opening_chat(uiCmdCmdId id,uiCmdValue *v,void *data){
    queued=0;
    if(GetTickCount64()>deadline){fprintf(logfile,"CHAT_TIMEOUT=%d\n",phase);return finish(0);}
    if(phase==1&&markup_window&&markup_capture.image){
        const MarkupImage *image=markup_capture.image;
        const MarkupOpening *opening=NULL;
        for(int i=0;i<image->opening_count;i++)if(image->openings[i].feature==opening_id)opening=&image->openings[i];
        if(!opening)return finish(0);
        RECT rect=markup_image_rect(markup_canvas);
        int x=rect.left+(int)lround(opening->x*(rect.right-rect.left)/image->width);
        int y=rect.top+(int)lround(opening->y*(rect.bottom-rect.top)/image->height);
        SendMessageW(markup_window,WM_COMMAND,MARKUP_TOOL_ARROW,0);
        SendMessageW(markup_canvas,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x-20,y-14));
        SendMessageW(markup_canvas,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x,y));
        SendMessageW(markup_canvas,WM_LBUTTONUP,0,MAKELPARAM(x,y));
        if(markup_count!=1)return finish(0);
        SetWindowTextW(markup_input,L"把中间的矩形开口缩小 4 毫米。");
        phase=2;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
    }else if(phase==2&&!markup_waiting&&markup_previous.ticket==markup_request.ticket){
        ImitationLocalProfile profile={0};
        int pass=!markup_proposal.count&&!markup_retake&&!imitation_local_profile(sheet,opening_id,&profile)&&part_close(profile.area,200);
        fprintf(logfile,"AMBIGUOUS_OPENING_UNCHANGED=%d\n",pass);
        if(!pass)return finish(0);
        SetWindowTextW(markup_input,L"短边总尺寸缩小 4 毫米，长边保持原样。");
        phase=3;
        PostMessageW(markup_window,WM_COMMAND,MARKUP_SEND,0);
    }else if(phase==3&&!markup_waiting&&markup_retake){
        ImitationLocalProfile profile={0};
        int pass=!imitation_local_profile(sheet,opening_id,&profile)&&part_close(profile.area,120);
        fprintf(logfile,"CODEX_OPENING_FOLLOWUP=%d AREA=%.8g\n",pass,profile.area);
        return finish(pass);
    }
    return 0;
}
static int verify(uiCmdCmdId id,uiCmdValue *v,void *data){
    double parsed=0;
    const wchar_t *valid[]={L"孔直径改为15mm",L"把这个孔的直径改成 15 毫米",L"把孔径改为15mm",
        L"孔径15",L"直径改到15mm",L"孔改为15mm",L"Ø15"};
    const wchar_t *invalid[]={L"孔直径增加15mm",L"把孔半径改为15mm",L"孔直径改为15mm，再移动2mm",
        L"孔直径改为15cm",L"孔15",L"15",L"孔直径改为0mm"};
    int parser=1;
    for(int i=0;i<_countof(valid);i++)parser=parser&&markup_parse_diameter(valid[i],&parsed)&&part_close(parsed,15);
    for(int i=0;i<_countof(invalid);i++)parser=parser&&!markup_parse_diameter(invalid[i],&parsed);
    fprintf(logfile,"PARSER=%d\n",parser);
    if(!parser)return finish(0);
    ProError e=markup_take(&markup_capture);
    fprintf(logfile,"CAPTURE=%d\n",e);
    if(e)return finish(0);
    MarkupImage *image=markup_capture.image;
    const MarkupHole *hole=NULL;
    for(int i=0;i<image->count;i++){
        fprintf(logfile,"hole=%d dim=%d owner=%d old=%.8g x=%.2f y=%.2f\n",image->holes[i].feature,
            image->holes[i].dimension,image->holes[i].dimension_owner,image->holes[i].old_value,image->holes[i].x,image->holes[i].y);
        if(image->holes[i].feature==cut_id)hole=&image->holes[i];
    }
    if(!hole||hole->dimension<0)return finish(0);
    MarkupRequest request={.ticket=image->ticket,.count=1,.text=L"孔直径改为15mm"};
    request.marks[0]=(MarkupMark){MARKUP_ARROW,hole->x-20,hole->y-14,hole->x,hole->y,1};
    ProDimension dim;
    int dim_id=hole->dimension;
    wchar_t reply[1000]=L"";
    double value=0;
    e=markup_apply(&markup_capture,&request,reply,_countof(reply));
    fprintf(logfile,"WRITE=%d\n",e);
    if(!e)e=ProModelitemInit(sheet,dim_id,PRO_DIMENSION,&dim);
    if(!e)e=ProDimensionValueGet(&dim,&value);
    fprintf(logfile,"CUT_DIAMETER=%.8g\n",value);
    if(e||!part_close(value,15))return finish(0);
    markup_capture_clear();
    e=markup_take(&markup_capture);
    if(e)return finish(0);
    image=markup_capture.image;
    const MarkupOpening *opening=NULL;
    for(int i=0;i<image->opening_count;i++){
        const MarkupOpening *candidate=&image->openings[i];
        fprintf(logfile,"OPENING=%d DRIVERS=%d,%d SIZE=%.8g,%.8g\n",candidate->feature,
            candidate->dimension[0],candidate->dimension[1],candidate->size[0],candidate->size[1]);
        if(candidate->feature==opening_id)opening=candidate;
    }
    if(!opening||opening->dimension[0]<0||opening->dimension[1]<0||
       !part_close(opening->size[0],30)||!part_close(opening->size[1],20))return finish(0);
    markup_request=(MarkupRequest){.ticket=image->ticket,.count=1,.text=L"横向缩小 20 毫米，竖向不变"};
    markup_request.marks[0]=(MarkupMark){MARKUP_ARROW,opening->x-20,opening->y-14,opening->x,opening->y,1};
    wchar_t invented[]=L"EDIT\n1 X SET 19",typed[]=L"EDIT\n1 X SUB 20",suggested[]=L"PROPOSE\n孔径和开口建议。\n1 X SET 18";
    MarkupIntent suggestion;
    int protocol=!markup_parse_intent(invented,&markup_request,&suggestion)&&
        markup_parse_intent(suggested,&markup_request,&suggestion)&&suggestion.proposal&&
        markup_parse_intent(typed,&markup_request,&markup_intent);
    fprintf(logfile,"SEMANTIC_PROTOCOL=%d\n",protocol);
    if(!protocol)return finish(0);
    reply[0]=0;
    e=markup_execute_intent(reply,_countof(reply));
    fprintf(logfile,"OPENING_WRITE=%d\n",e);
    if(e)return finish(0);
    ImitationLocalProfile profile={0};
    e=imitation_local_profile(sheet,opening_id,&profile);
    fprintf(logfile,"OPENING_AREA=%.8g CODE=%d\n",profile.area,e);
    if(e||!part_close(profile.area,200))return finish(0);
    markup_capture_clear();
    e=markup_take(&markup_capture);
    if(e)return finish(0);
    int verified=0;
    image=markup_capture.image;
    for(int i=0;i<image->opening_count;i++){
        const MarkupOpening *candidate=&image->openings[i];
        if(candidate->feature==opening_id)
            verified=part_close(candidate->size[0],10)&&part_close(candidate->size[1],20);
    }
    fprintf(logfile,"OPENING_WIDTH_AND_UNCHANGED_HEIGHT=%d\n",verified);
    if(!verified)return finish(0);
    markup_capture_clear();
    int window;
    if(!ProWindowCurrentGet(&window))ProWindowBrowserSizeSet(window,0);
    HWND owner=GetAncestor(GetActiveWindow(),GA_ROOT);
    if(!native_ui_open_threaded(owner)||!SetTimer(native_backend_window,9010,250,opening_timer))return finish(0);
    deadline=GetTickCount64()+130000;
    phase=1;
    PostMessageW(native_window,WM_COMMAND,CHAT_RESERVED_3,0);
    return 0;
}
static int prepare(uiCmdCmdId id,uiCmdValue *v,void *data){
    logfile=fopen("sheetmetal-markup.log","w");
    if(!logfile)return ProEngineerEnd(),0;
    setvbuf(logfile,NULL,_IONBF,0);
    const char *stage="empty";
    model_background_mode=1;
    ProError e=model_create_empty(2,name,&stage);
    if(!e)e=ProMdlnameInit(name,PRO_MDL_PART,&sheet);
    wchar_t cmd[512];
    if(!e){
        swprintf_s(cmd,_countof(cmd),L"CREO_OP SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",name);
        e=run(cmd);
    }
    if(!e){
        swprintf_s(cmd,_countof(cmd),L"CREO_OP SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",name);
        e=run(cmd);
    }
    if(!e){
        swprintf_s(cmd,_countof(cmd),L"CREO_OP SMT_SKETCH %ls CUT_PROFILE PLANE:FRONT PLANE:RIGHT CIRCLE 20 20 5",name);
        e=run(cmd);
    }
    if(!e){
        swprintf_s(cmd,_countof(cmd),L"CREO_OP SMT_CUT %ls CUT_A SKETCH:CUT_PROFILE NEG",name);
        e=run(cmd);
    }
    ProModelitem item;
    if(!e)e=ProModelitemByNameInit(sheet,PRO_FEATURE,L"CUT_A",&item);
    if(!e)cut_id=item.id;
    if(!e){
        swprintf_s(cmd,_countof(cmd),L"CREO_OP SMT_SKETCH %ls OPEN_PROFILE PLANE:FRONT PLANE:RIGHT RECT 37 24 30 20",name);
        e=run(cmd);
    }
    if(!e){
        swprintf_s(cmd,_countof(cmd),L"CREO_OP SMT_CUT %ls OPEN_A SKETCH:OPEN_PROFILE NEG",name);
        e=run(cmd);
    }
    if(!e)e=ProModelitemByNameInit(sheet,PRO_FEATURE,L"OPEN_A",&item);
    if(!e)opening_id=item.id;
    if(!e)e=dv_display_new(sheet);
    if(!e)e=ProMacroLoad(L"~ Command " L"\x60" L"BWTLMarkupSheetVerify" L"\x60;");
    if(e)return finish(0);
    return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId id;
    ProError e=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!e)e=ProCmdActionAdd("BWTLMarkupSheetPrepare",prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!e)e=ProCmdActionAdd("BWTLMarkupSheetVerify",verify,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!e)e=ProCmdActionAdd("BWTLMarkupOpeningChat",opening_chat,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return e?e:ProMacroLoad(L"~ Command " L"\x60" L"BWTLMarkupSheetPrepare" L"\x60;");
}
