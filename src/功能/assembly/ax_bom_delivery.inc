
enum AxBomField { AB_CODE,AB_DRAWING,AB_REV,AB_MATERIAL,AB_SPEC,AB_SOURCE,AB_KIND,AB_LENGTH,
    AB_AREA,AB_POSITION,AB_SPARE,AB_TORQUE,AB_CHECK,AB_FIELDS };
typedef struct { ProMdl model; ProMdlType type; ProMdlName name; ProLine value[AB_FIELDS]; int qty; } AxBomModel;
typedef struct { AxScan *scan; AxBomModel *models; int *rows,count; } AxBomData;

static void ax_bom_read(AxBomModel *row){
    static const wchar_t *const fields[AB_FIELDS][4]={
        {L"PART_CODE",L"零件号"},{L"DRAWING_NO",L"图号"},{L"REVISION",L"REV",L"版本"},
        {L"PTC_REPORTED_MATERIAL",L"MATERIAL",L"材料",L"材质"},{L"SPEC",L"SPECIFICATION",L"规格"},
        {L"BWTL_SOURCE",L"PROCUREMENT_TYPE",L"采购类别"},{L"BWTL_KIND"},{L"BWTL_LENGTH"},
        {L"BWTL_AREA"},{L"BWTL_POSITION"},{L"BWTL_SPARE"},{L"BWTL_TORQUE"},{L"BWTL_CHECK"}
    };
    for(int i=0;i<AB_FIELDS;i++){
        int count=0;
        while(count<4&&fields[i][count])count++;
        ax_parameter_text(row->model,fields[i],count,row->value[i]);
    }
    if(!_wcsicmp(row->value[AB_MATERIAL],L"PTC_SYSTEM_MTRL_PROPS"))row->value[AB_MATERIAL][0]=0;
    if(row->type==PRO_MDL_PART&&!row->value[AB_MATERIAL][0]){
        ProMaterial material;
        if(!ProMaterialCurrentGet((ProSolid)row->model,&material)&&_wcsicmp(material.matl_name,L"PTC_SYSTEM_MTRL_PROPS"))
            wcscpy_s(row->value[AB_MATERIAL],PRO_LINE_SIZE,material.matl_name);
    }
}

static void ax_bom_clear(AxBomData *data){
    free(data->rows);
    free(data->models);
    free(data->scan);
}

static ProError ax_bom_collect(ProMdl assembly,AxBomData *data,int allow_missing){
    ProError error=ax_scan(assembly,0,&data->scan);
    if(error)return error;
    if(data->scan->limited||(!allow_missing&&data->scan->unavailable))return PRO_TK_INCOMPLETE;
    size_t capacity=(size_t)data->scan->count+1;
    data->models=calloc(capacity,sizeof(*data->models));
    data->rows=calloc(capacity,sizeof(*data->rows));
    if(!data->models||!data->rows)return PRO_TK_OUT_OF_MEMORY;
    for(int i=-1;i<data->scan->count;i++){
        AxOccurrence *item=i<0?NULL:&data->scan->occurrences[i];
        ProMdl model=item?item->model:assembly;
        data->rows[i+1]=-1;
        if(!model)continue;
        int row=0;
        while(row<data->count&&data->models[row].model!=model)row++;
        AxBomModel *entry=&data->models[row];
        if(row==data->count){
            entry->model=model;
            if((error=ProMdlMdlnameGet(model,entry->name))||(error=ProMdlTypeGet(model,&entry->type)))return error;
            ax_bom_read(entry);
            data->count++;
        }
        entry->qty++;
        data->rows[i+1]=row;
    }
    return PRO_TK_NO_ERROR;
}

static int ax_bom_csv(FILE *file,const wchar_t *const *fields,int count){
    for(int i=0;i<count;i++)
        if((i&&fputwc(L',',file)==WEOF)||!dv_csv_text(file,fields[i]))return 0;
    return fputwc(L'\n',file)!=WEOF;
}

