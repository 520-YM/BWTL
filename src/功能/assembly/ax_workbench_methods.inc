#define AW_METHOD_LIMIT 256
typedef struct {
    wchar_t category[PRO_NAME_SIZE],source[PRO_NAME_SIZE],component[PRO_NAME_SIZE];
    wchar_t base[PART_TOKEN_MAX],own[PART_TOKEN_MAX],prior_base[PART_TOKEN_MAX],prior_own[PART_TOKEN_MAX];
    int prior_error;
} AwMethod;

static int aw_method_row(const wchar_t *line,AwMethod *method){
    wchar_t fields[7][PART_TOKEN_MAX],extra;
    int error;
    if(swscanf_s(line,L"%63ls %63ls %63ls %63ls %63ls %63ls %63ls %d %lc",
        fields[0],(unsigned)_countof(fields[0]),fields[1],(unsigned)_countof(fields[1]),fields[2],(unsigned)_countof(fields[2]),
        fields[3],(unsigned)_countof(fields[3]),fields[4],(unsigned)_countof(fields[4]),fields[5],(unsigned)_countof(fields[5]),
        fields[6],(unsigned)_countof(fields[6]),&error,&extra,1u)!=8)return 0;
    for(int i=0;i<3;i++)if(!fd_name(fields[i]))return 0;
    for(int i=3;i<7;i++)if(!ax_csys_token(fields[i]))return 0;
    wcscpy_s(method->category,_countof(method->category),fields[0]);
    wcscpy_s(method->source,_countof(method->source),fields[1]);
    wcscpy_s(method->component,_countof(method->component),fields[2]);
    wcscpy_s(method->base,_countof(method->base),fields[3]);
    wcscpy_s(method->own,_countof(method->own),fields[4]);
    wcscpy_s(method->prior_base,_countof(method->prior_base),fields[5]);
    wcscpy_s(method->prior_own,_countof(method->prior_own),fields[6]);
    method->prior_error=error;
    return 1;
}

static ProError aw_method_latest(const wchar_t *category,AwMethod *latest,int *rows){
    static const wchar_t filename[]=L"assembly-methods.txt";
    FILE *file=runtime_open(filename,L"rt, ccs=UTF-8");
    *rows=0;
    if(!file)return errno==ENOENT?PRO_TK_NO_ERROR:PRO_TK_CANT_OPEN;
    wchar_t line[640];
    ProError error=PRO_TK_NO_ERROR;
    while(fgetws(line,_countof(line),file)){
        if(++*rows>AW_METHOD_LIMIT||!wcschr(line,L'\n')){error=PRO_TK_INCOMPLETE;break;}
        AwMethod item={0};
        if(!aw_method_row(line,&item)){error=PRO_TK_INCOMPLETE;break;}
        if(!_wcsicmp(category,item.category))*latest=item;
    }
    if(!error&&ferror(file))error=PRO_TK_CANT_OPEN;
    fclose(file);
    return error;
}

static ProError aw_method_persist(const wchar_t *line){
    const wchar_t *filename=L"assembly-methods.txt";
#ifdef CREO_CHAT_STORE_H
    if(!store_root[0])return PRO_TK_CANT_WRITE;
    const size_t capacity=AW_METHOD_LIMIT*640+1;
    wchar_t *content=calloc(capacity,sizeof(*content));
    if(!content)return PRO_TK_OUT_OF_MEMORY;
    int written=store_read(filename,content,capacity)&&!wcscat_s(content,capacity,line)&&store_write(filename,content);
    free(content);
    return written?PRO_TK_NO_ERROR:PRO_TK_CANT_WRITE;
#else
    /* Standalone Toolkit fixture stores its data in the isolated workspace. */
    FILE *file=runtime_open(filename,L"at, ccs=UTF-8");
    if(!file)return PRO_TK_CANT_WRITE;
    int written=fputws(line,file)>=0;
    return fclose(file)||!written?PRO_TK_CANT_WRITE:PRO_TK_NO_ERROR;
#endif
}

