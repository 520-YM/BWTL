typedef struct {
    ProVector origin;
    ProVector axis;
    double radius;
} QualityBore;

static ProError qa_bore(ProMdl model,const wchar_t *token,FdRef *ref,QualityBore *bore){
    ProSurface surface;
    ProGeomitemdata *raw=NULL;
    ProError error;
    if((error=qa_surface(model,token,1,ref,&surface))||(error=ProSurfaceDataGet(surface,&raw)))goto done;
    const ProSurfacedata *data=raw->data.p_surface_data;
    if(data->type!=PRO_SRF_CYL){error=PRO_TK_INVALID_TYPE;goto done;}
    const ProCylinderdata *cylinder=&data->srf_shape.cylinder;
    memcpy(bore->origin,cylinder->origin,sizeof(bore->origin));
    memcpy(bore->axis,cylinder->e3,sizeof(bore->axis));
    bore->radius=cylinder->radius;
    if(!ad_unit(bore->axis)||!isfinite(bore->radius)||bore->radius<=0){
        error=PRO_TK_BAD_CONTEXT;
        goto done;
    }
    for(int i=0;i<5;i++)for(int j=0;j<5;j++){
        ProVector point,normal,radial;
        error=qa_interior_sample(model,surface,data,i,j,5,point,normal);
        if(error==PRO_TK_E_NOT_FOUND)continue;
        if(error)goto done;
        for(int k=0;k<3;k++)radial[k]=point[k]-bore->origin[k];
        double axial=ad_dot(radial,bore->axis);
        for(int k=0;k<3;k++)radial[k]-=axial*bore->axis[k];
        error=ad_unit(radial)&&ad_dot(radial,normal)<-0.9999?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
        goto done;
    }
    error=PRO_TK_INCOMPLETE;
done:
    if(raw)ProGeomitemdataFree(&raw);
    return error;
}

static ProError qa_hole_gap(ProMdl model,const wchar_t *first_token,const wchar_t *second_token,
                           int edge_mode,double scale,double *center,double *gap){
    FdRef first={0},second={0};
    QualityBore a,b;
    ProError error=qa_bore(model,first_token,&first,&a);
    if(error)goto done;
    if(edge_mode){
        Pro2dPnt param_a,param_b;
        Pro3dPnt point_a,point_b;
        if((error=fd_ref_resolve(model,second_token,PRO_EDGE,&second)))goto done;
        error=ProSelectionWithOptionsDistanceEval(first.selection,PRO_B_TRUE,second.selection,
            PRO_B_FALSE,param_a,param_b,point_a,point_b,center);
        if(error!=PRO_TK_NO_ERROR&&error!=PRO_TK_OUT_OF_RANGE)goto done;
        if(!isfinite(*center)||*center<0){error=PRO_TK_BAD_CONTEXT;goto done;}
        *gap=(*center-a.radius)*scale;
    }else{
        ProVector offset;
        if((error=qa_bore(model,second_token,&second,&b)))goto done;
        if(fabs(ad_dot(a.axis,b.axis))<1-1e-8){error=PRO_TK_BAD_CONTEXT;goto done;}
        for(int i=0;i<3;i++)offset[i]=b.origin[i]-a.origin[i];
        double axial=ad_dot(offset,a.axis);
        for(int i=0;i<3;i++)offset[i]-=axial*a.axis[i];
        *center=sqrt(ad_dot(offset,offset));
        if(*center*scale<=1e-7){error=PRO_TK_BAD_CONTEXT;goto done;}
        *gap=(*center-a.radius-b.radius)*scale;
    }
    *center*=scale;
    error=PRO_TK_NO_ERROR;
done:
    fd_ref_free(&second);
    fd_ref_free(&first);
    return error;
}

static ProError qa_holes(ProMdl model,const PartCommand *cmd,double scale,PartText *out){
    int edge_mode=!wcscmp(cmd->arg[2],L"EDGE");
    double center,gap,limit=wcstod(cmd->arg[5],NULL);
    ProError error=qa_hole_gap(model,cmd->arg[3],cmd->arg[4],edge_mode,scale,&center,&gap);
    if(!error)part_text_add(out,L"%ls ↔ %ls\n%ls %.12g mm，投影净距 %.12g mm。要求 ≥ %.12g mm，%ls。\n",
        cmd->arg[3],cmd->arg[4],edge_mode?L"孔轴至指定边最短距离":L"平行孔轴中心距",
        center,gap,limit,qa_at_least(gap,limit)?L"符合":L"低于要求");
    return error;
}
