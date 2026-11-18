
static ProError cbp_check_point(ProMdl model,const wchar_t *name,const ProVector expected,double scale){
    wchar_t ref[64];
    swprintf_s(ref,_countof(ref),L"POINT:%ls",name);
    FdRef point={0};
    ProError status=fd_ref_resolve(model,ref,PRO_POINT,&point);
    ProVector actual;
    if(!status)status=ad_point(&point,actual);
    fd_ref_free(&point);
    if(!status&&!ad_near(actual,expected,0.001*scale))status=PRO_TK_INCOMPLETE;
    return status;
}

static ProError cbp_roller_scale(CabinetPart *part,int axis,int sign,double slot_length,double slot_width,
    double increment,int count){
    double travel=axis?part->depth:part->width,cross=axis?part->width:part->depth;
    double from=travel/2-sign*(slot_length-slot_width)/2;
    for(int i=0;i<count;i++){
        ProName name;
        swprintf_s(name,_countof(name),L"TRAVEL_MARK_%02d",i+1);
        ProVector pos={0};
        pos[axis]=from+sign*increment*i;
        pos[1-axis]=cross/2;
        pos[2]=part->thickness;
        ProError status=ax_layout_point(part->txn.model,name,L"CSYS:PRT_CSYS_DEF",pos[0],pos[1],pos[2]);
        for(int j=0;j<3;j++)pos[j]*=part->txn.scale;
        if(!status)status=cbp_check_point(part->txn.model,name,pos,part->txn.scale);
        if(status)return status;
    }
    return PRO_TK_NO_ERROR;
}

static ProError cbp_roller_plate(const wchar_t *name,const wchar_t *direction,const double values[9],
    ProMdl *result,PartText *out){
    const double travel=values[0],cross=values[1],t=values[2],pitch=values[3],length=values[4],
        slot=values[5],step=values[6],stop_height=values[7],radius=values[8];
    int axis=-1,sign=0;
    *result=NULL;
    if((!wcscmp(direction,L"X+")||!wcscmp(direction,L"X-")))axis=0;
    if((!wcscmp(direction,L"Y+")||!wcscmp(direction,L"Y-")))axis=1;
    if(axis>=0)sign=direction[1]==L'+'?1:-1;
    if(axis<0||t<=0||radius<=0||stop_height<=0||step<=0||slot<=0||length<=slot||
       length+2*(radius+t)>=travel||pitch<=slot||pitch+slot>=cross||
       (length-slot)/step<1||(length-slot)/step>=16){
        part_text_add(out,L"请核对调节方向、槽距、槽长及刻度。有效滑程为槽长减槽宽，支持 2 至 16 个刻度基准点。\n");
        return PRO_TK_BAD_INPUTS;
    }
    CabinetPart part={0};
    ProError status=cbp_seed(&part,name,axis?cross:travel,axis?travel:cross,t,0);
    const wchar_t *stage=L"安装面底板";
    int marks=(int)floor((length-slot)/step+1e-8)+1;
    if(!status){stage=L"刻度基准点";status=cbp_roller_scale(&part,axis,sign,length,slot,step,marks);}
    if(!status){stage=L"底板转为钣金";status=cbp_convert(&part);}
    PanelProfile key[2];
    for(int i=0;i<2;i++){
        double transverse=(cross+(i?pitch:-pitch))/2;
        key[i]=(PanelProfile){PN_CAPSULE,axis?transverse:travel/2,axis?travel/2:transverse,length,slot,0,
            axis?AS_PI/2:0};
    }
    if(!status){stage=L"双长槽";status=cbp_top_openings(&part,L"ROLLER_SLOTS_PROFILE",L"ROLLER_SLIDING_SLOTS",key,2,1);}
    CabinetEdge edge,tip;
    ProFeature stop;
    if(!status){stage=L"实体定位止挡";status=cbp_stock_edge(&part,1-axis,sign>0,&edge);}
    if(!status)status=cbp_fold_up(&part,&edge,L"ROLLER_TRAVEL_STOP",0,cross,stop_height,radius,&stop,&tip);
    if(!status){stage=L"板厚及折弯回读";status=cbp_finish(&part,1,radius);}
    if(!status)part_text_add(out,L"已生成 %ls，调节方向 %ls，双槽总长 %.6g mm，理论滑程 %.6g mm。%d 个刻度基准点间距 %.6g mm，%ls 端有独立折弯止挡。待保存。\n",
        name,direction,length,length-slot,marks,step,direction);
    return cbp_end(&part,status,stage,L"导向轮可调底板",result,out);
}
