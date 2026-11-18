/* Read-only native library access. This runs on Creo's command thread.
   The caller resolves the source against the authorized library roots. */
static HANDLE company_library_source(const wchar_t *path,NativeFile *source,PartText *out){
    if(!path||wcslen(path)>=_countof(source->source)||towupper(path[0])!=storage_data_drive()||
       path[1]!=L':'||path[2]!=L'\\')return INVALID_HANDLE_VALUE;
    const wchar_t *leaf=wcsrchr(path,L'\\')+1;
    source->kind=native_file_kind(native_file_suffix(leaf));
    if(source->kind!=ATTACH_PART&&source->kind!=ATTACH_ASSEMBLY){
        part_text_add(out,L"库模型读取支持 Creo PRT、ASM 及其版本文件；STEP 请使用聊天附件分析。");
        return INVALID_HANDLE_VALUE;
    }
    HANDLE file=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE)return INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION info;
    wchar_t resolved[MAX_PATH];
    DWORD length=GetFinalPathNameByHandleW(file,resolved,_countof(resolved),VOLUME_NAME_DOS);
    int valid=length>=7&&length<_countof(resolved)&&!wcsncmp(resolved,L"\\\\?\\",4)&&!_wcsicmp(resolved+4,path)&&
        GetFileType(file)==FILE_TYPE_DISK&&GetFileInformationByHandle(file,&info)&&
        !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
        GetFileTime(file,NULL,NULL,&source->modified)&&GetFileSizeEx(file,&source->size)&&source->size.QuadPart>0;

    if(valid){
        wcscpy_s(source->source,_countof(source->source),path);
        wcscpy_s(source->name,_countof(source->name),leaf);
        return file;
    }
    CloseHandle(file);
    return INVALID_HANDLE_VALUE;
}

static void company_library_csys(const PartSnapshot *snapshot,PartText *out){
    int listed=0;
    for(int i=0;i<snapshot->features;i++){
        const PartFeature *f=&snapshot->feature[i];
        if(f->type!=PRO_FEAT_CSYS||f->status!=PRO_FEAT_ACTIVE||!fd_name(f->name))continue;
        if(listed==12){
            part_text_add(out,L"坐标系列表已达上限。\n");
            break;
        }
        ProFeature feature;
        ProGeomitem item;
        ProCsys csys;
        ProGeomitemdata *data=NULL;
        ProError error=ProFeatureInit((ProSolid)snapshot->model,f->id,&feature);
        if(!error)error=fd_feature_geom(&feature,PRO_CSYS,&item);
        if(!error)error=ProGeomitemToCsys(&item,&csys);
        if(!error)error=ProCsysDataGet(csys,&data);
        if(!error&&data&&data->data.p_csys_data){
            const ProCsysdata *p=data->data.p_csys_data;
            double mm=snapshot->mm_per_native;
            part_text_add(out,L"CSYS:%ls｜原点 (%.8g, %.8g, %.8g) mm｜X (%.6g, %.6g, %.6g)｜Y (%.6g, %.6g, %.6g)｜Z (%.6g, %.6g, %.6g)\n",
                f->name,p->origin[0]*mm,p->origin[1]*mm,p->origin[2]*mm,
                p->x_vector[0],p->x_vector[1],p->x_vector[2],p->y_vector[0],p->y_vector[1],p->y_vector[2],
                p->z_vector[0],p->z_vector[1],p->z_vector[2]);
            listed++;
        }
        if(data)ProGeomitemdataFree(&data);
    }
    if(!listed)part_text_add(out,L"未读到可验证的命名坐标系。\n");
}

static void company_library_part_refs(const PartSnapshot *snapshot,PartText *out){
    OrganizeView view={snapshot->model,snapshot->mm_per_native};
    PartCommand query={0};
    wcscpy_s(query.arg[2],_countof(query.arg[2]),L"MODEL");
    wcscpy_s(query.arg[3],_countof(query.arg[3]),L"16");
    ProError error=org_holes(&view,&query,out);
    if(error)part_text_add(out,L"未能核实孔位（Creo %d）。\n",error);
    part_text_add(out,L"\n");
    wcscpy_s(query.arg[2],_countof(query.arg[2]),L"ALL");
    wcscpy_s(query.arg[3],_countof(query.arg[3]),L"0");
    wcscpy_s(query.arg[4],_countof(query.arg[4]),L"12");
    error=org_planes(&view,&query,out);
    if(error)part_text_add(out,L"未能核实安装平面候选（Creo %d）。\n",error);
}

/* The top level is verified by filename and native disk identity. Component
   references are reported from Creo's session only when it can resolve them. */
static void company_library_assembly_refs(const PartSnapshot *snapshot,PartText *out){
    ProError error=ax_tree(snapshot->model,snapshot->name,out);
    if(error)part_text_add(out,L"未能读取组件树（Creo %d）。\n",error);
    part_text_add(out,L"组件树仅反映 Creo 本次解析结果。抑制、缺失和无法读取的组件不能用于定位验算；"
        L"需要安装在某个零件上时，请单独读取该组件的 PRT 版本。\n");
}

