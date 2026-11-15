typedef struct {
    ProMdl model;
    const wchar_t *bend;
    ProVector bend_extremes[3][2];
    double lower,upper,chord;
    int form,closest,count;
} SmyFormSpacing;

static ProError smy_surface_extremes(ProSurface surface,double chord,ProVector extremes[3][2]){
    ProSurfaceTessellationInput input=NULL;
    ProTessellation tessellation=NULL;
    Pro3dPnt *vertices=NULL;
    int count=0;
    ProError error=ProSurfacetessellationinputAlloc(&input);
    if(!error)error=ProSurfacetessellationinputChordheightSet(input,chord);
    if(!error)error=ProSurfaceTessellationGet(surface,input,&tessellation);
    if(!error)error=ProTessellationVerticesGet(tessellation,&vertices);
    if(!error)error=ProArraySizeGet((ProArray)vertices,&count);
    if(!error&&count<3)error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<3;i++){
        int low=0,high=0;
        for(int j=1;j<count;j++){
            if(vertices[j][i]<vertices[low][i])low=j;
            if(vertices[j][i]>vertices[high][i])high=j;
        }
        memcpy(extremes[i][0],vertices[low],sizeof(ProVector));
        memcpy(extremes[i][1],vertices[high],sizeof(ProVector));
    }
    if(vertices)ProArrayFree((ProArray *)&vertices);
    if(tessellation)ProTessellationFree(tessellation);
    if(input)ProSurfacetessellationinputFree(input);
    return error;
}

static ProError smy_form_distance(ProSurface surface,ProError status,ProAppData context){
    if(status)return status;
    SmyFormSpacing *scan=context;
    ProGeomitem item;
    ProFeature owner;
    ProError error=ProSurfaceToGeomitem((ProSolid)scan->model,surface,&item);
    if(!error)error=ProGeomitemFeatureGet(&item,&owner);
    if(error)return error;
    if(owner.id!=scan->form)return PRO_TK_NO_ERROR;
    ProSmtSurfType role;
    error=ProSmtSurfaceTypeGet((ProPart)scan->model,surface,&role);
    if(error)return error;
    if(role!=PRO_SMT_SURF_FACE)return PRO_TK_NO_ERROR;
    wchar_t token[32];
    swprintf_s(token,_countof(token),L"SURFACE:%d",item.id);
    double lower,upper;
    error=smx_surface_distance(scan->model,token,scan->bend,&lower);
    if(!error)upper=lower;
    else{
        ProVector extremes[3][2];
        error=smy_surface_extremes(surface,scan->chord,extremes);
        if(error)return error;
        double squared=0;
        upper=INFINITY;
        for(int i=0;i<3;i++){
            double gap=fmax(0,fmax(extremes[i][0][i]-scan->bend_extremes[i][1][i],
                scan->bend_extremes[i][0][i]-extremes[i][1][i]));
            squared+=gap*gap;
            for(int j=0;j<3;j++)for(int a=0;a<2;a++)for(int b=0;b<2;b++)
                upper=fmin(upper,cm_distance(extremes[i][a],scan->bend_extremes[j][b]));
        }
        lower=fmax(0,sqrt(squared)-2*scan->chord);
        upper+=2*scan->chord;
    }
    scan->lower=fmin(scan->lower,lower);
    if(upper<scan->upper){scan->upper=upper;scan->closest=item.id;}
    scan->count++;
    return PRO_TK_NO_ERROR;
}

static ProError smy_form_interaction(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm,t;
    ProFeature form;
    ProFeattype type;
    ProFeatStatus state;
    ProError error=fab_thickness(model,&mm,&t);
    if(!error)error=cm_feature_find(model,cmd->arg[2],&form);
    if(!error)error=ProFeatureTypeGet(&form,&type);
    if(!error)error=ProFeatureStatusGet(&form,&state);
    if(error)return error;
    if(state!=PRO_FEAT_ACTIVE||(type!=PRO_FEAT_SMT_SKETCH_FORM&&type!=PRO_FEAT_FORM)){
        part_text_add(out,L"指定特征未识别为当前模型的活动钣金成形。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    int bend;
    part_integer(cmd->arg[3]+8,&bend);
    FabBends bends={0};
    error=smy_bends_load(model,&bends);
    if(error){trace("workshop form bend inventory",error);return error;}
    int found=0;
    for(int i=0;i<bends.count;i++)found|=bends.rows[i].surface==bend&&bends.rows[i].id!=form.id;
    if(!found){
        part_text_add(out,L"指定面未识别为当前模型的原生折弯驱动面。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    SmyFormSpacing scan={0};
    scan.model=model;scan.form=form.id;scan.bend=cmd->arg[3];scan.lower=INFINITY;scan.upper=INFINITY;scan.chord=0.02/mm;
    ProSurface surface;
    error=ProSurfaceInit((ProSolid)model,bend,&surface);
    if(!error)error=smy_surface_extremes(surface,scan.chord,scan.bend_extremes);
    if(error){trace("workshop bend tessellation",error);return error;}
    error=ProSolidSurfaceVisit((ProSolid)model,smy_form_distance,NULL,&scan);
    if(error){trace("workshop form surface inventory",error);return error;}
    if(!scan.count||!isfinite(scan.lower)||!isfinite(scan.upper))return PRO_TK_E_NOT_FOUND;
    double lower=scan.lower*mm,upper=scan.upper*mm,min=as_arg(cmd,4);
    const wchar_t *result=lower+0.000001>=min?L"符合":upper+0.000001<min?L"超限":L"距离区间跨过限值，需复核";
    part_text_add(out,L"成形 %ls 的 %d 个板面与折弯面 %ls 的最小三维净距范围 %.8g–%.8g mm（上界来自成形面 SURFACE:%d），要求 %.8g mm，%ls。精确测距不可用时按 0.02 mm 弦高网格估算。\n",
        cmd->arg[2],scan.count,cmd->arg[3],lower,upper,scan.closest,min,result);
    return PRO_TK_NO_ERROR;
}
