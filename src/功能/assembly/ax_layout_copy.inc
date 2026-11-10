typedef struct {
    ProType type;
    int count;
    ProVector points[3];
} AxCopyGeometry;

typedef struct {
    ProMdl target;
    ProName names[42];
    int count;
} AxCopyTransaction;

static ProError ax_copy_geometry(ProMdl model,const wchar_t *token,AxCopyGeometry *copy){
    FdRef reference={0};
    ProError error=fd_ref_resolve(model,token,PRO_TYPE_UNUSED,&reference);
    ProFeature owner;
    ProFeatStatus status;
    if(!error)error=ProGeomitemFeatureGet(&reference.item,&owner);
    if(!error)error=ProFeatureStatusGet(&owner,&status);
    if(error||status!=PRO_FEAT_ACTIVE){fd_ref_free(&reference);return error?error:PRO_TK_BAD_CONTEXT;}
    copy->type=reference.item.type;
    ProVector direction,tangent;
    switch(copy->type){
    case PRO_POINT:
        copy->count=1;error=ad_point(&reference,copy->points[0]);break;
    case PRO_AXIS:
        copy->count=2;
        error=ad_line(&reference,copy->points[0],direction);
        if(!error)for(int i=0;i<3;i++)copy->points[1][i]=copy->points[0][i]+10*direction[i];
        break;
    case PRO_SURFACE:{
        ProSurface surface;
        ProBoolean datum;
        copy->count=3;
        error=ProGeomitemToSurface(&reference.item,&surface);
        if(!error)error=ProSurfaceIsDatumPlane((ProSolid)model,surface,&datum);
        if(!error&&!datum)error=PRO_TK_BAD_CONTEXT;
        if(!error)error=fd_plane_data(&reference.item,copy->points[0],direction);
        if(!error){
            ProVector other={0};
            other[fabs(direction[0])<0.8?0:1]=1;
            ad_cross(direction,other,tangent);ad_unit(tangent);
            ad_cross(direction,tangent,other);ad_unit(other);
            for(int i=0;i<3;i++){
                copy->points[1][i]=copy->points[0][i]+10*tangent[i];
                copy->points[2][i]=copy->points[0][i]+10*other[i];
            }
        }
        break;
    }
    case PRO_CSYS:{
        ProCsys csys;
        ProGeomitemdata *raw=NULL;
        copy->count=3;
        error=ProGeomitemToCsys(&reference.item,&csys);
        if(!error)error=ProCsysDataGet(csys,&raw);
        if(!error){
            const ProCsysdata *frame=raw->data.p_csys_data;
            memcpy(copy->points[0],frame->origin,sizeof(ProVector));
            for(int i=0;i<3;i++){
                copy->points[1][i]=copy->points[0][i]+10*frame->x_vector[i];
                copy->points[2][i]=copy->points[0][i]+10*frame->y_vector[i];
            }
        }
        if(raw)ProGeomitemdataFree(&raw);
        break;
    }
    case PRO_CURVE:{
        ProCurve curve;
        double length;
        copy->count=2;
        error=ad_single_curve(&owner,&reference.item,&curve);
        if(!error)error=ProCurveLengthEval(curve,&length);
        for(int i=0;!error&&i<2;i++)error=ProCurveXyzdataEval(curve,i,copy->points[i],NULL,NULL);
        if(!error){
            ProVector delta;
            ad_sub(copy->points[1],copy->points[0],delta);
            double chord=sqrt(ad_dot(delta,delta));
            if(chord<0.01||fabs(length-chord)>1e-8*fmax(chord,1))error=PRO_TK_INVALID_TYPE;
        }
        break;
    }
    default:error=PRO_TK_INVALID_TYPE;break;
    }
    fd_ref_free(&reference);
    return error;
}

static ProError ax_copy_advanced(ProMdl target,const wchar_t *line){
    PartCommand parsed;
    wchar_t report[PART_TEXT_MAX]=L"";
    if(part_command_parse(line,&parsed)!=1)return PRO_TK_BAD_INPUTS;
    return ad_execute(&parsed,target,report,_countof(report));
}

