typedef struct {
    const wchar_t *item,*material;
    int found;
    double hole,seat,opening,min_t,max_t;
    wchar_t revision[32];
} SmyCableTrim;

static int smy_cable_row(wchar_t **f,void *context){
    SmyCableTrim *trim=context;
    if(!dv_safe_leaf(f[0])||!dv_safe_leaf(f[1])||!dv_safe_leaf(f[7]))return 0;
    double v[5];
    for(int i=0;i<5;i++){
        if(!smx_number(f[i+2],0.01,1000))return 0;
        v[i]=wcstod(f[i+2],NULL);
    }
    if(v[2]>=v[0]||v[0]>=v[1]||v[3]>v[4])return 0;
    if(_wcsicmp(f[0],trim->item)||_wcsicmp(f[1],trim->material))return 1;
    if(trim->found)return 0;
    trim->found=1;
    trim->hole=v[0];trim->seat=v[1];trim->opening=v[2];trim->min_t=v[3];trim->max_t=v[4];
    wcscpy_s(trim->revision,_countof(trim->revision),f[7]);
    return 1;
}

static ProError smy_cable_entry(ProMdl model,const PartCommand *cmd,PartText *out){
    SmyCableTrim trim={cmd->arg[6],cmd->arg[7]};
    ProError error=smx_table(L"BWTL_SMT_CABLE_TRIM.tsv",8,smy_cable_row,&trim,out);
    if(error)return error;
    if(!trim.found)return PRO_TK_E_NOT_FOUND;
    double mm,t;
    error=fab_thickness(model,&mm,&t);
    if(error)return error;
    double bundle=as_arg(cmd,10),gap=as_arg(cmd,11),edge=as_arg(cmd,12);
    if(t<trim.min_t||t>trim.max_t||bundle+2*gap>trim.opening){
        part_text_add(out,L"护边实际夹持板厚或电缆束所需净径超出规则，孔未创建。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    FdTxn view={0};
    view.model=model;view.scale=1/mm;
    PanelFrame frame={0};
    error=pn_frame(&view,cmd,&frame);
    double x=as_arg(cmd,8),y=as_arg(cmd,9);
    PanelProfile seat={PN_CIRCLE,x,y,trim.seat,trim.seat};
    if(!error)error=pn_profile_probes(&view,&frame,&seat,t,0);
    if(!error)error=smx_outline_clearance(&view,&frame,&seat,edge);
    if(error){
        part_text_add(out,L"护边承托外径 %.8g mm 未取得完整承托区，孔未创建。\n",trim.seat);
        return error;
    }
    PanelProfile opening={PN_CIRCLE,x,y,trim.hole,trim.hole};
    error=smx_openings_apply(model,cmd,&opening,1,edge,0,out);
    if(!error)part_text_add(out,L"护边 %ls（%ls），已开 Ø%.8g mm 贯穿孔，护边外径 %.8g mm、装后净径 %.8g mm。电缆束加余量及承托环已核对，未保存。\n",
        trim.item,trim.revision,trim.hole,trim.seat,trim.opening);
    return error;
}

