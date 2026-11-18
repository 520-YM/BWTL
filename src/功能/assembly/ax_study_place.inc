
typedef struct {
    const AxStudyRef *learned;
    ProMdl model;
    ProModelitem item;
    int level,ties,named,same_model;
    double mm,distance;
} AxStudyMatch;


static int ax_study_same_cylinder(ProModelitem *a,ProModelitem *b,double mm){
    AxGeometry x,y;
    double first[2],second[2];
    if(ax_geometry(a,&x)||ax_geometry(b,&y)||x.type!=AX_GEOM_CYLINDER||y.type!=AX_GEOM_CYLINDER||
       fabs(x.radius-y.radius)*mm>0.0001||fabs(ad_dot(x.direction,y.direction))<1-1e-7||
       !ax_study_extent(a,&x,mm,first)||!ax_study_extent(b,&y,mm,second))return 0;
    if(ad_dot(x.direction,y.direction)<0){
        double lower=second[0];
        second[0]=-second[1];
        second[1]=-lower;
    }
    ProVector separation,radial;
    ad_sub(x.point,y.point,separation);
    ad_cross(separation,x.direction,radial);
    return sqrt(ad_dot(radial,radial))*mm<0.0001&&
       fabs(first[0]-second[0])<0.0001&&fabs(first[1]-second[1])<0.0001;
}

