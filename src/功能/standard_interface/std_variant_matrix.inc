
#define STD_VARIANT_LIMIT 8

typedef struct {wchar_t symbol[PRO_NAME_SIZE];double mm;} StdDriver;

static int std_drivers(const wchar_t *input,StdDriver drivers[STD_VARIANT_LIMIT]){
    wchar_t list[OR_CELL],*context=NULL;
    if(!*input||wcslen(input)>=_countof(list)||input[0]==L','||input[wcslen(input)-1]==L','||wcsstr(input,L",,"))return 0;
    wcscpy_s(list,_countof(list),input);
    int count=0;
    for(wchar_t *assignment=wcstok_s(list,L",",&context);assignment;assignment=wcstok_s(NULL,L",",&context)){
        wchar_t *value=wcschr(assignment,L'=');
        if(!value||value==assignment||count==STD_VARIANT_LIMIT)return 0;
        *value++=0;
        or_trim(assignment);
        if(wcslen(assignment)>=PRO_NAME_SIZE||!or_number(value,&drivers[count].mm)||drivers[count].mm<=0.01)return 0;
        wchar_t *symbol=assignment;
        if(*symbol!=L'd'&&*symbol!=L'D')return 0;
        if(!iswdigit(*++symbol))return 0;
        for(;*symbol;symbol++)if(*symbol<L'0'||*symbol>L'9')return 0;
        for(int i=0;i<count;i++)if(!_wcsicmp(drivers[i].symbol,assignment))return 0;
        wcscpy_s(drivers[count].symbol,PRO_NAME_SIZE,assignment);
        count++;
    }
    return count;
}

static ProError std_variant_edit(ProMdl model,const StdDriver *driver,int count){
    Revision changes;
    ProError error=rv_begin(&changes,model);
    for(int i=0;!error&&i<count;i++){
        UxDimension dim;
        ProFeature feat;
        ProDimensiontype type;
        error=ux_dimension(model,driver[i].symbol,&dim);
        if(!error)error=ProDimensionTypeGet(&dim.dim,&type);
        if(!error&&type!=PRODIMTYPE_LINEAR&&type!=PRODIMTYPE_DIAMETER&&type!=PRODIMTYPE_RADIUS)error=PRO_TK_INVALID_TYPE;
        if(!error)error=ProDimensionOwnerfeatureGet(&dim.dim,&feat);
        if(!error)error=rv_add(&changes,dim.dim,feat.id,dim.value,driver[i].mm/changes.mm);
    }
    if(!error)error=rv_apply(&changes);
    return error==PRO_TK_NO_CHANGE?PRO_TK_NO_ERROR:error;
}

static ProError std_variant_recheck(ProMdl model,const StdDriver *driver,int n){
    for(int i=0;i<n;i++){
        wchar_t text[PRO_NAME_SIZE+5];
        double readback=0;
        swprintf_s(text,_countof(text),L"DIM:%ls",driver[i].symbol);
        ProError error=or_read_number(model,text,&readback);
        if(error||fabs(readback-driver[i].mm)>0.005)return error?error:PRO_TK_INCOMPLETE;
    }
    return PRO_TK_NO_ERROR;
}

static ProError std_variant_erase(ProMdl *model,ProError error){
    if(!*model)return error;
    ProError cleanup=ProMdlErase(*model);
    if(!cleanup)*model=NULL;
    return error?error:cleanup;
}

static ProError std_variant_one(ProMdl source,const StdInterface *template,const wchar_t *name,
    const StdDriver *driver,int n,const wchar_t *scratch,StdInterface *measured,PartText *out){
    ProMdl probe=NULL;
    ProMdlName local;
    wcscpy_s(local,_countof(local),name);
    ProError error=ProMdlnameCopy(source,local,&probe);
    if(!error)error=std_variant_edit(probe,driver,n);
    if(!error)error=std_variant_recheck(probe,driver,n);
    if(!error)error=ProMdlSave(probe);
    ProPath stored;
    wchar_t absolute[MAX_PATH];
    if(!error)error=ProMdlOriginGet(probe,stored);
    if(!error&&(!or_real_path(stored,0,absolute)||!or_below(absolute,scratch)))error=PRO_TK_CANT_WRITE;
    error=std_variant_erase(&probe,error);
    if(!error)error=ProMdlFiletypeLoad(absolute,PRO_MDLFILE_PART,PRO_B_FALSE,&probe);
    if(!error)error=std_variant_recheck(probe,driver,n);
    wchar_t holes[MAX_PATH]=L"";
    for(int i=0;!error&&i<template->count;i++){
        if((i&&wcscat_s(holes,_countof(holes),L","))||wcscat_s(holes,_countof(holes),template->hole[i].ref))
            error=PRO_TK_OUT_OF_RANGE;
    }
    if(!error)error=std_measure(probe,template->revision,template->face,template->frame,holes,measured,out);
    if(!error){
        StdDriver temporary=driver[0];
        temporary.mm+=fmin(0.25,temporary.mm*0.005);
        error=std_variant_edit(probe,&temporary,1);
        if(!error)error=std_variant_recheck(probe,&temporary,1);
        if(!error)error=std_variant_edit(probe,driver,1);
        if(!error)error=std_variant_recheck(probe,driver,n);
    }
    return std_variant_erase(&probe,error);
}