static ProError ax_bom_open(const wchar_t *folder,const wchar_t *leaf,FILE **file){
    ProPath path;
    if(swprintf_s(path,_countof(path),L"%ls\\%ls",folder,leaf)<0)return PRO_TK_BAD_INPUTS;
    *file=_wfopen(path,L"wt, ccs=UTF-8");
    return *file?PRO_TK_NO_ERROR:PRO_TK_CANT_WRITE;
}

static int ax_bom_path(AxOccurrence *item,wchar_t path[256]){
    if(item)return ax_path_label(item->ids,item->depth,path,256);
    wcscpy_s(path,256,L"ROOT");
    return 1;
}

static int ax_bom_tree_csv(FILE *file,const AxBomData *data){
    int ok=fwprintf(file,L"层级,实例,父实例,模型名,类型,状态,实例数量,料号,图号,版本,材料,规格\n")>0;
    for(int i=-1;ok&&i<data->scan->count;i++){
        AxOccurrence *item=i<0?NULL:&data->scan->occurrences[i];
        int depth=item?item->depth:0;
        int index=data->rows[i+1];
        const AxBomModel *row=index>=0?&data->models[index]:NULL;
        wchar_t level[16],path[256],parent[256]=L"";
        swprintf_s(level,_countof(level),L"%d",depth);
        if(!ax_bom_path(item,path))return 0;
        if(depth==1)wcscpy_s(parent,_countof(parent),L"ROOT");
        else if(depth>1&&!ax_path_label(item->ids,depth-1,parent,_countof(parent)))return 0;
        ProMdlType type=row?row->type:item->type;
        const wchar_t *fields[]={level,path,parent,row?row->name:item->name,type==PRO_MDL_ASSEMBLY?L"装配":type==PRO_MDL_PART?L"零件":L"未知",
            item?ax_state_label(item->state):L"总装",row?L"1":L"0",row?row->value[AB_CODE]:L"",row?row->value[AB_DRAWING]:L"",
            row?row->value[AB_REV]:L"",row?row->value[AB_MATERIAL]:L"",row?row->value[AB_SPEC]:L""};
        ok=ax_bom_csv(file,fields,_countof(fields));
    }
    return ok;
}

static const wchar_t *ax_bom_source(const wchar_t *value){
    if(!_wcsicmp(value,L"MAKE")||!wcscmp(value,L"自制件"))return L"自制件";
    if(!_wcsicmp(value,L"BUY")||!wcscmp(value,L"外购件"))return L"外购件";
    if(!_wcsicmp(value,L"STANDARD")||!wcscmp(value,L"标准件"))return L"标准件";
    return L"待分类";
}

static int ax_bom_buy_csv(FILE *file,const AxBomData *data){
    static const wchar_t *const classes[]={L"自制件",L"外购件",L"标准件",L"待分类"};
    int ok=fwprintf(file,L"采购分类,模型名,料号,数量,图号,版本,材料,规格\n")>0;
    for(int c=0;ok&&c<_countof(classes);c++)for(int i=1;ok&&i<data->count;i++){
        const AxBomModel *row=&data->models[i];
        if(row->type!=PRO_MDL_PART||wcscmp(classes[c],ax_bom_source(row->value[AB_SOURCE])))continue;
        wchar_t qty[16];
        swprintf_s(qty,_countof(qty),L"%d",row->qty);
        const wchar_t *fields[]={classes[c],row->name,row->value[AB_CODE],qty,row->value[AB_DRAWING],
            row->value[AB_REV],row->value[AB_MATERIAL],row->value[AB_SPEC]};
        ok=ax_bom_csv(file,fields,_countof(fields));
    }
    return ok;
}

