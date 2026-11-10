typedef struct {
    ProModelitem owner;
    const wchar_t *name,*value;
    ProParamvalue original;
    int existed,unchanged,attempted;
} DeliveryAttribute;
#ifdef BWTL_DELIVERY_FAULT_TEST
static int dv_test_attribute_fail_at=-1;
#endif

static ProError dv_attribute_read(DeliveryAttribute *entry,ProParameter *parameter,ProParamvalue *value){
    ProError error=ProParameterInit(&entry->owner,(wchar_t *)entry->name,parameter);
    if(!error)error=ProParameterValueWithUnitsGet(parameter,value,NULL);
    return error;
}

static int dv_attribute_restore(DeliveryAttribute *entry){
    ProParameter parameter;
    ProParamvalue readback;
    ProError error=dv_attribute_read(entry,&parameter,&readback);
    if(!entry->existed){
        if(error==PRO_TK_E_NOT_FOUND)return 1;
        if(error||ProParameterDelete(&parameter))return 0;
        return dv_attribute_read(entry,&parameter,&readback)==PRO_TK_E_NOT_FOUND;
    }
    if(error)return 0;
    if(readback.type==PRO_PARAM_STRING&&!wcscmp(readback.value.s_val,entry->original.value.s_val))return 1;
    return !ProParameterValueWithUnitsSet(&parameter,&entry->original,NULL)&&
        !dv_attribute_read(entry,&parameter,&readback)&&readback.type==PRO_PARAM_STRING&&
        !wcscmp(readback.value.s_val,entry->original.value.s_val);
}

static ProError dv_attributes(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryAttribute entries[36]={0};
    ProMdl parts[6];
    int additional,count=0,written=0;
    part_integer(cmd->arg[2],&additional);
    ProError error=dv_loaded_parts(model,cmd,3,additional,parts);
    if(error)return error;

    for(int i=0;i<=additional;i++){
        ProModelitem owner;
        if((error=ProMdlToModelitem(parts[i],&owner)))return error;
        for(int j=3+additional;j<cmd->count;j+=2){
            DeliveryAttribute *entry=&entries[count++];
            entry->owner=owner;
            entry->name=cmd->arg[j];
            entry->value=cmd->arg[j+1];
            if(!_wcsnicmp(entry->name,L"PTC_",4)||!_wcsnicmp(entry->name,L"PRO_",4))return PRO_TK_BAD_INPUTS;
            ProParameter parameter;
            error=dv_attribute_read(entry,&parameter,&entry->original);
            if(error==PRO_TK_E_NOT_FOUND)continue;
            if(error)return error;
            entry->existed=1;
            if(entry->original.type!=PRO_PARAM_STRING)return PRO_TK_BAD_CONTEXT;
            entry->unchanged=!wcscmp(entry->original.value.s_val,entry->value);
            if(!entry->unchanged&&entry->original.value.s_val[0]){
                ProMdlName name;
                ProMdlMdlnameGet(parts[i],name);
                part_text_add(out,L"%ls 的 %ls 已有不同值，批次预检未通过。\n",name,entry->name);
                return PRO_TK_E_FOUND;
            }
            if(!entry->unchanged){
                ProLockstatus lock;
                error=ProParameterLockstatusGet(&parameter,&lock);
                if(error)return error;
                if(lock!=PRO_PARAMLOCKSTATUS_UNLOCKED)return PRO_TK_BAD_CONTEXT;
            }
        }
    }
    for(int i=0;i<count;i++){
        DeliveryAttribute *entry=&entries[i];
        if(entry->unchanged)continue;
        ProParameter parameter;
        ProParamvalue value={0},readback;
        value.type=PRO_PARAM_STRING;
        wcscpy_s(value.value.s_val,_countof(value.value.s_val),entry->value);
        entry->attempted=1;
#ifdef BWTL_DELIVERY_FAULT_TEST
        if(i==dv_test_attribute_fail_at)error=PRO_TK_GENERAL_ERROR;
        else
#endif
        if(entry->existed){
            error=ProParameterInit(&entry->owner,(wchar_t *)entry->name,&parameter);
            if(!error)error=ProParameterValueWithUnitsSet(&parameter,&value,NULL);
        }else error=ProParameterWithUnitsCreate(&entry->owner,(wchar_t *)entry->name,&value,NULL,&parameter);
        if(!error)error=dv_attribute_read(entry,&parameter,&readback);
        if(!error&&(readback.type!=PRO_PARAM_STRING||wcscmp(readback.value.s_val,entry->value)))
            error=PRO_TK_GENERAL_ERROR;
        if(error){
            int restored=1;
            for(int j=i;j>=0;j--)if(entries[j].attempted&&!dv_attribute_restore(&entries[j]))restored=0;
            if(!restored)dv_recovery_uncertain=1;
            part_text_add(out,L"%ls\n",restored?L"已核对本批写入全部回滚。":
                L"批量属性恢复未能核实，本次会话已停止交付模块写入。");
            return error;
        }
        written++;
    }
    part_text_add(out,L"已核对 %d 个零件的 %d 项字符串属性，填写 %d 项，同值保持 %d 项。修改留在当前会话内。\n",
        additional+1,count,written,count-written);
    return PRO_TK_NO_ERROR;
}
