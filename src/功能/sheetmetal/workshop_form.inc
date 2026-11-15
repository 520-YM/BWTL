typedef struct {
    const wchar_t *key,*grade,*direction;
    double thickness,depth;
    int found;
    wchar_t tool[32],revision[32];
} SmyFormRule;

static int smy_form_row(wchar_t **f,void *context){
    SmyFormRule *rule=context;
    if(!dv_safe_leaf(f[0])||!dv_safe_leaf(f[1])||!dv_safe_leaf(f[2])||
       !smx_number(f[3],0.01,100)||!smx_number(f[4],0.01,100)||
       (wcscmp(f[5],L"IN")&&wcscmp(f[5],L"OUT"))||!smx_number(f[6],0.01,1000)||!dv_safe_leaf(f[7]))return 0;
    double lo=wcstod(f[3],NULL),hi=wcstod(f[4],NULL),max_depth=wcstod(f[6],NULL);
    if(lo>hi)return 0;
    if(_wcsicmp(rule->key,f[0])||_wcsicmp(rule->grade,f[1])||wcscmp(rule->direction,f[5])||
       rule->thickness<lo||rule->thickness>hi||rule->depth>max_depth)return 1;
    if(rule->found)return 0;
    rule->found=1;
    wcscpy_s(rule->tool,_countof(rule->tool),f[2]);
    wcscpy_s(rule->revision,_countof(rule->revision),f[7]);
    return 1;
}

static ProError smy_form_library(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(error)return error;
    const wchar_t *direction=!wcscmp(cmd->arg[4],L"IN")?L"IN":L"OUT";
    SmyFormRule rule={cmd->arg[2],cmd->arg[3],direction,thickness,as_arg(cmd,5)};
    error=smx_table(L"BWTL_SMT_FORM_TOOLS.tsv",8,smy_form_row,&rule,out);
    if(!error&&!rule.found){
        part_text_add(out,L"没有符合材料、实测板厚、方向和深度的批准模具。\n");
        error=PRO_TK_E_NOT_FOUND;
    }
    if(!error)part_text_add(out,L"模具 %ls，版本 %ls，适用 %ls %.8g mm，方向 %ls，成形深度 %.8g mm。\n",
        rule.tool,rule.revision,rule.grade,rule.thickness,rule.direction,rule.depth);
    return error;
}

static ProError smy_form_clearance_value(FdTxn *tx,const PanelFrame *frame,double x,double y,
    int bends,double *distance,int *edge){
    SmxClearance check={0};
    check.model=tx->model;check.mm=1/tx->scale;check.frame=*frame;
    check.profile.x=x;check.profile.y=y;check.bends_only=bends;
    check.nearest=INFINITY;
    ProError error=ProSurfaceInit((ProSolid)tx->model,frame->face_id,&check.surface);
    if(!error)error=ProSurfaceContourVisit(check.surface,smx_clearance_contour,NULL,&check);
    *distance=check.nearest;
    *edge=check.edge;
    return error;
}

static ProError smy_form_clearance(ProMdl model,const PartCommand *cmd,PartText *out){
    PartCommand frame_args={0};
    for(int i=0;i<3;i++)wcscpy_s(frame_args.arg[3+i],PART_TOKEN_MAX,cmd->arg[2+i]);
    FdTxn tx={0};
    PanelFrame frame={0};
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(error)return error;
    tx.model=model;tx.scale=1/mm;
    error=pn_frame(&tx,&frame_args,&frame);
    double x=as_arg(cmd,5),y=as_arg(cmd,6);
    PanelProfile footprint=pn_rect(x,y,as_arg(cmd,7),as_arg(cmd,8));
    if(!error)error=pn_profile_probes(&tx,&frame,&footprint,thickness,0);
    double edge,bend;
    int edge_id,bend_id;
    if(!error)error=smy_form_clearance_value(&tx,&frame,x,y,0,&edge,&edge_id);
    if(!error)error=smy_form_clearance_value(&tx,&frame,x,y,1,&bend,&bend_id);
    if(!error){
        double cut_bound=hypot(as_arg(cmd,7),as_arg(cmd,8))/2;
        double tool_bound=hypot(as_arg(cmd,9),as_arg(cmd,10))/2;
        double minimum=as_arg(cmd,11),nearest=fmin(edge,bend);
        if(!isfinite(edge))return PRO_TK_BAD_CONTEXT;
        part_text_add(out,L"外轮廓 EDGE:%d，成形包络保守净距 %.8g mm，模具包络保守净距 %.8g mm。\n",
            edge_id,edge-cut_bound,edge-tool_bound);
        if(isfinite(bend))part_text_add(out,L"最近折弯切边 EDGE:%d，成形包络净距下界 %.8g mm，模具包络净距下界 %.8g mm。\n",
            bend_id,bend-cut_bound,bend-tool_bound);
        else part_text_add(out,L"所选板面的边界未发现相邻折弯切边。\n");
        part_text_add(out,L"按净距下限 %.8g mm：%ls。\n",minimum,nearest-cut_bound<minimum?L"成形几何裕量不足":
            nearest-tool_bound<minimum?L"成形几何有裕量，模具包络不足":L"所选板面内的成形和模具包络均符合");
    }
    return error;
}

typedef struct {
    ProMdl model;
    const PanelFrame *frame;
    double wanted,mm,error,width,height,offset;
    int face;
} SmyFormCrown;