static int ax_bom_diff_csv(FILE *file,const AxBomData *current,const AxBomData *prior){
    int matched[AX_MAX_OCCURRENCES+1]={0};
    int pairs[AX_MAX_OCCURRENCES+1];
    int ok=fwprintf(file,L"变化,旧模型,新模型,旧料号,新料号,旧版本,新版本,旧数量,新数量,旧材料,新材料\n")>0;
    for(int i=1;i<current->count;i++){
        pairs[i]=0;
        const AxBomModel *a=&current->models[i];
        for(int j=1;j<prior->count;j++){
            const AxBomModel *b=&prior->models[j];
            if(!matched[j]&&a->type==b->type&&!_wcsicmp(a->name,b->name)){
                pairs[i]=j;
                matched[j]=1;
                break;
            }
        }
    }
    for(int i=1;i<current->count;i++)if(!pairs[i]&&current->models[i].value[AB_CODE][0]){
        const AxBomModel *a=&current->models[i];
        int candidate=0,old_count=0,new_count=0;
        for(int j=1;j<prior->count;j++){
            const AxBomModel *b=&prior->models[j];
            if(a->type==b->type&&!_wcsicmp(a->value[AB_CODE],b->value[AB_CODE])){candidate=j;old_count++;}
        }
        for(int j=1;j<current->count;j++){
            const AxBomModel *b=&current->models[j];
            if(a->type==b->type&&!_wcsicmp(a->value[AB_CODE],b->value[AB_CODE]))new_count++;
        }
        if(old_count==1&&new_count==1&&!matched[candidate]){pairs[i]=candidate;matched[candidate]=1;}
    }
    for(int i=1;ok&&i<current->count+prior->count-1;i++){
        const AxBomModel *a=i<current->count?&current->models[i]:NULL;
        int j=a?pairs[i]:i-current->count+1;
        if(!a&&matched[j])continue;
        const AxBomModel *b=j?&prior->models[j]:NULL;
        if(a&&b&&a->qty==b->qty&&!_wcsicmp(a->name,b->name)&&!_wcsicmp(a->value[AB_CODE],b->value[AB_CODE])&&
           !_wcsicmp(a->value[AB_REV],b->value[AB_REV])&&!_wcsicmp(a->value[AB_MATERIAL],b->value[AB_MATERIAL]))continue;
        wchar_t old_qty[16],new_qty[16];
        swprintf_s(old_qty,_countof(old_qty),L"%d",b?b->qty:0);
        swprintf_s(new_qty,_countof(new_qty),L"%d",a?a->qty:0);
        const wchar_t *fields[]={a?(b?L"变更":L"新增"):L"移除",b?b->name:L"",a?a->name:L"",b?b->value[AB_CODE]:L"",
            a?a->value[AB_CODE]:L"",b?b->value[AB_REV]:L"",a?a->value[AB_REV]:L"",old_qty,new_qty,
            b?b->value[AB_MATERIAL]:L"",a?a->value[AB_MATERIAL]:L""};
        ok=ax_bom_csv(file,fields,_countof(fields));
    }
    return ok;
}

static int ax_bom_check_result(FILE *file,const AxBomData *data,PartText *out){
    int issues=0,ok=!file||fwprintf(file,L"实例,模型,缺料号,缺图号,缺材料,缺规格,状态\n")>0;
    for(int i=-1;ok&&i<data->scan->count;i++){
        AxOccurrence *item=i<0?NULL:&data->scan->occurrences[i];
        if(item&&ax_suppressed(item->state))continue;
        int index=data->rows[i+1];
        const AxBomModel *row=index>=0?&data->models[index]:NULL;
        int code=!row||!row->value[AB_CODE][0],drawing=!row||!row->value[AB_DRAWING][0];
        int material=!row||(row->type==PRO_MDL_PART&&!row->value[AB_MATERIAL][0]),spec=!row||!row->value[AB_SPEC][0];
        if(!code&&!drawing&&!material&&!spec)continue;
        wchar_t path[256];
        if(!ax_bom_path(item,path))return 0;
        const wchar_t *fields[]={path,row?row->name:item->name,code?L"是":L"",drawing?L"是":L"",material?L"是":L"",
            spec?L"是":L"",row?L"资料缺项":L"实例不可读"};
        issues++;
        if(file)ok=ax_bom_csv(file,fields,_countof(fields));
        else if(issues<=50)part_text_add(out,L"%ls %ls：%ls%ls%ls%ls%ls\n",path,fields[1],row?L"":L"实例不可读。 ",
            code?L"缺料号 ":L"",drawing?L"缺图号 ":L"",material?L"缺材料 ":L"",spec?L"缺规格":L"");
    }
    if(!file)part_text_add(out,L"BOM 资料完整性：%d 个实例需核对。%ls\n",issues,issues>50?L"可导出完整明细。":L"");
    return ok;
}

