typedef struct {
    const wchar_t *punch,*die,*machine;
    const FabBend *bend;
    double thickness;
    PartText *out;
    int found;
} SmyBendTool;

static int smy_bend_tool_row(wchar_t **f,void *context){
    SmyBendTool *job=context;
    for(int i=0;i<3;i++)if(!dv_safe_leaf(f[i]))return 0;
    if(!dv_safe_leaf(f[10]))return 0;
    for(int i=3;i<10;i++)if(!smx_number(f[i],0.001,100000))return 0;
    double opening=wcstod(f[3],NULL),tip=wcstod(f[4],NULL),min_t=wcstod(f[5],NULL),max_t=wcstod(f[6],NULL),
        min_angle=wcstod(f[7],NULL),max_angle=wcstod(f[8],NULL),working=wcstod(f[9],NULL);
    if(min_t>max_t||min_angle>max_angle||max_angle>180||tip>=opening/2)return 0;
    if(_wcsicmp(job->punch,f[0])||_wcsicmp(job->die,f[1])||_wcsicmp(job->machine,f[2]))return 1;
    if(job->found)return 0;
    job->found=1;
    double angle=fabs(job->bend->angle);
    int match=job->thickness>=min_t&&job->thickness<=max_t&&angle>=min_angle&&angle<=max_angle&&
        job->bend->length<=working&&job->bend->radius>=tip-0.001;
    part_text_add(job->out,L"上模 %ls，下模 %ls，设备 %ls，批准版本 %ls。V 口 %.8g mm，冲头尖 R %.8g mm，最大有效长度 %.8g mm。\n",
        f[0],f[1],f[2],f[10],opening,tip,working);
    part_text_add(job->out,L"实测弯长 %.8g mm、板厚 %.8g mm、折弯角 %.8g°、内 R %.8g mm；库参数初筛 %ls。实际行程干涉需单独验证。\n",
        job->bend->length,job->thickness,angle,job->bend->radius,match?L"符合":L"超限");
    return 1;
}

static ProError smy_bend_tool(ProMdl model,const PartCommand *cmd,PartText *out){
    FabBends bends={0};
    ProError error=smy_bends_load(model,&bends);
    if(error)return error;
    int id=_wtoi(cmd->arg[2]+wcslen(L"SURFACE:"));
    const FabBend *bend=NULL;
    for(int i=0;i<bends.count;i++)if(bends.rows[i].surface==id){bend=&bends.rows[i];break;}
    if(!bend)return PRO_TK_E_NOT_FOUND;
    SmyBendTool job={cmd->arg[3],cmd->arg[4],cmd->arg[5],bend,bends.thickness,out,0};
    error=smx_table(L"BWTL_SMT_BEND_TOOLS.tsv",11,smy_bend_tool_row,&job,out);
    if(!error&&!job.found){
        part_text_add(out,L"公司库未登记该上模、下模与设备组合。\n");
        return PRO_TK_E_NOT_FOUND;
    }
    return error;
}

