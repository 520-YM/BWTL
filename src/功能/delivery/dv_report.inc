typedef struct {
    FILE *file;
    int rows;
} DeliveryReport;

static ProError dv_report_metadata(ProMdl model,FILE *file){
    static const wchar_t *const fields[]={
        L"PART_CODE",L"DRAWING_NO",L"REVISION",L"MATERIAL",L"PTC_REPORTED_MATERIAL",
        L"零件号",L"图号",L"版本",L"材料",L"材质"
    };
    ProModelitem owner;
    ProError error=ProMdlToModelitem(model,&owner);
    if(error)return error;
    for(int i=0;i<_countof(fields);i++){
        ProParameter parameter;
        ProParamvalue value;
        if(ProParameterInit(&owner,(wchar_t *)fields[i],&parameter)||
           ProParameterValueWithUnitsGet(&parameter,&value,NULL)||value.type!=PRO_PARAM_STRING||
           !value.value.s_val[0])continue;
        if(fwprintf(file,L"属性,,")<0||!dv_csv_text(file,fields[i])||fwprintf(file,L",,,")<0||
           !dv_csv_text(file,value.value.s_val)||fwprintf(file,L",,,,,\n")<0)return PRO_TK_CANT_WRITE;
    }
    return PRO_TK_NO_ERROR;
}

static ProError dv_report_feature(ProFeature *feature,ProError status,ProAppData context){
    if(status)return status;
    DeliveryReport *report=context;
    ProName name;
    ProFeattype type;
    ProFeatStatus state;
    ProError error;
    if((error=ProFeatureTypeGet(feature,&type))||(error=ProFeatureStatusGet(feature,&state)))return error;
    qa_feature_label(feature,name);
    if(fwprintf(report->file,L"特征,%d,",feature->id)<0||!dv_csv_text(report->file,name)||
       fwprintf(report->file,L",%d,%d,,,,,,\n",type,state)<0)return PRO_TK_CANT_WRITE;
    report->rows++;
    return PRO_TK_NO_ERROR;
}

static ProError dv_report_file(ProMdl model,const wchar_t *directory,int *feature_rows,int *bore_rows){
    ProPath path;
    ProMdlName name;
    ProSolidBody body;
    ProOutline bounds;
    ProMassProperty mass;
    double scale;
    ProError error;
    if((error=ProMdlMdlnameGet(model,name))||(error=qa_mm_per_unit(model,&scale))||
       (error=qa_geometry_mass(model,&mass))||(error=qa_single_body(model,&body))||
       (error=ProSolidBodyOutlineGet(&body,bounds)))return error;
    DeliveryBores *scan=calloc(1,sizeof(*scan));
    if(!scan)return PRO_TK_OUT_OF_MEMORY;
    error=dv_bores_scan(model,0,scale,scan);
    if(error){free(scan);return error;}
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\machining.csv",directory)<0){
        free(scan);return PRO_TK_BAD_INPUTS;
    }
    FILE *file=_wfopen(path,L"wt, ccs=UTF-8");
    if(!file){free(scan);return PRO_TK_CANT_WRITE;}
    DeliveryReport report={file,0};
    if(fwprintf(file,L"记录类型,ID,名称或说明,类型,状态,值1,值2,值3,值4,值5,单位\n零件,,")<0||
       !dv_csv_text(file,name)||fwprintf(file,L",,,,,,,,\n"
        L"包围盒,,XYZ外形尺寸,,,%.12g,%.12g,%.12g,,,mm\n"
        L"体积,,实体,,,%.12g,,,,,mm³\n面积,,实体,,,%.12g,,,,,mm²\n",
        (bounds[1][0]-bounds[0][0])*scale,(bounds[1][1]-bounds[0][1])*scale,
        (bounds[1][2]-bounds[0][2])*scale,mass.volume*scale*scale*scale,mass.surface_area*scale*scale)<0)
        error=PRO_TK_CANT_WRITE;
    if(!error)error=dv_report_metadata(model,file);
    if(!error){
        error=ProSolidFeatVisit((ProSolid)model,dv_report_feature,NULL,&report);
        if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    }
    if(!error&&fwprintf(file,L"说明,,孔候选的值1至值5依次为直径 轴向跨度和轴向XYZ,,,,,,,,\n")<0)
        error=PRO_TK_CANT_WRITE;
    if(!error&&scan->limited&&fwprintf(file,L"说明,,孔候选达到扫描上限,,,,,,,,\n")<0)
        error=PRO_TK_CANT_WRITE;
    for(int i=0;!error&&i<scan->count;i++){
        DeliveryBore *bore=&scan->bores[i];
        if(fwprintf(file,L"孔候选,%d,",bore->faces[0])<0||!dv_csv_text(file,dv_bore_class(bore->classification))||
           fwprintf(file,L",,,%.12g,%.12g,%.12g,%.12g,%.12g,mm\n",2*bore->radius*scale,
                (bore->high-bore->low)*scale,bore->axis[0],bore->axis[1],bore->axis[2])<0)
            error=PRO_TK_CANT_WRITE;
    }
    if(fclose(file)!=0&&!error)error=PRO_TK_CANT_WRITE;
    *feature_rows=report.rows;
    *bore_rows=scan->count;
    free(scan);
    return !error&&!dv_file_nonempty(path)?PRO_TK_CANT_WRITE:error;
}

static ProError dv_report(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput output={0};
    int features,bores;
    ProError error=dv_stage_begin(cmd->arg[2],&output);
    if(error)return error;
    error=dv_report_file(model,output.stage,&features,&bores);
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"已生成加工信息表 %ls\\machining.csv。包含实体尺寸、体积、面积、%d 条特征及 %d 组孔候选。\n",
        output.target,features,bores);
    return error;
}
