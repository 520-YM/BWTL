static int markup_values(const MarkupRequest *request,double *values){
    if(request->count<1||request->count>MARKUP_MAX_MARKS)return 0;
    double mm;
    const wchar_t *text=markup_skip_space(request->text);
    const wchar_t *const all[]={L"所有标注的",L"全部标注的"};
    if((request->count==1||markup_take_phrase(&text,all,_countof(all)))&&markup_parse_diameter(text,&mm)){
        for(int i=0;i<request->count;i++)values[i]=mm;
        return 1;
    }
    wchar_t lines[256];
    wcscpy_s(lines,_countof(lines),request->text);
    wchar_t *context=NULL;
    unsigned seen=0;
    for(wchar_t *line=wcstok_s(lines,L"；;，,\r\n",&context);line;line=wcstok_s(NULL,L"；;，,\r\n",&context)){
        while(iswspace(*line))line++;
        int index=*line++-L'1';
        if(index<0||index>=request->count||*line++!=L'号'||(seen&(1u<<index)))return 0;
        if(!markup_parse_diameter(line,&values[index]))return 0;
        seen|=1u<<index;
    }
    return seen==(1u<<request->count)-1;
}

typedef enum { MARKUP_DIAMETER, MARKUP_WIDTH, MARKUP_HEIGHT } MarkupMeasure;
typedef struct { int mark; MarkupMeasure measure; double value; } MarkupEdit;

