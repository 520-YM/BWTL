typedef struct {
    ProMdl model;
    FILE *file;
    PartText *out;
    int rows,holes[512],edges[512];
} SmyJoinCard;

static int smy_card_geometry(SmyJoinCard *card,wchar_t **f,int *quantity,double *value,const wchar_t **reference){
    if(!sf_id(f[3],L"EDGE:")||(wcscmp(f[4],L"CONT")&&wcscmp(f[4],L"STITCH")))return 0;
    double args[4];
    for(int i=0;i<4;i++){
        if(!smx_number(f[5+i],0,100000))return 0;
        args[i]=wcstod(f[5+i],NULL);
    }
    int id=_wtoi(f[3]+wcslen(L"EDGE:"));
    for(int i=0;i<card->rows;i++)if(card->edges[i]==id)return 0;
    SmyWeldSpan span={0};
    if(smy_weld_span(card->model,f[3],!wcscmp(f[4],L"CONT"),args[0],args[1],args[2],args[3],&span))return 0;
    *quantity=span.count;
    *value=span.count*span.segment;
    card->edges[card->rows]=id;
    *reference=f[3];
    return 1;
}

static int smy_card_fastener(SmyJoinCard *card,wchar_t **f,double *diameter,wchar_t revision[32]){
    if((wcscmp(f[0],L"PRESS")&&wcscmp(f[0],L"STUD")&&wcscmp(f[0],L"RIVET"))||
       !sf_id(f[3],L"SURFACE:")||!sf_id(f[4],L"SURFACE:"))return 0;
    for(int i=5;i<9;i++)if(wcscmp(f[i],L"0"))return 0;
    int id=_wtoi(f[4]+wcslen(L"SURFACE:"));
    for(int i=0;i<card->rows;i++)if(card->holes[i]==id)return 0;
    SmyFastener rule={f[1],f[2],f[0]};
    wchar_t text[1000]=L"";
    PartText detail={text,_countof(text),0};
    if(smx_table(L"BWTL_SMT_FASTENERS.tsv",9,smy_fastener_row,&rule,&detail)||!rule.found){
        part_text_add(card->out,L"紧固件 %ls 缺少可用的批准型号记录。\n",f[1]);
        return 0;
    }
    ShopMouth hole={0};
    double mm,t;
    if(smy_access_mouth(card->model,f[3],f[4],&hole,&mm,&t))return 0;
    FdTxn view={0};
    view.model=card->model;view.scale=1/mm;
    double clearance;
    int edge;
    if(smy_form_clearance_value(&view,&hole.frame,0,0,0,&clearance,&edge)||!isfinite(clearance))return 0;
    double rim=fmin(0.2,(clearance-hole.radius)/2),local=0;
    if(rim<0.0002||mp_read_stock(&view,&hole.frame,as_point(hole.radius+rim,0),0,&local,1))return 0;
    *diameter=2*hole.radius;
    if(*diameter<rule.hole_min-1e-7||*diameter>rule.hole_max+1e-7||
       local<rule.grip_min-1e-7||local>rule.grip_max+1e-7||clearance-hole.radius<rule.edge_min-1e-7){
        part_text_add(card->out,L"%ls 的实际孔径、局部板厚或孔边距超出紧固件批准范围。\n",f[4]);
        return 0;
    }
    card->holes[card->rows]=id;
    wcscpy_s(revision,32,rule.revision);
    return 1;
}

static int smy_join_row(wchar_t **f,void *context){
    SmyJoinCard *card=context;
    if(!dv_safe_leaf(f[1])||!dv_safe_leaf(f[2])||card->rows==_countof(card->holes))return 0;
    int weld=!wcscmp(f[0],L"WELD"),count=1;
    double value;
    wchar_t revision[32]=L"";
    const wchar_t *reference=f[4];
    if(weld){
        if(!smy_card_geometry(card,f,&count,&value,&reference))return 0;
    }else if(!smy_card_fastener(card,f,&value,revision))return 0;
    if(fwprintf(card->file,L"%d,%ls,",card->rows+1,f[0])<0)return 0;
    if(!dv_csv_text(card->file,f[1])||fputwc(L',',card->file)==WEOF||!dv_csv_text(card->file,f[2]))return 0;
    if(fwprintf(card->file,L",%ls,%d,%ls,%.10g,%ls\n",reference,count,weld?L"焊缝总长_mm":L"实测孔径_mm",value,revision)<0)return 0;
    card->rows++;
    return 1;
}

static ProError smy_join_card(ProMdl model,const PartCommand *cmd,PartText *out){
    SmyJoinCard card={0};
    card.model=model;card.out=out;
    ProPath directory,path;
    ProError error=ProDirectoryCurrentGet(directory);
    if(!error)error=smy_stock_file(directory,cmd->arg[3],L".tsv",path);
    DeliveryOutput work={0};
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,L"fastening-weld-card.csv",&card.file);
    if(!error&&fwprintf(card.file,L"序号,工序,型号或焊接方法,材料,几何参照,数量,数值类型,实测值,批准版本\n")<0)error=PRO_TK_CANT_WRITE;
    if(!error)error=smx_table_read(path,cmd->arg[3],9,smy_join_row,&card,out);
    error=dv_stage_finish(&work,fab_close(card.file,error),out);
    if(!error)part_text_add(out,L"已核对 %d 道指定紧固和焊接工序，生成 fastening-weld-card.csv。焊接方法按输入记录，未进行焊接资格或强度判定。%ls。\n",card.rows,work.target);
    return error;
}

