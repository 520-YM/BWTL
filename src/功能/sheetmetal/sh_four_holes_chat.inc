static void sh_four_chat_separators(const wchar_t **text){
    while(**text&&(iswspace(**text)||wcschr(L"，,：:；;",**text)))(*text)++;
}

static int sh_four_chat_number(const wchar_t **text,double *value){
    sh_four_chat_separators(text);
    const wchar_t *start=*text,*end=start+wcsspn(start,L"0123456789.");
    wchar_t token[24];
    if(end==start||end-start>=_countof(token)||wcsncpy_s(token,_countof(token),start,(size_t)(end-start))||
       !part_number(token,value)||*value<0.1||*value>10000)return 0;
    *text=sh_plate_spacing(end);
    if(!_wcsnicmp(*text,L"mm",2))*text=sh_plate_spacing(*text+2);
    else sh_plate_take(text,L"毫米");
    return 1;
}

static int sh_four_chat_request(const wchar_t *text,double *diameter,double *pitch_x,double *pitch_y){
    static const wchar_t *const plates[]={L"在当前钣金平板",L"在这块钣金平板",L"在当前钣金板"};
    static const wchar_t *const actions[]={L"打4个通孔",L"打四个通孔",L"开4个通孔",L"开四个通孔"};
    text=model_request_action_start(text);
    if(sh_plate_take(&text,L"平板居中四孔")){
        if(!sh_four_chat_number(&text,diameter)||!sh_four_chat_number(&text,pitch_x)||
           !sh_four_chat_number(&text,pitch_y))return 0;
    }else{
        if(!sh_plate_take_one(&text,plates,_countof(plates)))return 0;
        sh_plate_take(&text,L"上");
        if(!sh_plate_take(&text,L"居中")||!sh_plate_take_one(&text,actions,_countof(actions)))return 0;
        sh_four_chat_separators(&text);
        if(!sh_plate_take(&text,L"孔径"))return 0;
        sh_plate_take(&text,L"为");
        if(!sh_four_chat_number(&text,diameter))return 0;
        sh_four_chat_separators(&text);
        if(sh_plate_take(&text,L"横纵孔距各")||sh_plate_take(&text,L"横纵孔距均为")){
            if(!sh_four_chat_number(&text,pitch_x))return 0;
            *pitch_y=*pitch_x;
        }else{
            if(!sh_plate_take(&text,L"孔距")||!sh_four_chat_number(&text,pitch_x)||
               !*text||!wcschr(L"×*xX",*text))return 0;
            text++;
            if(!sh_four_chat_number(&text,pitch_y))return 0;
        }
    }
    while(*text&&(iswspace(*text)||wcschr(L"。！!",*text)))text++;
    return !*text;
}

static int sh_four_holes_chat(const wchar_t *text){
    double diameter,pitch_x,pitch_y;
    if(!sh_four_chat_request(text,&diameter,&pitch_x,&pitch_y))return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    smart_begin(text);
    smart_local_plan=1;
    smart_intent=SMART_INTENT_CURRENT;
    smart_readonly=0;
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    if(!storage_prepare_output()){smart_reply(storage_output_message);return 1;}
    ProMdl model=NULL;
    ProMdlType type;
    ProMdlsubtype subtype;
    if(ProMdlCurrentGet(&model)||!model||ProMdlTypeGet(model,&type)||type!=PRO_MDL_PART||
       ProMdlSubtypeGet(model,&subtype)||subtype!=PROMDLSTYPE_PART_SHEETMETAL){
        smart_reply(L"请激活要加工的钣金平板窗口，我会在这块平板上打四个贯穿孔。");
        return 1;
    }
    part_snapshot_clear(&request_snapshot);
    if(part_snapshot_take_mode(model,&request_snapshot,0)){
        smart_reply(L"Creo 未能读取当前平板，尚未加工。");
        return 1;
    }
    wchar_t protocol[400];
    if(swprintf_s(protocol,_countof(protocol),L"CREO_OP SMT_FLAT4 %ls %.12g %.12g %.12g",
                   request_snapshot.name,diameter,pitch_x,pitch_y)<0||
       part_command_parse(protocol,&pending_part)!=1){
        smart_reply(L"请核对孔径和横纵孔距，单位为毫米。");
        return 1;
    }
    smart_target_model=model;
    execute_part_command();
    return 1;
}
