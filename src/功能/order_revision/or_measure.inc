static ProError or_outline(ProMdl model,ProOutline bounds){
    ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProSolidOutlExclTypes excludes[]={PRO_OUTL_EXC_DATUM_PLANE,PRO_OUTL_EXC_DATUM_POINT,
        PRO_OUTL_EXC_DATUM_CSYS,PRO_OUTL_EXC_DATUM_AXES,PRO_OUTL_EXC_FACETS,PRO_OUTL_EXC_ALL_CRVS};
    return ProSolidOutlineCompute((ProSolid)model,identity,excludes,_countof(excludes),bounds);
}

static ProError or_measure_on(ProMdl active,const wchar_t *metric,double *actual){
    ProMdl model=active;
    if(!model)return PRO_TK_BAD_CONTEXT;
    const wchar_t *mark=wcschr(metric,L'|');
    if(mark){
        wchar_t path[180];
        AxComponent child={0};
        if((size_t)(mark-metric)>=_countof(path))return PRO_TK_BAD_INPUTS;
        wcsncpy_s(path,_countof(path),metric,mark-metric);
        ProError error=ax_component_resolve(active,path,&child);
        if(error)return error;
        model=child.model;
        metric=mark+1;
    }
    double scale;
    ProMdlType type;
    ProError error=ProMdlTypeGet(model,&type);
    if(!error)error=qa_mm_per_unit(model,&scale);
    if(error||(type!=PRO_MDL_PART&&type!=PRO_MDL_ASSEMBLY))return error?error:PRO_TK_BAD_CONTEXT;
    if(!wcsncmp(metric,L"BOX:",4)){
        if(wcslen(metric)!=5||metric[4]<L'X'||metric[4]>L'Z')return PRO_TK_BAD_INPUTS;
        ProOutline bounds;
        error=or_outline(model,bounds);
        int axis=(int)(metric[4]-L'X');
        if(!error)*actual=(bounds[1][axis]-bounds[0][axis])*scale;
        return error;
    }
    if(!wcsncmp(metric,L"DIM:",4)){
        UxDimension d;
        ProDimensiontype dt;
        error=ux_dimension(model,metric+4,&d);
        if(!error)error=ProDimensionTypeGet(&d.dim,&dt);
        if(!error&&dt!=PRODIMTYPE_LINEAR&&dt!=PRODIMTYPE_DIAMETER&&dt!=PRODIMTYPE_RADIUS)return PRO_TK_INVALID_TYPE;
        if(!error)*actual=d.value*scale;
        return error;
    }
    if(!wcsncmp(metric,L"DIAM:",5)){
        FdRef ref={0};
        AxGeometry geometry;
        error=fd_ref_resolve(model,metric+5,PRO_SURFACE,&ref);
        if(!error)error=ax_geometry(&ref.item,&geometry);
        fd_ref_free(&ref);
        if(!error&&geometry.type!=AX_GEOM_CYLINDER)return PRO_TK_INVALID_TYPE;
        if(!error)*actual=2*geometry.radius*scale;
        return error;
    }

    int pin_axis=-1,plane_axis=-1;
    if(wcslen(metric)>7&&!wcsncmp(metric,L"HOLE_",5)&&metric[6]==L':'&&metric[5]>=L'X'&&metric[5]<=L'Z')pin_axis=metric[5]-L'X';
    if(wcslen(metric)>8&&!wcsncmp(metric,L"PLANE_",6)&&metric[7]==L':'&&metric[6]>=L'X'&&metric[6]<=L'Z')plane_axis=metric[6]-L'X';
    if(pin_axis>=0||plane_axis>=0){
        FdRef ref={0};
        AxGeometry geometry;
        const int axis=pin_axis>=0?pin_axis:plane_axis;
        error=fd_ref_resolve(model,metric+(pin_axis>=0?7:8),PRO_SURFACE,&ref);
        if(!error)error=ax_geometry(&ref.item,&geometry);
        fd_ref_free(&ref);
        if(error)return error;
        const double normal=fabs(geometry.direction[axis]);
        if((pin_axis>=0&&(geometry.type!=AX_GEOM_CYLINDER||normal>1e-7))||
           (plane_axis>=0&&(geometry.type!=AX_GEOM_PLANE||normal<1-1e-7)))return PRO_TK_BAD_CONTEXT;
        *actual=geometry.point[axis]*scale;
        return PRO_TK_NO_ERROR;
    }

    if(!wcsncmp(metric,L"GAP:",4)){
        wchar_t a[OR_CELL];
        const wchar_t *comma=wcschr(metric+4,L',');
        if(!comma||comma==metric+4||comma-(metric+4)>=_countof(a))return PRO_TK_BAD_INPUTS;
        wcsncpy_s(a,_countof(a),metric+4,comma-(metric+4));
        FdRef left={0},right={0};
        AxGeometry p,q;
        error=fd_ref_resolve(model,a,PRO_SURFACE,&left);
        if(!error)error=fd_ref_resolve(model,comma+1,PRO_SURFACE,&right);
        if(!error)error=ax_geometry(&left.item,&p);
        if(!error)error=ax_geometry(&right.item,&q);
        fd_ref_free(&left);fd_ref_free(&right);
        if(error)return error;
        if(p.type!=AX_GEOM_PLANE||q.type!=AX_GEOM_PLANE||fabs(ad_dot(p.direction,q.direction))<0.999999)
            return PRO_TK_BAD_CONTEXT;
        ProVector delta;
        ad_sub(q.point,p.point,delta);
        *actual=fabs(ad_dot(delta,p.direction))*scale;
        return PRO_TK_NO_ERROR;
    }
    return PRO_TK_BAD_INPUTS;
}

