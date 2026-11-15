static ProError smy_flat_compare(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput work={0};
    FILE *index=NULL;
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d)return PRO_TK_OUT_OF_MEMORY;
    ProError error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=fab_csv_open(work.stage,L"fixed-face-options.csv",&index);
    if(!error)fwprintf(index,L"参照,选择,文件,宽_mm,高_mm,最小X_mm,最小Y_mm,孔槽图元,折弯线\n");
    int passed=0;
    for(int i=3;!error&&i<cmd->count;i++){
        ProPath path;
        wchar_t name[32];
        swprintf_s(name,_countof(name),L"fixed_%ls",cmd->arg[i]+8);
        ProError trial=fab_flat_read(model,work.stage,name,cmd->arg[i],d,path);
        if(trial){
            part_text_add(out,L"%ls 原生展开失败（Creo %d）。\n",cmd->arg[i],trial);
            if(i==3)error=trial;
            continue;
        }
        double w=d->high[0]-d->low[0],h=d->high[1]-d->low[1];
        fwprintf(index,L"%ls,%ls,%ls.dxf,%.10g,%.10g,%.10g,%.10g,%d,%d\n",cmd->arg[i],
            i==3?L"已选":L"备选",name,w,h,d->low[0],d->low[1],d->holes,d->bends);
        part_text_add(out,L"%ls %ls：包络 %.8g × %.8g mm，原点相对外框 %.8g / %.8g mm，%d 条折弯线。\n",
            cmd->arg[i],i==3?L"已选":L"备选",w,h,-d->low[0],-d->low[1],d->bends);
        passed++;
    }
    free(d);
    if(!error&&passed<2)error=PRO_TK_BAD_CONTEXT;
    error=dv_stage_finish(&work,fab_close(index,error),out);
    if(!error)part_text_add(out,L"已保存选定固定面、备选方案和原生展开文件：%ls。\n",work.target);
    return error;
}

static ProError smy_flat_match(ProMdl model,const PartCommand *cmd,const wchar_t *source,PartText *out){
    DeliveryOutput scratch={0};
    FabDxf *d=calloc(1,sizeof(*d)),*native=calloc(1,sizeof(*native));
    if(!d||!native){free(d);free(native);return PRO_TK_OUT_OF_MEMORY;}
    ProPath path;
    ProError error=fab_dxf_read(source,d);
    if(!error)error=fab_scratch(&scratch);
    if(!error)error=dv_stage_enter(&scratch);
    if(!error)error=fab_flat_read(model,scratch.stage,L"match",cmd->arg[3],native,path);
    double a,b,tolerance=as_arg(cmd,4);
    int box;
    if(!error)error=smy_cut_area(d,&a,&box);
    if(!error)error=smy_cut_area(native,&b,&box);
    if(!error){
        part_text_add(out,L"文件/原生展开净面积 %.8g / %.8g mm²，包络 %.8g × %.8g / %.8g × %.8g mm，孔槽图元 %d / %d。\n",
            a,b,d->high[0]-d->low[0],d->high[1]-d->low[1],native->high[0]-native->low[0],native->high[1]-native->low[1],d->holes,native->holes);
        unsigned char used[FAB_DXF_MAX]={0};
        int differing=0;
        for(int i=0;i<d->count;i++){
            int match=-1;
            for(int j=0;j<native->count;j++)
                if(!used[j]&&fab_same_entity(&d->entities[i],&native->entities[j],tolerance)){match=j;break;}
            if(match>=0){used[match]=1;continue;}
            const FabEntity *e=&d->entities[i];
            const double *p=e->type==1?e->a:e->center;
            if(++differing<=20)part_text_add(out,L"文件图元 #%d 图层 %d，位置 %.8g / %.8g mm 未匹配原生图元。\n",
                i+1,e->layer,p[0],p[1]);
        }
        for(int j=0;j<native->count;j++)if(!used[j]){
            const FabEntity *e=&native->entities[j];
            const double *p=e->type==1?e->a:e->center;
            if(++differing<=20)part_text_add(out,L"原生图元 #%d 图层 %d，位置 %.8g / %.8g mm 未匹配文件图元。\n",
                j+1,e->layer,p[0],p[1]);
        }
        if(differing)error=PRO_TK_BAD_CONTEXT;
        else part_text_add(out,L"文件和当前原生展开的图层、几何、位置及方向一致。\n");
    }
    free(d);free(native);
    return fab_discard(&scratch,error,out);
}