static const wchar_t *ax_bom_wire_type(const wchar_t *kind){
    if(!_wcsicmp(kind,L"HARNESS"))return L"线束";
    if(!_wcsicmp(kind,L"TIE"))return L"扎带";
    if(!_wcsicmp(kind,L"CLIP"))return L"线夹";
    if(!_wcsicmp(kind,L"SLEEVE"))return L"护套";
    return NULL;
}

static const wchar_t *ax_bom_known(const wchar_t *value){return *value?value:L"未标注";}

static void ax_bom_occurrence_value(ProMdl assembly,const AxOccurrence *item,const wchar_t *field,
    const wchar_t *fallback,ProLine value){
    wcscpy_s(value,PRO_LINE_SIZE,fallback);
    ProMdl parent=assembly;
    for(int i=0;i<item->depth;i++){
        ProAsmcomp feature;
        if(ax_component_feature(parent,item->ids[i],&feature))return;
        if(i==item->depth-1){
            ProParameter parameter;
            ProParamvalue actual;
            if(!ProParameterInit(&feature,(wchar_t *)field,&parameter)&&
               !ProParameterValueWithUnitsGet(&parameter,&actual,NULL)&&actual.type==PRO_PARAM_STRING&&actual.value.s_val[0])
                wcscpy_s(value,PRO_LINE_SIZE,actual.value.s_val);
        }else if(ProAsmcompMdlGet(&feature,&parent))return;
    }
}

static int ax_bom_usage_csv(FILE *file,const AxBomData *data,int kind,int *written){
    int fast=kind==AX_B_FAST,spare=kind==AX_B_SPARE;
    int ok=fwprintf(file,fast?L"规格,长度,安装区域,数量,统计口径\n":
        spare?L"实例,模型,料号,分类,规格,安装区域,安装位置,实例数量,同型号总用量\n":
        L"实例,模型,料号,分类,规格,安装区域,安装位置,实例数量\n")>0;
    int used[AX_MAX_OCCURRENCES]={0};
    ProLine *areas=calloc((size_t)data->scan->count+1,sizeof(*areas));
    if(!areas)return 0;
    for(int i=0;i<data->scan->count;i++)if(data->rows[i+1]>=0){
        const AxBomModel *row=&data->models[data->rows[i+1]];
        ax_bom_occurrence_value(data->models[0].model,&data->scan->occurrences[i],L"BWTL_AREA",row->value[AB_AREA],areas[i]);
    }
    for(int i=0;ok&&i<data->scan->count;i++){
        int index=data->rows[i+1];
        if(index<0||used[i])continue;
        const AxBomModel *row=&data->models[index];
        const wchar_t *type=ax_bom_wire_type(row->value[AB_KIND]);
        if(fast&&_wcsicmp(row->value[AB_KIND],L"FASTENER"))continue;
        if(!fast&&!spare&&!type)continue;
        if(spare&&_wcsicmp(row->value[AB_SPARE],L"YES")&&wcscmp(row->value[AB_SPARE],L"是"))continue;
        if(fast){
            int qty=1;
            int complete=row->value[AB_SPEC][0]&&row->value[AB_LENGTH][0]&&areas[i][0];
            for(int j=i+1;complete&&j<data->scan->count;j++){
                int index2=data->rows[j+1];
                if(index2<0||used[j])continue;
                const AxBomModel *other=&data->models[index2];
                if(!_wcsicmp(other->value[AB_KIND],L"FASTENER")&&!_wcsicmp(row->value[AB_SPEC],other->value[AB_SPEC])&&
                    !_wcsicmp(row->value[AB_LENGTH],other->value[AB_LENGTH])&&!_wcsicmp(areas[i],areas[j])){
                    used[j]=1;
                    qty++;
                }
            }
            wchar_t quantity[16];
            swprintf_s(quantity,_countof(quantity),L"%d",qty);
            const wchar_t *fields[]={ax_bom_known(row->value[AB_SPEC]),ax_bom_known(row->value[AB_LENGTH]),
                ax_bom_known(areas[i]),quantity,complete?L"同规格、长度与区域汇总":row->name};
            ok=ax_bom_csv(file,fields,_countof(fields));
        }else{
            AxOccurrence *item=&data->scan->occurrences[i];
            wchar_t path[256],qty[16];
            ProLine position;
            if(!ax_bom_path(item,path)){ok=0;break;}
            ax_bom_occurrence_value(data->models[0].model,item,L"BWTL_POSITION",row->value[AB_POSITION],position);
            swprintf_s(qty,_countof(qty),L"%d",row->qty);
            const wchar_t *fields[]={path,row->name,row->value[AB_CODE],spare?L"可独立更换":type,
                ax_bom_known(row->value[AB_SPEC]),ax_bom_known(areas[i]),ax_bom_known(position),L"1",qty};
            ok=ax_bom_csv(file,fields,_countof(fields)-(spare?0:1));
        }
        (*written)++;
    }
    free(areas);
    return ok;
}

