static ProError fab_csv_open(const wchar_t *folder,const wchar_t *leaf,FILE **file){
    ProPath path;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",folder,leaf)<0)return PRO_TK_BAD_INPUTS;
    *file=_wfopen(path,L"wt, ccs=UTF-8");
    return *file?PRO_TK_NO_ERROR:PRO_TK_CANT_WRITE;
}

static ProError fab_close(FILE *file,ProError error){
    if(file){
        int failed=ferror(file)||fflush(file)||_commit(_fileno(file));
        if(fclose(file)||failed){if(!error)error=PRO_TK_CANT_WRITE;}
    }
    return error;
}

static ProError fab_discard(DeliveryOutput *work,ProError error,PartText *out){
    if(!work->owned)return error;
    if(work->in_stage){
        ProError restore=ProDirectoryChange(work->parent);
        if(restore){
            dv_recovery_uncertain=1;
            part_text_add(out,L"工作目录恢复未能核实，临时目录 %ls。",work->stage);
            return restore;
        }
        work->in_stage=0;
    }
    if(!dv_stage_clean(work)){
        part_text_add(out,L"临时检查目录未能清理 %ls。",work->stage);
        if(!error)error=PRO_TK_CANT_WRITE;
    }
    work->owned=0;
    return error;
}

static ProError fab_scratch(DeliveryOutput *work){
    wchar_t name[64];
    static LONG serial;
    swprintf_s(name,_countof(name),L"fb_%08lx_%08lx",GetCurrentProcessId(),InterlockedIncrement(&serial));
    return dv_stage_begin(name,work);
}

static ProError fab_thickness(ProMdl model,double *mm,double *thickness){
    ProMdlsubtype subtype;
    ProDimension dim;
    double actual;
    ProError error=ProMdlSubtypeGet(model,&subtype);
    if(error||subtype!=PROMDLSTYPE_PART_SHEETMETAL||sh_recovery_uncertain)return error?error:PRO_TK_BAD_CONTEXT;
    if((error=qa_single_body(model,NULL))||(error=qa_mm_per_unit(model,mm))||
       (error=ProSmtPartThicknessGet((ProPart)model,&dim))||(error=ProDimensionValueGet(&dim,&actual)))return error;
    *thickness=actual * *mm;
    return *thickness>0?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError fab_native_flat(ProMdl model,const wchar_t *base,const wchar_t *face){
    double mm,thickness;
    InspectionPlane plane;
    ProSmtSurfType side;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=in_plane(model,face,&plane);
    if(!error)error=ProSmtSurfaceTypeGet((ProPart)model,plane.handle,&side);
    if(error||side!=PRO_SMT_SURF_FACE)return error?error:PRO_TK_BAD_CONTEXT;
    PartCommand export_cmd={0};
    wcscpy_s(export_cmd.arg[2],PART_TOKEN_MAX,base);
    wcscpy_s(export_cmd.arg[3],PART_TOKEN_MAX,face);
    wchar_t internal[512];
    return sh_export(&export_cmd,model,internal,_countof(internal));
}

static ProError fab_leaf_path(const wchar_t *directory,const wchar_t *base,const wchar_t *extension,ProPath path){
    return swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls%ls",directory,base,extension)<0?PRO_TK_BAD_INPUTS:PRO_TK_NO_ERROR;
}
