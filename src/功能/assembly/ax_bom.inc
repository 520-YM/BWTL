typedef struct {
    ProMdl model;
    const wchar_t *name;
    int quantity;
} AxBomRow;

static void ax_parameter_text(ProMdl model,const wchar_t *const *fields,int count,ProLine output){
    output[0]=0;
    ProModelitem item;
    if(ProMdlToModelitem(model,&item))return;
    for(int i=0;i<count;i++){
        ProParameter parameter;
        ProParamvalue value;
        if(!ProParameterInit(&item,(wchar_t *)fields[i],&parameter)&&
           !ProParameterValueWithUnitsGet(&parameter,&value,NULL)&&
           value.type==PRO_PARAM_STRING&&value.value.s_val[0]){
            wcscpy_s(output,PRO_LINE_SIZE,value.value.s_val);
            return;
        }
    }
}

static ProError ax_bom_file(const AxBomRow *rows,int count,const wchar_t *folder){
    ProPath path;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\bom.csv",folder)<0)return PRO_TK_BAD_INPUTS;
    FILE *file=_wfopen(path,L"wt, ccs=UTF-8");
    if(!file)return PRO_TK_CANT_WRITE;
    ProError error=fwprintf(file,L"模型名,数量,料号,图号,材料\n")<0?PRO_TK_CANT_WRITE:PRO_TK_NO_ERROR;
    static const wchar_t *const codes[]={L"PART_CODE",L"零件号"};
    static const wchar_t *const drawings[]={L"DRAWING_NO",L"图号"};
    static const wchar_t *const materials[]={L"PTC_REPORTED_MATERIAL",L"MATERIAL",L"材料",L"材质"};
    for(int i=0;!error&&i<count;i++){
        ProLine code,drawing,material;
        ax_parameter_text(rows[i].model,codes,_countof(codes),code);
        ax_parameter_text(rows[i].model,drawings,_countof(drawings),drawing);
        ax_parameter_text(rows[i].model,materials,_countof(materials),material);
        if(!dv_csv_text(file,rows[i].name)||fwprintf(file,L",%d,",rows[i].quantity)<0||
           !dv_csv_text(file,code)||fputwc(L',',file)==WEOF||!dv_csv_text(file,drawing)||
           fputwc(L',',file)==WEOF||!dv_csv_text(file,material)||fputwc(L'\n',file)==WEOF)
            error=PRO_TK_CANT_WRITE;
    }
    if(fclose(file)!=0&&!error)error=PRO_TK_CANT_WRITE;
    return !error&&!dv_file_nonempty(path)?PRO_TK_CANT_WRITE:error;
}

static ProError ax_bom(ProMdl assembly,const PartCommand *cmd,PartText *out){
    AxScan *scan=NULL;
    DeliveryOutput output={0};
    ProError error=ax_scan(assembly,1,&scan);
    if(error)return error;
    if(!scan->parts){free(scan);return PRO_TK_E_NOT_FOUND;}
    AxBomRow *rows=calloc(scan->parts,sizeof(*rows));
    if(!rows){free(scan);return PRO_TK_OUT_OF_MEMORY;}
    int count=0;
    for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(!item->model||item->type!=PRO_MDL_PART)continue;
        int row=0;
        while(row<count&&rows[row].model!=item->model)row++;
        if(row==count){
            rows[row].model=item->model;
            rows[row].name=item->name;
            count++;
        }
        rows[row].quantity++;
    }
    error=dv_stage_begin(cmd->arg[2],&output);
    if(!error)error=ax_bom_file(rows,count,output.stage);
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"装配 BOM 已生成，%d 种模型、%d 个活动零件实例。抑制项 %d。\n%ls\\bom.csv\n",
        count,scan->parts,scan->suppressed,output.target);
    free(rows);
    free(scan);
    return error;
}