static ProError smy_flat_transform(const PartCommand *cmd,int kind,const ProPath directory,PartText *out){
    DeliveryOutput work={0};
    FabDxf *d=calloc(1,sizeof(*d)),*read=calloc(1,sizeof(*read));
    if(!d||!read){free(d);free(read);return PRO_TK_OUT_OF_MEMORY;}
    int clean=kind==SMY_FLAT_CLEAN,first=clean?2:6,last=clean?3:cmd->count;
    ProError error=dv_stage_begin(cmd->arg[clean?3:2],&work);
    for(int i=first;!error&&i<last;i++){
        ProPath source,destination;
        error=smy_stock_file(directory,cmd->arg[i],L".dxf",source);
        if(!error)error=fab_dxf_load(source,d,!clean);
        int removed=0,joins=0;
        if(!error&&clean){
            if(d->count>512)error=PRO_TK_OUT_OF_RANGE;
            else if(!smy_flat_clean_geometry(d,as_arg(cmd,4),&removed,&joins))error=PRO_TK_BAD_CONTEXT;
        }
        if(!error&&!clean)smy_flat_frame(d,as_arg(cmd,3),as_arg(cmd,4),as_arg(cmd,5));
        if(!error)error=fab_leaf_path(work.stage,cmd->arg[i],L".dxf",destination);
        if(!error)error=smy_flat_write(destination,d);
        if(!error)error=fab_dxf_read(destination,read);
        if(!error&&!fab_dxf_equivalent(d,read,0.0000001))error=PRO_TK_GENERAL_ERROR;
        if(!error){
            if(clean)part_text_add(out,L"%ls：移除 %d 个重复或零长图元，合拢 %d 对微小断点；输出已回读。\n",cmd->arg[i],removed,joins);
            else part_text_add(out,L"%ls：以指定点为零点并转向 %.8g°，%d 个原图元的变换结果已逐一回读。\n",
                cmd->arg[i],as_arg(cmd,5),d->count);
        }
    }
    free(d);free(read);
    error=dv_stage_finish(&work,error,out);
    if(!error)part_text_add(out,L"输出目录 %ls。\n",work.target);
    return error;
}

