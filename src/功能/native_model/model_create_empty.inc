static ProError model_create_empty(int kind, ProMdlName name, const char **stage) {
    static unsigned serial;
    SYSTEMTIME now;ProPath path;ProMdl source=NULL,copy=NULL;
    ProMdlType type;ProMdlsubtype subtype;
    ProMdlfileType filetype=kind==3?PRO_MDLFILE_ASSEMBLY:PRO_MDLFILE_PART;
    TemplateDatums before={0},after={0};ProError error=PRO_TK_BAD_INPUTS;int window;
    if(kind<1||kind>3){*stage="Unsupported empty model type";return error;}
    GetLocalTime(&now);
    swprintf_s(name,PRO_NAME_SIZE,L"ai_%ls_%02u%02u%02u_%02u%02u%02u_%u",kind==2?L"smt":kind==3?L"asm":L"prt",now.wYear%100,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,++serial);
    MODEL_TRY("Read requested default template",ProConfigoptionGet(kind==2?L"template_sheetmetalpart":kind==3?L"template_designasm":L"template_solidpart",path));
    if(!path[0]){error=PRO_TK_E_NOT_FOUND;goto cleanup;}
    MODEL_TRY("Load requested template",ProMdlFiletypeLoad(path,filetype,PRO_B_FALSE,&source));
    MODEL_TRY("Read template model type",ProMdlTypeGet(source,&type));
    if(type!=(kind==3?PRO_MDL_ASSEMBLY:PRO_MDL_PART)){*stage="Configured template has wrong model type";error=PRO_TK_BAD_CONTEXT;goto cleanup;}
    MODEL_TRY("Read template subtype",ProMdlSubtypeGet(source,&subtype));
    if((kind==2 && subtype!=PROMDLSTYPE_PART_SHEETMETAL)||(kind==1 && subtype==PROMDLSTYPE_PART_SHEETMETAL)){
        *stage="Configured template has wrong solid or sheetmetal subtype";error=PRO_TK_BAD_CONTEXT;goto cleanup;
    }
    MODEL_TRY("Inspect original template features",ProSolidFeatVisit((ProSolid)source,template_feature,NULL,&before));
    if(kind!=3)MODEL_TRY("Check template has no solid geometry",template_empty((ProSolid)source));
    else if(before.components){*stage="Assembly template contains components";error=PRO_TK_BAD_CONTEXT;goto cleanup;}
    MODEL_TRY("Copy requested empty template",ProMdlnameCopy(source,name,&copy));
    MODEL_TRY("Verify copied model subtype",ProMdlSubtypeGet(copy,&subtype));
    if(kind==2 && subtype!=PROMDLSTYPE_PART_SHEETMETAL){error=PRO_TK_BAD_CONTEXT;goto cleanup;}
    MODEL_TRY("Inspect copied template features",ProSolidFeatVisit((ProSolid)copy,template_feature,NULL,&after));
    trace("Original template feature count",before.total);trace("Copied template feature count",after.total);
    trace("Copied datum planes",after.planes);trace("Copied coordinate systems",after.csys);
    if(before.total!=after.total||before.planes!=after.planes||before.csys!=after.csys){*stage="Template feature preservation mismatch";error=PRO_TK_GENERAL_ERROR;goto cleanup;}
    if(kind!=3)MODEL_TRY("Verify copy remains empty",template_empty((ProSolid)copy));
    if(!model_background_mode){
    MODEL_TRY("Open empty model window",ProObjectwindowMdlnameCreate(name,kind==3?PRO_ASSEMBLY:PRO_PART,&window));
    MODEL_TRY("Set empty model window",ProWindowCurrentSet(window));
    MODEL_TRY("Display empty model",ProMdlDisplay(copy));
    /* CurrentSet only changes Toolkit's graphics context. Activation is
       deferred until this command returns to Creo and updates model UI. */
    MODEL_TRY("Activate empty model window",ProWindowActivate(window));
    }
cleanup:return error;
}
