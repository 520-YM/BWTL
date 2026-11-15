typedef struct { FILE *file; int count; } ReplayDiagnosticDimensions;

static ProError replay_diagnostic_dimension(ProDimension *dimension,ProError status,ProAppData data){
    ReplayDiagnosticDimensions *output=data;
    if(status||output->count>=32)return PRO_TK_NO_ERROR;
    ProDimensiontype type;
    double value;
    if(!ProDimensionTypeGet(dimension,&type)&&!ProDimensionValueGet(dimension,&value)){
        fwprintf(output->file,L" %d:%d=%.15g",dimension->id,type,value);
        output->count++;
    }
    return PRO_TK_NO_ERROR;
}

static void replay_diagnostic_begin(ImitationReplay *replay,ProFeature *feature,ProFeattype type,
                                    const wchar_t *label,unsigned int state_flags){
    FILE *log=runtime_open(L"imitation-native-diagnostics.log",L"at, ccs=UTF-8");
    if(!log)return;
    ProMdlName source=L"",target=L"";
    ProMdlMdlnameGet(replay->source,source);
    ProMdlMdlnameGet(replay->target,target);
    fwprintf(log,L"BEGIN pid=%lu target=%ls source=%ls step=%d feature=%d name=%ls type=%d flags=0x%x\n",
        GetCurrentProcessId(),target,source,replay->steps,feature->id,label,type,state_flags);
    fflush(log);
    int *parents=NULL,*children=NULL,up=0,down=0;
    ProError parent_status=ProFeatureParentsGet(feature,&parents,&up);
    ProError child_status=ProFeatureChildrenGet(feature,&children,&down);
    fwprintf(log,L"CONTEXT feature=%d parents(%d)=",feature->id,parent_status);
    if(!parent_status&&parents)for(int i=0;i<up&&i<32;i++)fwprintf(log,L" %d",parents[i]);
    fwprintf(log,L" children(%d)=",child_status);
    if(!child_status&&children)for(int i=0;i<down&&i<32;i++)fwprintf(log,L" %d",children[i]);
    fwprintf(log,L" dimensions=");
    ReplayDiagnosticDimensions dimensions={log,0};
    ProError dimension_status=ProFeatureDimensionVisit(feature,replay_diagnostic_dimension,NULL,&dimensions);
    fwprintf(log,L" status=%d\n",dimension_status);
    if(parents)ProArrayFree((ProArray *)&parents);
    if(children)ProArrayFree((ProArray *)&children);
    fclose(log);
}

static void replay_diagnostic_end(ImitationReplay *replay,ProFeature *feature,ProError error){
    FILE *log=runtime_open(L"imitation-native-diagnostics.log",L"at, ccs=UTF-8");
    if(!log)return;
    ProMdlName target=L"";
    ProMdlMdlnameGet(replay->target,target);
    fwprintf(log,L"END pid=%lu target=%ls step=%d feature=%d creo=%d\n",
        GetCurrentProcessId(),target,replay->steps,feature->id,error);
    fclose(log);
}
