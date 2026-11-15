
/* Company files are configuration data. Tables accept whitespace-delimited
   fields, exact keys and bounded values. No file text is executed. */
typedef int (*SmxTableVisit)(wchar_t **fields,void *context);
static ProPath smx_data_directory;

static ProError smx_table_file(const wchar_t *leaf,ProPath path){
    ProPath directory;
    ProError error=ProDirectoryCurrentGet(directory);
    if(error)return error;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",directory,leaf)<0)return PRO_TK_BAD_INPUTS;
    if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)
        return dv_file_nonempty(path)?PRO_TK_NO_ERROR:PRO_TK_CANT_OPEN;
    if(!smx_data_directory[0])return PRO_TK_E_NOT_FOUND;
    wcscpy_s(directory,PRO_PATH_SIZE,smx_data_directory);
    wchar_t *end=wcsrchr(directory,L'\\');
    if(!end||_wcsicmp(end+1,L"data"))return PRO_TK_E_NOT_FOUND;
    *end=0;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\公司资料\\公司标准\\%ls",directory,leaf)<0)return PRO_TK_BAD_INPUTS;
    return dv_file_nonempty(path)?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static ProError smx_table_read(const wchar_t *path,const wchar_t *leaf,int columns,SmxTableVisit visit,void *context,PartText *out){
    wchar_t *fields[16];
    if(columns<1||columns>_countof(fields))return PRO_TK_BAD_INPUTS;
    ProError error=PRO_TK_NO_ERROR;
    FILE *file=_wfopen(path,L"rt, ccs=UTF-8");
    if(!file)return PRO_TK_CANT_OPEN;
    wchar_t line[512];
    int rows=0,line_no=0;
    while(fgetws(line,_countof(line),file)){
        line_no++;
        if(!wcschr(line,L'\n')&&!feof(file)){error=PRO_TK_LINE_TOO_LONG;break;}
        wchar_t *p=line+wcsspn(line,L" \t\r\n");
        if(!*p||*p==L'#')continue;
        wchar_t *save=NULL;
        int count=0;
        for(wchar_t *field=wcstok_s(p,L" \t\r\n",&save);field;field=wcstok_s(NULL,L" \t\r\n",&save)){
            if(count==columns){count++;break;}
            fields[count++]=field;
        }
        if(++rows>512||count!=columns||!visit(fields,context)){error=PRO_TK_BAD_INPUTS;break;}
    }
    if(!error&&(ferror(file)||!rows))error=PRO_TK_BAD_INPUTS;
    fclose(file);
    if(error)part_text_add(out,L"公司表 %ls 第 %d 行格式、重复键或数值范围有误。\n",leaf,line_no);
    else part_text_add(out,L"规则来源 %ls。\n",leaf);
    return error;
}

static ProError smx_table(const wchar_t *leaf,int columns,SmxTableVisit visit,void *context,PartText *out){
    ProPath path;
    ProError error=smx_table_file(leaf,path);
    if(error){
        part_text_add(out,L"公司表 %ls 不可用（%d），规则待核实。\n",leaf,error);
        return error;
    }
    return smx_table_read(path,leaf,columns,visit,context,out);
}

typedef struct {
    const wchar_t *grade;
    double target,nominal,measured,parallel,cross;
    wchar_t finish[32],bend_table[32],version[32];
    int found;
} SmxMaterial;

static int smx_material_row(wchar_t **f,void *context){
    SmxMaterial *row=context;
    double nominal,measured,parallel,cross;
    if(!smx_word(f[0])||!smx_number(f[1],0.01,100)||!smx_number(f[2],0.01,100)||
        !smx_word(f[3])||!smx_number(f[4],0.01,1000)||!smx_number(f[5],0.01,1000)||
        !smx_word(f[6])||!smx_word(f[7]))return 0;
    part_number(f[1],&nominal);part_number(f[2],&measured);
    part_number(f[4],&parallel);part_number(f[5],&cross);
    if(_wcsicmp(f[0],row->grade)||fabs(nominal-row->target)>0.00001)return 1;
    if(row->found)return 0;
    row->found=1;
    row->nominal=nominal;row->measured=measured;row->parallel=parallel;row->cross=cross;
    wcscpy_s(row->finish,_countof(row->finish),f[3]);
    wcscpy_s(row->bend_table,_countof(row->bend_table),f[6]);
    wcscpy_s(row->version,_countof(row->version),f[7]);
    return 1;
}

