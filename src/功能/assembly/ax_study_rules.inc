
#define AX_STUDY_SUFFIX L".bwtlrules.txt"
#define AX_STUDY_ROWS 12
#define AX_STUDY_PATH 204

typedef struct {
    wchar_t path[AX_STUDY_PATH];
    ProFamilyMdlName model;
    ProName name;
    int id,type,shape,side,has_extent;
    double radius,extent[2];
    ProVector point,direction;
} AxStudyRef;

typedef struct {
    int type,flags,has_array_gap;
    double offset,array_gap;
    AxStudyRef base,part;
} AxStudyRow;

typedef struct {
    int found,state,type,pattern,under,count,reported;
    ProFamilyMdlName name;
    wchar_t parent[AX_STUDY_PATH];
    ProMatrix pose;
    AxStudyRow rows[AX_STUDY_ROWS];
} AxStudyRule;

static int ax_study_key(const wchar_t *name){
    size_t size=wcslen(name);
    if(!size||size>=PRO_NAME_SIZE)return 0;
    for(;*name;name++)if(!ax_learning_word(*name))return 0;
    return 1;
}

static int ax_study_casefile(const wchar_t *name,wchar_t file[MAX_PATH],int create){
    wchar_t folder[MAX_PATH];
    return ax_study_key(name)&&ax_learning_directory(folder,create)&&
        swprintf_s(file,MAX_PATH,L"%ls\\%ls%ls",folder,name,AX_STUDY_SUFFIX)>=0;
}

static FILE *ax_study_atomic(const wchar_t *path,wchar_t temp[MAX_PATH]){
    static unsigned serial;
    if(swprintf_s(temp,MAX_PATH,L"%ls.%lx.%u.tmp",path,GetCurrentProcessId(),++serial)<0)return NULL;
    return _wfopen(temp,L"wtx, ccs=UTF-8");
}

