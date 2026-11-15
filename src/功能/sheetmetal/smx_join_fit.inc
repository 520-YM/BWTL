
static ProError smx_hem_fit(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    FdTxn tx={0};
    CabinetEdge base;
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=cm_edge_get(model,cmd->arg[3],&base);
    if(!error)error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_BEND);
    if(!error)error=cm_native_command(model,L"CREO_OP SMT_HEM %ls %ls %ls %ls %.12g %.12g %ls",
        cmd->arg[1],cmd->arg[2],cmd->arg[3],kind==SMX_PIN_BARREL?L"ROUND":L"OPEN",as_arg(cmd,4),as_arg(cmd,5),cmd->arg[6]);
    ProFeature bend;
    if(!error)error=cm_feature_find(model,cmd->arg[2],&bend);
    SmxGeometry *geometry=calloc(1,sizeof(*geometry));
    if(!error&&!geometry)error=PRO_TK_OUT_OF_MEMORY;
    if(!error)error=smx_geometry(model,geometry);
    if(!error&&kind==SMX_PIN_BARREL){
        const SmxFace *inside=NULL;
        for(int i=0;i<geometry->count;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type==PRO_SRF_CYL&&face->owner==bend.id&&fabs(ad_dot(base.axis,face->axis))>0.999999&&
               (!inside||face->radius<inside->radius))inside=face;
        }
        if(!inside)error=PRO_TK_E_NOT_FOUND;
        if(!error){
            double radial=inside->radius-as_arg(cmd,7)/2,axial=as_arg(cmd,8)-inside->length;
            double tol=as_arg(cmd,11);
            if(fabs(inside->radius-as_arg(cmd,5))>tol||radial<as_arg(cmd,9)||axial<as_arg(cmd,10))error=PRO_TK_BAD_CONTEXT;
            part_text_add(out,L"卷筒实测内 R %.8g mm，圆柱轴向长度 %.8g mm，径向间隙 %.8g mm，轴向总间隙 %.8g mm。\n",
                inside->radius,inside->length,radial,axial);
        }
    }else if(!error){
        double gap=INFINITY,stack=0,tol=as_arg(cmd,9);
        for(int i=0;i<geometry->count;i++){
            const SmxFace *first=&geometry->faces[i];
            if(first->type!=PRO_SRF_PLANE||first->owner!=bend.id||fabs(ad_dot(first->axis,base.normal))<0.999999)continue;
            for(int j=0;j<geometry->count;j++){
                const SmxFace *second=&geometry->faces[j];
                if(second->type!=PRO_SRF_PLANE||second->owner==bend.id||ad_dot(first->axis,second->axis)>-0.999999)continue;
                ProVector delta;
                ad_sub(second->point,first->point,delta);
                double plane=ad_dot(delta,first->axis)*mm;
                if(plane<0||plane>as_arg(cmd,7)+tol||plane>=gap)continue;
                wchar_t a[32],b[32];
                swprintf_s(a,_countof(a),L"SURFACE:%d",first->id);
                swprintf_s(b,_countof(b),L"SURFACE:%d",second->id);
                double measured;
                if(!smx_surface_distance(model,a,b,&measured)&&fabs(measured*mm-plane)<tol)gap=plane;
            }
        }
        if(isfinite(gap)){
            stack=2*thickness+gap;
            part_text_add(out,L"回折两面间隙 %.8g mm，含两层料的叠厚 %.8g mm。\n",gap,stack);
        }
        if(!isfinite(gap)||gap>as_arg(cmd,7)||fabs(stack-as_arg(cmd,8))>tol)error=PRO_TK_BAD_CONTEXT;
    }
    free(geometry);
    if(error&&tx.root){
        wchar_t rollback[768];
        am_finish(&tx,error,1,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",rollback);
    }else fd_cleanup(&tx,0);
    if(!error)part_text_add(out,L"原生回折 %ls 及其间隙已验收，未保存。\n",cmd->arg[2]);
    return error;
}

