static ProError qa_wall_pair(ProMdl model,const wchar_t *first_token,const wchar_t *second_token,
                            double scale,double *thickness,ProVector witness){
    FdRef first={0},second={0};
    ProSurface a,b;
    ProGeomitemdata *raw=NULL;
    ProVector on_b,normal_b;
    ProError error;
    if((error=qa_surface(model,first_token,1,&first,&a))||
       (error=qa_surface(model,second_token,1,&second,&b)))goto done;
    if(first.item.id==second.item.id){error=PRO_TK_BAD_INPUTS;goto done;}
    if((error=fd_plane_data(&second.item,on_b,normal_b))||
       (error=ProSurfaceDataGet(a,&raw)))goto done;
    const ProSurfacedata *data=raw->data.p_surface_data;
    if(data->type!=PRO_SRF_PLANE||!ad_unit(normal_b)){
        error=PRO_TK_INVALID_TYPE;
        goto done;
    }

    /* Find an interior normal crossing bounded by exactly the requested faces.
       A parallel pair across an air gap does not qualify as a wall. */
    const double epsilon=1e-7/scale;
    for(int row=0;row<9;row++)for(int column=0;column<9;column++){
        ProRay line;
        ProVector normal_a;
        error=qa_interior_sample(model,a,data,row,column,9,line.start_point,normal_a);
        if(error==PRO_TK_E_NOT_FOUND)continue;
        if(error)goto done;
        if(ad_dot(normal_a,normal_b)>-1.0+1e-8){error=PRO_TK_BAD_CONTEXT;goto done;}
        for(int j=0;j<3;j++)line.dir_vector[j]=-normal_a[j];
        QualityRay ray;
        error=qa_ray(model,&line,epsilon,&ray);
        if(error==PRO_TK_INCOMPLETE)continue;
        if(error)goto done;
        for(int i=0;i+1<ray.count;i+=2){
            QualityCrossing enter=ray.crossing[i],exit=ray.crossing[i+1];
            if(enter.surface_id!=first.item.id||exit.surface_id!=second.item.id||
               fabs(enter.depth)>epsilon||exit.depth<=epsilon)continue;
            *thickness=(exit.depth-enter.depth)*scale;
            for(int j=0;j<3;j++)witness[j]=line.start_point[j]*scale;
            error=PRO_TK_NO_ERROR;
            goto done;
        }
    }
    error=PRO_TK_INCOMPLETE;
done:
    if(raw)ProGeomitemdataFree(&raw);
    fd_ref_free(&second);
    fd_ref_free(&first);
    return error;
}

static ProError qa_wall(ProMdl model,const PartCommand *cmd,double scale,PartText *out){
    double minimum=HUGE_VAL,limit=wcstod(cmd->arg[2],NULL);
    int unresolved=0;
    for(int i=3;i<cmd->count;i+=2){
        double thickness;
        ProVector witness;
        ProError error=qa_wall_pair(model,cmd->arg[i],cmd->arg[i+1],scale,&thickness,witness);
        if(error){
            unresolved++;
            part_text_add(out,L"%ls ↔ %ls：未验证为同一连续材料壁（Creo %d）。\n",cmd->arg[i],cmd->arg[i+1],error);
            continue;
        }
        minimum=fmin(minimum,thickness);
        part_text_add(out,L"%ls ↔ %ls：%.12g mm，材料穿透起点=(%.8g, %.8g, %.8g) mm\n",
            cmd->arg[i],cmd->arg[i+1],thickness,witness[0],witness[1],witness[2]);
    }
    if(isfinite(minimum))part_text_add(out,L"已测面组最小壁厚 %.12g mm，要求 ≥ %.12g mm。\n",minimum,limit);
    part_text_add(out,L"判定：%ls。未判定面组 %d。范围为指定的平行相对面组。\n",
        !qa_at_least(minimum,limit)?L"低于要求":unresolved?L"检查范围未全部覆盖":L"所测面组符合要求",unresolved);
    return PRO_TK_NO_ERROR;
}