static int ax_bom_route_csv(FILE *file,const AxBomData *data,const PartCommand *cmd,int *written){
    int ok=fwprintf(file,L"工序,实例,模型,安装区域,安装位置,紧固要求,关键检查点\n")>0;
    for(int i=3;ok&&i<cmd->count;i++){
        int ids[AX_PATH_DEPTH],depth=ax_path_parse(cmd->arg[i],ids),found=0;
        for(int j=0;j<data->scan->count;j++){
            const AxOccurrence *item=&data->scan->occurrences[j];
            int index=data->rows[j+1];
            if(index<0||depth!=item->depth||memcmp(ids,item->ids,(size_t)depth*sizeof(*ids)))continue;
            const AxBomModel *row=&data->models[index];
            wchar_t step[16];
            ProLine area,position,torque,check;
            ProMdl root=data->models[0].model;
            ax_bom_occurrence_value(root,item,L"BWTL_AREA",row->value[AB_AREA],area);
            ax_bom_occurrence_value(root,item,L"BWTL_POSITION",row->value[AB_POSITION],position);
            ax_bom_occurrence_value(root,item,L"BWTL_TORQUE",row->value[AB_TORQUE],torque);
            ax_bom_occurrence_value(root,item,L"BWTL_CHECK",row->value[AB_CHECK],check);
            swprintf_s(step,_countof(step),L"%d",i-2);
            const wchar_t *fields[]={step,cmd->arg[i],row->name,ax_bom_known(area),ax_bom_known(position),
                ax_bom_known(torque),ax_bom_known(check)};
            ok=ax_bom_csv(file,fields,_countof(fields));
            found=1;
            (*written)++;
            break;
        }
        if(!found)return 0;
    }
    return ok;
}

static const wchar_t *const ax_bom_deliver_files[]={L"hierarchy.csv",L"procurement.csv",L"bom-diff.csv",L"bom-check.csv",
    L"fasteners.csv",L"wiring.csv",L"spares.csv",L"process.csv"};

static ProError ax_bom_csv_output(int kind,const AxBomData *data,const AxBomData *prior,const PartCommand *cmd,
    const wchar_t *folder,int *written){
    FILE *file=NULL;
    ProError error=ax_bom_open(folder,ax_bom_deliver_files[kind-AX_B_TREE],&file);
    if(error)return error;
    int ok=0;
    switch(kind){
    case AX_B_TREE:ok=ax_bom_tree_csv(file,data);break;
    case AX_B_BUY:ok=ax_bom_buy_csv(file,data);break;
    case AX_B_DIFF:ok=ax_bom_diff_csv(file,data,prior);break;
    case AX_B_CHECK:ok=ax_bom_check_result(file,data,NULL);break;
    case AX_B_FAST:case AX_B_WIRE:case AX_B_SPARE:ok=ax_bom_usage_csv(file,data,kind,written);break;
    case AX_B_ROUTE:ok=ax_bom_route_csv(file,data,cmd,written);break;
    }
    return fclose(file)==0&&ok?PRO_TK_NO_ERROR:PRO_TK_CANT_WRITE;
}
