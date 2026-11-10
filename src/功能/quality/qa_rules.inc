static ProError qa_rules(ProMdl model,const PartCommand *cmd,double scale,PartText *out){
    int passed=0,failed=0,unknown=0;
    for(int i=2;i<cmd->count;){
        int rule=qa_rule_find(cmd->arg[i]);
        const wchar_t (*argument)[PART_TOKEN_MAX]=cmd->arg+i+1;
        int argument_count=qa_rule_specs[rule].arguments;
        double actual=0,limit=wcstod(argument[argument_count-1],NULL);
        int meets=0,sampled=0;
        const wchar_t *unit=L"mm";
        ProError error=PRO_TK_NO_ERROR;
        part_text_add(out,L"%ls",cmd->arg[i]);
        for(int j=0;j<argument_count;j++)part_text_add(out,L" %ls",argument[j]);
        if(rule==QR_REQUIRE_PARAM){
            ProModelitem owner;
            ProParameter parameter;
            error=ProMdlToModelitem(model,&owner);
            if(!error)error=ProParameterInit(&owner,(wchar_t *)argument[0],&parameter);
            if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
            else if(!error){
                ProParamvalue value;
                error=ProParameterValueWithUnitsGet(&parameter,&value,NULL);
                meets=!error&&(value.type!=PRO_PARAM_STRING||value.value.s_val[0]!=0);
            }
            if(!error)part_text_add(out,L"：%ls",meets?L"参数有值":L"参数缺失或为空");
        }else{
            if(rule==QR_MIN_VOLUME||rule==QR_MAX_VOLUME){
                ProMassProperty mass;
                error=qa_geometry_mass(model,&mass);
                if(!error)actual=mass.volume*scale*scale*scale;
                unit=L"mm³";
            }else if(rule==QR_MIN_DRAFT){
                double maximum;
                int samples,exact=0;
                error=qa_draft_range(model,argument[0],argument[1][0],1,&actual,&maximum,&samples,&exact);
                sampled=!exact;
                unit=L"°";
            }else if(!(error=qa_single_body(model,NULL))){
                if(rule==QR_MIN_WALL){
                    ProVector witness;
                    error=qa_wall_pair(model,argument[0],argument[1],scale,&actual,witness);
                }else{
                    double center;
                    error=qa_hole_gap(model,argument[0],argument[1],rule==QR_MIN_EDGE_GAP,scale,&center,&actual);
                }
            }
            if(!error){
                meets=rule==QR_MAX_VOLUME?qa_at_least(limit,actual):qa_at_least(actual,limit);
                part_text_add(out,L"：%ls %.12g %ls，要求 %ls %.12g %ls",sampled?L"采样最小值":L"实测",
                    actual,unit,rule==QR_MAX_VOLUME?L"≤":L"≥",limit,unit);
            }
        }
        if(error){
            unknown++;
            part_text_add(out,L" → 未判定（Creo %d）。\n",error);
        }else if(meets){
            passed++;
            part_text_add(out,L" → %ls。\n",sampled?L"采样符合":L"符合");
        }else{
            failed++;
            part_text_add(out,L" → 不符合。\n");
        }
        i+=argument_count+1;
    }
    part_text_add(out,L"本次明确规则：符合 %d，不符合 %d，未判定 %d。\n",passed,failed,unknown);
    return PRO_TK_NO_ERROR;
}
