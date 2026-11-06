typedef struct {
    double radius,height;
    int cylinders,bottom,top;
} CenteredCylinderGeometry;

static ProError centered_cylinder_surface(ProSurface surface,ProError status,ProAppData data){
    CenteredCylinderGeometry *geometry=data;
    ProGeomitemdata *raw=NULL;
    ProSurfacedata *s;
    ProError error;
    if(status)return status;
    error=ProSurfaceDataGet(surface,&raw);
    if(error)return error;
    s=raw->data.p_surface_data;
    if(s->type==PRO_SRF_CYL){
        const ProCylinderdata *c=&s->srf_shape.cylinder;
        if(!part_close(c->radius,geometry->radius)||!part_close(c->origin[0],0)||!part_close(c->origin[1],0)||
           !part_close(c->e3[0],0)||!part_close(c->e3[1],0)||!part_close(fabs(c->e3[2]),1))error=PRO_TK_GENERAL_ERROR;
        else geometry->cylinders++;
    }else if(s->type==PRO_SRF_PLANE){
        const ProPlanedata *plane=&s->srf_shape.plane;
        if(!part_close(fabs(plane->e3[2]),1))error=PRO_TK_GENERAL_ERROR;
        else if(part_close(plane->origin[2],0))geometry->bottom++;
        else if(part_close(plane->origin[2],geometry->height))geometry->top++;
        else error=PRO_TK_GENERAL_ERROR;
    }else error=PRO_TK_GENERAL_ERROR;
    ProGeomitemdataFree(&raw);
    return error;
}

static ProError centered_cylinder_verify(ProMdl model,const ModelPlan *plan){
    CenteredCylinderGeometry geometry={0};
    ProUnitsystem units;
    ProUnititem mm,length;
    ProUnitConversion conversion;
    ProSolidBody *bodies=NULL;
    ProMassProperty mass;
    ProError error;
    int count=0,active=0,i;
    double scale;
    if((error=ProMdlPrincipalunitsystemGet(model,&units))||
       (error=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&length))||
       (error=ProUnitInit(model,L"mm",&mm))||
       (error=ProUnitConversionCalculate(&mm,&length,&conversion)))return error;
    scale=conversion.scale;
    if(!isfinite(scale)||scale<=0||conversion.offset!=0)return PRO_TK_BAD_CONTEXT;
    geometry.radius=plan->a*scale/2;
    geometry.height=plan->height*scale;
    error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(error)return error;
    error=ProArraySizeGet((ProArray)bodies,&count);
    for(i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(!error&&state==PRO_BODY_STATE_ACTIVE){
            active++;
            error=ProSolidBodySurfaceVisit(&bodies[i],centered_cylinder_surface,&geometry);
        }
    }
    ProArrayFree((ProArray *)&bodies);
    if(error)return error;
    if(active!=1||geometry.cylinders<1||geometry.bottom!=1||geometry.top!=1)return PRO_TK_GENERAL_ERROR;
    error=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass);
    if(error)return error;
    return part_close(mass.volume,model_plan_volume(plan)*scale*scale*scale)?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError centered_cylinder_step(ProMdl model,const wchar_t *protocol){
    PartCommand command;
    static wchar_t receipt[PART_TEXT_MAX];
    if(part_command_parse(protocol,&command)!=1)return PRO_TK_BAD_INPUTS;
    return foundation_execute(&command,model,receipt,PART_TEXT_MAX);
}

static ProError model_create_centered_cylinder(const ModelPlan *plan,ProMdlName name,const char **stage){
    wchar_t protocol[256];
    ProMdl model=NULL;
    ProError error;
    int background=model_background_mode,window;
#define CC_TRY(label,call) do{*stage=label;error=(call);trace(label,error);if(error)goto done;}while(0)
    if(!plan->centered||!model_plan_valid(plan))return PRO_TK_BAD_INPUTS;
    model_background_mode=1;
    error=model_create_empty(1,name,stage);
    model_background_mode=background;
    if(error)return error;
    CC_TRY("Read new cylinder template copy",ProMdlnameInit(name,PRO_MDL_PART,&model));
    swprintf_s(protocol,_countof(protocol),L"CREO_OP SKETCH_CREATE %ls CYL_PROFILE PLANE:FRONT PLANE:RIGHT CIRCLE 0 0 %.17g",name,plan->a/2);
    CC_TRY("Create centered circle on FRONT (orientation RIGHT)",centered_cylinder_step(model,protocol));
    swprintf_s(protocol,_countof(protocol),L"CREO_OP SOLID_EXTRUDE %ls CYL_ADD CYL_PROFILE ADD BLIND %.17g NEG",name,plan->height);
    CC_TRY("Extrude the centered profile toward +Z",centered_cylinder_step(model,protocol));
    CC_TRY("Verify centered cylinder axis, end faces and volume",centered_cylinder_verify(model,plan));
    if(!background){
        CC_TRY("Open verified cylinder window",ProObjectwindowMdlnameCreate(name,PRO_PART,&window));
        CC_TRY("Set cylinder graphics context",ProWindowCurrentSet(window));
        CC_TRY("Display verified cylinder",ProMdlDisplay(model));
        CC_TRY("Activate verified cylinder",ProWindowActivate(window));
    }
done:
    if(error&&model){
        if(fd_last_sketch_model==model){fd_last_sketch_model=NULL;fd_last_sketch[0]=0;}
        trace("Erase incomplete new cylinder from session",ProMdlErase(model));
    }
    return error;
#undef CC_TRY
}