static void company_library_feature_summary(const PartSnapshot *snapshot,PartText *out){
    const int feature_limit=60;
    const int dimension_limit=80;
    part_text_add(out,L"\n原生特征顺序（已读取 %d 项）：\n",snapshot->features);
    for(int i=0;i<snapshot->features&&i<feature_limit;i++){
        const PartFeature *item=&snapshot->feature[i];
        ProFeature feature;
        ProName name=L"";
        if(!ProFeatureInit((ProSolid)snapshot->model,item->id,&feature))ProFeatureTypenameGet(&feature,name);
        part_text_add(out,L"%d. ID:%d｜%ls｜%ls｜TYPE=%d 状态=%d\n",
            i+1,item->id,name[0]?name:L"未取得特征类型名称",item->name[0]?item->name:L"未命名",
            item->type,item->status);
    }
    if(snapshot->features>feature_limit)part_text_add(out,L"只列出前 %d 项特征。\n",feature_limit);
    part_text_add(out,L"\n尺寸原生读值（已读取 %d 项；线性单位 %ls，角度为度）：\n",
        snapshot->dimensions,snapshot->length_unit.name);
    for(int i=0;i<snapshot->dimensions&&i<dimension_limit;i++){
        const PartDimension *dim=&snapshot->dimension[i];
        part_text_add(out,L"ID:%d %ls=%.12g｜TYPE=%d%ls\n",dim->feature,dim->symbol,
            dim->negative?-fabs(dim->value):dim->value,dim->type,dim->related?L"｜关系驱动":L"");
    }
    if(snapshot->dimensions>dimension_limit)part_text_add(out,L"只列出前 %d 项尺寸。\n",dimension_limit);
    if(snapshot->truncated||snapshot->read_errors)
        part_text_add(out,L"部分原生记录无法读取或超过扫描限制，列表并不完整。\n");
    if(snapshot->unhealthy)
        part_text_add(out,L"发现失败特征或外部参照错误，不可将此版本作为已验证装配依据。\n");
}

static ProError company_library_model(const wchar_t *verified_path,wchar_t *report,size_t capacity){
    if(!report||capacity<128)return PRO_TK_BAD_INPUTS;
    PartText out={report,capacity,0};
    NativeFile source={0};
    ProMdl model=NULL;
    int loaded=0;
    report[0]=0;
    HANDLE source_lock=company_library_source(verified_path,&source,&out);
    if(source_lock==INVALID_HANDLE_VALUE){
        if(!report[0])part_text_add(&out,L"未打开所选数据盘上的库模型。请确认该版本仍在指定位置且文件可读取。");
        return PRO_TK_CANT_OPEN;
    }
    if(attachment_release_count>=NATIVE_ATTACHMENT_MAX){
        CloseHandle(source_lock);
        part_text_add(&out,L"Creo 正在回收上一批只读模型，请稍后重新读取。");
        return PRO_TK_E_IN_USE;
    }
    PartSnapshot snapshot={0};
    ProError error=attachment_open_native(&source,&model,&loaded,&out);
    CloseHandle(source_lock);
    int retrieval_issues=0;
    if(!error&&loaded&&model){
        ProSolidretrievalerrs details=NULL;
        if(!ProSolidRetrievalErrorsGet((ProSolid)model,&details)&&details){
            retrieval_issues=1;
            int count=0;
            if(!ProSolidRetrievalerrsCount(details,&count)&&count>0)retrieval_issues=count;
        }
    }
    if(!error&&!model)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=part_snapshot_take_options(model,&snapshot,1,0);
    if(!error){
        wchar_t overview[1024];
        if(snapshot.type==PRO_MDL_ASSEMBLY){
            ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
            ProSolidOutlExclTypes excludes[]={PRO_OUTL_EXC_DATUM_PLANE,PRO_OUTL_EXC_DATUM_POINT,
                PRO_OUTL_EXC_DATUM_CSYS,PRO_OUTL_EXC_DATUM_AXES,PRO_OUTL_EXC_FACETS,PRO_OUTL_EXC_ALL_CRVS};
            snapshot.has_outline=!ProSolidOutlineCompute((ProSolid)model,identity,excludes,_countof(excludes),snapshot.outline);
            if(snapshot.has_outline&&ad_near(snapshot.outline[0],snapshot.outline[1],1e-10))snapshot.has_outline=0;
        }
        part_snapshot_format_basic(&snapshot,overview,_countof(overview));
        part_text_add(&out,L"已读取磁盘库模型（只读）：%ls\n%ls",verified_path,overview);
        if(retrieval_issues)
            part_text_add(&out,L"Creo 载入时记录 %d 项问题，未核实的组件或外部参照不可用于定位。\n",retrieval_issues);
        part_text_add(&out,L"\n安装参照均为库模型自身坐标。模型重开、换版或装入组件后，需重新核对对应的参照 ID。\n");
        company_library_csys(&snapshot,&out);
        if(snapshot.type==PRO_MDL_ASSEMBLY)company_library_assembly_refs(&snapshot,&out);
        else company_library_part_refs(&snapshot,&out);
        company_library_feature_summary(&snapshot,&out);
    }
    part_snapshot_clear(&snapshot);
    if(loaded&&model){
        int known=0;
        for(int i=0;i<attachment_release_count;i++)if(attachment_release[i]==model)known=1;
        if(!known)attachment_release[attachment_release_count++]=model;
        ProError scheduled=ProMacroLoad(L"~ Command \x0060" L"BWTLAttachmentCleanup\x0060;");
        if(scheduled)trace("Could not queue library model cleanup",scheduled);
    }
    if(error)part_text_add(&out,L"\n库模型读取失败（Creo %d）。请核对版本和装配引用。",error);
    if(out.truncated){
        const wchar_t note[]=L"\n输出已截断。请指定零件、尺寸或参照再核对。";
        wcscpy_s(report+capacity-_countof(note),_countof(note),note);
    }
    return error;
}