static ProError ax_copy_csys(ProMdl model,const wchar_t *name,const wchar_t *origin,const wchar_t *x,const wchar_t *y,
    ProVector points[3]){
    FdTxn tx={0};
    FdRef refs[3]={0};
    ProElement array,row;
    ProCsysdata actual;
    ProSelection selection=NULL;
    ProVector direction[2];
    ProError error;
#define CC(call) do{if((error=(call))){trace("layout copied CSYS " #call,error);goto done;}}while(0)
    CC(fd_begin(&tx,model,name,PRO_FEAT_CSYS));
    CC(fd_ref_resolve(model,origin,PRO_POINT,&refs[0]));
    CC(fd_ref_resolve(model,x,PRO_AXIS,&refs[1]));
    CC(fd_ref_resolve(model,y,PRO_AXIS,&refs[2]));
    CC(model_element(tx.root,PRO_E_CSYS_ORIGIN_CONSTRS,0,0,&array));
    CC(model_element(array,PRO_E_CSYS_ORIGIN_CONSTR,0,0,&row));
    CC(fd_ref_element(&tx,row,PRO_E_CSYS_ORIGIN_CONSTR_REF,&refs[0]));
    CC(model_element(tx.root,PRO_E_CSYS_ORIENT_BY_METHOD,1,PRO_CSYS_ORIENT_BY_SEL_REFS,NULL));
    const ProElemId ref_ids[]={PRO_E_CSYS_ORIENTSELAXIS1_REF,PRO_E_CSYS_ORIENTSELAXIS2_REF};
    const ProElemId opt_ids[]={PRO_E_CSYS_ORIENTSELAXIS1_OPT,PRO_E_CSYS_ORIENTSELAXIS2_OPT};
    const ProElemId flip_ids[]={PRO_E_CSYS_ORIENTSELAXIS1_FLIP,PRO_E_CSYS_ORIENTSELAXIS2_FLIP};
    for(int i=0;i<2;i++){
        ProVector at,axis;
        CC(ad_line(&refs[i+1],at,axis));
        ad_sub(points[i+1],points[0],direction[i]);ad_unit(direction[i]);
        CC(fd_ref_element(&tx,tx.root,ref_ids[i],&refs[i+1]));
        CC(model_element(tx.root,opt_ids[i],1,i?PRO_CSYS_ORIENTMOVE_AXIS_OPT_Y:PRO_CSYS_ORIENTMOVE_AXIS_OPT_X,NULL));
        CC(model_element(tx.root,flip_ids[i],1,ad_dot(direction[i],axis)<0?PRO_CSYS_ORIENTSELAXIS_FLIP_YES:PRO_CSYS_ORIENTSELAXIS_FLIP_NO,NULL));
    }
    CC(fd_create(&tx,0));CC(fd_verify(&tx));
    wchar_t label[PRO_NAME_SIZE+6];
    swprintf_s(label,_countof(label),L"CSYS:%ls",name);
    CC(ax_csys_selection(model,label,0,&selection,&actual));
    if(!ad_near(actual.origin,points[0],1e-5)||ad_dot(actual.x_vector,direction[0])<1-1e-7||
       ad_dot(actual.y_vector,direction[1])<1-1e-7)error=PRO_TK_GENERAL_ERROR;
done:
    fd_cleanup(&tx,error);
    for(int i=0;i<3;i++)fd_ref_free(&refs[i]);
    if(selection)ProSelectionFree(&selection);
    return error;
#undef CC
}

static ProError ax_copy_make(AxCopyTransaction *tx,const wchar_t *name,const wchar_t *anchor,
    const AxCopyGeometry *geometry,const AxDeviceFrame *frame,double mm){
    ProVector coordinates[3];
    for(int i=0;i<geometry->count;i++)for(int j=0;j<3;j++)
        coordinates[i][j]=ax_project(geometry->points[i],frame,j)*mm;
    ProError error;
    if(geometry->type==PRO_POINT||geometry->type==PRO_CURVE){
        if((error=ux_new_name(tx->target,name)))return error;
        if(geometry->type==PRO_POINT)error=ax_layout_point(tx->target,name,anchor,coordinates[0][0],coordinates[0][1],coordinates[0][2]);
        else{
            wchar_t line[640];
            swprintf_s(line,_countof(line),L"CREO_OP DATUM_CURVE TARGET %ls %ls %.10g %.10g %.10g %.10g %.10g %.10g",
                name,anchor,coordinates[0][0],coordinates[0][1],coordinates[0][2],coordinates[1][0],coordinates[1][1],coordinates[1][2]);
            error=ax_layout_token_datum(tx->target,line);
        }
        if(!error){
            wcscpy_s(tx->names[tx->count++],PRO_NAME_SIZE,name);
            AxCopyGeometry actual={0};
            wchar_t reference[PRO_NAME_SIZE+7];
            swprintf_s(reference,_countof(reference),L"%ls:%ls",geometry->type==PRO_POINT?L"POINT":L"CURVE",name);
            error=ax_copy_geometry(tx->target,reference,&actual);
            for(int i=0;!error&&i<geometry->count;i++)
                if(!ad_near(actual.points[i],geometry->points[i],0.001/mm))error=PRO_TK_GENERAL_ERROR;
        }
        return error;
    }
    wchar_t point_refs[3][PRO_NAME_SIZE+7],axis_refs[2][PRO_NAME_SIZE+6],line[640];
    ProName point_names[3],axis_names[2];
    if((error=ux_new_name(tx->target,name)))return error;
    for(int i=0;i<geometry->count;i++){
        swprintf_s(point_names[i],PRO_NAME_SIZE,L"%ls_P%d",name,i+1);
        if((error=ux_new_name(tx->target,point_names[i])))return error;
        swprintf_s(point_refs[i],_countof(point_refs[i]),L"POINT:%ls",point_names[i]);
    }
    if(geometry->type==PRO_CSYS)for(int i=0;i<2;i++){
        swprintf_s(axis_names[i],PRO_NAME_SIZE,L"%ls_A%d",name,i+1);
        if((error=ux_new_name(tx->target,axis_names[i])))return error;
        swprintf_s(axis_refs[i],_countof(axis_refs[i]),L"AXIS:%ls",axis_names[i]);
    }
    for(int i=0;i<geometry->count;i++){
        error=ax_layout_point(tx->target,point_names[i],anchor,coordinates[i][0],coordinates[i][1],coordinates[i][2]);
        if(error)return error;
        wcscpy_s(tx->names[tx->count++],PRO_NAME_SIZE,point_names[i]);
    }
    if(geometry->type==PRO_CSYS){
        for(int i=0;i<2;i++){
            swprintf_s(line,_countof(line),L"CREO_OP DATUM_AXIS_2POINT TARGET %ls %ls %ls",axis_names[i],point_refs[0],point_refs[i+1]);
            if((error=ax_copy_advanced(tx->target,line)))return error;
            wcscpy_s(tx->names[tx->count++],PRO_NAME_SIZE,axis_names[i]);
        }
        ProVector expected[3];
        memcpy(expected,geometry->points,sizeof(expected));
        error=ax_copy_csys(tx->target,name,point_refs[0],axis_refs[0],axis_refs[1],expected);
    }else if(geometry->type==PRO_AXIS){
        swprintf_s(line,_countof(line),L"CREO_OP DATUM_AXIS_2POINT TARGET %ls %ls %ls",name,point_refs[0],point_refs[1]);
        error=ax_copy_advanced(tx->target,line);
    }else{
        swprintf_s(line,_countof(line),L"CREO_OP DATUM_PLANE_3POINT TARGET %ls %ls %ls %ls",name,point_refs[0],point_refs[1],point_refs[2]);
        error=ax_copy_advanced(tx->target,line);
    }
    if(!error)wcscpy_s(tx->names[tx->count++],PRO_NAME_SIZE,name);
    return error;
}