static ProError smx_joggle_fit(ProMdl model,const PartCommand *cmd,PartText *out){
    FdTxn tx={0};
    CabinetEdge edge;
    InspectionPlane other;
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=cm_edge_get(model,cmd->arg[3],&edge);
    if(!error)error=in_plane(model,cmd->arg[8],&other);
    if(!error&&fabs(ad_dot(edge.normal,other.normal))<0.999999)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_WALL);
    if(!error)error=cm_z(&tx,cmd,out);
    ProFeature step;
    CabinetFace landing;
    if(!error)error=cm_feature_find(model,cmd->arg[2],&step);
    if(!error)error=cm_wall_face(&step,&landing);
    int other_id;
    part_integer(cmd->arg[8]+8,&other_id);
    CabinetSpan a,b,c,d;
    if(!error)error=cm_face_span(model,landing.id,edge.axis,&a);
    if(!error)error=cm_face_span(model,other_id,edge.axis,&b);
    if(!error)error=cm_face_span(model,landing.id,edge.inward,&c);
    if(!error)error=cm_face_span(model,other_id,edge.inward,&d);
    if(!error){
        double overlap_axis=(fmin(a.max,b.max)-fmax(a.min,b.min))*mm;
        double overlap_depth=(fmin(c.max,d.max)-fmax(c.min,d.min))*mm;
        wchar_t face[32];
        swprintf_s(face,_countof(face),L"SURFACE:%d",landing.id);
        double clearance;
        error=smx_surface_distance(model,face,cmd->arg[8],&clearance);
        if(!error){
            clearance*=mm;
            ProVector separation;
            ad_sub(other.point,landing.point,separation);
            double plane_gap=fabs(ad_dot(separation,other.normal))*mm;
            if(overlap_axis<=0||overlap_depth<as_arg(cmd,9)||clearance>as_arg(cmd,10)||
               fabs(clearance-plane_gap)>0.001||ad_dot(landing.normal,other.normal)>-0.999999||
               ad_dot(separation,landing.normal)*mm<-0.001)error=PRO_TK_BAD_CONTEXT;
            part_text_add(out,L"搭接沿边 %.8g mm、深度 %.8g mm，指定两面实测间隙 %.8g mm，两层总叠厚 %.8g mm。\n",
                overlap_axis,overlap_depth,clearance,2*thickness+clearance);
        }
    }
    if(error&&tx.root){
        wchar_t rollback[768];
        am_finish(&tx,error,1,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",rollback);
    }else fd_cleanup(&tx,0);
    return error;
}

static ProError smx_tabs_pair(ProMdl model,const PartCommand *cmd,PartText *out){
    ProMdl other=NULL,tab_copy=NULL,slot_copy=NULL;
    ProMdlsubtype subtype;
    FdTxn tx={0};
    CabinetEdge edge,tip;
    ProFeature tab;
    int loaded=0;
    double mm,thickness,slot_mm,slot_thickness;
    double width=as_arg(cmd,10),length=as_arg(cmd,11),start=as_arg(cmd,9),clearance=as_arg(cmd,16);
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=ax_load_part(cmd->arg[3],&other,&loaded);
    if(!error)error=ProMdlSubtypeGet(other,&subtype);
    if(!error&&(model==other||subtype!=PROMDLSTYPE_PART_SHEETMETAL))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=fab_thickness(other,&slot_mm,&slot_thickness);
    if(!error)error=smx_model_copy(model,cmd->arg[2],&tab_copy);
    if(!error)error=smx_model_copy(other,cmd->arg[4],&slot_copy);
    if(!error)error=sh_begin(&tx,tab_copy,L"BWTL_TAB",PRO_FEAT_WALL);
    if(!error)error=cm_edge_get(tab_copy,cmd->arg[5],&edge);
    if(!error)error=cm_attach(&tx,tx.name,&edge,start/mm,(start+width)/mm,length,90,as_arg(cmd,12),cmd->arg[13],&tab);
    if(!error)error=cm_wall_tip(tab_copy,&tab,&edge,&tip);
    if(!error&&fabs(tip.length*mm-width)>0.0001)error=PRO_TK_GENERAL_ERROR;
    if(!error)error=cm_bend_verified(&tab,90,as_arg(cmd,12),1/mm);
    fd_cleanup(&tx,0);
    if(!error){
        PartCommand slot={0};
        wcscpy_s(slot.arg[2],PART_TOKEN_MAX,L"BWTL_TAB_SLOT");
        for(int i=0;i<3;i++)wcscpy_s(slot.arg[3+i],PART_TOKEN_MAX,cmd->arg[6+i]);
        PanelProfile profile=pn_rect(as_arg(cmd,14),as_arg(cmd,15),width+2*clearance,thickness+2*clearance);
        error=smx_openings_apply(slot_copy,&slot,&profile,1,as_arg(cmd,17),0,out);
    }
    if(loaded&&other)ProMdlErase(other);
    if(error){
        if(tab_copy&&ProMdlErase(tab_copy))sh_recovery_uncertain=1;
        if(slot_copy&&ProMdlErase(slot_copy))sh_recovery_uncertain=1;
    }else part_text_add(out,L"已生成配对副本 %ls / %ls：折弯定位片实宽 %.8g mm，槽 %.8g × %.8g mm，单侧预留 %.8g mm。原件保留，两个副本均未保存。\n",
        cmd->arg[2],cmd->arg[4],tip.length*mm,width+2*clearance,thickness+2*clearance,clearance);
    return error;
}
