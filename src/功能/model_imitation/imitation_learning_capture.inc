static void method_add_operation(ModelingMethod *method,wchar_t operation){
    size_t length=wcslen(method->sequence);
    if(length>=METHOD_MAX_STEPS)return;
    method->sequence[length]=operation;
    method->sequence[length+1]=0;
    method->tags|=method_categories[method_category(operation)].tag;
}

static wchar_t method_feature_code(ProFeattype type,const wchar_t *operation,int conversion){
    if(wcsstr(operation,L"旋转")||wcsstr(operation,L"扫描"))return L'V';
    if(wcsstr(operation,L"切除")||wcsstr(operation,L"去料")||wcsstr(operation,L"切口"))return L'C';
    switch(type){
    case PRO_FEAT_FIRST_FEAT:case PRO_FEAT_PROTRUSION:return L'B';
    case PRO_FEAT_DATUM_SURF:return conversion?L'O':L'W';
    case PRO_FEAT_WALL:case PRO_FEAT_BEND:return L'F';
    case PRO_FEAT_HOLE:return L'H';
    case PRO_FEAT_CUT:case PRO_FEAT_CUT_SMT:return L'C';
    case PRO_FEAT_ROUND:case PRO_FEAT_CHAMFER:case PRO_FEAT_CORN_CHAMF:return L'R';
    case PRO_FEAT_CURVE:return L'S';
    case PRO_FEAT_DATUM:case PRO_FEAT_DATUM_AXIS:case PRO_FEAT_DATUM_POINT:case PRO_FEAT_CSYS:return L'D';
    case PRO_FEAT_GROUP_HEAD:case PRO_FEAT_SRF_MDL:return L'G';
    default:return L'O';
    }
}

typedef struct { PartText *text; double mm; int count; } MethodDimensions;

static ProError method_dimension(ProDimension *dimension,ProError status,ProAppData data){
    MethodDimensions *dimensions=data;
    if(status)return status;
    if(dimensions->count>=12)return PRO_TK_NO_ERROR;
    double value;
    ProDimensiontype type;
    ProBoolean driving,related=PRO_B_FALSE;
    if(ProDimensionTypeGet(dimension,&type)||ProDimensionValueGet(dimension,&value)||
       !isfinite(value)||ProDimensionIsDriving(dimension,&driving))return PRO_TK_NO_ERROR;
    if(driving)ProDimensionIsReldriven(dimension,&related);
    part_text_add(dimensions->text,L" %d/%d=%.8g %ls（%ls）",dimension->id,type,
        value*(type==PRODIMTYPE_ANGLE?1.0:dimensions->mm),type==PRODIMTYPE_ANGLE?L"°":L"mm",
        related?L"关系式驱动":driving?L"驱动":L"参考");
    dimensions->count++;
    return PRO_TK_NO_ERROR;
}

static void method_section_evidence(ProFeature *feature,int count,PartText *out){
    for(int i=0;i<count&&i<8;i++){
        ProSection section=NULL;
        int *entities=NULL,entity_count=0;
        ProIntlist constraints=NULL;
        int constraint_count=0;
        ProError error=ProFeatureSectionCopy(feature,i,&section);
        if(!error)error=ProSectionEntityIdsGet(section,&entities,&entity_count);
        if(!error){
            part_text_add(out,L"  截面 %d，图元 %d。",i+1,entity_count);
            if(!ProSectionConstraintsIdsGet(section,&constraints,&constraint_count)){
                part_text_add(out,L"已启用约束（原生类型/图元.位置）");
                for(int j=0;j<constraint_count&&j<16;j++){
                    ProConstraintType type;
                    ProConstraintStatus state;
                    int *refs=NULL,ref_count=0;
                    ProSectionPointType *senses=NULL;
                    if(!ProSectionConstraintsGet(section,constraints[j],&type,&state,&ref_count,&refs,&senses)&&
                       state==PRO_TK_CONSTRAINT_ENABLED){
                        part_text_add(out,L" %d[",type);
                        for(int k=0;k<ref_count&&k<8;k++)
                            part_text_add(out,L"%ls%d.%d",k?L",":L"",refs[k],senses[k]);
                        part_text_add(out,L"]");
                    }
                    if(refs)ProArrayFree((ProArray *)&refs);
                    if(senses)ProArrayFree((ProArray *)&senses);
                }
                if(constraint_count>16)part_text_add(out,L" …");
            }
            part_text_add(out,L"\n");
        }else part_text_add(out,L"  截面 %d 读取返回 Creo %d。\n",i+1,error);
        if(entities)ProArrayFree((ProArray *)&entities);
        if(constraints)ProArrayFree((ProArray *)&constraints);
        if(section)ProSectionFree(&section);
    }
}

