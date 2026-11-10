static ProError fab_measure(ProMdl model,ProSolidBody *body,double mm,const wchar_t *symbol,double *value,const wchar_t **unit){
    static const wchar_t axes[]=L"XYZ";
    const wchar_t *axis=symbol[0]&&!symbol[1]?wcschr(axes,towupper(symbol[0])):NULL;
    *unit=L"mm";
    if(axis){
        ProOutline bounds;
        ProError error=ProSolidBodyOutlineGet(body,bounds);
        if(!error){int i=(int)(axis-axes);*value=(bounds[1][i]-bounds[0][i])*mm;}
        return error;
    }
    UxDimension dim;
    ProDimensiontype type;
    ProError error=ux_dimension(model,symbol,&dim);
    if(!error)error=ProDimensionTypeGet(&dim.dim,&type);
    if(error)return error;
    if(type==PRODIMTYPE_ANGLE){*unit=L"deg";*value=dim.value;}
    else if(type==PRODIMTYPE_LINEAR||type==PRODIMTYPE_RADIUS||type==PRODIMTYPE_DIAMETER||type==PRODIMTYPE_ARC_LENGTH)
        *value=dim.value*mm;
    else return PRO_TK_INVALID_TYPE;
    return isfinite(*value)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError fab_inspect(ProMdl model,const PartCommand *cmd,PartText *out){
    ProSolidBody body;
    double mm;
    ProError error=qa_single_body(model,&body);
    if(!error)error=qa_mm_per_unit(model,&mm);
    double values[7];
    const wchar_t *units[7];
    const int count=(cmd->count-4)/4;
    for(int i=0;!error&&i<count;i++)error=fab_measure(model,&body,mm,cmd->arg[4+4*i],&values[i],&units[i]);
    DeliveryOutput work={0};
    FILE *file=NULL;
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,L"inspection-dimensions.csv",&file);
    int failed=0;
    if(!error){
        fwprintf(file,L"尺寸,单位,名义值,允许下限,允许上限,模型值,判定\n");
        for(int i=0;i<count;i++){
            int at=4+4*i;
            double nominal=as_arg(cmd,at+1),low=nominal-as_arg(cmd,at+2),high=nominal+as_arg(cmd,at+3);
            int pass=qa_at_least(values[i],low)&&qa_at_least(high,values[i]);
            failed+=!pass;
            if(!dv_csv_text(file,cmd->arg[at]))error=PRO_TK_CANT_WRITE;
            fwprintf(file,L",%ls,%.12g,%.12g,%.12g,%.12g,%ls\n",units[i],nominal,low,high,values[i],pass?L"合格":L"超差");
        }
    }
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"已核对 %d 项模型尺寸，其中 %d 项超差。\n%ls\\inspection-dimensions.csv",count,failed,work.target);
    return error;
}
