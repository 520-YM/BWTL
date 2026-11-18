

static ProError cbp_add_drains(CabinetPart *part,const CabinetEdge *tip,double first,double second,double depth){
    const double scale=part->txn.scale,width=tip->length/scale,t=part->thickness;
    if(tip->a[0]>tip->b[0]){double swap=first;first=second;second=swap;}
    if(first<=0||second<=0||first+second>=width||depth<=0)return PRO_TK_BAD_INPUTS;
    PanelFrame frame={.face_id=tip->face};
    ProCsysdata *csys=&frame.placement.csys;
    memcpy(csys->origin,tip->a,sizeof(ProVector));
    memcpy(csys->x_vector,tip->axis,sizeof(ProVector));
    memcpy(csys->y_vector,tip->inward,sizeof(ProVector));
    memcpy(csys->z_vector,tip->normal,sizeof(ProVector));
    for(int i=0;i<3;i++)frame.inward[i]=-tip->normal[i];

    ShOrient orient={0};
    memcpy(orient.normal,tip->normal,sizeof(ProVector));
    ProError status=ProSolidFeatVisit((ProSolid)part->txn.model,sh_orient_plane,NULL,&orient);
    if(status!=PRO_TK_E_FOUND)return status?status:PRO_TK_E_NOT_FOUND;
    PartCommand setup={0};
    swprintf_s(setup.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",tip->face);
    swprintf_s(setup.arg[5],PART_TOKEN_MAX,L"PLANE:%ls",orient.name);

    AsPoint open[]={{first/2,depth/2},{first-fmin(0.01,first/4),depth/2},
        {width-second/2,depth/2},{width-second+fmin(0.01,second/4),depth/2}};
    AsPoint keep[]={{(first+width-second)/2,depth/2},{first/2,depth+t/2},{width-second/2,depth+t/2}};
    for(int i=0;i<_countof(open);i++){
        status=pn_probe(&part->txn,&frame,open[i],0,t,1);
        if(status)return status;
    }
    for(int i=0;i<_countof(keep);i++){
        status=pn_probe(&part->txn,&frame,keep[i],0,t,1);
        if(status)return status;
    }
    double overhang=fmin(0.05,fmin(first,fmin(second,depth))/4);
    PanelProfile profile[]={pn_rect((first-overhang)/2,(depth-overhang)/2,first+overhang,depth+overhang),
        pn_rect(width-(second-overhang)/2,(depth-overhang)/2,second+overhang,depth+overhang)};
    ProMassProperty stock;
    if((status=qa_geometry_mass(part->txn.model,&stock)))return status;
    part->txn.volume=stock.volume;
    ProMatrix location;
    status=pn_sketch(part->txn.model,&setup,&frame,L"DRAIN_ENDS_PROFILE",profile,2,1,location);
    if(!status)status=pn_cut(&part->txn,&frame,L"DRAIN_ENDS",L"DRAIN_ENDS_PROFILE",location,t,1);
    if(!status)status=md_expected_volume(&part->txn,-(first+second)*depth*t);
    for(int i=0;!status&&i<_countof(open);i++)status=pn_probe(&part->txn,&frame,open[i],0,0,0);
    for(int i=0;!status&&i<_countof(keep);i++)status=pn_probe(&part->txn,&frame,keep[i],0,t,0);
    return status;
}

/* The cover width includes the measured door fold, overlap and side gap.
   Drainage cuts open from the lip ends while retaining material at the bend root. */
static ProError cbp_rain_strip(const wchar_t *name,const double values[9],ProMdl *result,PartText *out){
    const double length=values[0],door=values[1],overlap=values[2],gap=values[3],t=values[4],
        return_height=values[5],radius=values[6],drain_a=values[7],drain_b=values[8];
    const double width=door+overlap+gap;
    *result=NULL;
    if(length<=0||door<=0||overlap<=0||gap<0||return_height<=0||drain_a<=0||drain_b<=0||
        drain_a+drain_b>=length||radius<=0||width<=radius+t){
        part_text_add(out,L"请核对门板折边、搭接、侧隙和排水缺口。两端缺口应留出中间连续的折边。\n");
        return PRO_TK_BAD_INPUTS;
    }

    CabinetPart part={0};
    ProError status=cbp_seed(&part,name,length,width,t,1);
    const wchar_t *stage=L"搭接板基体";
    CabinetEdge base,tip;
    ProFeature wall;
    if(!status){stage=L"回折边与两端排水开口";status=cbp_stock_edge(&part,0,1,&base);}
    if(!status)status=cbp_fold_up(&part,&base,L"RAIN_RETURN_LIP",0,length,return_height,radius,&wall,&tip);
    CabinetSpan lip;
    if(!status){stage=L"两端排水缺口";status=cm_face_span(part.txn.model,tip.face,tip.inward,&lip);}
    if(!status)status=cbp_add_drains(&part,&tip,drain_a,drain_b,(lip.max-lip.min)/part.txn.scale-t);
    if(!status){stage=L"折弯及板厚回读";status=cbp_finish(&part,1,radius);}
    if(!status)part_text_add(out,L"已生成 %ls。盖边有效宽 %.6g mm，搭接 %.6g mm，侧隙 %.6g mm。回折边实测两端排水缺口 %.6g / %.6g mm。待保存。\n",
        name,width,overlap,gap,drain_a,drain_b);
    return cbp_end(&part,status,stage,L"防雨折边压条",result,out);
}
