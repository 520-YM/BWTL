static ProError qa_draft_range(ProMdl model,const wchar_t *token,wchar_t axis,int sign,
                              double *minimum,double *maximum,int *samples,int *exact){
    FdRef ref={0};
    ProSurface surface;
    ProGeomitemdata *raw=NULL;
    ProError error;
    *minimum=HUGE_VAL;
    *maximum=-HUGE_VAL;
    *samples=0;
    if((error=qa_surface(model,token,1,&ref,&surface))||(error=ProSurfaceDataGet(surface,&raw)))goto done;
    const ProSurfacedata *data=raw->data.p_surface_data;
    *exact=data->type==PRO_SRF_PLANE;
    if(*exact){
        ProVector point,normal;
        error=fd_plane_data(&ref.item,point,normal);
        if(error)goto done;
        if(!ad_unit(normal)){error=PRO_TK_BAD_CONTEXT;goto done;}
        *minimum=*maximum=asin(qa_clamp(sign*normal[axis-L'X']))*180.0/AS_PI;
        *samples=1;
    }else for(int row=0;row<9;row++)for(int column=0;column<9;column++){
        ProVector point,normal;
        error=qa_interior_sample(model,surface,data,row,column,9,point,normal);
        if(error==PRO_TK_E_NOT_FOUND)continue;
        if(error)goto done;
        double component=sign*normal[axis-L'X'];
        double angle=component==0.0?0.0:asin(qa_clamp(component))*180.0/AS_PI;
        *minimum=fmin(*minimum,angle);
        *maximum=fmax(*maximum,angle);
        (*samples)++;
    }
    error=*samples?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
done:
    if(raw)ProGeomitemdataFree(&raw);
    fd_ref_free(&ref);
    return error;
}

static ProError qa_draft(ProMdl model,const PartCommand *cmd,PartText *out){
    int sign=wcscmp(cmd->arg[4],L"POS")?-1:1;
    double minimum,maximum,limit=wcstod(cmd->arg[5],NULL);
    int samples,exact;
    ProError error=qa_draft_range(model,cmd->arg[2],cmd->arg[3][0],sign,&minimum,&maximum,&samples,&exact);
    if(!error)part_text_add(out,L"%ls 沿 %lc%lc 的有符号拔模角：%.9g° 至 %.9g°。%ls，有效点 %d。\n"
        L"要求 ≥ %.9g°，%ls。\n",cmd->arg[2],sign>0?L'+':L'-',cmd->arg[3][0],minimum,maximum,
        exact?L"平面恒定角":L"曲面内部采样",samples,limit,
        qa_at_least(minimum,limit)?(exact?L"符合":L"采样点符合"):L"发现低于要求的位置");
    return error;
}
