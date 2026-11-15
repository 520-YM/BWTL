typedef struct {
    const wchar_t *kind,*item;
    int found;
    double width,height,bolt_x,bolt_y,bolt_d,seat_w,seat_h,min_t,max_t;
    wchar_t revision[32];
} SmyVentInterface;

static int smy_vent_row(wchar_t **fields,void *context){
    SmyVentInterface *rule=context;
    if(!smy_vent_kind(fields[0])||!dv_safe_leaf(fields[1])||!dv_safe_leaf(fields[11]))return 0;
    double v[9];
    int fan=!wcscmp(fields[0],L"FAN");
    for(int i=0;i<9;i++){
        if(!smx_number(fields[2+i],i==1&&fan?0:0.01,i==1&&fan?0:10000))return 0;
        v[i]=wcstod(fields[2+i],NULL);
    }
    if(v[5]<fmax(v[0],v[2]+v[4])||v[6]<fmax(fan?v[0]:v[1],v[3]+v[4])||v[7]>v[8])return 0;
    if(_wcsicmp(rule->kind,fields[0])||_wcsicmp(rule->item,fields[1]))return 1;
    if(rule->found)return 0;
    rule->found=1;
    rule->width=v[0];rule->height=fan?v[0]:v[1];rule->bolt_x=v[2];rule->bolt_y=v[3];rule->bolt_d=v[4];
    rule->seat_w=v[5];rule->seat_h=v[6];rule->min_t=v[7];rule->max_t=v[8];
    wcscpy_s(rule->revision,_countof(rule->revision),fields[11]);
    return 1;
}

static ProError smy_vent_interface(ProMdl model,const PartCommand *cmd,PartText *out){
    SmyVentInterface rule={cmd->arg[6],cmd->arg[7]};
    ProError error=smx_table(L"BWTL_SMT_VENT_INTERFACES.tsv",12,smy_vent_row,&rule,out);
    if(error)return error;
    if(!rule.found){
        part_text_add(out,L"接口规格 %ls / %ls 未登记。\n",rule.kind,rule.item);
        return PRO_TK_E_NOT_FOUND;
    }
    double mm,t,x=as_arg(cmd,8),y=as_arg(cmd,9),margin=as_arg(cmd,10);
    error=fab_thickness(model,&mm,&t);
    if(error)return error;
    if(t<rule.min_t||t>rule.max_t){
        part_text_add(out,L"板厚 %.8g mm 超出接口允许的 %.8g–%.8g mm。\n",t,rule.min_t,rule.max_t);
        return PRO_TK_BAD_CONTEXT;
    }
    FdTxn view={0};
    view.model=model;view.scale=1/mm;
    PanelFrame frame={0};
    error=pn_frame(&view,cmd,&frame);
    PanelProfile seat=pn_rect(x,y,rule.seat_w,rule.seat_h);
    if(!error)error=pn_profile_probes(&view,&frame,&seat,t,0);
    if(!error)error=smx_outline_clearance(&view,&frame,&seat,margin);
    if(error){
        part_text_add(out,L"接口外框未通过板面承托和净边检查，未切孔。\n");
        return error;
    }
    PanelProfile profiles[5];
    profiles[0]=!wcscmp(rule.kind,L"FAN")?(PanelProfile){PN_CIRCLE,x,y,rule.width,rule.width}:pn_rect(x,y,rule.width,rule.height);
    int at=1;
    for(int i=-1;i<=1;i+=2)for(int j=-1;j<=1;j+=2)
        profiles[at++]=(PanelProfile){PN_CIRCLE,x+i*rule.bolt_x/2,y+j*rule.bolt_y/2,rule.bolt_d,rule.bolt_d};
    error=smx_openings_apply(model,cmd,profiles,_countof(profiles),margin,0,out);
    if(!error)part_text_add(out,L"%ls / %ls（%ls）：开口 %.8g × %.8g mm，4 个 Ø%.8g mm 安装孔，中心距 %.8g × %.8g mm。未保存。\n",
        rule.kind,rule.item,rule.revision,rule.width,rule.height,rule.bolt_d,rule.bolt_x,rule.bolt_y);
    return error;
}