static int ax_study_commit(FILE *out,const wchar_t *temp,const wchar_t *path){
    int good=!ferror(out);
    if(fclose(out))good=0;
    if(good&&MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return 1;
    DeleteFileW(temp);
    return 0;
}

static void ax_study_token(wchar_t *value){
    for(;*value;value++)if(*value<=L' '||*value==L'|'||*value==L'<'||*value==L'>')*value=L'_';
}


static int ax_study_extent(ProModelitem *item,const AxGeometry *geometry,double mm,double bounds[2]){
    ProSurface surface;
    ProVector first,last;
    if(ProGeomitemToSurface(item,&surface)||ProSurfaceExtremesEval(surface,(double *)geometry->direction,first,last))return 0;
    const double a=ad_dot(first,(double *)geometry->direction)*mm,b=ad_dot(last,(double *)geometry->direction)*mm;
    if(!isfinite(a)||!isfinite(b))return 0;
    bounds[0]=fmin(a,b);
    bounds[1]=fmax(a,b);
    return 1;
}

static int ax_study_ref(ProSelection selection,ProMdl boundary,int side,AxStudyRef *ref){
    ProModelitem item;
    ProFeature feature;
    ProAsmcomppath path;
    wcscpy_s(ref->path,_countof(ref->path),L"ROOT");
    ref->side=side;
    if(!selection||ProSelectionModelitemGet(selection,&item)||!item.owner||
       ProMdlMdlnameGet(item.owner,ref->model))return 0;
    if(item.owner!=boundary&&!ProSelectionAsmcomppathGet(selection,&path)&&
       path.table_num>0&&path.table_num<=AX_PATH_DEPTH)
        ax_path_label(path.comp_id_table,path.table_num,ref->path,_countof(ref->path));
    if(item.owner!=boundary&&!wcscmp(ref->path,L"ROOT"))wcscpy_s(ref->path,_countof(ref->path),L"?");
    ref->id=item.id;
    ref->type=item.type;
    if(!ProGeomitemFeatureGet(&item,&feature))ProModelitemNameGet(&feature,ref->name);
    ax_study_token(ref->model);
    ax_study_token(ref->name);
    if(item.type==PRO_CSYS){ref->shape=4;return 1;}
    AxGeometry geometry;
    double mm;
    if(ax_geometry(&item,&geometry)||qa_mm_per_unit(item.owner,&mm))return 0;
    ref->shape=geometry.type+1;
    ref->radius=geometry.radius*mm;
    if(geometry.type==AX_GEOM_CYLINDER)ref->has_extent=ax_study_extent(&item,&geometry,mm,ref->extent);
    for(int i=0;i<3;i++){
        ref->point[i]=geometry.point[i]*mm;
        ref->direction[i]=geometry.direction[i];
    }
    return 1;
}

static void ax_study_write_ref(FILE *out,const AxStudyRef *ref){
    fwprintf(out,L"REF %ls %ls %ls %d %d %d %d %.17g %.17g %.17g %.17g %.17g %.17g %.17g %d %.17g %.17g\n",
       ref->path[0]?ref->path:L"?",ref->model[0]?ref->model:L"-",ref->name[0]?ref->name:L"-",
       ref->id,ref->type,ref->shape,ref->side,ref->radius,
       ref->point[0],ref->point[1],ref->point[2],ref->direction[0],ref->direction[1],ref->direction[2],ref->has_extent,ref->extent[0],ref->extent[1]);
}

static int ax_study_row(ProAsmcompconstraint source,ProMdl owner,ProMdl child,double mm,AxStudyRow *row){
    ProAsmcompConstrType type;
    if(ProAsmcompconstraintTypeGet(source,&type)||ProAsmcompconstraintAttributesGet(source,&row->flags))return 0;
    row->type=type;
    if(ax_offset_type(type)){
        if(!mm||ProAsmcompconstraintOffsetGet(source,&row->offset)||!isfinite(row->offset))return 0;
        row->offset*=mm;
    }
    if(type==PRO_ASM_FIX||type==PRO_ASM_DEF_PLACEMENT)return 1;
    ProSelection a=NULL,b=NULL;
    ProDatumside sa=0,sb=0;
    return !ProAsmcompconstraintAsmreferenceGet(source,&a,&sa)&&!ProAsmcompconstraintCompreferenceGet(source,&b,&sb)&&
        ax_study_ref(a,owner,sa,&row->base)&&ax_study_ref(b,child,sb,&row->part);
}


static void ax_study_local_path(AxStudyRef *ref,const int *prefix,int depth){
    ProIdTable ids={0};
    const int n=ax_path_parse(ref->path,ids);
    if(depth>0&&n>depth&&!memcmp(ids,prefix,depth*sizeof(int)))ax_path_label(ids+depth,n-depth,ref->path,_countof(ref->path));
}

static ProError ax_study_plane(ProSelection selected,ProMdl boundary,AxGeometry *shape){
    ProModelitem item;
    ProError error=ProSelectionModelitemGet(selected,&item);
    if(!error)error=ax_geometry(&item,shape);
    if(error)return error;
    if(shape->type!=AX_GEOM_PLANE)return PRO_TK_INVALID_TYPE;
    if(item.owner==boundary)return PRO_TK_NO_ERROR;
    ProAsmcomppath path={0};
    error=ProSelectionAsmcomppathGet(selected,&path);
    if(error)return error;
    if(path.owner!=(ProSolid)boundary){
        ProAsmcomppath local;
        int found=0;
        for(int skip=0;skip<path.table_num;skip++){
            ProIdTable ids={0};
            ProMdl owner=NULL;
            int remaining=path.table_num-skip;
            if(remaining>AX_PATH_DEPTH)continue;
            memcpy(ids,path.comp_id_table+skip,remaining*sizeof(int));
            if(!ProAsmcomppathInit((ProSolid)boundary,ids,remaining,&local)&&
               !ProAsmcomppathMdlGet(&local,&owner)&&owner==item.owner){path=local;found=1;break;}
        }
        if(!found)return PRO_TK_E_NOT_FOUND;
    }
    AxGeometry original=*shape;
    ProMatrix placement;
    if((error=ProAsmcomppathTrfGet(&path,PRO_B_TRUE,placement))||
       (error=ProPntTrfEval(original.point,placement,shape->point))||
       (error=ProVectorTrfEval(original.direction,placement,shape->direction)))return error;
    return ad_unit(shape->direction)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_study_airgap(ProSelection a,ProSelection b,ProMdl owner,ProMdl child,ProMatrix pose,
    double mm,double *distance){
    AxGeometry base,device;
    ProVector point,direction,delta;
    ProError error=ax_study_plane(a,owner,&base);
    if(!error)error=ax_study_plane(b,child,&device);
    if(!error)error=ProPntTrfEval(device.point,pose,point);
    if(!error)error=ProVectorTrfEval(device.direction,pose,direction);
    if(error)return error;
    if(fabs(ad_dot(direction,base.direction))<0.9999)return PRO_TK_BAD_CONTEXT;
    ad_sub(point,base.point,delta);
    *distance=ad_dot(delta,base.direction)*mm;
    return PRO_TK_NO_ERROR;
}

static void ax_study_pattern_offset(ProAsmcompconstraint row,ProMdl owner,ProMdl child,ProMatrix pose,double mm,AxStudyRow *rule){
    if((rule->type!=PRO_ASM_MATE&&rule->type!=PRO_ASM_ALIGN&&!ax_offset_type(rule->type))||
       rule->base.shape!=1||rule->part.shape!=1)return;
    ProSelection a=NULL,b=NULL;
    ProDatumside side;
    double gap=0;
    if(!ProAsmcompconstraintAsmreferenceGet(row,&a,&side)&&!ProAsmcompconstraintCompreferenceGet(row,&b,&side)&&
       !ax_study_airgap(a,b,owner,child,pose,mm,&gap)&&fabs(fabs(gap)-fabs(rule->offset))>0.005){
        rule->array_gap=gap;
        rule->has_array_gap=1;
    }
}

static void ax_study_capture_instance(FILE *out,ProMdl assembly,const AxScan *scan,int index,int *ready){
    const AxOccurrence *item=&scan->occurrences[index];
    wchar_t key[AX_STUDY_PATH],parent[AX_STUDY_PATH]=L"ROOT";
    ProFamilyMdlName label;
    wcscpy_s(label,_countof(label),item->name);
    ax_study_token(label);
    ax_path_label(item->ids,item->depth,key,_countof(key));
    const int depth=item->depth-1;
    if(depth)ax_path_label(item->ids,depth,parent,_countof(parent));
    ProIdTable ids={0};
    memcpy(ids,item->ids,depth*sizeof(int));
    ProAsmcomppath scope;
    ProMdl owner=NULL;
    ProAsmcomp comp;
    ProPatternStatus pattern=PRO_PATTERN_NONE;
    ProBoolean under=PRO_B_TRUE,packaged=PRO_B_TRUE;
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProAsmcompconstraint *native=NULL;
    AxStudyRow rows[AX_STUDY_ROWS]={0};
    int observed=0,valid=0;
    double mm=0;
    if(item->model&&item->state==PRO_FEAT_ACTIVE&&
       !ProAsmcomppathInit((ProSolid)assembly,ids,depth,&scope)&&!ProAsmcomppathMdlGet(&scope,&owner)&&
       !ax_component_feature(owner,item->ids[depth],&comp)){
        ProFeaturePatternStatusGet(&comp,&pattern);
        ProAsmcompIsPackaged(&comp,&packaged);
        ProAsmcompIsUnderconstrained(&comp,&under);
        if(!qa_mm_per_unit(owner,&mm)&&!ax_actual_pose(owner,&comp,pose)&&
           !ax_read_constraints(&comp,&scope,&native,&observed)&&observed<=AX_STUDY_ROWS){
            for(int i=0;i<observed;i++){
                AxStudyRow row={0};
                if(ax_study_row(native[i],owner,item->model,mm,&row)){
                    ax_study_local_path(&row.base,item->ids,depth);
                    ax_study_local_path(&row.part,item->ids,item->depth);
                    if(pattern==PRO_PATTERN_MEMBER)ax_study_pattern_offset(native[i],owner,item->model,pose,mm,&row);
                    rows[valid++]=row;
                }
            }
        }
    }
    if(native)ProAsmcompconstraintArrayFree(native);
    *ready+=observed&&valid==observed&&!under&&!packaged;
    fwprintf(out,L"MODULE %ls %ls %ls %d %d %d %d %d %d\n",key,parent,label[0]?label:L"-",
        item->type,item->state,pattern,under||packaged,valid,observed);
    fwprintf(out,L"POSE");
    for(int i=0;i<4;i++)for(int j=0;j<3;j++)fwprintf(out,L" %.17g",pose[i][j]*(i==3?mm:1));
    fputwc(L'\n',out);
    for(int i=0;i<valid;i++){
        fwprintf(out,L"RULE %d %d %.17g %d %.17g\n",rows[i].type,rows[i].flags,rows[i].offset,rows[i].has_array_gap,rows[i].array_gap);
        ax_study_write_ref(out,&rows[i].base);
        ax_study_write_ref(out,&rows[i].part);
    }
}

static ProError ax_study_capture(ProMdl assembly,const AxScan *scan,const wchar_t *name,PartText *result){
    wchar_t file[MAX_PATH],temp[MAX_PATH];
    FILE *out=ax_study_casefile(name,file,1)?ax_study_atomic(file,temp):NULL;
    if(!out){part_text_add(result,L"装配规则文件无法写入。用不含空格的 Creo 图号命名，或检查数据目录。\n");return PRO_TK_CANT_WRITE;}
    fwprintf(out,L"BWTL-ASSEMBLY-RULES-3 %ls\n",name);
    int ready=0;
    for(int i=0;i<scan->count&&!ferror(out);i++)ax_study_capture_instance(out,assembly,scan,i,&ready);
    fwprintf(out,L"END %d %d %d\n",scan->count,ready,scan->limited);
    if(!ax_study_commit(out,temp,file))return PRO_TK_CANT_WRITE;
    part_text_add(result,L"装配规则卡已保存：%ls\n%d 个实例保存了完整定位条件。每条卡片包括上级、方向、重复件状态和参照。\n",file,ready);
    return PRO_TK_NO_ERROR;
}

static int ax_study_read_ref(FILE *in,AxStudyRef *ref){
    wchar_t line[850],path[AX_STUDY_PATH],model[180],name[180];
    if(!fgetws(line,_countof(line),in)||swscanf_s(line,L"REF %203ls %179ls %179ls %d %d %d %d %lf %lf %lf %lf %lf %lf %lf %d %lf %lf",
       path,(unsigned)_countof(path),model,(unsigned)_countof(model),name,(unsigned)_countof(name),
       &ref->id,&ref->type,&ref->shape,&ref->side,&ref->radius,&ref->point[0],&ref->point[1],&ref->point[2],
       &ref->direction[0],&ref->direction[1],&ref->direction[2],&ref->has_extent,&ref->extent[0],&ref->extent[1])!=17)return 0;
    if(wcslen(model)>=_countof(ref->model)||wcslen(name)>=_countof(ref->name)||ref->side<-1||ref->side>1)return 0;
    wcscpy_s(ref->path,_countof(ref->path),path);
    wcscpy_s(ref->model,_countof(ref->model),model);
    if(wcscmp(name,L"-"))wcscpy_s(ref->name,_countof(ref->name),name);
    if(!isfinite(ref->radius)||!isfinite(ref->extent[0])||!isfinite(ref->extent[1]))return 0;
    for(int i=0;i<3;i++)if(!isfinite(ref->point[i])||!isfinite(ref->direction[i]))return 0;
    return 1;
}

static ProError ax_study_read(const wchar_t *source,const wchar_t *id,AxStudyRule *rule){
    wchar_t file[MAX_PATH],line[850],wanted[220],stored[AX_STUDY_PATH],parent[AX_STUDY_PATH],label[180];
    ProIdTable ids={0};
    if(!ax_path_parse(id,ids)||!ax_study_casefile(source,file,0))return PRO_TK_BAD_INPUTS;
    FILE *in=ax_learning_open(file);
    if(!in)return PRO_TK_E_NOT_FOUND;
    ProError error=PRO_TK_E_NOT_FOUND;
    swprintf_s(wanted,_countof(wanted),L"BWTL-ASSEMBLY-RULES-3 %ls",source);
    if(!fgetws(line,_countof(line),in)){error=PRO_TK_BAD_INPUTS;goto done;}
    line[wcscspn(line,L"\r\n")]=0;
    if(_wcsicmp(line[0]==0xfeff?line+1:line,wanted)){error=PRO_TK_BAD_INPUTS;goto done;}
    while(fgetws(line,_countof(line),in)){
        if(wcsncmp(line,L"MODULE ",7))continue;
        AxStudyRule next={0};
        if(swscanf_s(line,L"MODULE %203ls %203ls %179ls %d %d %d %d %d %d",stored,(unsigned)_countof(stored),
            parent,(unsigned)_countof(parent),label,(unsigned)_countof(label),&next.type,&next.state,&next.pattern,
            &next.under,&next.count,&next.reported)!=9||wcscmp(stored,id))continue;
        error=PRO_TK_BAD_INPUTS;
        if(next.count<0||next.count>AX_STUDY_ROWS||wcslen(label)>=_countof(next.name))break;
        wcscpy_s(next.parent,_countof(next.parent),parent);
        wcscpy_s(next.name,_countof(next.name),label);
        next.pose[3][3]=1;
        if(!fgetws(line,_countof(line),in)||swscanf_s(line,L"POSE %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
            &next.pose[0][0],&next.pose[0][1],&next.pose[0][2],&next.pose[1][0],&next.pose[1][1],&next.pose[1][2],
            &next.pose[2][0],&next.pose[2][1],&next.pose[2][2],&next.pose[3][0],&next.pose[3][1],&next.pose[3][2])!=12)break;
        int i=0;
        for(;i<next.count;i++){
            AxStudyRow *row=&next.rows[i];
            if(!fgetws(line,_countof(line),in)||swscanf_s(line,L"RULE %d %d %lf %d %lf",&row->type,&row->flags,&row->offset,&row->has_array_gap,&row->array_gap)!=5||
               !isfinite(row->offset)||!isfinite(row->array_gap)||!ax_study_read_ref(in,&row->base)||!ax_study_read_ref(in,&row->part))break;
        }
        if(i!=next.count)break;
        for(i=0;i<4;i++)for(int j=0;j<3;j++)if(!isfinite(next.pose[i][j]))goto done;
        for(i=0;i<3;i++)if(fabs(ad_dot(next.pose[i],next.pose[i])-1)>1e-5||fabs(ad_dot(next.pose[i],next.pose[(i+1)%3]))>1e-5)goto done;
        *rule=next;
        error=PRO_TK_NO_ERROR;
        break;
    }
done:
    fclose(in);
    return error;
}
