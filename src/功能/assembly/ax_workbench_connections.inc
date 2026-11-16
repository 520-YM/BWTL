static ProError aw_verify_port(ProMdl assembly,const wchar_t *token){
    wchar_t id[PART_TOKEN_MAX],csys[PART_TOKEN_MAX];
    AxDeviceFrame frame;
    return aw_connection_token(token,id,csys)?aw_frame_world(assembly,id,csys,&frame):PRO_TK_BAD_INPUTS;
}

static ProError aw_connection_audit(ProMdl assembly,const PartCommand *cmd,PartText *out){
    AxBomData data={0};
    ProError error=ax_bom_collect(assembly,&data,1);
    if(error){ax_bom_clear(&data);return error;}
    const int n=(cmd->count-2)/2;
    int matches[PART_ARG_MAX/2]={0},missing=0,wrong=0,unread=0,duplicates=0;
    unsigned char verified[PART_ARG_MAX/2]={0};
    for(int i=0;i<n;i++){
        const wchar_t *from=cmd->arg[2+i*2],*to=cmd->arg[3+i*2];
        if(!_wcsicmp(from,to)||aw_verify_port(assembly,from)||aw_verify_port(assembly,to)){
            part_text_add(out,L"要求连接 %ls → %ls 的端点在当前装配未得到核实。\n",from,to);
            unread++;continue;
        }
        int prior=0;
        for(;prior<i;prior++)
            if(!_wcsicmp(from,cmd->arg[2+prior*2])||!_wcsicmp(from,cmd->arg[3+prior*2])||
               !_wcsicmp(to,cmd->arg[2+prior*2])||!_wcsicmp(to,cmd->arg[3+prior*2]))break;
        if(prior<i){part_text_add(out,L"要求连接中端口出现重复：%ls 或 %ls。\n",from,to);unread++;continue;}
        verified[i]=1;
    }
    for(int i=0;i<data.scan->count;i++){
        const int row=data.rows[i+1];
        if(row<0)continue;
        ProLine from=L"",to=L"",kind=L"";
        aw_bom_value(assembly,&data,i,L"BWTL_KIND",kind);
        if(_wcsicmp(kind,L"HARNESS"))continue;
        aw_bom_value(assembly,&data,i,L"BWTL_WIRE_FROM",from);
        aw_bom_value(assembly,&data,i,L"BWTL_WIRE_TO",to);
        wchar_t path[256];
        if(!ax_bom_path(&data.scan->occurrences[i],path))continue;
        if(!from[0]||!to[0]||aw_verify_port(assembly,from)||aw_verify_port(assembly,to)){
            unread++;
            part_text_add(out,L"线束 %ls 的端口未标注或无法读取。\n",path);continue;
        }
        int found=-1;
        for(int j=0;j<n;j++)if(verified[j]&&
            ((!_wcsicmp(from,cmd->arg[2+j*2])&&!_wcsicmp(to,cmd->arg[3+j*2]))||
             (!_wcsicmp(to,cmd->arg[2+j*2])&&!_wcsicmp(from,cmd->arg[3+j*2])))){found=j;break;}
        if(found<0){
            wrong++;
            part_text_add(out,L"线束 %ls 连接 %ls → %ls 不在要求表里。\n",path,from,to);
        }else if(++matches[found]>1){
            duplicates++;
            part_text_add(out,L"线束 %ls 重复连接 %ls ↔ %ls。\n",path,from,to);
        }
    }
    for(int j=0;j<n;j++)if(verified[j]&&!matches[j]){
        missing++;
        part_text_add(out,L"未找到要求连接 %ls ↔ %ls。\n",cmd->arg[2+j*2],cmd->arg[3+j*2]);
    }
    part_text_add(out,L"连接要求 %d 对，缺连接 %d、表外连接 %d、重复 %d、无法核实 %d。检查依据 Creo 中可读端口和线束标注。\n",
        n,missing,wrong,duplicates,unread);
    ax_bom_clear(&data);
    return unread?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