static ProError smy_form_crown(ProGeomitem *item,ProError status,ProAppData context){
    if(status)return status;
    SmyFormCrown *read=context;
    ProSurface surface;
    ProSrftype type;
    ProError error=ProGeomitemToSurface(item,&surface);
    if(!error)error=ProSurfaceTypeGet(surface,&type);
    if(error||type!=PRO_SRF_PLANE)return error;
    ProVector point,normal,relative;
    if((error=fd_plane_data(item,point,normal)))return error;
    const ProCsysdata *csys=&read->frame->placement.csys;
    if(fabs(ad_dot(normal,csys->z_vector))<0.999999)return PRO_TK_NO_ERROR;
    ad_sub(point,csys->origin,relative);
    double offset=ad_dot(relative,csys->z_vector)*read->mm;
    double delta=fabs(offset-read->wanted);
    if(delta>=read->error)return PRO_TK_NO_ERROR;
    ProVector low,high,span;
    if((error=ProSurfaceExtremesEval(surface,(double *)csys->x_vector,low,high)))return error;
    ad_sub(high,low,span);
    double w=fabs(ad_dot(span,csys->x_vector))*read->mm;
    if((error=ProSurfaceExtremesEval(surface,(double *)csys->y_vector,low,high)))return error;
    ad_sub(high,low,span);
    double h=fabs(ad_dot(span,csys->y_vector))*read->mm;
    if(w<=0||h<=0)return PRO_TK_NO_ERROR;
    read->offset=offset;read->error=delta;read->width=w;read->height=h;read->face=item->id;
    return PRO_TK_NO_ERROR;
}

static ProError smy_form_make(ProMdl model,const PartCommand *cmd,int kind,const wchar_t *sense,PartText *out){
    FdTxn tx={0};
    PanelFrame frame={0};
    ProMatrix sketch_frame;
    ProName sketch;
    ProMdlName model_name;
    double mm=1,thickness,depth=as_arg(cmd,11),radius=as_arg(cmd,12);
    double x=as_arg(cmd,7),y=as_arg(cmd,8),w=as_arg(cmd,9),h=as_arg(cmd,10);
    int round=!wcscmp(cmd->arg[6],L"ROUND");
    if((round&&h!=0)||(!round&&h<=0))return PRO_TK_BAD_INPUTS;
    PanelProfile profile=round?(PanelProfile){PN_CIRCLE,x,y,w,w}:pn_rect(x,y,w,h);
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error&&radius*2>=fmin(profile.width,profile.height))error=PRO_TK_BAD_INPUTS;
    if(!error)error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_SMT_SKETCH_FORM);
    if(!error)error=pn_frame(&tx,cmd,&frame);
    if(!error)error=pn_profile_probes(&tx,&frame,&profile,thickness,0);
    if(!error)error=smx_outline_clearance(&tx,&frame,&profile,radius+0.01);
    swprintf_s(sketch,_countof(sketch),L"%.25ls_SK",cmd->arg[2]);
    if(!error)error=pn_sketch(model,cmd,&frame,sketch,&profile,1,1,sketch_frame);
    if(!error)error=ProMdlMdlnameGet(model,model_name);
    if(!error)error=cm_native_command(model,L"CREO_OP SMT_FORM %ls %ls SKETCH:%ls %.12g %.12g %ls",
        model_name,cmd->arg[2],sketch,depth,radius,sense);
    ProFeature feature;
    if(!error)error=cm_feature_find(model,cmd->arg[2],&feature);
    SmyFormCrown crown={model,&frame,kind==SMY_FORM_RECESS?-depth:depth,mm,INFINITY};
    if(!error)error=ProFeatureGeomitemVisit(&feature,PRO_SURFACE,smy_form_crown,NULL,&crown);
    if(!error&&(!crown.face||crown.error>0.005))error=PRO_TK_BAD_CONTEXT;
    if(error&&tx.root){
        wchar_t reply[768];
        am_finish(&tx,error,1,reply,_countof(reply));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",reply);
    }else fd_cleanup(&tx,0);
    if(!error)part_text_add(out,L"原生成形 %ls，面 SURFACE:%d 实测偏移 %+.8g mm，实际平面包络 %.8g × %.8g mm，成形内 R 设置 %.8g mm。未保存。\n",
        cmd->arg[2],crown.face,crown.offset,crown.width,crown.height,radius);
    return error;
}

static ProError smy_form_execute(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    if(kind==SMY_FORM_LIBRARY)return smy_form_library(model,cmd,out);
    if(kind==SMY_FORM_CLEARANCE)return smy_form_clearance(model,cmd,out);
    const wchar_t *directions[]={L"POS",L"NEG"};
    DeliveryOutput scratch={0};
    ProError error=fab_scratch(&scratch);
    if(!error)error=dv_stage_enter(&scratch);
    int chosen=-1;
    for(int i=0;!error&&chosen<0&&i<2;i++){
        ProMdl copy=NULL;
        error=smx_copy_scratch(model,&copy);
        if(error)break;
        wchar_t reply[1500]=L"";
        PartText receipt={reply,_countof(reply),0};
        ProError trial=smy_form_make(copy,cmd,kind,directions[i],&receipt);
        trace("workshop scratch form trial",trial);
        if(!trial)chosen=i;
        error=ProMdlErase(copy);
        if(error)sh_recovery_uncertain=1;
    }
    error=fab_discard(&scratch,error,out);
    if(error)return error;
    if(chosen<0){
        part_text_add(out,L"副本两种成形方向均未得到目标平面偏移，当前件保持原状。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    return smy_form_make(model,cmd,kind,directions[chosen],out);
}