static ProError ax_study_candidate(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    AxStudyMatch *match=data;
    const AxStudyRef *from=match->learned;
    if(item->type!=from->type)return PRO_TK_NO_ERROR;
    ProFeature feature;
    ProName name=L"";
    if(!ProGeomitemFeatureGet(item,&feature))ProModelitemNameGet(&feature,name);
    ax_study_token(name);
    const int named=from->name[0]&&!_wcsicmp(name,from->name);
    const int same_id=match->same_model&&from->id==item->id;
    double distance=0;
    if(from->shape==4){
        if(!named&&!same_id)return PRO_TK_NO_ERROR;
    }else{
        AxGeometry shape;
        if(ax_geometry(item,&shape)||shape.type+1!=from->shape||
           fabs(ad_dot(shape.direction,(double *)from->direction))<0.9999)return PRO_TK_NO_ERROR;
        ProVector difference,perpendicular;
        for(int i=0;i<3;i++)difference[i]=shape.point[i]*match->mm-from->point[i];
        if(shape.type==AX_GEOM_PLANE)distance=fabs(ad_dot(difference,shape.direction));
        else{
            ad_cross(difference,shape.direction,perpendicular);
            distance=sqrt(ad_dot(perpendicular,perpendicular));
            if(shape.type==AX_GEOM_CYLINDER){
                distance+=fabs(shape.radius*match->mm-from->radius);
                double target[2];
                if(from->has_extent&&ax_study_extent(item,&shape,match->mm,target)){
                    if(ad_dot(shape.direction,(double *)from->direction)<0){
                        double a=target[0];target[0]=-target[1];target[1]=-a;
                    }
                    const double errors[]={fabs(target[0]-from->extent[0]),fabs(target[1]-from->extent[1])};
                    distance+=0.5*fmin(errors[0],errors[1])+0.1*fmax(errors[0],errors[1]);
                }
            }
        }
    }
    const int level=named?4:same_id&&distance<0.01?3:distance<0.01?2:from->shape==AX_GEOM_CYLINDER+1&&distance<0.5?2:1;
    if(level>match->level||(level>=2&&level==match->level&&distance<match->distance-0.002)){
        match->level=level;
        match->distance=distance;
        match->item=*item;
        match->ties=1;
        match->named=named;
    }else if(level==match->level&&(level==1||fabs(distance-match->distance)<0.002)&&
        item->id!=match->item.id){
        if(from->shape==AX_GEOM_CYLINDER+1&&ax_study_same_cylinder(&match->item,item,match->mm)){
            if(item->id<match->item.id)match->item=*item;
        }else match->ties++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_surface(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    AxStudyMatch *match=data;
    ProGeomitem item;
    if(!ProSurfaceToGeomitem((ProSolid)match->model,surface,&item))return ax_study_candidate(&item,status,data);
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_feature(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    AxStudyMatch *match=data;
    ProFeatStatus active;
    ProFeattype type;
    if(ProFeatureStatusGet(feature,&active)||active!=PRO_FEAT_ACTIVE||ProFeatureTypeGet(feature,&type))return PRO_TK_NO_ERROR;
    if(type!=PRO_FEAT_DATUM&&type!=PRO_FEAT_DATUM_AXIS&&type!=PRO_FEAT_CSYS)return PRO_TK_NO_ERROR;
    ProError error=ProFeatureGeomitemVisit(feature,match->learned->type,ax_study_candidate,NULL,data);
    return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}

static ProError ax_study_item(ProMdl model,const AxStudyRef *ref,ProModelitem *item,int *level,int *side){
    ProMdlName name=L"";
    if(!model||ref->shape<1||ref->shape>4)return PRO_TK_BAD_INPUTS;
    AxStudyMatch match={.learned=ref,.model=model};
    ProError error=ProMdlMdlnameGet(model,name);
    if(!error)error=qa_mm_per_unit(model,&match.mm);
    if(error)return error;
    match.same_model=!_wcsicmp(name,ref->model);
    if(ref->type==PRO_SURFACE){
        error=ProSolidSurfaceVisit((ProSolid)model,ax_study_surface,NULL,&match);
        if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    }
    error=ProSolidFeatVisit((ProSolid)model,ax_study_feature,NULL,&match);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    if(!match.level||match.ties!=1)return match.ties>1?PRO_TK_E_FOUND:PRO_TK_E_NOT_FOUND;
    *side=ref->side;
    if(*side&&ref->shape==AX_GEOM_PLANE+1){
        AxGeometry plane;
        if(!ax_geometry(&match.item,&plane)&&ad_dot(plane.direction,(double *)ref->direction)<0)*side=-*side;
    }
    *item=match.item;
    *level=match.level;
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_owner(ProMdl model,const AxScan *scan,const AxStudyRef *ref,const wchar_t *mapped,
    ProMdl *owner,ProAsmcomppath *path){
    if(!wcscmp(ref->path,L"ROOT")){*owner=model;return ax_root_path(model,path);}
    if(mapped&&*mapped){
        AxComponent explicit_component={0};
        ProError error=ax_component_resolve(model,mapped,&explicit_component);
        if(!error){*owner=explicit_component.model;*path=explicit_component.path;}
        return error;
    }
    if(!scan)return PRO_TK_E_NOT_FOUND;
    int named=-1,matches=0;
    ProIdTable ids={0};

    for(int i=0;i<scan->count;i++){
        const AxOccurrence *entry=&scan->occurrences[i];
        if(!entry->model||entry->state!=PRO_FEAT_ACTIVE||_wcsicmp(entry->name,ref->model))continue;

        named=i;
        matches++;
    }
    if(matches!=1)return matches?PRO_TK_E_FOUND:PRO_TK_E_NOT_FOUND;
    const AxOccurrence *entry=&scan->occurrences[named];
    memcpy(ids,entry->ids,entry->depth*sizeof(int));
    *owner=entry->model;
    return ProAsmcomppathInit((ProSolid)model,ids,entry->depth,path);
}

static ProError ax_study_selection(ProMdl model,const AxScan *scan,const AxStudyRef *ref,const wchar_t *mapped,
    int base,ProSelection *selection,int *side,int *level,int *item_id){
    ProMdl owner=NULL;
    ProAsmcomppath path;
    ProModelitem item;
    ProError error=ax_study_owner(model,scan,ref,mapped,&owner,&path);
    if(!error)error=ax_study_item(owner,ref,&item,level,side);
    if(!error)error=ProSelectionAlloc(base||path.table_num?&path:NULL,&item,selection);
    if(!error)*item_id=item.id;
    return error;
}

static int ax_study_supported(int type){
    return type==PRO_ASM_MATE||type==PRO_ASM_ALIGN||type==PRO_ASM_INSERT||type==PRO_ASM_CSYS||
           type==PRO_ASM_ORIENT||ax_offset_type(type);
}


typedef struct {wchar_t source[AX_STUDY_PATH],target[AX_STUDY_PATH];} AxSupportBinding;

static ProError ax_support_bindings(const wchar_t *map,const AxStudyRule *rule,AxSupportBinding data[AX_STUDY_ROWS],int *count){
    *count=0;
    if(!map||!*map)return PRO_TK_NO_ERROR;
    const int pairs=wcschr(map,L'=')!=NULL;
    ProIdTable ids={0};
    if(!pairs){
        if(!ax_path_parse(map,ids))return PRO_TK_BAD_INPUTS;
        for(int i=0;i<rule->count;i++){
            const wchar_t *source=rule->rows[i].base.path;
            if(!wcscmp(source,L"ROOT"))continue;
            if(*count&&wcscmp(source,data[0].source))return PRO_TK_BAD_CONTEXT;
            wcscpy_s(data[0].source,AX_STUDY_PATH,source);
            wcscpy_s(data[0].target,AX_STUDY_PATH,map);
            *count=1;
        }
        return *count?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
    }
    const wchar_t *p=map;
    while(*p){
        if(*count==AX_STUDY_ROWS)return PRO_TK_OUT_OF_RANGE;
        const wchar_t *equal=wcschr(p,L'=');
        const wchar_t *comma=wcschr(p,L',');
        const wchar_t *end=comma?comma:p+wcslen(p);
        if(!equal||equal>=end||equal==p||equal+1>=end||
           equal-p>=AX_STUDY_PATH||end-equal-1>=AX_STUDY_PATH)return PRO_TK_BAD_INPUTS;
        AxSupportBinding *row=&data[*count];
        wcsncpy_s(row->source,AX_STUDY_PATH,p,equal-p);
        wcsncpy_s(row->target,AX_STUDY_PATH,equal+1,end-equal-1);
        if(!ax_path_parse(row->source,ids)||!ax_path_parse(row->target,ids))return PRO_TK_BAD_INPUTS;
        int belongs=0;
        for(int i=0;i<rule->count;i++)belongs|=!wcscmp(rule->rows[i].base.path,row->source);
        if(!belongs)return PRO_TK_E_NOT_FOUND;
        for(int i=0;i<*count;i++)if(!wcscmp(data[i].source,row->source))return PRO_TK_BAD_INPUTS;
        (*count)++;
        if(!comma)break;
        p=end+1;
        if(!*p)return PRO_TK_BAD_INPUTS;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_rows(ProMdl assembly,const AxScan *base_scan,ProMdl child,const AxScan *child_scan,
    const AxStudyRule *rule,const wchar_t *map,double mm,ProAsmcompconstraint **rows,PartText *out){
    AxSupportBinding bindings[AX_STUDY_ROWS]={0};
    int count=0;
    ProError error=ax_support_bindings(map,rule,bindings,&count);
    if(error){
        part_text_add(out,L"支撑配对无法核对。多个旧支撑请分别写成 ID:原路径=ID:新路径，并用逗号隔开。\n");
        return error;
    }
    error=ProArrayAlloc(0,sizeof(**rows),4,(ProArray *)rows);
    for(int i=0;!error&&i<rule->count;i++){
        const AxStudyRow *learned=&rule->rows[i];
        ProSelection a=NULL,b=NULL;
        ProAsmcompconstraint row=NULL;
        int a_level=0,b_level=0,sa=0,sb=0,ai=0,bi=0;
        if(!ax_study_supported(learned->type)||(learned->flags&~PRO_ASM_CONSTR_ATTR_FORCE))error=PRO_TK_UNSUPPORTED;
        const wchar_t *mapped=NULL;
        for(int j=0;j<count;j++)
            if(!wcscmp(bindings[j].source,learned->base.path)){mapped=bindings[j].target;break;}
        if(!error)error=ax_study_selection(assembly,base_scan,&learned->base,mapped,1,&a,&sa,&a_level,&ai);
        if(error)part_text_add(out,L"第 %d 条的支撑参照 %ls / %ls 未能唯一对应。请核对支撑件是否已装、方向是否正确。\n",
            i+1,learned->base.model,learned->base.name);
        if(!error){
            error=ax_study_selection(child,child_scan,&learned->part,NULL,0,&b,&sb,&b_level,&bi);
            if(error)part_text_add(out,L"第 %d 条的组件参照 %ls / %ls 未能唯一对应。请核对安装面、孔轴和组件规格。\n",
                i+1,learned->part.model,learned->part.name);
        }
        if(!error)error=ProAsmcompconstraintAlloc(&row);
        if(!error)error=ProAsmcompconstraintTypeSet(row,learned->type);
        if(!error)error=ProAsmcompconstraintAsmreferenceSet(row,a,(ProDatumside)sa);
        if(!error)error=ProAsmcompconstraintCompreferenceSet(row,b,(ProDatumside)sb);
        if(!error&&ax_offset_type(learned->type))error=ProAsmcompconstraintOffsetSet(row,learned->offset/mm);
        if(!error&&learned->flags)error=ProAsmcompconstraintAttributesSet(row,learned->flags);
        if(!error)error=ProArrayObjectAdd((ProArray *)rows,PRO_VALUE_UNUSED,1,&row);
        if(!error)part_text_add(out,L"第 %d 条 %ls，参照 %d ↔ %d（%ls / %ls）。\n",
            i+1,ax_constraint_label(learned->type),ai,bi,a_level>=3?L"名称或原项复核":L"实测几何",b_level>=3?L"名称或原项复核":L"实测几何");
        if(error&&row)ProAsmcompconstraintFree(row);
        if(a)ProSelectionFree(&a);
        if(b)ProSelectionFree(&b);
    }
    return error;
}

static ProError ax_study_skip_duplicate(ProMdl assembly,const AxScan *scan,ProMdl child,ProMatrix pose,double mm,int *same){
    *same=0;
    for(int i=0;i<scan->count;i++){
        const AxOccurrence *entry=&scan->occurrences[i];
        if(entry->depth!=1||entry->model!=child||entry->state!=PRO_FEAT_ACTIVE)continue;
        ProAsmcomp feature;
        ProMatrix actual;
        ProError error=ax_component_feature(assembly,entry->ids[0],&feature);
        if(!error)error=ax_actual_pose(assembly,&feature,actual);
        if(error)return error;
        if(ax_same_pose(actual,pose,mm))*same=entry->ids[0];
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_verify_rows(ProAsmcomp *feature,ProAsmcomppath *root,ProAsmcompconstraint *wanted,int count,double mm){
    ProAsmcompconstraint *actual=NULL;
    int observed=0;
    ProError error=ax_read_constraints(feature,root,&actual,&observed);
    if(!error&&observed!=count)error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<count;i++){
        ProAsmcompConstrType a,b;
        if(ProAsmcompconstraintTypeGet(actual[i],&a)||ProAsmcompconstraintTypeGet(wanted[i],&b)||a!=b){
            error=PRO_TK_INCOMPLETE;break;
        }
        for(int side=0;!error&&side<=1;side++){
            ProSelection as=NULL,bs=NULL;
            ProDatumside ad,bd;
            ProModelitem ai,bi;
            error=side?ProAsmcompconstraintCompreferenceGet(actual[i],&as,&ad):ProAsmcompconstraintAsmreferenceGet(actual[i],&as,&ad);
            if(!error)error=side?ProAsmcompconstraintCompreferenceGet(wanted[i],&bs,&bd):ProAsmcompconstraintAsmreferenceGet(wanted[i],&bs,&bd);
            if(!error)error=ProSelectionModelitemGet(as,&ai);
            if(!error)error=ProSelectionModelitemGet(bs,&bi);
            if(!error&&(ai.owner!=bi.owner||ai.type!=bi.type||ai.id!=bi.id||
               (ad!=bd&&!(bd==PRO_DATUM_SIDE_NONE&&ad==PRO_DATUM_SIDE_YELLOW))))error=PRO_TK_INCOMPLETE;
        }
        if(!error&&ax_offset_type(a)){
            double first,second;
            error=ProAsmcompconstraintOffsetGet(actual[i],&first);
            if(!error)error=ProAsmcompconstraintOffsetGet(wanted[i],&second);
            if(!error&&fabs(first-second)*mm>0.001)error=PRO_TK_INCOMPLETE;
        }
    }
    if(actual)ProAsmcompconstraintArrayFree(actual);
    return error;
}


static ProError ax_study_rows_spacing(ProMdl assembly,ProMdl child,ProAsmcomp *placed,ProAsmcompconstraint *rows,
    const AxStudyRule *rule,double mm,ProMatrix actual,PartText *out){
    for(int i=0;i<rule->count;i++){
        if(!rule->rows[i].has_array_gap)continue;
        ProSelection a=NULL,b=NULL;
        ProDatumside side;
        double gap=0;
        ProError error=ProAsmcompconstraintAsmreferenceGet(rows[i],&a,&side);
        if(!error)error=ProAsmcompconstraintCompreferenceGet(rows[i],&b,&side);
        if(!error)error=ax_study_airgap(a,b,assembly,child,actual,mm,&gap);
        if(error)return error;
        double desired=rule->rows[i].array_gap;
        double delta=desired-gap;
        if(fabs(delta)<0.005)continue;
        int kind=rule->rows[i].type;
        double offset=rule->rows[i].offset;
        ProModelitem item;
        AxGeometry plane;
        if(!ProSelectionModelitemGet(a,&item)&&!ax_geometry(&item,&plane)&&
           ad_dot(plane.direction,(double *)rule->rows[i].base.direction)<0)desired=-desired;
        delta=desired-gap;
        error=ProAsmcompconstraintTypeSet(rows[i],kind==PRO_ASM_MATE||kind==PRO_ASM_MATE_OFF?PRO_ASM_MATE_OFF:PRO_ASM_ALIGN_OFF);
        if(error)return error;
        int ok=0;
        for(int sign=1;sign>=-1&&!ok;sign-=2){
            if(ProAsmcompconstraintOffsetSet(rows[i],(offset+sign*delta)/mm)||
               ProAsmcompConstraintsSet(NULL,placed,rows)||part_regenerate(assembly)||
               ax_actual_pose(assembly,placed,actual)||ax_study_airgap(a,b,assembly,child,actual,mm,&gap))continue;
            ok=fabs(gap-desired)<0.005;
        }
        if(!ok)return PRO_TK_INCOMPLETE;
        part_text_add(out,L"阵列成员第 %d 条定位已按本位置校正，配合面间距 %.6g mm。\n",i+1,gap);
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_place(ProMdl assembly,ProMdl child,const AxStudyRule *rule,const wchar_t *map,
    int *id,PartText *out){
    ProError error=PRO_TK_BAD_CONTEXT;
    *id=0;
    if(!assembly||!child||assembly==child||!rule->count||rule->count!=rule->reported||
       rule->state!=PRO_FEAT_ACTIVE||rule->under)return error;
    ProMdlType parent_type,child_type;
    double mm=0,child_mm=0;
    if((error=ProMdlTypeGet(assembly,&parent_type))||(error=ProMdlTypeGet(child,&child_type))||
       (error=qa_mm_per_unit(assembly,&mm))||(error=qa_mm_per_unit(child,&child_mm)))return error;
    if(parent_type!=PRO_MDL_ASSEMBLY||child_type!=rule->type||!part_close(mm,child_mm))return PRO_TK_BAD_CONTEXT;
    if((error=ax_master(assembly))||(error=part_health(assembly)))return error;
    AxScan *base=NULL,*sub=NULL;
    ProAsmcompconstraint *rows=NULL;
    ProAsmcomp occurrence={0};
    ProAsmcomppath root;
    ProMatrix seed,actual;
    ProBoolean packaged=PRO_B_TRUE,under=PRO_B_TRUE;
    int duplicate=0;
    if((error=ax_scan(assembly,0,&base)))goto done;
    if(child_type==PRO_MDL_ASSEMBLY&&(error=ax_scan(child,0,&sub)))goto done;
    if(sub)for(int i=0;i<sub->count;i++)if(sub->occurrences[i].model==assembly){error=PRO_TK_BAD_CONTEXT;goto done;}
    if(base->limited||(sub&&sub->limited)){error=PRO_TK_INCOMPLETE;goto done;}
    if((error=ax_study_rows(assembly,base,child,sub,rule,map,mm,&rows,out)))goto done;
    memcpy(seed,rule->pose,sizeof(seed));
    for(int i=0;i<3;i++)seed[3][i]/=mm;
    if((error=ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)child,seed,&occurrence))||
       (error=ProAsmcompConstraintsSet(NULL,&occurrence,rows))||(error=part_regenerate(assembly))||
       (error=ax_actual_pose(assembly,&occurrence,actual))||
       ((!map||!*map)&&(error=ax_study_rows_spacing(assembly,child,&occurrence,rows,rule,mm,actual,out)))||
       (error=ax_root_path(assembly,&root))||(error=ax_study_verify_rows(&occurrence,&root,rows,rule->count,mm))||
       (error=ProAsmcompIsPackaged(&occurrence,&packaged))||(error=ProAsmcompIsUnderconstrained(&occurrence,&under))||
       (error=ax_actual_pose(assembly,&occurrence,actual)))goto done;
    if(packaged||under){error=PRO_TK_INCOMPLETE;goto done;}
    if((error=ax_study_skip_duplicate(assembly,base,child,actual,mm,&duplicate)))goto done;
    if(duplicate){
        ProAsmcomp existing;
        ProBoolean existing_packaged=PRO_B_TRUE,existing_under=PRO_B_TRUE;
        if((error=ax_component_feature(assembly,duplicate,&existing))||
           (error=ProAsmcompIsPackaged(&existing,&existing_packaged))||
           (error=ProAsmcompIsUnderconstrained(&existing,&existing_under))||
           (error=ax_study_verify_rows(&existing,&root,rows,rule->count,mm))||existing_packaged||existing_under){
            part_text_add(out,L"ID:%d 已占用同一安装位，现有配合与记忆不同。原实例已保留。\n",duplicate);
            if(!error)error=PRO_TK_BAD_CONTEXT;
        }
        goto done;
    }
    *id=occurrence.id;
    part_text_add(out,L"已在 ID:%d 建立 %d 条原生约束，Creo 定位检查通过。实测位置 %.5g / %.5g / %.5g mm，当前修改待保存。\n",
        *id,rule->count,actual[3][0]*mm,actual[3][1]*mm,actual[3][2]*mm);
done:
    if((error||duplicate)&&occurrence.id>0){
        if(model_remove_feature((ProSolid)assembly,occurrence.id)||part_regenerate(assembly)){
            part_text_add(out,L"请核对本次新增实例 ID:%d 的状态，自动清理未能验证。\n",occurrence.id);
            error=PRO_TK_INCOMPLETE;
        }
        else if(duplicate&&!error){*id=duplicate;part_text_add(out,L"相同组件已在正确位置 ID:%d，原实例保留。\n",duplicate);}
        else part_text_add(out,L"本次定位未通过，已撤回本次新增实例。\n");
    }
    if(rows)ProAsmcompconstraintArrayFree(rows);
    free(base);
    free(sub);
    return error;
}