static ProError aw_method_record(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    if(!aw_same_workspace(assembly,aw_last_model,aw_last_name,aw_last_directory))return PRO_TK_E_NOT_FOUND;
    int match=-1;
    for(int i=0;i<aw_job_count;i++)if(!wcscmp(aw_job[i].path,cmd->arg[2])&&aw_job[i].committed)match=i;
    if(match<0){
        part_text_add(out,L"最近的定位未记录该实例的原生成功结果。\n");
        return PRO_TK_E_NOT_FOUND;
    }
    AxComponent part={0};
    ProMdlName component;
    ProError error=ax_component_resolve(assembly,cmd->arg[2],&part);
    if(!error)error=ProMdlMdlnameGet(part.model,component);
    if(error)return error;
    AwJobResult *origin=&aw_job[match];
    if(!aw_aligned(assembly,&part,origin->base,origin->own,mm)||!fd_name(component)||!fd_name(aw_last_name))return PRO_TK_BAD_CONTEXT;
    AwMethod latest={0};
    int rows;
    if((error=aw_method_latest(cmd->arg[3],&latest,&rows)))return error;
    if(latest.source[0]&&!_wcsicmp(latest.source,aw_last_name)&&!_wcsicmp(latest.component,component)&&
       !_wcsicmp(latest.base,origin->base)&&!_wcsicmp(latest.own,origin->own)&&latest.prior_error==origin->prior_error){
        part_text_add(out,L"已有相同的已验证装配方法，保持原记录。\n");
        return PRO_TK_NO_ERROR;
    }
    if(rows>=AW_METHOD_LIMIT)return PRO_TK_INCOMPLETE;
    wchar_t line[640];
    if(swprintf_s(line,_countof(line),L"%ls %ls %ls %ls %ls %ls %ls %d\n",cmd->arg[3],aw_last_name,component,
        origin->base,origin->own,origin->prior_root,origin->prior_part,origin->prior_error)<0)return PRO_TK_BAD_INPUTS;
    if((error=aw_method_persist(line)))return error;
    AwMethod confirmed={0};
    int after=0;
    if((error=aw_method_latest(cmd->arg[3],&confirmed,&after)))return error;
    if(after!=rows+1||_wcsicmp(confirmed.source,aw_last_name)||_wcsicmp(confirmed.base,origin->base))return PRO_TK_CANT_WRITE;
    part_text_add(out,L"已保存并回读装配方法 %ls。成功参照 %ls / %ls，第一组参照 Creo 结果 %d。\n",
        cmd->arg[3],origin->base,origin->own,origin->prior_error);
    return PRO_TK_NO_ERROR;
}

static ProError aw_method_recall(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AwMethod latest={0};
    int rows;
    ProError error=aw_method_latest(cmd->arg[3],&latest,&rows);
    if(error)return error;
    if(!latest.source[0]){part_text_add(out,L"尚无类型 %ls 的已验案例。\n",cmd->arg[3]);return PRO_TK_E_NOT_FOUND;}
    AxComponent part={0},root={0};
    AxDeviceFrame base,own;
    if((error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&part))||
       (error=ax_ref_occurrence(assembly,L"ROOT",&root)))return error;
    if((error=ax_device_frame(&root,latest.base,0,&base))||(error=ax_device_frame(&part,latest.own,0,&own))){
        part_text_add(out,L"历史案例 %ls / %ls 的安装参照未全部出现在当前装配。需重新读取参照。\n",latest.source,latest.component);
        return error;
    }
    ProMatrix current,planned;
    if((error=ax_device_pose(&part,current)))return error;
    ax_device_align(&own,&base,planned);
    part_text_add(out,L"已验案例 %ls / %ls，第一组 %ls / %ls 的原结果为 Creo %d。当前可读的成功方案为 %ls / %ls。\n",
        latest.source,latest.component,latest.prior_base,latest.prior_own,latest.prior_error,latest.base,latest.own);
    part_text_add(out,L"当前件预计移动 %.8g mm、旋转 %.8g° 才能对齐。需按当前装配目标及允许范围执行并再验收，读取案例未修改模型。\n",
        ax_layout_shift(current,mm,planned,mm),ax_layout_angle(current,planned));
    return PRO_TK_NO_ERROR;
}
