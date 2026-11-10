static ProError ux_param(ProMdl model,const PartCommand *cmd,wchar_t *report,size_t capacity){
 ProModelitem owner;ProParameter param;ProParamvalue value={0},read;ProUnititem units,*unit=NULL;ProError e;int created=0;
 if((e=ProMdlToModelitem(model,&owner)))return e;if(!ProParameterInit(&owner,(wchar_t *)cmd->arg[2],&param))return PRO_TK_E_FOUND;
 if(!wcscmp(cmd->arg[3],L"STRING")){value.type=PRO_PARAM_STRING;wcscpy_s(value.value.s_val,PRO_LINE_SIZE,cmd->arg[4]);}
 else if(!wcscmp(cmd->arg[3],L"INTEGER")){value.type=PRO_PARAM_INTEGER;value.value.i_val=(int)wcstod(cmd->arg[4],NULL);}
 else if(!wcscmp(cmd->arg[3],L"BOOLEAN")){value.type=PRO_PARAM_BOOLEAN;value.value.l_val=!wcscmp(cmd->arg[4],L"TRUE");}
 else {value.type=PRO_PARAM_DOUBLE;value.value.d_val=wcstod(cmd->arg[4],NULL);if(cmd->count==6){if((e=ProUnitInit(model,L"mm",&units)))return e;unit=&units;}}
 e=ProParameterWithUnitsCreate(&owner,(wchar_t *)cmd->arg[2],&value,unit,&param);if(!e){created=1;e=ProParameterValueWithUnitsGet(&param,&read,NULL);}
 if(!e&&(read.type!=value.type||(value.type==PRO_PARAM_STRING?wcscmp(read.value.s_val,value.value.s_val):value.type==PRO_PARAM_DOUBLE?!part_close(read.value.d_val,value.value.d_val):value.type==PRO_PARAM_INTEGER?read.value.i_val!=value.value.i_val:read.value.l_val!=value.value.l_val)))e=PRO_TK_GENERAL_ERROR;
 if(e&&created&&ProParameterDelete(&param))ux_recovery_uncertain=1;
 if(!e)swprintf_s(report,capacity,L"模型参数 %ls 已创建，类型%ls，值%ls%ls；原生读回一致，未保存。",cmd->arg[2],cmd->arg[3],cmd->arg[4],unit?L" mm":L"");return e;
}
