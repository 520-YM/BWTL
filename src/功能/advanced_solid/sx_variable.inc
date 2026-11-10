static ProError sx_variable(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    static const wchar_t *orientations[]={L"PLANE:RIGHT",L"PLANE:TOP",L"PLANE:FRONT"};
    FdTxn tx={0};
    FdRef ref={0};
    SxPath path={0};
    ProCurve curve;
    ProEnttype type;
    ProVector delta,origin,normal;
    ProName points[2],planes[2],sketches[2];
    const wchar_t *orientation=NULL;
    PartCommand step={0};
    ProMassProperty mass;
    double diameters[]={as_arg(cmd,4),as_arg(cmd,5)},length,expected,best=1;
    int bodies=0;
    ProError e;
#define VAR_TRY(label,call) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    VAR_TRY("prepare straight variable section",fd_begin(&tx,model,cmd->arg[2],PRO_FEAT_PROTRUSION));
    VAR_TRY("read variable-section body count",sd_body_count(model,&bodies));
    if(bodies||diameters[0]==diameters[1]){e=PRO_TK_BAD_CONTEXT;goto done;}
    VAR_TRY("read open variable-section path",sx_path_resolve(model,cmd->arg[3]+6,tx.scale,&path));
    if(path.count!=1){e=PRO_TK_BAD_INPUTS;goto done;}
    VAR_TRY("read variable-section curve",ProGeomitemToCurve(&path.items[0].item,&curve));
    VAR_TRY("verify straight variable-section curve",ProCurveTypeGet(curve,&type));
    if(type!=PRO_ENT_LINE){e=PRO_TK_BAD_INPUTS;goto done;}
    VAR_TRY("measure variable-section path length",ProCurveLengthEval(curve,&length));
    ad_sub(path.last,path.first,delta);
    if(!ad_unit(delta)){e=PRO_TK_BAD_INPUTS;goto done;}
    for(int i=0;i<_countof(orientations);i++){
        double alignment;
        e=fd_ref_resolve(model,orientations[i],PRO_SURFACE,&ref);
        if(e)continue;
        e=fd_plane_data(&ref.item,origin,normal);
        fd_ref_free(&ref);
        if(e)continue;
        alignment=fabs(ad_dot(delta,normal));
        if(alignment<best){best=alignment;orientation=orientations[i];}
    }
    if(!orientation||best>1-1e-7){e=PRO_TK_E_NOT_FOUND;goto done;}
    wcscpy_s(step.arg[1],PART_TOKEN_MAX,cmd->arg[1]);
    for(int i=0;i<2;i++){
        swprintf_s(points[i],PRO_NAME_SIZE,L"%.24ls_PT%d",cmd->arg[2],i+1);
        swprintf_s(planes[i],PRO_NAME_SIZE,L"%.24ls_PL%d",cmd->arg[2],i+1);
        swprintf_s(sketches[i],PRO_NAME_SIZE,L"%.24ls_SK%d",cmd->arg[2],i+1);
        wcscpy_s(step.arg[0],PART_TOKEN_MAX,L"DATUM_POINT_CURVE");
        wcscpy_s(step.arg[2],PART_TOKEN_MAX,points[i]);
        wcscpy_s(step.arg[3],PART_TOKEN_MAX,cmd->arg[3]);
        wcscpy_s(step.arg[4],PART_TOKEN_MAX,i?L"END":L"START");
        wcscpy_s(step.arg[5],PART_TOKEN_MAX,L"0");
        step.count=6;
        VAR_TRY("create linked endpoint datum",ad_execute(&step,model,report,capacity));
        wcscpy_s(step.arg[0],PART_TOKEN_MAX,L"DATUM_PLANE_NORMAL");
        wcscpy_s(step.arg[2],PART_TOKEN_MAX,planes[i]);
        swprintf_s(step.arg[4],PART_TOKEN_MAX,L"POINT:%ls",points[i]);
        step.count=5;
        VAR_TRY("create path-normal section plane",ad_execute(&step,model,report,capacity));
        wcscpy_s(step.arg[0],PART_TOKEN_MAX,L"SKETCH_CREATE");
        wcscpy_s(step.arg[2],PART_TOKEN_MAX,sketches[i]);
        swprintf_s(step.arg[3],PART_TOKEN_MAX,L"PLANE:%ls",planes[i]);
        wcscpy_s(step.arg[4],PART_TOKEN_MAX,orientation);
        wcscpy_s(step.arg[5],PART_TOKEN_MAX,L"CIRCLE");
        wcscpy_s(step.arg[6],PART_TOKEN_MAX,L"0");
        wcscpy_s(step.arg[7],PART_TOKEN_MAX,L"0");
        swprintf_s(step.arg[8],PART_TOKEN_MAX,L"%.15g",diameters[i]/2);
        step.count=9;
        VAR_TRY("create centered endpoint profile",fd_sketch_create_at(&step,model,i?path.last:path.first,report,capacity));
    }
    wcscpy_s(step.arg[2],PART_TOKEN_MAX,cmd->arg[2]);
    wcscpy_s(step.arg[3],PART_TOKEN_MAX,sketches[0]);
    wcscpy_s(step.arg[4],PART_TOKEN_MAX,sketches[1]);
    VAR_TRY("create exact linear taper between the profiles",sd_blend_sections(&step,model,report,capacity,1));
    VAR_TRY("measure actual variable-section volume",ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass));
    expected=AS_PI*(length/tx.scale)*(diameters[0]*diameters[0]+diameters[0]*diameters[1]+diameters[1]*diameters[1])/12;
    if(fabs(mass.volume/pow(tx.scale,3)-expected)>fmax(1e-5,expected*0.0005)){e=PRO_TK_GENERAL_ERROR;goto done;}
    swprintf_s(report,capacity,L"已沿直线路径 %ls 创建线性变径实体 %ls，直径 %.9g → %.9g mm。模型树中采用两端法向截面的关联混合。已核对实体体积和重生成，未保存。",
        cmd->arg[3]+6,cmd->arg[2],diameters[0],diameters[1]);
done:
    for(int i=0;i<path.count;i++)fd_ref_free(&path.items[i]);
    sx_compound_finish(&tx,e,bodies,report,capacity);
    return e;
#undef VAR_TRY
}
