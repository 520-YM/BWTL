
static ProError cbp_baffle_corner_profile(ProSection section,const ProMatrix frame,double scale,void *context){
    const double *legs=context;
    const double over=fmin(legs[0],legs[1])/4;
    const double world[3][3]={
        {-over,-over,legs[2]},
        {legs[0]*(1+over/legs[1]),-over,legs[2]},
        {-over,legs[1]*(1+over/legs[0]),legs[2]}
    };
    AsPoint points[3];
    for(int i=0;i<3;i++){
        ProVector delta;
        for(int j=0;j<3;j++)delta[j]=world[i][j]*scale-frame[3][j];
        points[i]=as_point(ad_dot(delta,frame[0]),ad_dot(delta,frame[1]));
    }
    for(int i=0;i<3;i++){
        int geometry;
        ProError status=as_add_line(section,points[i],points[(i+1)%3],&geometry);
        if(status)return status;
    }
    return PRO_TK_NO_ERROR;
}

static ProError cbp_baffle_corner(CabinetPart *part,double x,double y){
    FdTxn *tx=&part->txn;
    PartCommand setup={0};
    PanelFrame face={0};
    ProError status=quick_frame(tx,part->thickness,&setup,&face);
    AsPoint removed=as_point(x*0.2,y*0.2),retained=as_point(x*0.8,y*0.8);
    if(!status)status=smx_trim_probe(tx,&face,removed,part->thickness,1);
    if(!status)status=smx_trim_probe(tx,&face,retained,part->thickness,1);
    ProMassProperty stock;
    if(!status)status=qa_geometry_mass(tx->model,&stock);
    double cut[]={x,y,part->thickness};
    ProMatrix frame;
    if(!status)status=smx_custom_sketch(tx->model,L"BAFFLE_CORNER_PROFILE",setup.arg[3],setup.arg[5],
        cbp_baffle_corner_profile,cut,frame);
    if(!status)status=pn_cut(tx,&face,L"BAFFLE_CORNER_CLEARANCE",L"BAFFLE_CORNER_PROFILE",frame,part->thickness,1);
    if(!status){tx->volume=stock.volume;status=md_expected_volume(tx,-x*y*part->thickness/2);}
    if(!status)status=smx_trim_probe(tx,&face,removed,part->thickness,0);
    if(!status)status=smx_trim_probe(tx,&face,retained,part->thickness,1);
    return status;
}

static ProError cbp_baffle_withdraw_points(CabinetPart *part,int sign,int create){
    FdTxn *tx=&part->txn;
    const wchar_t *names[]={L"SERVICE_IN",L"SERVICE_OUT"};
    const double x[]={part->width/2,part->width/2+part->width*sign};
    for(int i=0;i<2;i++){
        ProError status=PRO_TK_NO_ERROR;
        if(create)status=ax_layout_point(tx->model,names[i],L"CSYS:PRT_CSYS_DEF",x[i],part->depth/2,part->thickness/2);
        const ProVector expected={x[i]*tx->scale,part->depth/2*tx->scale,part->thickness/2*tx->scale};
        if(!status)status=cbp_check_point(tx->model,names[i],expected,tx->scale);
        if(status)return status;
    }
    return PRO_TK_NO_ERROR;
}

static ProError cbp_air_baffle(const wchar_t *name,const wchar_t *direction,const wchar_t *fasteners,
    const double dim[11],ProMdl *result,PartText *out){
    const double w=dim[0],d=dim[1],t=dim[2],dx=dim[3],dy=dim[4],end=dim[5],inset=dim[6],bore=dim[7],
        slot=dim[8],door_width=dim[9],door_height=dim[10];
    const int sign=!wcscmp(direction,L"X+")?1:!wcscmp(direction,L"X-")?-1:0;
    const int slots=!wcscmp(fasteners,L"SLOTS");
    const double reach=(slots?slot:bore)/2;
    *result=NULL;
    if(!sign||(!slots&&wcscmp(fasteners,L"HOLES"))||w<=0||d<=0||t<=0||dx<=0||dx>=w||dy<=0||dy>=d||
       bore<=0||(slots?slot<=bore:slot!=0)||end<=reach||2*(end+reach)>=w||inset<=bore/2||
       d-inset-bore/2<=dy||door_width<=d||door_height<=t){
        part_text_add(out,L"请核对避让转角、孔槽位置及检修口内宽与净高。检修口需要大于隔板横向宽度与板厚。\n");
        return PRO_TK_BAD_INPUTS;
    }
    CabinetPart part={0};
    const wchar_t *stage=L"独立隔板底板";
    ProError status=cbp_seed(&part,name,w,d,t,0);
    if(!status){stage=L"拆卸方向基准";status=cbp_baffle_withdraw_points(&part,sign,1);}
    if(!status){stage=L"底板转为钣金";status=cbp_convert(&part);}
    if(!status){stage=L"内腔转角避让";status=cbp_baffle_corner(&part,dx,dy);}
    PanelProfile mounts[2];
    for(int i=0;i<2;i++)mounts[i]=(PanelProfile){slots?PN_CAPSULE:PN_CIRCLE,i?w-end:end,d-inset,2*reach,bore,0,0};
    if(!status){stage=L"可拆固定孔槽";status=cbp_top_openings(&part,L"BAFFLE_FASTENERS_PROFILE",L"BAFFLE_FASTENERS",mounts,2,1);}
    if(!status){stage=L"拆卸方向和板厚回读";status=cbp_baffle_withdraw_points(&part,sign,0);}
    if(!status)status=cbp_finish(&part,0,0);
    if(!status)part_text_add(out,L"已生成 %ls，转角避让 %.6g × %.6g mm，两个可拆%ls。松开紧固件后沿 %ls 抽出，检修口横向 / 净高余量 %.6g / %.6g mm。待保存。\n",
        name,dx,dy,slots?L"长槽":L"圆孔",direction,door_width-d,door_height-t);
    return cbp_end(&part,status,stage,L"风仓可拆内隔板",result,out);
}