static ProError smy_flat_lock(const PartCommand *cmd,const ProPath directory,PartText *out){
    ProPath input;
    ProError error=smy_stock_file(directory,cmd->arg[2],L".tsv",input);
    if(error)return error;
    FILE *file=_wfopen(input,L"rt, ccs=UTF-8"),*index=NULL;
    if(!file)return PRO_TK_CANT_OPEN;
    DeliveryOutput work={0};
    FabDxf *d=calloc(1,sizeof(*d));
    error=d?dv_stage_begin(cmd->arg[3],&work):PRO_TK_OUT_OF_MEMORY;
    if(!error)error=fab_csv_open(work.stage,L"dxf-version-index.csv",&index);
    if(!error)fwprintf(index,L"料号,版本,材料,板厚_mm,数量,输出,字节,SHA256\n");
    wchar_t line[512];
    int row=0;
    while(!error&&fgetws(line,_countof(line),file)){
        if(!wcschr(line,L'\n')&&!feof(file)){error=PRO_TK_LINE_TOO_LONG;break;}
        wchar_t *p=line+wcsspn(line,L" \t\r\n"),*cursor=NULL;
        if(!*p||*p==L'#')continue;
        wchar_t *f[6];
        int n=0,quantity;
        for(wchar_t *t=wcstok_s(p,L" \t\r\n",&cursor);t;t=wcstok_s(NULL,L" \t\r\n",&cursor)){
            if(n==_countof(f)){n++;break;}
            f[n++]=t;
        }
        if(n!=_countof(f)||++row>128||!dv_safe_leaf(f[0])||!dv_safe_leaf(f[1])||!dv_safe_leaf(f[2])||
           !smx_number(f[3],0.01,100)||!part_integer(f[4],&quantity)||quantity<1||quantity>100000){
            error=PRO_TK_BAD_INPUTS;break;
        }
        wchar_t leaf[160],hash[65],verify[65];
        ProPath source,target;
        ULONGLONG bytes,actual;
        swprintf_s(leaf,_countof(leaf),L"%ls_%ls_%ls_T%.10g_Q%d.dxf",f[0],f[1],f[2],wcstod(f[3],NULL),quantity);
        error=smy_stock_file(directory,f[5],L".dxf",source);
        if(!error)error=fab_dxf_read(source,d);
        if(!error)error=fab_leaf_path(work.stage,leaf,L"",target);
        if(!error&&!dv_sha256(source,hash,&bytes))error=PRO_TK_CANT_OPEN;
        if(!error&&!CopyFileW(source,target,TRUE)){
            DWORD reason=GetLastError();
            error=reason==ERROR_FILE_EXISTS||reason==ERROR_ALREADY_EXISTS?PRO_TK_E_FOUND:PRO_TK_CANT_WRITE;
            if(error==PRO_TK_E_FOUND)part_text_add(out,L"输出名 %ls 重复，整批未交付。\n",leaf);
        }
        if(!error&&(!dv_sha256(target,verify,&actual)||actual!=bytes||wcscmp(hash,verify)))error=PRO_TK_GENERAL_ERROR;
        for(int i=0;!error&&i<3;i++)if(!dv_csv_text(index,f[i])||fputwc(L',',index)==WEOF)error=PRO_TK_CANT_WRITE;
        if(!error&&fwprintf(index,L"%.10g,%d,",wcstod(f[3],NULL),quantity)<0)error=PRO_TK_CANT_WRITE;
        if(!error&&(!dv_csv_text(index,leaf)||fwprintf(index,L",%llu,%ls\n",bytes,hash)<0))error=PRO_TK_CANT_WRITE;
    }
    if(!error&&(!row||ferror(file)))error=PRO_TK_BAD_INPUTS;
    fclose(file);free(d);
    error=dv_stage_finish(&work,fab_close(index,error),out);
    if(!error)part_text_add(out,L"已交付 %d 份按版本命名的 DXF，文件与 SHA-256 已逐份核对：%ls。\n",row,work.target);
    return error;
}

static ProError smy_flat_execute(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    if(kind==SMY_FLAT_COMPARE)return smy_flat_compare(model,cmd,out);
    ProPath directory,source;
    ProError error=ProDirectoryCurrentGet(directory);
    if(error)return error;
    if(kind==SMY_FLAT_LOCK)return smy_flat_lock(cmd,directory,out);
    if(kind==SMY_FLAT_DATUM||kind==SMY_FLAT_CLEAN)return smy_flat_transform(cmd,kind,directory,out);
    error=smy_stock_file(directory,cmd->arg[2],L".dxf",source);
    if(error)return error;
    if(kind==SMY_FLAT_MATCH)return smy_flat_match(model,cmd,source,out);
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d)return PRO_TK_OUT_OF_MEMORY;
    error=fab_dxf_load(source,d,0);
    if(!error){
        SmyFlatIssues issues=smy_flat_issues(d,out);
        if(!fab_dxf_contours(d)||issues.duplicates||issues.zero||issues.loose||issues.branches||issues.crossings||issues.islands)
            error=PRO_TK_BAD_CONTEXT;
    }
    free(d);
    return error;
}