static ProError ax_layout_copy(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent source={0},target={0};
    AxCopyTransaction tx={0};FdTxn before={0};
    AxCopyGeometry copies[7]={0};
    AxDeviceFrame frame;
    ProMatrix a,b,inverse;
    ProMdlType type;
    double units;
    int count=cmd->count-5;
    ProError error;
#define CT(call) do{if((error=(call))){trace("layout reference copy " #call,error);goto done;}}while(0)
    CT(ax_ref_occurrence(assembly,cmd->arg[2],&source));
    CT(ax_component_resolve(assembly,cmd->arg[3],&target));
    if(source.model==target.model){error=PRO_TK_BAD_CONTEXT;goto done;}
    CT(ax_layout_single_use(assembly,target.model));
    CT(ProMdlTypeGet(target.model,&type));
    tx.target=target.model;
    const wchar_t *anchor=type==PRO_MDL_ASSEMBLY?L"CSYS:ASM_CSYS":L"CSYS:PRT_CSYS_DEF";
    CT(qa_mm_per_unit(source.model,&units));if(!part_close(units,mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
    CT(qa_mm_per_unit(target.model,&units));if(!part_close(units,mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
    CT(ax_device_frame(&target,anchor,0,&frame));
    CT(ax_device_pose(&target,b));
    if(source.path.table_num)CT(ax_device_pose(&source,a));
    else{memset(a,0,sizeof(a));for(int i=0;i<4;i++)a[i][i]=1;}
    ax_device_inverse(b,inverse);
    for(int i=0;i<count;i++){
        CT(ax_copy_geometry(source.model,cmd->arg[i+5],&copies[i]));
        for(int j=0;j<copies[i].count;j++){
            ProVector root;
            CT(ProPntTrfEval(copies[i].points[j],a,root));
            CT(ProPntTrfEval(root,inverse,copies[i].points[j]));
        }
    }
    CT(ProSolidFeatVisit((ProSolid)tx.target,fd_feature_ids,NULL,&before));
    for(int i=0;i<count;i++){
        ProName name;
        swprintf_s(name,_countof(name),L"%ls_%d",cmd->arg[4],i+1);
        CT(ax_copy_make(&tx,name,anchor,&copies[i],&frame,mm));
    }
    CT(part_regenerate(assembly));
    part_text_add(out,L"已向 %ls 复制 %d 个参照，名称 %ls_1～%d。按当前装配位置转换为目标模型的本地基准或直线轮廓。一次性复制，未保存。\n",
        cmd->arg[3],count,cmd->arg[4],count);
done:
    if(error&&before.before_count){
        int failed=0;
        for(int i=tx.count-1;i>=0;i--){
            ProFeature feature;
            if(ux_named_feature(tx.target,tx.names[i],&feature)||model_remove_feature((ProSolid)tx.target,feature.id))failed=1;
        }
        if(part_regenerate(tx.target)||part_regenerate(assembly))failed=1;
        FdTxn after={0};
        if(ProSolidFeatVisit((ProSolid)tx.target,fd_feature_ids,NULL,&after)||after.before_count!=before.before_count)failed=1;
        for(int i=0;i<after.before_count;i++)if(!fd_was_feature(&before,after.before_ids[i]))failed=1;
        if(failed)ax_uncertain(out);
    }
    return error;
#undef CT
}