static void method_capture(ProMdl source,ProMdl verified,ProMdlsubtype subtype,
                           const ReplayOrder *order,int edit_verified,ModelingLesson *lesson){
    lesson->method.kind=subtype==PROMDLSTYPE_PART_SHEETMETAL?L'M':L'S';
    lesson->method.edit_verified=edit_verified;
    ProMdlName source_name=L"";
    if(!ProMdlMdlnameGet(source,source_name))
        wcsncpy_s(lesson->source,_countof(lesson->source),source_name,_TRUNCATE);
    PartText out={lesson->evidence,_countof(lesson->evidence),0};
    ProUnitsystem units;
    ProUnititem length,mm;
    ProUnitConversion scale;
    double factor=0;
    if(!ProMdlPrincipalunitsystemGet(verified,&units)&&
       !ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&length)&&
       !ProUnitInit(verified,L"mm",&mm)&&!ProUnitConversionCalculate(&length,&mm,&scale))
        factor=scale.scale;
    part_text_add(&out,L"源零件 %ls\n模型类型 %ls\n验收：已保存重开，并通过特征树、依赖、几何和材料核对。%ls\n"
        L"以下尺寸和编号仅描述学习样件。建模时使用新任务的尺寸、几何和本地参照。\n\n",
        lesson->source,lesson->method.kind==L'M'?L"钣金":L"实体",
        edit_verified?L" 已完成尺寸试改并恢复。":L"");
    for(int i=0;i<order->count;i++){
        ProFeature feature;
        if(ProFeatureInit((ProSolid)verified,order->step[i].id,&feature))continue;
        const ProFeattype type=order->step[i].type;
        const wchar_t *operation=replay_operation_name(type);
        ProElement tree=NULL;
        if(!ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)){
            replay_definition_supported(tree,type,&operation);
            ProFeatureElemtreeFree(&feature,tree);
        }
        if(order->step[i].conversion)operation=L"转换为钣金";
        ProName name=L"";
        ProModelitemNameGet(&feature,name);
        for(wchar_t *p=name;*p;p++)if(*p<L' ')*p=L' ';
        const wchar_t code=method_feature_code(type,operation,order->step[i].conversion);
        method_add_operation(&lesson->method,code);
        part_text_add(&out,L"%d. %ls %ls（验收件特征 %d）\n",i+1,operation,name,feature.id);
        if(order->step[i].group)part_text_add(&out,L"  属于第 %d 步的特征组。\n",order->step[i].group);
        if(order->step[i].section_link>0)part_text_add(&out,L"  引用第 %d 步的独立草绘。\n",order->step[i].section_link);
        else if(order->step[i].sections)part_text_add(&out,L"  保留特征内截面 %d 个。\n",order->step[i].sections);
        int *parents=NULL,parent_count=0;
        if(!ProFeatureParentsGet(&feature,&parents,&parent_count)&&parent_count){
            part_text_add(&out,L"  父特征");
            for(int j=0;j<parent_count&&j<16;j++){
                const int position=replay_order_position(order,parents[j]);
                if(position)part_text_add(&out,L" 第 %d 步",position);
                else part_text_add(&out,L" 本地 ID %d",parents[j]);
            }
            part_text_add(&out,parent_count>16?L" …\n":L"\n");
        }
        if(parents)ProArrayFree((ProArray *)&parents);
        if(order->step[i].members){
            method_add_operation(&lesson->method,L'P');
            part_text_add(&out,L"  本特征作为阵列种子，原生阵列类型 %d，成员 %d。\n",
                order->step[i].pattern_type,order->step[i].members);
        }
        if(factor>0){
            MethodDimensions dimensions={&out,factor,0};
            part_text_add(&out,L"  模型尺寸（编号/类型）");
            ProFeatureDimensionVisit(&feature,method_dimension,NULL,&dimensions);
            part_text_add(&out,dimensions.count?L"\n":L" 未读到。\n");
        }
        method_section_evidence(&feature,order->step[i].sections,&out);
        part_text_add(&out,L"\n");
    }

}

