static ProError ad_spline_file(const PartCommand *cmd,double scale,ProVector points[8],int *count,ProPath file){
    ProPath profile,directory;
    char contents[1600]="open arclength\nbegin section\nbegin curve\n";
    size_t used=strlen(contents);
    DWORD length=GetEnvironmentVariableW(L"BWTL_PROFILE_ROOT",profile,_countof(profile)),attributes,written;
    HANDLE handle;
    int i,j,n;
    BOOL ok;
    *count=(cmd->count-4)/3;
    if(!length||length>=_countof(profile)||swprintf_s(directory,_countof(directory),L"%ls\\datum-inputs",profile)<0)return PRO_TK_BAD_CONTEXT;
    if(!CreateDirectoryW(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return PRO_TK_CANT_WRITE;
    attributes=GetFileAttributesW(directory);
    if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return PRO_TK_BAD_CONTEXT;
    for(i=0;i<*count;i++){
        for(j=0;j<3;j++){part_number(cmd->arg[4+3*i+j],&points[i][j]);points[i][j]*=scale;}
        for(j=0;j<i;j++)if(ad_near(points[i],points[j],0.001*scale))return PRO_TK_BAD_INPUTS;
        n=sprintf_s(contents+used,_countof(contents)-used,"%.15g %.15g %.15g\n",points[i][0],points[i][1],points[i][2]);
        if(n<0)return PRO_TK_CANT_WRITE;
        used+=(size_t)n;
    }
    if(swprintf_s(file,PRO_PATH_SIZE,L"%ls\\spline_%lx_%llx.ibl",directory,GetCurrentProcessId(),GetTickCount64())<0)return PRO_TK_CANT_WRITE;
    handle=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(handle==INVALID_HANDLE_VALUE){file[0]=0;return PRO_TK_CANT_WRITE;}
    ok=WriteFile(handle,contents,(DWORD)used,&written,NULL)&&written==used;
    CloseHandle(handle);
    if(!ok){DeleteFileW(file);file[0]=0;return PRO_TK_CANT_WRITE;}
    return PRO_TK_NO_ERROR;
}

static ProError ad_spline(FdTxn *tx,const PartCommand *cmd,FdRef ref[3],ProPath file){
    ProCsys csys;
    ProCurve curve;
    ProGeomitem item;
    ProEnttype type;
    ProGeomitemdata *raw=NULL;
    ProVector local[8],xyz,actual;
    double param;
    int count,i,j;
    ProError e;
#define SPLINE_STEP(call) do{e=(call);if(e)goto done;}while(0)
    SPLINE_STEP(fd_ref_resolve(tx->model,cmd->arg[3],PRO_CSYS,&ref[0]));
    SPLINE_STEP(ProGeomitemToCsys(&ref[0].item,&csys));
    SPLINE_STEP(ProCsysDataGet(csys,&raw));
    tx->stage="write isolated spline coordinate input";
    SPLINE_STEP(ad_spline_file(cmd,tx->scale,local,&count,file));
    tx->stage="import interpolated native spline";
    e=ProDatumcurveFromfileCreate((ProSolid)tx->model,file,PRO_INTF_IBL,csys,&tx->feature);
    if(tx->feature.owner==tx->model&&tx->feature.id>0&&!fd_was_feature(tx,tx->feature.id))tx->created=1;
    if(e)goto done;
    if(!tx->created){e=PRO_TK_GENERAL_ERROR;goto done;}
    SPLINE_STEP(ProModelitemNameSet((ProModelitem *)&tx->feature,tx->name));
    tx->stage="verify spline type and all input points";
    SPLINE_STEP(ad_single_curve(&tx->feature,&item,&curve));
    SPLINE_STEP(ProCurveTypeGet(curve,&type));
    if(type!=PRO_ENT_SPLINE&&type!=PRO_ENT_B_SPLINE){e=PRO_TK_INVALID_TYPE;goto done;}
    for(i=0;i<count;i++){
        ProCsysdata *frame=raw->data.p_csys_data;
        for(j=0;j<3;j++)xyz[j]=frame->origin[j]+local[i][0]*frame->x_vector[j]+local[i][1]*frame->y_vector[j]+local[i][2]*frame->z_vector[j];
        SPLINE_STEP(ProCurveParamEval(curve,xyz,&param));
        SPLINE_STEP(ProCurveXyzdataEval(curve,param,actual,NULL,NULL));
        if(!ad_near(xyz,actual,0.001*tx->scale)){e=PRO_TK_GENERAL_ERROR;goto done;}
    }
done:
    if(raw)ProGeomitemdataFree(&raw);
    return e;
#undef SPLINE_STEP
}
