
static void or_help(PartText *out){
    part_text_add(out,L"订单改型 001–010\n"
        L"先发送「提取订单规格 订单号 \"订单文本.txt\"」。输入使用字段=内容，支持外宽、外高、外深、屏尺寸、板厚、屏型号、安装方式、散热方案、开门方向和交付格式。\n"
        L"推荐订单母型 订单号 \"机型.tsv\"\n"
        L"核对订单规格映射 订单号 \"尺寸映射.tsv\"\n"
        L"记录改型保留项 订单号 \"测量项.tsv\" / 核对改型保留项 订单号\n"
        L"核对尺寸冲突 \"尺寸证据.tsv\"\n"
        L"试算参数边界 驱动尺寸名 最小毫米 最大毫米\n"
        L"拆分改型任务 订单号 \"任务.tsv\"\n"
        L"记录改型尺寸 订单号 \"测量项.tsv\" / 比较改型尺寸 订单号\n"
        L"生成改型闭环单 订单号 \"验收.tsv\"\n"
        L"绑定订单工作目录 订单号 / 核对订单工作目录 订单号\n"
        L"支持 BOX:X/Y/Z、DIM:d12、DIAM:SURFACE:面号、GAP:PLANE:基准1,PLANE:基准2；组件可在前面加 ID:路径 和竖线。\n"
        L"TSV 列顺序见本版本《订单改型001-010.md》。\n");
}

static int or_chat(const wchar_t *request){
    static const wchar_t *const names[]={
        L"订单改型",L"推荐订单母型",L"提取订单规格",L"核对订单规格映射",L"记录改型保留项",
        L"核对改型保留项",L"核对尺寸冲突",L"试算参数边界",L"拆分改型任务",L"记录改型尺寸",
        L"比较改型尺寸",L"生成改型闭环单",L"绑定订单工作目录",L"核对订单工作目录"
    };
    int action=-1;
    size_t prefix=0;
    for(int i=0;i<_countof(names);i++){
        size_t n=wcslen(names[i]);
        if(!wcsncmp(request,names[i],n)&&(!request[n]||iswspace(request[n]))){action=i;prefix=n;break;}
    }
    if(action<0)return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    wchar_t fields[9][MAX_PATH]={0},reply[PART_TEXT_MAX]=L"";
    PartText out={reply,_countof(reply),0};
    int n=ax_study_words(request+prefix,fields);
    ProMdl model=NULL;
    ProMdlCurrentGet(&model);
    ProError error=PRO_TK_BAD_INPUTS;
    native_set_busy(3);
    if(action==0){or_help(&out);error=PRO_TK_NO_ERROR;}
    else if(action==1&&n==2)error=or_templates(fields[0],fields[1],&out);
    else if(action==2&&n==2)error=or_import(fields[0],fields[1],&out);
    else if(action==3&&n==2)error=or_mapping(model,fields[0],fields[1],&out);
    else if((action==4||action==9)&&n==2)error=or_capture(model,fields[0],fields[1],action==4,&out);
    else if((action==5||action==10)&&n==1)error=or_compare(model,fields[0],action==5,&out);
    else if(action==6&&n==1)error=or_conflicts(fields[0],&out);
    else if(action==7&&n==3){
        double low=0,high=0;
        if(or_number(fields[1],&low)&&or_number(fields[2],&high))error=or_trial(model,fields[0],low,high,&out);
    }else if(action==8&&n==2)error=or_tasks(fields[0],fields[1],&out);
    else if(action==11&&n==2)error=or_closeout(model,fields[0],fields[1],&out);
    else if((action==12||action==13)&&n==1)error=or_workspace(model,fields[0],action==12,&out);
    if(error){
        if(!reply[0]){
            if(error==PRO_TK_BAD_INPUTS)part_text_add(&out,L"输入格式不正确。发送「订单改型」查看用法。\n");
            else if(error==PRO_TK_E_NOT_FOUND)part_text_add(&out,L"没找到输入表、订单记录或指定参照。\n");
            else if(!model)part_text_add(&out,L"请先打开订单对应的零件或装配。\n");
        }
        part_text_add(&out,L"操作未全部通过（Creo %d）。\n",error);
    }
    native_prompt_set(L"");
    append_card(L"你",request);
    save_turn(L"用户",request);
    append_card(L"订单改型",reply);
    save_turn(L"订单改型",reply);
    native_set_busy(0);
    return 1;
}