static ProError markup_apply_edits(const MarkupCapture *capture,const MarkupRequest *request,
    const MarkupEdit *edits,int count,wchar_t *reply,size_t capacity){
    if(count<1||count>MARKUP_MAX_EDITS)return PRO_TK_BAD_INPUTS;
    if(!capture->image||request->ticket!=capture->ticket||markup_check_capture(capture)){
        wcscpy_s(reply,capacity,L"模型、版本或工作目录已变化。请重新截图。");
        return PRO_TK_BAD_CONTEXT;
    }
    if(!storage_directory_on_drive(capture->directory,storage_data_drive())){
        wcscpy_s(reply,capacity,storage_output_message);
        return PRO_TK_CANT_WRITE;
    }
    int driver[MARKUP_MAX_EDITS],owner[MARKUP_MAX_EDITS],feature[MARKUP_MAX_EDITS];
    double expected[MARKUP_MAX_EDITS];
    for(int i=0;i<count;i++){
        const MarkupEdit *edit=&edits[i];
        if(edit->mark<0||edit->mark>=request->count||edit->measure<MARKUP_DIAMETER||edit->measure>MARKUP_HEIGHT||
           !isfinite(edit->value)||edit->value<0.1||edit->value>10000)return PRO_TK_BAD_INPUTS;
        const MarkupMark *mark=&request->marks[edit->mark];
        int index=edit->measure==MARKUP_DIAMETER?markup_match(capture->image,mark):markup_match_opening(capture->image,mark);
        if(index<0){
            swprintf_s(reply,capacity,L"%d 号标注%ls。请缩小标注范围或调整视角，本次未改图。",
                edit->mark+1,index==-2?L"包含多个候选":L"未定位到清楚可见的目标");
            return PRO_TK_BAD_INPUTS;
        }
        if(edit->measure==MARKUP_DIAMETER){
            const MarkupHole *hole=&capture->image->holes[index];
            driver[i]=hole->dimension;owner[i]=hole->dimension_owner;feature[i]=hole->feature;expected[i]=hole->old_value;
        }else{
            const MarkupOpening *opening=&capture->image->openings[index];
            int axis=edit->measure-MARKUP_WIDTH;
            driver[i]=opening->dimension[axis];owner[i]=opening->owner;feature[i]=opening->feature;expected[i]=opening->size[axis];
        }
        if(driver[i]<0){
            swprintf_s(reply,capacity,L"%d 号目标没有可独立修改的驱动尺寸。本次未改图。",edit->mark+1);
            return PRO_TK_UNSUPPORTED;
        }
        for(int j=0;j<i;j++)if(driver[j]==driver[i]){
            swprintf_s(reply,capacity,L"%d 号与 %d 号对应同一个驱动尺寸。本次未改图。",edits[j].mark+1,edit->mark+1);
            return PRO_TK_BAD_INPUTS;
        }
    }
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    if(!snapshot)return PRO_TK_OUT_OF_MEMORY;
    ProError error=part_snapshot_take(capture->model,snapshot);
    if(!error&&(!snapshot->stamp||!ProVerstampEqual(snapshot->stamp,capture->stamp)||
       snapshot->window_model!=capture->model||snapshot->in_assembly||snapshot->unhealthy||
       !isfinite(snapshot->mm_per_native)||snapshot->mm_per_native<=0))error=PRO_TK_BAD_CONTEXT;
    ProDimension dimensions[MARKUP_MAX_EDITS];
    double old[MARKUP_MAX_EDITS],next[MARKUP_MAX_EDITS],mm_per_value[MARKUP_MAX_EDITS];
    for(int i=0;!error&&i<count;i++){
        const PartDimension *dimension=NULL;
        for(int j=0;j<snapshot->dimensions;j++){
            const PartDimension *candidate=&snapshot->dimension[j];
            int type_matches=edits[i].measure==MARKUP_DIAMETER?
                candidate->type==PRODIMTYPE_DIAMETER||candidate->type==PRODIMTYPE_RADIUS:candidate->type==PRODIMTYPE_LINEAR;
            if(candidate->handle.id==driver[i]&&candidate->feature==owner[i]&&type_matches&&
               part_close(candidate->value,expected[i])&&!candidate->related&&!candidate->negative){
                dimension=candidate;
                break;
            }
        }
        if(!dimension){
            error=PRO_TK_BAD_CONTEXT;
            break;
        }
        dimensions[i]=dimension->handle;
        old[i]=dimension->value;
        mm_per_value[i]=snapshot->mm_per_native*(dimension->type==PRODIMTYPE_RADIUS?2:1);
        next[i]=edits[i].value/mm_per_value[i];
    }
    if(!error)error=part_write_verified_dimensions(capture->model,snapshot,dimensions,old,next,count,reply,capacity);
    if(!error){
        int diameters=1;
        for(int i=0;i<count;i++)diameters&=edits[i].measure==MARKUP_DIAMETER;
        swprintf_s(reply,capacity,diameters?L"已修改 %d 处孔径。Creo 再生及读回通过，未保存。":
            L"已修改 %d 个尺寸。Creo 再生及读回通过，未保存。",count);
        for(int i=0;i<count;i++){
            const wchar_t *labels[]={L"孔径 Ø",L"草绘 X 向开口 ",L"草绘 Y 向开口 "};
            wchar_t item[85];
            double factor=mm_per_value[i];
            swprintf_s(item,_countof(item),L"\n%d 号 · 特征 %d · %ls%.8g → %.8g mm",
                edits[i].mark+1,feature[i],labels[edits[i].measure],old[i]*factor,next[i]*factor);
            if(wcslen(reply)+wcslen(item)<capacity)wcscat_s(reply,capacity,item);
        }
    }else if(!reply[0])wcscpy_s(reply,capacity,L"未执行修改。请重新截图，并核对零件再生状态及驱动尺寸。");
    part_snapshot_clear(snapshot);
    free(snapshot);
    return error;
}

static ProError markup_apply(const MarkupCapture *capture,const MarkupRequest *request,wchar_t *reply,size_t capacity){
    double requested[MARKUP_MAX_MARKS];
    MarkupEdit edits[MARKUP_MAX_MARKS];
    if(!markup_values(request,requested)){
        wcscpy_s(reply,capacity,L"请输入要改的孔径，或说明希望怎样调整。");
        return PRO_TK_BAD_INPUTS;
    }
    for(int i=0;i<request->count;i++)edits[i]=(MarkupEdit){i,MARKUP_DIAMETER,requested[i]};
    return markup_apply_edits(capture,request,edits,request->count,reply,capacity);
}

static void markup_deliver(unsigned long ticket,int success,int retake,const wchar_t *text){
    MarkupResult *result=malloc(sizeof(*result));
    if(result){
        result->ticket=ticket;
        result->success=success;
        result->retake=retake;
        size_t used=0;
        for(const wchar_t *p=text;*p&&used+2<_countof(result->text);p++){
            if(*p==L'\n'&&(p==text||p[-1]!=L'\r'))result->text[used++]=L'\r';
            result->text[used++]=*p;
        }
        result->text[used]=0;
        if(native_window&&PostMessageW(native_window,MARKUP_RESULT,0,(LPARAM)result))return;
        free(result);
    }
}

