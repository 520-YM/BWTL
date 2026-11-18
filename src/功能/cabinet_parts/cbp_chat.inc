
#include "cbp_common.inc"
#include "cbp_rain_strip.inc"
#include "cbp_transition_plate.inc"
#include "cbp_roller_plate.inc"
#include "cbp_hinge_guard.inc"
#include "cbp_air_baffle.inc"

enum CabinetPartKind {CBP_RAIN,CBP_STEP,CBP_ROLLER,CBP_GUARD,CBP_BAFFLE};

static const struct {
    enum CabinetPartKind kind;
    const wchar_t *phrase, *fields;
    int leading, lengths, flange;
} cbp_commands[]={
    {CBP_RAIN,L"新建防雨折边压条",L"零件名 长度 门板折边宽 搭接宽 侧隙 板厚 回折高 内R 左缺口 右缺口",1,9,1},
    {CBP_STEP,L"新建多段过渡连接板",L"零件名 横宽 低平台深 中平台深 高平台深 板厚 内R 中平台高 总高差 低侧孔距 高侧孔距 孔径",1,11,1},
    {CBP_ROLLER,L"新建导向轮可调底板",L"零件名 X+/X-/Y+/Y- 调节向长 横向宽 板厚 槽中心距 槽总长 槽宽 刻度距 止挡高 内R",2,9,1},
    {CBP_GUARD,L"新建快拆铰链护架",L"零件名 铰链件号 X/Y/Z 拔销端余量 侧净隙 深向净隙 螺钉窗口长 螺钉窗口宽 端孔边距 安装孔径 板厚 内R",3,9,1},
    {CBP_BAFFLE,L"新建风仓可拆内隔板",L"零件名 X+/X- HOLES/SLOTS 长 宽 板厚 避让沿X 避让沿Y 孔端距 孔侧距 孔径或槽宽 槽全长 检修口宽 检修口净高",3,11,0},
};

typedef struct {
    wchar_t buffer[1600];
    const wchar_t *lead[3];
    double dimension[18];
    int index;
} CabinetPartRequest;

static int cbp_parse(const wchar_t *input,CabinetPartRequest *request){
    size_t length=0;
    request->index=-1;
    for(int i=0;i<_countof(cbp_commands);i++){
        size_t n=wcslen(cbp_commands[i].phrase);
        if(!wcsncmp(input,cbp_commands[i].phrase,n)&&(!input[n]||iswspace(input[n]))){
            request->index=i;
            length=n;
            break;
        }
    }
    if(request->index<0)return 0;
    if(wcslen(input+length)>=_countof(request->buffer))return -1;
    wcscpy_s(request->buffer,_countof(request->buffer),input+length);
    wchar_t *state=NULL;
    wchar_t *token=wcstok_s(request->buffer,L" \r\n\t",&state);
    const int count=cbp_commands[request->index].leading;
    for(int i=0;i<count;i++){
        if(!token)return -1;
        request->lead[i]=token;
        token=wcstok_s(NULL,L" \r\n\t",&state);
    }
    if(!ax_study_key(request->lead[0]))return -1;
    for(int i=0;i<cbp_commands[request->index].lengths;i++){
        if(!token||!or_number(token,&request->dimension[i]))return -1;
        token=wcstok_s(NULL,L" \r\n\t",&state);
    }
    return token?-1:1;
}

static ProError cbp_execute(const CabinetPartRequest *request,ProMdl *built,PartText *out){
    *built=NULL;
    if(!storage_prepare_output()){
        part_text_add(out,L"%ls\n",storage_output_message);
        return PRO_TK_BAD_CONTEXT;
    }
    const wchar_t *name=request->lead[0];
    const double *dim=request->dimension;
    switch(cbp_commands[request->index].kind){
    case CBP_RAIN:return cbp_rain_strip(name,dim,built,out);
    case CBP_STEP:return cbp_transition_plate(name,dim,built,out);
    case CBP_ROLLER:return cbp_roller_plate(name,request->lead[1],dim,built,out);
    case CBP_GUARD:return cbp_hinge_guard(name,request->lead[1],request->lead[2],dim,built,out);
    case CBP_BAFFLE:return cbp_air_baffle(name,request->lead[1],request->lead[2],dim,built,out);
    }
    return PRO_TK_BAD_INPUTS;
}

static int cbp_chat(const wchar_t *input){
    CabinetPartRequest request={0};
    int parsed=cbp_parse(input,&request);
    if(!parsed)return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    append_card(L"你",input);
    save_turn(L"用户",input);
    native_prompt_set(L"");

    wchar_t result[PART_TEXT_MAX]=L"";
    PartText out={result,_countof(result),0};
    if(parsed<0){
        part_text_add(&out,L"请按顺序填写「%ls %ls」。尺寸用毫米。\n",
            cbp_commands[request.index].phrase,cbp_commands[request.index].fields);
    }else{
        /* These model generators dispatch directly to the validated wall
           functions. Reuse the normal risk notice before entering them. */
        if(cbp_commands[request.index].flange){
            PartCommand wall={.verb=PART_FOUNDATION};
            wcscpy_s(wall.arg[0],PART_TOKEN_MAX,L"SMT_FLANGE");
            bwtl_warn_traceback(&wall);
        }
        native_set_busy(3);
        ProMdl built=NULL;
        ProError status=cbp_execute(&request,&built,&out);
        if(!status&&built){
            ProError displayed=dv_display_new(built);
            if(displayed)part_text_add(&out,L"零件已创建，显示窗口返回 Creo %d。\n",displayed);
        }
        if(status&&!result[0])part_text_add(&out,L"未完成（Creo %d）。请核对新零件名、参照和尺寸。\n",status);
        native_set_busy(0);
    }
    append_card(L"钣金结构",result);
    save_turn(L"钣金结构",result);
    return 1;
}
