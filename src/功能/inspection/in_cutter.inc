static ProError in_slot_width(const Inspection *inspect,const wchar_t *first,const wchar_t *second,double *width){
    InspectionPlane a,b;
    ProError error=in_plane(inspect->model,first,&a);
    if(!error)error=in_plane(inspect->model,second,&b);
    if(error)return error;
    ProVector delta;
    ad_sub(b.point,a.point,delta);
    double gap=ad_dot(delta,a.normal);
    if(gap<=0||ad_dot(a.normal,b.normal)>-1+1e-8)return PRO_TK_BAD_CONTEXT;
    ProGeomitemdata *raw=NULL;
    ProOutline box;
    if((error=ProSolidBodyOutlineGet((ProSolidBody *)&inspect->body,box))||
       (error=ProSurfaceDataGet(a.handle,&raw)))return error;
    double sweep=cm_distance(box[0],box[1])+1/inspect->mm;
    int valid=0;
    for(int i=0;!valid&&i<5;i++)for(int j=0;!valid&&j<5;j++){
        ProVector point,n,projected;
        error=qa_interior_sample(inspect->model,a.handle,raw->data.p_surface_data,i,j,5,point,n);
        if(error==PRO_TK_E_NOT_FOUND)continue;
        if(error)goto done;
        cm_point(point,a.normal,gap,projected);
        if(!in_on_plane(inspect->model,&b,projected))continue;
        ProRay ray;
        cm_point(point,a.normal,-sweep,ray.start_point);
        memcpy(ray.dir_vector,a.normal,sizeof(ProVector));
        QualityRay hits;
        error=qa_ray(inspect->model,&ray,0.0001/inspect->mm,&hits);
        if(error==PRO_TK_INCOMPLETE)continue;
        if(error)goto done;
        for(int k=1;k+1<hits.count;k+=2)if(hits.crossing[k].surface_id==a.id&&hits.crossing[k+1].surface_id==b.id&&
           fabs(hits.crossing[k+1].depth-hits.crossing[k].depth-gap)<0.0002/inspect->mm)valid=1;
    }
    error=valid?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
    if(valid)*width=gap*inspect->mm;
done:
    ProGeomitemdataFree(&raw);
    return error;
}

static ProError in_cutter(const Inspection *inspect,const PartCommand *cmd,PartText *out){
    double width;
    ProError error;
    if(cmd->count==6){
        FdRef ref={0};
        QualityBore bore;
        error=qa_bore(inspect->model,cmd->arg[3],&ref,&bore);
        fd_ref_free(&ref);
        if(!error)width=2*bore.radius*inspect->mm;
    }else error=in_slot_width(inspect,cmd->arg[3],cmd->arg[4],&width);
    if(error)return error;
    double smallest=as_arg(cmd,cmd->count-2),margin=as_arg(cmd,cmd->count-1),allowed=fmax(0,width-2*margin);
    part_text_add(out,L"刀具直径筛查%ls。实测局部%ls %.8g mm，扣除双侧余量后的允许最大刀径 %.8g mm，输入最小可用刀径 %.8g mm。",
        qa_at_least(allowed,smallest)?L"符合":L"不符合",cmd->count==6?L"内圆柱直径":L"槽宽",width,allowed,smallest);
    return PRO_TK_NO_ERROR;
}