static ProError std_variant_matrix(ProMdl source,const wchar_t *key,const wchar_t *revision,const wchar_t *plan,PartText *out){
    StdRecord part={0};
    ProError error=std_lookup(key,revision,&part);
    ProMdlName original;
    ProBoolean modified=PRO_B_TRUE;
    char before[240],after[240];
    if(!error)error=source?ProMdlMdlnameGet(source,original):PRO_TK_BAD_CONTEXT;
    if(!error)error=ProMdlModificationVerify(source,&modified);
    if(!error)error=ax_study_verstamp(source,before);
    if(!error&&(modified||_wcsicmp(original,part.geometry.model)||strcmp(before,part.geometry.stamp))){
        part_text_add(out,L"请打开与已登记标准接口同名、同版本的已保存零件再做规格试改。\n");
        error=PRO_TK_BAD_CONTEXT;
    }
    if(!error)error=dv_no_external_dependencies(source);
    if(error)return error;
    OrTable rows={0};
    StdDriver drivers[STD_VARIANT_LIMIT][STD_VARIANT_LIMIT];
    int sizes[STD_VARIANT_LIMIT]={0};
    if(!or_table_load(&rows,plan,2,out)||rows.size<2||rows.size>STD_VARIANT_LIMIT||!or_unique_column(&rows,0,out))
        error=PRO_TK_BAD_INPUTS;
    for(int i=0;!error&&i<rows.size;i++){
        sizes[i]=std_drivers(rows.rows[i].value[1],drivers[i]);
        if(!or_safe_id(rows.rows[i].value[0])||!sizes[i])error=PRO_TK_BAD_INPUTS;
    }
    if(error){or_free(&rows);return error;}
    ProPath previous;
    wchar_t base[MAX_PATH],scratch[MAX_PATH],receipt[MAX_PATH];
    static unsigned serial;
    error=ProDirectoryCurrentGet(previous);
    if(!error&&(!storage_method_directory(base,L"标准件规格试改",1)||
        swprintf_s(scratch,_countof(scratch),L"%ls\\probe-%lx-%llx-%u",base,GetCurrentProcessId(),GetTickCount64(),++serial)<0||
        swprintf_s(receipt,_countof(receipt),L"%ls\\重开验收.tsv",scratch)<0||!CreateDirectoryW(scratch,NULL)))error=PRO_TK_CANT_WRITE;
    if(error){or_free(&rows);return error;}
    FILE *write=_wfopen(receipt,L"wtx, ccs=UTF-8");
    if(!write){or_free(&rows);return PRO_TK_CANT_WRITE;}
    fwprintf(write,L"BWTL-STANDARD-MATRIX-1\t%ls\t%ls\t%hs\t%d\n",part.key,part.geometry.revision,before,rows.size);
    int passed=0,entered=0;
    error=ProDirectoryChange(scratch);
    if(!error){
        entered=1;
        if(!storage_prepare_output())error=PRO_TK_CANT_WRITE;
    }
    for(int i=0;!error&&i<rows.size;i++){
        StdInterface candidate={0};
        ProMdlName name;
        swprintf_s(name,_countof(name),L"BW_SIZE_%lx_%u_%d",GetCurrentProcessId(),serial,i+1);
        rv_recovery_uncertain=0;
        ProError verdict=std_variant_one(source,&part.geometry,name,drivers[i],sizes[i],scratch,&candidate,out);
        passed+=!verdict;
        fwprintf(write,L"%ls\t%ls\t%d\t%d\t%.17g\t%.17g\t%.17g\n",rows.rows[i].value[0],rows.rows[i].value[1],
            verdict,candidate.count,candidate.box[0],candidate.box[1],candidate.box[2]);
        part_text_add(out,L"%ls，%ls。%ls。接口孔数 %d，外形 %.6g × %.6g × %.6g mm。\n",
            rows.rows[i].value[0],rows.rows[i].value[1],verdict?L"试改、重开或再编辑未通过":L"换规格保存、重开、再改尺寸和实测接口通过",
            candidate.count,candidate.box[0],candidate.box[1],candidate.box[2]);
        if(rv_recovery_uncertain)error=PRO_TK_INCOMPLETE;
    }
    if(entered){
        ProError restore=ProDirectoryChange(previous);
        if(!error)error=restore;
    }
    if(!error)error=ax_study_verstamp(source,after);
    if(!error&&strcmp(before,after))error=PRO_TK_INCOMPLETE;
    fwprintf(write,L"PASS\t%d\t%d\t%d\n",passed,rows.size,error);
    int failed=ferror(write);
    if(fclose(write)||failed)error=PRO_TK_CANT_WRITE;
    part_text_add(out,L"规格通过 %d/%d 组。验收回执 %ls。\n",passed,rows.size,receipt);
    if(passed!=rows.size&&!error)error=PRO_TK_INCOMPLETE;
    or_free(&rows);
    return error;
}
