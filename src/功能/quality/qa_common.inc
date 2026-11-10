static ProError qa_mm_per_unit(ProMdl model,double *scale){
    ProUnitsystem units;
    ProUnititem native,mm;
    ProUnitConversion conversion;
    ProError error;
    if((error=ProMdlPrincipalunitsystemGet(model,&units))||
       (error=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&native))||
       (error=ProUnitInit(model,L"mm",&mm))||
       (error=ProUnitConversionCalculate(&native,&mm,&conversion)))return error;
    if(!isfinite(conversion.scale)||conversion.scale<=0||conversion.offset!=0)
        return PRO_TK_BAD_CONTEXT;
    *scale=conversion.scale;
    return PRO_TK_NO_ERROR;
}

static double qa_clamp(double value){return fmax(-1.0,fmin(1.0,value));}

static int qa_at_least(double value,double limit){
    return value>=limit-1e-9*fmax(1.0,fabs(limit));
}

static ProError qa_surface(ProMdl model,const wchar_t *token,int material_only,
                           FdRef *reference,ProSurface *surface){
    ProError error;
    if((error=fd_ref_resolve(model,token,PRO_SURFACE,reference))||
       (error=ProGeomitemToSurface(&reference->item,surface)))return error;
    if(material_only){
        ProBoolean datum;
        ProQuilt quilt;
        error=ProSurfaceIsDatumPlane((ProSolid)model,*surface,&datum);
        if(error||datum)return error?error:PRO_TK_BAD_CONTEXT;
        error=ProSurfaceQuiltGet((ProSolid)model,*surface,&quilt);
        if(error!=PRO_TK_E_NOT_FOUND)return error?error:PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}

static ProError qa_geometry_mass(ProMdl model,ProMassProperty *mass){
    ProError error=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,
        PRO_MP_DENS_USE_ALWAYS,1.0,mass);
    if(error)return error;
    return isfinite(mass->volume)&&mass->volume>0?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError qa_single_body(ProMdl model,ProSolidBody *body){
    ProSolidBody *bodies=NULL;
    int count;
    ProError error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    if(!error&&count!=1)error=PRO_TK_BAD_CONTEXT;
    if(!error){
        ProSolidBodyState state;
        ProBoolean construction;
        error=ProSolidBodyStateGet(&bodies[0],&state);
        if(!error)error=ProSolidBodyIsConstruction(&bodies[0],&construction);
        if(!error&&(state!=PRO_BODY_STATE_ACTIVE||construction))error=PRO_TK_BAD_CONTEXT;
        if(!error&&body)*body=bodies[0];
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    return error;
}

/* Only parameters inside the trimmed face contribute to the measurement. */
static ProError qa_interior_sample(ProMdl model,ProSurface surface,const ProSurfacedata *data,
    int row,int column,int grid,ProVector point,ProVector normal){
    ProUvParam uv;
    ProUvStatus status;
    ProError error;
    uv[0]=data->uv_min[0]+(data->uv_max[0]-data->uv_min[0])*(row+0.5)/grid;
    uv[1]=data->uv_min[1]+(data->uv_max[1]-data->uv_min[1])*(column+0.5)/grid;
    error=ProSurfaceUvpntVerify((ProSolid)model,surface,uv,&status);
    if(error)return error;
    if(status!=PRO_UV_INSIDE)return PRO_TK_E_NOT_FOUND;
    error=ProSurfaceXyzdataEval(surface,uv,point,NULL,NULL,normal);
    return !error&&!ad_unit(normal)?PRO_TK_BAD_CONTEXT:error;
}