static ProError method_save_last(PartText *out){
    const ModelingMethod *method=&method_last.method;
    if(!method->sequence[0]){
        part_text_add(out,L"本次会话还没有通过验收的仿照案例。请先完成一次按原步骤重建。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    wchar_t directory[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    if(!method_directory(directory,1)){
        part_text_add(out,L"无法在本会话所选数据盘的目录中创建建模方法库。\n");
        return PRO_TK_CANT_WRITE;
    }
    if(method_last.saved[0]){
        ModelingMethod saved={0};
        if(method_header_read(method_last.saved,&saved)&&!wcscmp(saved.sequence,method->sequence)){
            part_text_add(out,L"这次建模方法已学习。\n%ls\n",method_last.saved);
            return PRO_TK_NO_ERROR;
        }
        method_last.saved[0]=0;
    }
    wchar_t label[25];
    size_t name_length=wcsnlen(method_last.source,_countof(label)-1);
    for(size_t i=0;i<name_length;i++){
        const wchar_t ch=method_last.source[i];
        label[i]=ch>=L' '&&!wcschr(L"<>:\"/\\|?*",ch)?ch:L'_';
    }
    label[name_length]=0;
    SYSTEMTIME utc;
    GetSystemTime(&utc);
    if(swprintf_s(path,MAX_PATH,L"%ls\\%04u%02u%02u-%02u%02u%02u-%03u-%08lx-%ls.bwtlmethod.txt",directory,
        utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,GetCurrentProcessId(),label[0]?label:L"model")<0||
       swprintf_s(temp,MAX_PATH,L"%ls.tmp",path)<0)return PRO_TK_CANT_WRITE;
    FILE *file=_wfopen(temp,L"wtx, ccs=UTF-8");
    if(!file)return PRO_TK_CANT_WRITE;
    int ok=fwprintf(file,L"%ls%lc %d %ls\n\n%ls\n可复用做法（依据实际特征归纳）\n",METHOD_FILE_PREFIX,
        method->kind,method->edit_verified,method->sequence,method_last.evidence)>=0;
    for(int i=0;ok&&i<_countof(method_categories);i++)
        if((method->tags&method_categories[i].tag)&&*method_categories[i].principle)
            ok=fwprintf(file,L"- %ls\n",method_categories[i].principle)>=0;
    if(fclose(file))ok=0;
    if(!ok||!MoveFileExW(temp,path,MOVEFILE_WRITE_THROUGH)){
        DeleteFileW(temp);
        return PRO_TK_CANT_WRITE;
    }
    wcscpy_s(method_last.saved,_countof(method_last.saved),path);
    part_text_add(out,L"已学习 %ls 的建模方法，并保存为独立文件。之后建模时，BWTL 会按新需求选用相关做法。\n%ls\n",
        method_last.source,path);
    return PRO_TK_NO_ERROR;
}