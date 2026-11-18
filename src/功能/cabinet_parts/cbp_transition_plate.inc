
static ProError cbp_landing(CabinetPart *part,const CabinetEdge *edge,const wchar_t *name,
    double rise,double platform,double radius,CabinetFace *upper,CabinetEdge *free_edge,PartText *out){
    PartCommand command={0};
    swprintf_s(command.arg[3],PART_TOKEN_MAX,L"EDGE:%d",edge->id);
    swprintf_s(command.arg[4],PART_TOKEN_MAX,L"%.12g",rise);
    swprintf_s(command.arg[5],PART_TOKEN_MAX,L"%.12g",platform);
    swprintf_s(command.arg[6],PART_TOKEN_MAX,L"%.12g",radius);
    wcscpy_s(command.arg[7],PART_TOKEN_MAX,edge->normal[2]>0?L"NEG":L"POS");
    wcscpy_s(part->txn.name,_countof(part->txn.name),name);
    ProError status=cm_z(&part->txn,&command,out);
    ProFeature wall;
    if(!status)status=cm_feature_find(part->txn.model,name,&wall);
    if(!status)status=cm_wall_face(&wall,upper);
    if(!status)status=cm_wall_tip(part->txn.model,&wall,edge,free_edge);
    if(!status&&(fabs(fabs(upper->normal[2])-1)>1e-7||
       fabs((upper->point[2]-edge->a[2])/part->txn.scale-rise)>0.001)){
        part_text_add(out,L"目标向上 %.6g mm，实际高差 %.6g mm。\n",
            rise,(upper->point[2]-edge->a[2])/part->txn.scale);
        status=PRO_TK_INCOMPLETE;
    }
    return status;
}

static ProError cbp_landing_holes(CabinetPart *part,const CabinetFace *face,
    const wchar_t *sketch,const wchar_t *feature,double pitch,double bore,PartText *out){
    FdTxn *tx=&part->txn;
    PartCommand setup={0};
    PanelFrame frame={0};
    double top=face->point[2]/tx->scale;
    ProError status=quick_frame(tx,top,&setup,&frame);
    if(status==PRO_TK_E_NOT_FOUND){
        top+=part->thickness;
        status=quick_frame(tx,top,&setup,&frame);
    }
    if(status){part_text_add(out,L"高平台外侧面定位失败（Creo %d）。\n",status);return status;}
    CabinetSpan measured;
    const ProVector depth={0,1,0};
    status=cm_face_span(tx->model,frame.face_id,depth,&measured);
    if(status)return status;
    if((measured.max-measured.min)/tx->scale<=bore)return PRO_TK_BAD_INPUTS;
    const double center=(measured.min+measured.max)/(2*tx->scale);
    PanelProfile holes[]={cbp_circle((part->width-pitch)/2,center,bore),cbp_circle((part->width+pitch)/2,center,bore)};
    const double sample=bore*0.4;
    AsPoint probes[2][5];
    for(int i=0;i<2;i++){
        double x=holes[i].x;
        probes[i][0]=as_point(x,center);
        probes[i][1]=as_point(x-sample,center);
        probes[i][2]=as_point(x+sample,center);
        probes[i][3]=as_point(x,center-sample);
        probes[i][4]=as_point(x,center+sample);
    }
    for(int i=0;!status&&i<2;i++)for(int j=0;!status&&j<5;j++)
        status=smx_trim_probe(tx,&frame,probes[i][j],part->thickness,1);
    const wchar_t *check=L"钻孔前局部板厚";
    ProMassProperty stock;
    if(!status)status=qa_geometry_mass(tx->model,&stock);
    ProMatrix local;
    if(!status){check=L"高侧孔组草绘";status=pn_sketch(tx->model,&setup,&frame,sketch,holes,2,1,local);}
    if(!status){check=L"高侧孔组限深切除";status=smx_trim_cut(tx,&frame,feature,sketch,local,part->thickness);}
    if(!status){check=L"高侧孔去除体积";tx->volume=stock.volume;status=md_expected_volume(tx,-2*pn_area(&holes[0])*part->thickness);}
    if(!status){
        check=L"高侧孔口贯穿";
        for(int i=0;!status&&i<2;i++)for(int j=0;!status&&j<5;j++)
            status=smx_trim_probe(tx,&frame,probes[i][j],part->thickness,0);
    }
    if(status)part_text_add(out,L"%ls失败，所选平面 ID %d，Z=%.6g mm，孔组 Y=%.6g mm（Creo %d）。\n",
        check,frame.face_id,top,center,status);
    return status;
}

static ProError cbp_transition_plate(const wchar_t *name,const double values[11],ProMdl *result,PartText *out){
    const double w=values[0],low=values[1],middle=values[2],high=values[3],t=values[4],radius=values[5],
        mid_rise=values[6],total_rise=values[7],low_pitch=values[8],high_pitch=values[9],bore=values[10];
    const int three=middle>0;
    *result=NULL;
    if(w<=0||low<=0||middle<0||high<=0||t<=0||radius<=0||total_rise<=2*radius||
        (three?(mid_rise<=2*radius||total_rise-mid_rise<=2*radius):mid_rise!=0)||
        bore<=0||low_pitch<=bore||high_pitch<=bore||fmax(low_pitch,high_pitch)+bore>=w){
        part_text_add(out,L"请核对平台深度、高差和两组孔距。两平台时中平台深度与高度填 0。\n");
        return PRO_TK_BAD_INPUTS;
    }
    const wchar_t *stage=L"低侧平台";
    CabinetPart part={0};
    ProError status=cbp_seed(&part,name,w,low,t,1);
    CabinetEdge base,top;
    CabinetFace middle_face={0},upper={0};
    if(!status){
        stage=L"独立低侧功能孔";
        PanelProfile openings[]={cbp_circle((w-low_pitch)/2,low/2,bore),cbp_circle((w+low_pitch)/2,low/2,bore)};
        status=cbp_top_openings(&part,L"LOW_HOLE_PROFILE",L"LOW_INTERFACE_HOLES",openings,2,1);
    }
    if(!status){stage=L"低平台原生直边";status=cbp_stock_edge(&part,0,1,&base);}
    if(!status&&three){
        stage=L"中平台折弯";
        status=cbp_landing(&part,&base,L"LEVEL_MID",mid_rise,middle,radius,&middle_face,&top,out);
        if(!status)base=top;
    }
    if(!status){
        stage=L"高平台折弯";
        status=cbp_landing(&part,&base,L"LEVEL_HIGH",three?total_rise-mid_rise:total_rise,high,radius,&upper,&top,out);
    }
    if(!status&&fabs(upper.point[2]/part.txn.scale-t-total_rise)>0.001)status=PRO_TK_INCOMPLETE;
    if(!status){
        stage=L"独立高侧功能孔";
        status=cbp_landing_holes(&part,&upper,L"HIGH_HOLE_PROFILE",L"HIGH_INTERFACE_HOLES",high_pitch,bore,out);
    }
    if(!status){stage=L"折弯与最终高差";status=cbp_finish(&part,three?4:2,radius);}
    if(!status)part_text_add(out,L"\n已生成 %ls，%d 个平行安装平台。端面实测高差 %.6g mm。两侧孔组各两孔，孔距 %.6g / %.6g mm，分属独立切口。待保存。\n",
        name,three?3:2,total_rise,low_pitch,high_pitch);
    return cbp_end(&part,status,stage,L"多段高低过渡连接板",result,out);
}
