typedef struct {double length,width,thickness;} ShPlateSize;

static const wchar_t *sh_plate_spacing(const wchar_t *text){
    while(*text==L' '||*text==L'\t'||*text==L'　')text++;
    return text;
}

static int sh_plate_take(const wchar_t **text,const wchar_t *word){
    const wchar_t *start=sh_plate_spacing(*text);
    size_t length=wcslen(word);
    if(wcsncmp(start,word,length))return 0;
    *text=sh_plate_spacing(start+length);
    return 1;
}

static int sh_plate_take_one(const wchar_t **text,const wchar_t *const *words,size_t count){
    for(size_t i=0;i<count;i++)if(sh_plate_take(text,words[i]))return 1;
    return 0;
}

static int sh_plate_size(const wchar_t **text,ShPlateSize *size){
    double dimensions[3];
    for(int i=0;i<3;i++){
        const wchar_t *begin=sh_plate_spacing(*text),*end=begin;
        while(*end>=L'0'&&*end<=L'9')end++;
        if(*end==L'.'){
            end++;
            while(*end>=L'0'&&*end<=L'9')end++;
        }
        if(end==begin||end-begin>=24)return 0;
        wchar_t number[24];
        wcsncpy_s(number,_countof(number),begin,(size_t)(end-begin));
        if(!part_number(number,&dimensions[i])||dimensions[i]<0.01||
           dimensions[i]>(i==2?1000:1000000))return 0;
        *text=sh_plate_spacing(end);
        if(_wcsnicmp(*text,L"mm",2)==0)*text=sh_plate_spacing(*text+2);
        else sh_plate_take(text,L"毫米");
        if(i<2){
            if(!**text||!wcschr(L"×*xXｘＸ＊",**text))return 0;
            (*text)++;
        }
    }
    *size=(ShPlateSize){dimensions[0],dimensions[1],dimensions[2]};
    return 1;
}

/* Take over only simple standalone plates with three complete dimensions.
   Other shapes, references, questions and constraints go to normal planning. */
static int sh_plate_request(const wchar_t *text,ShPlateSize *size){
    static const wchar_t *const verbs[]={L"新建",L"另建",L"创建",L"建立",L"做",L"画"};
    static const wchar_t *const units[]={L"一个",L"一块",L"一件",L"一张",L"个",L"块",L"张"};
    static const wchar_t *const types[]={L"矩形钣金平板",L"钣金矩形平板",L"钣金矩形板",L"矩形钣金板",
        L"钣金平板",L"钣金平面",L"钣金板"};
    text=model_request_action_start(text);
    if(!sh_plate_take_one(&text,verbs,_countof(verbs)))return 0;
    sh_plate_take_one(&text,units,_countof(units));
    sh_plate_take(&text,L"独立");
    if(sh_plate_take_one(&text,types,_countof(types))){
        if(*text&&wcschr(L"，,：:",*text))text=sh_plate_spacing(text+1);
        if(sh_plate_take(&text,L"尺寸")&&*text&&wcschr(L"为：:",*text))
            text=sh_plate_spacing(text+1);
        if(!sh_plate_size(&text,size))return 0;
    }else{
        if(!sh_plate_size(&text,size))return 0;
        sh_plate_take(&text,L"的");
        sh_plate_take(&text,L"独立");
        if(!sh_plate_take_one(&text,types,_countof(types)))return 0;
    }
    if(*text&&wcschr(L"，,。；;",*text)){
        const wchar_t *after=sh_plate_spacing(text+1);
        static const wchar_t *const protections[]={L"别动当前装配",L"不要动当前装配",L"保持当前装配不变"};
        if(sh_plate_take_one(&after,protections,_countof(protections)))text=after;
    }
    while(*text&&wcschr(L"。！!",*text))text=sh_plate_spacing(text+1);
    return !*text;
}

static ProError sh_plate_verify(const CabinetBlank *blank,const ProVector long_axis,const ShPlateSize *size){
    double mm,thickness;
    ProMassProperty mass;
    ProError error=fab_thickness(blank->model,&mm,&thickness);
    if(!error)error=qa_geometry_mass(blank->model,&mass);
    if(error)return error;
    if(!part_close(thickness,size->thickness)||
       !part_close(mass.volume*mm*mm*mm,size->length*size->width*size->thickness))
        return PRO_TK_GENERAL_ERROR;
    int along=0,cross=0;
    for(int i=0;i<blank->count;i++){
        double alignment=fabs(ad_dot(long_axis,blank->edges[i].axis));
        double extent=blank->edges[i].length*mm;
        if(part_close(alignment,1)&&part_close(extent,size->length))along++;
        else if(part_close(alignment,0)&&part_close(extent,size->width))cross++;
        else return PRO_TK_GENERAL_ERROR;
    }
    return along==2&&cross==2?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError sh_plate_create(const ShPlateSize *size,ProMdlName name,const wchar_t **stage){
    FdTxn stock={0};
    CabinetBlank blank={0};
    ProVector long_axis;
    PartCommand outline={0};
    int background=model_background_mode;
    const char *template_stage="Create independent empty sheetmetal";
    ProError error;
    model_background_mode=1;
    error=model_create_empty(2,name,&template_stage);
    model_background_mode=background;
    if(error){trace(template_stage,error);return error;}
    *stage=L"读取独立钣金件";
    error=ProMdlnameInit(name,PRO_MDL_PART,&stock.model);
    if(error)return error;
    wcscpy_s(stock.name,_countof(stock.name),L"BWTL_PLATE");
    wcscpy_s(outline.arg[3],_countof(outline.arg[3]),L"PLANE:FRONT");
    wcscpy_s(outline.arg[4],_countof(outline.arg[4]),L"PLANE:RIGHT");
#define PLATE_TRY(label,call) do{*stage=label;error=(call);if(error)goto done;}while(0)
    PLATE_TRY(L"创建矩形草绘和首壁",cm_blank_make(&stock,&outline,size->length,size->width,size->thickness,&blank,long_axis));
    PLATE_TRY(L"回读长、宽、板厚和体积",sh_plate_verify(&blank,long_axis,size));
    if(!background)PLATE_TRY(L"打开新钣金件窗口",dv_display_new(stock.model));
done:
    if(error){
        if(fd_last_sketch_model==stock.model){fd_last_sketch_model=NULL;fd_last_sketch[0]=0;}
        trace("Erase incomplete independent sheet plate",ProMdlErase(stock.model));
    }
    return error;
#undef PLATE_TRY
}

static int sh_plate_chat(const wchar_t *request){
    ShPlateSize size;
    if(!sh_plate_request(request,&size))return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    smart_begin(request);
    smart_local_plan=1;
    append_card(L"你",request);
    save_turn(L"用户",request);
    native_prompt_set(L"");
    if(!storage_prepare_output()){smart_reply(storage_output_message);return 1;}
    ProMdlName name=L"";
    const wchar_t *stage=L"创建独立钣金件";
    wchar_t receipt[512];
    ProError error=sh_plate_create(&size,name,&stage);
    if(error)swprintf_s(receipt,_countof(receipt),L"独立钣金平板未完成，失败步骤为「%ls」（Creo %d）。",stage,error);
    else swprintf_s(receipt,_countof(receipt),L"已新建独立钣金平板 %ls.prt，%.10g × %.10g × %.10g mm。已回读长、宽和板厚，模型含可编辑的矩形草绘与原生首壁，板件几何待保存。",name,size.length,size.width,size.thickness);
    smart_step(NULL,L"新建独立钣金平板",error,receipt,!error,1,sh_recovery_uncertain);
    return 1;
}