static ProError or_read_number(ProMdl active,const wchar_t *metric,double *actual){
    *actual=0;
    ProError error=or_measure_on(active,metric,actual);
    return error?error:isfinite(*actual)?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError or_mapping(ProMdl active,const wchar_t *order,const wchar_t *map,PartText *out){
    wchar_t path[MAX_PATH];
    if(!active||!or_path(order,L"订单规格.tsv",path,0))return PRO_TK_E_NOT_FOUND;
    OrTable req={0},bindings={0};
    if(!or_table_load(&req,path,3,out)||!or_table_load(&bindings,map,4,out)||!or_unique_column(&bindings,1,out)){
        or_free(&req);or_free(&bindings);return PRO_TK_BAD_INPUTS;
    }
    int passed=0;
    for(int i=0;i<bindings.size;i++){
        OrRow *row=&bindings.rows[i];
        int actual_row=or_find(&req,0,row->value[0]);
        double want=0,got=0,tol=0;
        if(actual_row<0||!or_number(req.rows[actual_row].value[1],&want)||!or_number(row->value[3],&tol)||tol<0){
            part_text_add(out,L"第 %u 行的订单字段或公差无法核实。\n",row->line);continue;
        }
        ProError error=or_read_number(active,row->value[2],&got);
        if(error){part_text_add(out,L"%ls → %ls 参照无法读取（Creo %d）。\n",row->value[0],row->value[1],error);continue;}
        int good=fabs(got-want)<=tol;
        passed+=good;
        part_text_add(out,L"%ls → %ls [%ls]，订单 %.9g，实测 %.9g mm，%ls。\n",
            row->value[0],row->value[1],row->value[2],want,got,good?L"符合":L"超出公差");
    }
    int size=bindings.size;
    or_free(&req);or_free(&bindings);
    return passed==size?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError or_capture(ProMdl model,const wchar_t *order,const wchar_t *path,int guard,PartText *out){
    if(!model)return PRO_TK_BAD_CONTEXT;
    OrTable inputs={0};
    if(!or_table_load(&inputs,path,3,out)||!or_unique_column(&inputs,0,out)){or_free(&inputs);return PRO_TK_BAD_INPUTS;}
    ProMdlName name;
    char stamp[240];
    ProError error=ProMdlMdlnameGet(model,name);
    if(!error)error=ax_study_verstamp(model,stamp);
    OrOutput result={0};
    if(!error&&!or_output_begin(&result,order,guard?L"保留项.tsv":L"尺寸基线.tsv"))error=PRO_TK_CANT_WRITE;
    if(!error)fwprintf(result.file,L"# 实测模型 %ls 版本 %hs\n",name,stamp);
    for(int i=0;!error&&i<inputs.size;i++){
        OrRow *row=&inputs.rows[i];
        double tolerance,value;
        if(!or_number(row->value[2],&tolerance)||tolerance<0)error=PRO_TK_BAD_INPUTS;
        if(!error)error=or_read_number(model,row->value[1],&value);
        if(error)part_text_add(out,L"%ls 不能记录，测量 %ls（Creo %d）。\n",row->value[0],row->value[1],error);
        else fwprintf(result.file,L"%ls\t%ls\t%.17g\t%.17g\t%ls\n",row->value[0],row->value[1],value,tolerance,name);
    }
    if(result.file&&!or_output_end(&result,!error)&&!error)error=PRO_TK_CANT_WRITE;
    if(!error)part_text_add(out,L"已记录 %d 个实测项目和源模型版本。\n%ls\n",inputs.size,result.final);
    or_free(&inputs);
    return error;
}

static ProError or_compare(ProMdl model,const wchar_t *order,int guard,PartText *out){
    if(!model)return PRO_TK_BAD_CONTEXT;
    wchar_t path[MAX_PATH];
    if(!or_path(order,guard?L"保留项.tsv":L"尺寸基线.tsv",path,0))return PRO_TK_E_NOT_FOUND;
    OrTable saved={0};
    if(!or_table_load(&saved,path,5,out)){or_free(&saved);return PRO_TK_E_NOT_FOUND;}
    ProMdlName name;
    char stamp[240];
    ProError identity=ProMdlMdlnameGet(model,name);
    if(!identity)identity=ax_study_verstamp(model,stamp);
    if(identity){or_free(&saved);return identity;}
    part_text_add(out,L"当前实测模型 %ls，版本 %hs。\n",name,stamp);
    int changed=0,missing=0;
    for(int i=0;i<saved.size;i++){
        OrRow *row=&saved.rows[i];
        double before,after,tol;
        ProError error=or_read_number(model,row->value[1],&after);
        if(error||!or_number(row->value[2],&before)||!or_number(row->value[3],&tol)||tol<0){
            part_text_add(out,L"%ls 的参照 %ls 待核实（Creo %d）。\n",row->value[0],row->value[1],error);
            missing++;continue;
        }
        const int diff=fabs(after-before)>tol;
        changed+=diff;
        part_text_add(out,L"%ls %.10g → %.10g mm（%+.7g），%ls。\n",row->value[0],before,after,after-before,
            diff?guard?L"保留项超差":L"有变更":L"公差内");
    }
    or_free(&saved);
    return missing||(guard&&changed)?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static ProError or_trial_one(ProMdl source,const wchar_t *symbol,double requested,PartText *out){
    static unsigned serial;
    ProMdlName name;
    swprintf_s(name,_countof(name),L"OR_PROBE_%lx_%u",GetCurrentProcessId(),++serial);
    ProMdl copy=NULL;
    ProError error=ProMdlnameCopy(source,name,&copy);
    if(error)return error;
    Revision tx;
    error=rv_begin(&tx,copy);
    UxDimension d;
    ProFeature feature;
    ProDimensiontype kind;
    if(!error)error=ux_dimension(copy,symbol,&d);
    if(!error)error=ProDimensionTypeGet(&d.dim,&kind);
    if(!error&&kind!=PRODIMTYPE_LINEAR&&kind!=PRODIMTYPE_DIAMETER&&kind!=PRODIMTYPE_RADIUS)error=PRO_TK_INVALID_TYPE;
    if(!error)error=ProDimensionOwnerfeatureGet(&d.dim,&feature);
    if(!error)error=rv_add(&tx,d.dim,feature.id,d.value,requested/tx.mm);
    if(!error)error=rv_apply(&tx);
    if(error==PRO_TK_NO_CHANGE)error=PRO_TK_NO_ERROR;
    if(!error)error=part_health(copy);
    double result=0;
    if(!error)error=ProDimensionValueGet(&d.dim,&result);
    if(!error){
        ProOutline extents;
        error=or_outline(copy,extents);
        if(!error)part_text_add(out,L"%.9g mm 试算通过，尺寸读回 %.9g mm，包络 %.9g × %.9g × %.9g mm。\n",
            requested,result*tx.mm,(extents[1][0]-extents[0][0])*tx.mm,(extents[1][1]-extents[0][1])*tx.mm,
            (extents[1][2]-extents[0][2])*tx.mm);
    }
    if(error){
        PartSnapshot *s=calloc(1,sizeof(*s));
        if(s){
            if(!ProSolidFeatVisit((ProSolid)copy,part_feature_visit,NULL,s)){
                for(int i=0;i<s->features;i++){
                    if(s->feature[i].flags&(PRO_FEAT_STAT_FAILED|PRO_FEAT_STAT_CHILD_OF_FAILED|PRO_FEAT_STAT_CHILD_OF_EXT_FAILED))
                        part_text_add(out,L"受影响特征 %ls ID:%d。\n",s->feature[i].name,s->feature[i].id);
                }
            }
            part_snapshot_clear(s);free(s);
        }
        part_text_add(out,L"%.9g mm 试算失败（Creo %d）。\n",requested,error);
    }
    ProError erased=ProMdlErase(copy);
    return error?error:erased;
}

static ProError or_trial(ProMdl source,const wchar_t *symbol,double low,double high,PartText *out){
    if(!source||low<=0||high<=low)return PRO_TK_BAD_INPUTS;
    ProMdlType type;
    ProError error=ProMdlTypeGet(source,&type);
    if(error||type!=PRO_MDL_PART)return error?error:PRO_TK_BAD_CONTEXT;
    if((error=dv_no_external_dependencies(source)))return error;
    ProWVerstamp before=NULL,after=NULL;
    error=ProMdlVerstampGet(source,&before);
    if(error)return error;

    ProPath previous;
    wchar_t scratch[MAX_PATH],area[MAX_PATH];
    static unsigned runs;
    if(!error)error=ProDirectoryCurrentGet(previous);
    if(!error&&(!storage_method_directory(area,L"订单试算",1)||
       swprintf_s(scratch,MAX_PATH,L"%ls\\probe-%lx-%llu-%u",area,GetCurrentProcessId(),GetTickCount64(),++runs)<0||
       !CreateDirectoryW(scratch,NULL)))error=PRO_TK_CANT_WRITE;
    if(!error)error=ProDirectoryChange(scratch);
    if(error){if(before)ProVerstampFree(&before);return error;}
    ProError a=or_trial_one(source,symbol,low,out);
    ProError b=or_trial_one(source,symbol,high,out);
    error=ProDirectoryChange(previous);
    if(!error)error=ProMdlVerstampGet(source,&after);

    if(!error&&!ProVerstampEqual(before,after))error=PRO_TK_INCOMPLETE;
    ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    return error?error:a?a:b;
}