#include "markup_intent.inc"

static int markup_native_command(uiCmdCmdId id,uiCmdValue *value,void *data){
    int action=markup_pending;
    markup_pending=0;
    if(!native_window||!action)return 0;
    if(smart_active||ai_process||native_busy){
        if(action!=1)markup_deliver(markup_request.ticket,0,0,L"当前 BWTL 有任务正在运行。完成后可重试。");
        else markup_deliver(0,0,0,L"当前 BWTL 有任务正在运行。完成后可重新截图。");
        return 0;
    }
    if(action==1){
        markup_capture_clear();
        ProError error=markup_take(&markup_capture);
        if(error){
            markup_capture_clear();
            const wchar_t *message=error==PRO_TK_INVALID_TYPE?L"请先激活一个 Creo 零件。":
                L"无法截取当前 Creo 窗口。请确认窗口已展开，并检查 所选数据盘的目录。";
            append_card(L"截图改图",message);
            markup_deliver(0,0,0,message);
            trace("Screenshot capture failed",error);
        }else{
            MarkupImage *copy=malloc(sizeof(*copy));
            if(copy){
                *copy=*markup_capture.image;
                copy->bitmap=(HBITMAP)CopyImage(markup_capture.image->bitmap,IMAGE_BITMAP,0,0,LR_CREATEDIBSECTION);
            }
            if(!copy||!copy->bitmap||!PostMessageW(native_window,MARKUP_READY,0,(LPARAM)copy)){
                markup_image_free(copy);
                markup_capture_clear();
                append_card(L"截图改图",L"无法打开截图标注窗口。");
                markup_deliver(0,0,0,L"无法打开截图标注窗口。");
            }
        }
    }else{
        wchar_t reply[1000]=L"";
        ProError error;
        if(action==3){
            if(markup_intent.proposal){markup_show_proposal();return 0;}
            error=markup_execute_intent(reply,_countof(reply));
        }else{
            if(!markup_capture.image||markup_request.ticket!=markup_capture.ticket||markup_check_capture(&markup_capture)){
                markup_finish_request(0,1,L"模型、版本或工作目录已变化。请重新截图。");
                return 0;
            }
            int confirmed=markup_confirm_proposal(&markup_request);
            if(confirmed)markup_intent=markup_proposal;
            markup_proposal=(MarkupIntent){0};
            double explicit_values[MARKUP_MAX_MARKS];
            int opening=0;
            for(int i=0;i<markup_request.count&&!opening;i++)
                opening=markup_match_opening(markup_capture.image,&markup_request.marks[i])!=-1;
            if(confirmed){
                error=markup_execute_intent(reply,_countof(reply));
            }else if(!markup_values(&markup_request,explicit_values)||
                     (opening&&!markup_named_hole(markup_request.text))){
                if(!markup_start_intent())markup_finish_request(0,0,
                    L"无法启动 Codex 分析。本次未修改模型，可以保留标注重试。");
                return 0;
            }else error=markup_apply(&markup_capture,&markup_request,reply,_countof(reply));
        }
        trace("Apply screenshot annotation",error);
        if(!reply[0])wcscpy_s(reply,_countof(reply),L"修改未完成。请重试。");
        markup_finish_request(!error,error==PRO_TK_BAD_CONTEXT,reply);
    }
    return 0;
}

static void markup_queue(int action,MarkupRequest *request){
    if(!markup_pending&&!markup_ai.process&&!smart_active&&!ai_process&&!native_busy){
        if(request)markup_request=*request;
        markup_pending=action;
        if(!ProMacroLoad(L"~ Command " L"\x60" L"BWTLScreenshotMarkup" L"\x60;")){
            free(request);
            return;
        }
        markup_pending=0;
    }
    markup_deliver(request?request->ticket:0,0,0,L"当前无法提交到 Creo，请等待正在运行的任务结束后重试。");
    free(request);
}
