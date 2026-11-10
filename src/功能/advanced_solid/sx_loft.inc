static ProError sx_loft(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};
    FdRef profile={0};
    ProMatrix first,frame;
    ProName names[6];
    int sections=cmd->count-3,bodies=0,direction=0;
    double previous=0;
    ProError e;
    PartCommand interval={0};
#define LOFT_TRY(label,call) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    LOFT_TRY("prepare multi-section loft",fd_begin(&tx,model,cmd->arg[2],PRO_FEAT_PROTRUSION));
    LOFT_TRY("read initial loft body count",sd_body_count(model,&bodies));
    if(bodies>1){e=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<sections;i++){
        double offset=0,dot=0;
        LOFT_TRY("resolve closed loft section",sd_blend_section(model,cmd->arg[i+3],&profile,frame,0));
        fd_ref_free(&profile);
        memset(&profile,0,sizeof(profile));
        if(!i){memcpy(first,frame,sizeof(first));continue;}
        for(int j=0;j<3;j++){
            dot+=first[2][j]*frame[2][j];
            offset+=(frame[3][j]-first[3][j])*first[2][j];
        }
        if(i==1)direction=offset>0?1:-1;
        tx.stage="check ordered parallel loft sections";
        if(fabs(fabs(dot)-1)>1e-6||(offset-previous)*direction<0.001*tx.scale){e=PRO_TK_BAD_INPUTS;goto done;}
        previous=offset;
    }
    wcscpy_s(names[0],PRO_NAME_SIZE,cmd->arg[2]);
    for(int i=1;i<sections-1;i++){
        ProModelitem found;
        swprintf_s(names[i],PRO_NAME_SIZE,L"%.24ls_S%d",cmd->arg[2],i+1);
        if(!ProModelitemByNameInit(model,PRO_FEATURE,names[i],&found)){e=PRO_TK_E_FOUND;goto done;}
    }
    for(int i=0;i<sections-1;i++){
        wcscpy_s(interval.arg[2],PART_TOKEN_MAX,names[i]);
        wcscpy_s(interval.arg[3],PART_TOKEN_MAX,cmd->arg[i+3]);
        wcscpy_s(interval.arg[4],PART_TOKEN_MAX,cmd->arg[i+4]);
        LOFT_TRY("place linked native loft interval",sd_blend_sections(&interval,model,report,capacity,0));
    }
    swprintf_s(report,capacity,L"已创建 %d 截面实体放样 %ls，模型树中有 %d 个关联混合特征。已核对各截面关联、单实体和重生成，未保存。",sections,cmd->arg[2],sections-1);
done:
    fd_ref_free(&profile);
    sx_compound_finish(&tx,e,bodies,report,capacity);
    return e;
#undef LOFT_TRY
}