static ProError smx_material_get(const wchar_t *grade,double thickness,SmxMaterial *result,PartText *out){
    result->grade=grade;
    result->target=thickness;
    ProError error=smx_table(L"BWTL_SMT_MATERIALS.tsv",8,smx_material_row,result,out);
    if(!error&&!result->found)part_text_add(out,L"%ls %.8g mm 未收录，待核实。\n",grade,thickness);
    return error?error:result->found?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static void smx_material_print(const SmxMaterial *row,PartText *out){
    part_text_add(out,L"%ls：标称 %.8g mm，实测记录 %.8g mm，表面 %ls，平行纹向内 R 下限 %.8g mm，横向 %.8g mm，折弯表 %ls，版本 %ls。\n",
        row->grade,row->nominal,row->measured,row->finish,row->parallel,row->cross,row->bend_table,row->version);
}

static ProError smx_material_query(const PartCommand *cmd,int kind,PartText *out){
    SmxMaterial original={0},candidate={0};
    double thickness=as_arg(cmd,kind==SMX_MATERIAL?3:4);
    ProError error=smx_material_get(cmd->arg[2],thickness,&original,out);
    if(error)return error;
    smx_material_print(&original,out);
    if(kind==SMX_MATERIAL)return PRO_TK_NO_ERROR;
    error=smx_material_get(cmd->arg[3],thickness,&candidate,out);
    if(error)return error;
    smx_material_print(&candidate,out);
    part_text_add(out,L"代料差异：实测厚度 %+.8g mm，平行/横向内 R 下限 %+.8g/%+.8g mm，折弯表 %ls，表面处理 %ls。原模型资料保留。\n",
        candidate.measured-original.measured,candidate.parallel-original.parallel,candidate.cross-original.cross,
        _wcsicmp(candidate.bend_table,original.bend_table)?L"需换表复核":L"相同",
        _wcsicmp(candidate.finish,original.finish)?L"需重新确认":L"相同");
    return PRO_TK_NO_ERROR;
}

typedef struct {
    const wchar_t *grade,*method;
    double thickness,radius,angle,factor;
    wchar_t version[32];
    int found;
} SmxBendRule;

static int smx_bend_row(wchar_t **f,void *context){
    SmxBendRule *row=context;
    if(!smx_word(f[0])||!smx_number(f[1],0.01,100)||!smx_number(f[2],0.01,1000)||
       !smx_number(f[3],1,180)||!smx_word(f[4])||!smx_number(f[5],0,1)||!smx_word(f[6]))return 0;
    double t,r,a,k;
    part_number(f[1],&t);part_number(f[2],&r);part_number(f[3],&a);part_number(f[5],&k);
    if(_wcsicmp(f[0],row->grade)||_wcsicmp(f[4],row->method)||fabs(t-row->thickness)>0.00001||
       fabs(r-row->radius)>0.00001||fabs(a-row->angle)>0.00001)return 1;
    if(row->found)return 0;
    row->found=1;row->factor=k;
    wcscpy_s(row->version,_countof(row->version),f[6]);
    return 1;
}

static ProError smx_bend_rule(const PartCommand *cmd,PartText *out){
    SmxBendRule row={cmd->arg[2],cmd->arg[6],as_arg(cmd,3),as_arg(cmd,4),as_arg(cmd,5)};
    ProError error=smx_table(L"BWTL_SMT_BENDS.tsv",7,smx_bend_row,&row,out);
    if(error)return error;
    if(!row.found){
        part_text_add(out,L"未匹配到材料、板厚、内 R、角度和加工方式完全一致的规则，待核实。\n");
        return PRO_TK_E_NOT_FOUND;
    }
    double ba=row.angle*AS_PI/180*(row.radius+row.factor*row.thickness);
    part_text_add(out,L"%ls 规则 %ls：K=%.8g，按该规则的展开弧长 %.8g mm。未改变模型。\n",row.method,row.version,row.factor,ba);
    return PRO_TK_NO_ERROR;
}

typedef struct {
    const wchar_t *name,*text;
    ProParameter parameter;
    ProParamvalue old;
    int existed,written;
} SmxParameter;

static ProError smx_text_get(ProMdl model,const wchar_t *name,ProLine value){
    ProModelitem item;
    ProParameter parameter;
    ProParamvalue old;
    value[0]=0;
    ProError error=ProMdlToModelitem(model,&item);
    if(!error)error=ProParameterInit(&item,(wchar_t *)name,&parameter);
    if(!error)error=ProParameterValueWithUnitsGet(&parameter,&old,NULL);
    if(!error&&old.type!=PRO_PARAM_STRING)error=PRO_TK_BAD_CONTEXT;
    if(!error)wcscpy_s(value,PRO_LINE_SIZE,old.value.s_val);
    return error;
}

static ProError smx_params(ProMdl model,SmxParameter *rows,int count,int overwrite,PartText *out){
    ProModelitem item;
    ProError error=ProMdlToModelitem(model,&item);
    for(int i=0;!error&&i<count;i++){
        SmxParameter *row=&rows[i];
        error=ProParameterInit(&item,(wchar_t *)row->name,&row->parameter);
        if(error==PRO_TK_E_NOT_FOUND){error=PRO_TK_NO_ERROR;continue;}
        if(error)break;
        row->existed=1;
        error=ProParameterValueWithUnitsGet(&row->parameter,&row->old,NULL);
        if(!error&&(row->old.type!=PRO_PARAM_STRING||(!overwrite&&wcscmp(row->old.value.s_val,row->text)))){
            part_text_add(out,L"参数 %ls 已有不同值或不是文本，整组未更新。\n",row->name);
            error=PRO_TK_BAD_CONTEXT;
        }
    }
    for(int i=0;!error&&i<count;i++){
        SmxParameter *row=&rows[i];
        ProParamvalue next={0};
        ProLine actual;
        if(row->existed&&!wcscmp(row->old.value.s_val,row->text))continue;
        next.type=PRO_PARAM_STRING;
        if(wcscpy_s(next.value.s_val,_countof(next.value.s_val),row->text))return PRO_TK_BAD_INPUTS;
        if(row->existed)error=ProParameterValueWithUnitsSet(&row->parameter,&next,NULL);
        else error=ProParameterWithUnitsCreate(&item,(wchar_t *)row->name,&next,NULL,&row->parameter);
        if(!error){row->written=1;error=smx_text_get(model,row->name,actual);}
        if(!error&&wcscmp(actual,row->text))error=PRO_TK_GENERAL_ERROR;
    }
    if(error)for(int i=count-1;i>=0;i--)if(rows[i].written){
        SmxParameter *row=&rows[i];
        ProLine actual;
        ProError restore=row->existed?ProParameterValueWithUnitsSet(&row->parameter,&row->old,NULL):ProParameterDelete(&row->parameter);
        ProError verify=smx_text_get(model,row->name,actual);
        if(restore||(row->existed?(verify||wcscmp(actual,row->old.value.s_val)):verify!=PRO_TK_E_NOT_FOUND)){
            sh_recovery_uncertain=1;
            part_text_add(out,L"参数 %ls 的恢复需要检查，已暂停钣金写入。\n",row->name);
        }
    }
    return error;
}

static ProError smx_metadata(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(error)return error;
    if(fabs(thickness-as_arg(cmd,7))>0.001){
        part_text_add(out,L"当前实际板厚 %.8g mm 与填写值 %.8g mm 不一致。\n",thickness,as_arg(cmd,7));
        return PRO_TK_BAD_CONTEXT;
    }
    SmxMaterial rule={0};
    error=smx_material_get(cmd->arg[5],thickness,&rule,out);
    if(error)return error;
    SmxParameter values[]={{L"PART_CODE",cmd->arg[2]},{L"DRAWING_NO",cmd->arg[3]},{L"REVISION",cmd->arg[4]},
        {L"MATERIAL",cmd->arg[5]},{L"BWTL_FINISH",cmd->arg[6]},{L"BWTL_THICKNESS_MM",cmd->arg[7]},
        {L"BWTL_PROCESS_ROUTE",cmd->arg[8]},{L"BWTL_MATERIAL_REV",rule.version}};
    error=smx_params(model,values,_countof(values),0,out);
    if(!error)part_text_add(out,L"八项制造资料参数已回读，实体厚度 %.8g mm。材料字段为制造元数据，Creo 物理材料由材料库另行设置。模型未保存。\n",thickness);
    return error;
}

typedef struct { const wchar_t *key; ProMdlName model; wchar_t version[32]; int found; } SmxTemplate;

static int smx_template_row(wchar_t **f,void *context){
    SmxTemplate *row=context;
    if(!smx_word(f[0])||!fd_name(f[1])||!smx_word(f[2]))return 0;
    if(_wcsicmp(f[0],row->key))return 1;
    if(row->found)return 0;
    row->found=1;
    wcscpy_s(row->model,_countof(row->model),f[1]);
    wcscpy_s(row->version,_countof(row->version),f[2]);
    return 1;
}

static ProError smx_template_create(const PartCommand *cmd,PartText *out){
    SmxTemplate row={cmd->arg[2]};
    ProError error=smx_table(L"BWTL_SMT_TEMPLATES.tsv",3,smx_template_row,&row,out);
    if(error)return error;
    if(!row.found){part_text_add(out,L"未找到模板类型 %ls，待核实。\n",row.key);return PRO_TK_E_NOT_FOUND;}
    if(!part_target_available(NULL,cmd->arg[1],L"prt"))return PRO_TK_E_FOUND;
    ProMdl source=NULL,copy=NULL;
    int loaded=0;
    ProMdlsubtype subtype;
    TemplateDatums before={0},after={0};
    ProMdlName name;
    wcscpy_s(name,_countof(name),cmd->arg[1]);
    error=ax_load_part(row.model,&source,&loaded);
    if(!error)error=ProMdlSubtypeGet(source,&subtype);
    if(!error&&subtype!=PROMDLSTYPE_PART_SHEETMETAL)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=template_empty((ProSolid)source);
    if(!error)error=ProSolidFeatVisit((ProSolid)source,template_feature,NULL,&before);
    if(!error)error=ProMdlnameCopy(source,name,&copy);
    if(!error)error=ProSolidFeatVisit((ProSolid)copy,template_feature,NULL,&after);
    if(!error&&(before.total!=after.total||before.planes!=after.planes||before.csys!=after.csys))error=PRO_TK_GENERAL_ERROR;
    if(!error){
        SmxParameter tag[]={{L"BWTL_TEMPLATE_REV",row.version}};
        error=smx_params(copy,tag,_countof(tag),1,out);
    }
    if(!error)error=dv_display_new(copy);
    if(error&&copy)ProMdlErase(copy);
    if(loaded&&source)ProMdlErase(source);
    if(!error)part_text_add(out,L"已从模板 %ls（%ls）创建钣金件 %ls，保留 %d 个基准面、%d 个坐标系。模型未保存修改。\n",
        row.model,row.version,name,after.planes,after.csys);
    return error;
}

typedef struct { wchar_t release[32],material[32],bend[32],shape[32]; int found; } SmxVersion;

static int smx_version_row(wchar_t **f,void *context){
    SmxVersion *row=context;
    if(row->found)return 0;
    for(int i=0;i<4;i++)if(!smx_word(f[i]))return 0;
    wcscpy_s(row->release,_countof(row->release),f[0]);
    wcscpy_s(row->material,_countof(row->material),f[1]);
    wcscpy_s(row->bend,_countof(row->bend),f[2]);
    wcscpy_s(row->shape,_countof(row->shape),f[3]);
    row->found=1;
    return 1;
}

static ProError smx_revision(ProMdl model,const PartCommand *cmd,PartText *out){
    SmxVersion row={0};
    ProError error=smx_table(L"BWTL_SMT_STANDARD.tsv",4,smx_version_row,&row,out);
    if(error)return error;
    if(_wcsicmp(row.release,cmd->arg[2]))return PRO_TK_BAD_CONTEXT;
    const wchar_t *names[]={L"BWTL_MATERIAL_REV",L"BWTL_BEND_REV",L"BWTL_TEMPLATE_REV"};
    const wchar_t *expected[]={row.material,row.bend,row.shape};
    int differences=0;
    for(int i=0;i<_countof(names);i++){
        ProLine actual;
        error=smx_text_get(model,names[i],actual);
        if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
        int same=!error&&!_wcsicmp(actual,expected[i]);
        differences+=!same;
        part_text_add(out,L"%ls：当前 %ls，标准 %ls，%ls。\n",names[i],actual[0]?actual:L"缺项",expected[i],same?L"一致":L"待复核");
    }
    if(cmd->count==3)return PRO_TK_NO_ERROR;
    ProLine old;
    error=smx_text_get(model,L"BWTL_REVIEW_REV",old);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    if(_wcsicmp(cmd->arg[4],error?L"NONE":old))return PRO_TK_BAD_CONTEXT;
    if(differences){
        part_text_add(out,L"有 %d 项来源未匹配，请逐项核对后登记审查版本。\n",differences);
        return PRO_TK_BAD_CONTEXT;
    }
    SmxParameter change[]={{L"BWTL_REVIEW_REV",row.release}};
    error=smx_params(model,change,_countof(change),1,out);
    if(!error)part_text_add(out,L"已登记标准审核版 %ls。模型未保存。\n",row.release);
    return error;
}
