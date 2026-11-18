/* Check only the requested Creo file type, including its disk revisions. */
static ProError model_named_disk(const wchar_t *directory,const wchar_t *name,const wchar_t *ext,
                                    ProPath newest,int *occupied){
    ProPath pattern;
    if(newest)newest[0]=0;
    *occupied=0;
    if(swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.%ls*",directory,name,ext)<0)
        return PRO_TK_BAD_INPUTS;
    WIN32_FIND_DATAW file;
    HANDLE find=FindFirstFileW(pattern,&file);
    if(find==INVALID_HANDLE_VALUE)
        return GetLastError()==ERROR_FILE_NOT_FOUND?PRO_TK_NO_ERROR:PRO_TK_CANT_OPEN;
    size_t suffix=wcslen(name)+1+wcslen(ext);
    int latest=-1;
    do{
        size_t length=wcslen(file.cFileName);
        if(length<suffix||(file.cFileName[suffix]&&file.cFileName[suffix]!=L'.'))continue;
        *occupied=1;
        if(!newest||(file.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
           (!file.nFileSizeHigh&&!file.nFileSizeLow))continue;
        const wchar_t *number=file.cFileName+suffix;
        int revision=0;
        if(*number){
            wchar_t *end=NULL;
            long value=wcstol(number+1,&end,10);
            if(end==number+1||*end||value<1)continue;
            revision=(int)value;
        }
        if(revision>latest){
            if(swprintf_s(newest,PRO_PATH_SIZE,L"%ls\\%ls",directory,file.cFileName)<0){
                FindClose(find);
                return PRO_TK_BAD_INPUTS;
            }
            latest=revision;
        }
    }while(FindNextFileW(find,&file));
    DWORD reason=GetLastError();
    FindClose(find);
    return reason==ERROR_NO_MORE_FILES?PRO_TK_NO_ERROR:PRO_TK_CANT_OPEN;
}

static ProError model_assembly_disk(const wchar_t *directory,const wchar_t *name,
                                    ProPath newest,int *occupied){
    return model_named_disk(directory,name,L"asm",newest,occupied);
}

static ProError model_named_available(const wchar_t *name,const wchar_t *directory,ProMdlType type){
    if(!model_assembly_name_valid(name))return PRO_TK_INVALID_NAME;
    ProMdl existing=NULL;
    ProError error=ProMdlnameInit((wchar_t *)name,type,&existing);
    if(error!=PRO_TK_E_NOT_FOUND)return error?error:PRO_TK_E_FOUND;
    int occupied;
    error=model_named_disk(directory,name,type==PRO_MDL_ASSEMBLY?L"asm":L"prt",NULL,&occupied);
    return error?error:occupied?PRO_TK_E_FOUND:PRO_TK_NO_ERROR;
}

static ProError model_assembly_available(const wchar_t *name,const wchar_t *directory){
    return model_named_available(name,directory,PRO_MDL_ASSEMBLY);
}
#ifdef BWTL_RUNTIME_PATHS_H
typedef struct {ProMdl model;ProMdlName name;ProPath directory,origin;} BwtlCreatedAssembly;
static BwtlCreatedAssembly model_created_assembly[64];
static unsigned model_created_assembly_slot;

static int model_assembly_creation_index(ProMdl model,const wchar_t *name,
                                         const wchar_t *directory,const wchar_t *origin){
    for(int i=0;i<_countof(model_created_assembly);i++){
        const BwtlCreatedAssembly *entry=&model_created_assembly[i];
        if(entry->model==model&&!_wcsicmp(entry->name,name)&&
           !_wcsicmp(entry->directory,directory)&&!_wcsicmp(entry->origin,origin))return i;
    }
    return -1;
}
static void model_assembly_creation_record(ProMdl model,const wchar_t *name){
    ProPath directory,origin;
    if(ProDirectoryCurrentGet(directory)||ProMdlOriginGet(model,origin))return;
    unsigned slot=model_created_assembly_slot++%_countof(model_created_assembly);
    BwtlCreatedAssembly *entry=&model_created_assembly[slot];
    entry->model=model;
    wcscpy_s(entry->name,_countof(entry->name),name);
    wcscpy_s(entry->directory,_countof(entry->directory),directory);
    wcscpy_s(entry->origin,_countof(entry->origin),origin);
}
#endif
static ProError model_create_empty_named(int kind,const wchar_t *requested,ProMdlName name,const char **stage) {
    static unsigned serial;
    SYSTEMTIME now;ProPath path;ProMdl source=NULL,copy=NULL;
    ProMdlType type;ProMdlsubtype subtype;
    ProMdlfileType filetype=kind==3?PRO_MDLFILE_ASSEMBLY:PRO_MDLFILE_PART;
    TemplateDatums before={0},after={0};ProError error=PRO_TK_BAD_INPUTS;int window;
    if(kind<1||kind>3){*stage="Unsupported empty model type";return error;}
    if(requested&&requested[0]){

        ProPath directory;
        MODEL_TRY("Read current Creo working directory",ProDirectoryCurrentGet(directory));
#ifdef BWTL_RUNTIME_PATHS_H
        if(!storage_output_directory(directory)){*stage="Select the configured data drive";return PRO_TK_BAD_CONTEXT;}
#endif
        MODEL_TRY("Check model name in session and working directory",model_named_available(requested,directory,kind==3?PRO_MDL_ASSEMBLY:PRO_MDL_PART));
        wcscpy_s(name,PRO_NAME_SIZE,requested);
    }else{
        GetLocalTime(&now);
        swprintf_s(name,PRO_NAME_SIZE,L"ai_%ls_%02u%02u%02u_%02u%02u%02u_%u",kind==2?L"smt":kind==3?L"asm":L"prt",now.wYear%100,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,++serial);
    }
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
    if(requested&&requested[0]){
        ProMdlName actual;
        MODEL_TRY("Read back named model",ProMdlMdlnameGet(copy,actual));
        if(_wcsicmp(actual,name)){*stage="Named model readback mismatch";error=PRO_TK_GENERAL_ERROR;goto cleanup;}
    }
    if((kind==2&&subtype!=PROMDLSTYPE_PART_SHEETMETAL)||(kind==1&&subtype==PROMDLSTYPE_PART_SHEETMETAL)){error=PRO_TK_BAD_CONTEXT;goto cleanup;}
    MODEL_TRY("Inspect copied template features",ProSolidFeatVisit((ProSolid)copy,template_feature,NULL,&after));
    trace("Original template feature count",before.total);trace("Copied template feature count",after.total);
    trace("Copied datum planes",after.planes);trace("Copied coordinate systems",after.csys);
    if(before.total!=after.total||before.planes!=after.planes||before.csys!=after.csys){*stage="Template feature preservation mismatch";error=PRO_TK_GENERAL_ERROR;goto cleanup;}
    if(kind!=3)MODEL_TRY("Verify copy remains empty",template_empty((ProSolid)copy));
#ifdef BWTL_RUNTIME_PATHS_H
    else model_assembly_creation_record(copy,name);
#endif
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

static ProError model_create_empty(int kind,ProMdlName name,const char **stage){
    return model_create_empty_named(kind,NULL,name,stage);
}