static ProError ux_family(ProMdl source,const PartCommand *cmd,wchar_t *report,size_t capacity){
 ProMdl generic=NULL,instances[8]={0};ProMdlName name;ProFamtable table;ProFamtableItem column;ProFaminstance row;UxDimension dimension;ProParamvalue value,read;ProError e;int created=0,directory_owned=0,directory_changed=0;double original,after;ProModelitem item;ProPath original_directory,output_directory;
#define UF(call) do{if((e=(call))){trace("reuse family " #call,e);goto done;}}while(0)
 UF(ux_solid(source,&original));UF(ux_dimension(source,cmd->arg[3]+4,&dimension));{ProBoolean driven;UF(ProDimensionIsReldriven(&dimension.dim,&driven));if(driven){e=PRO_TK_BAD_CONTEXT;goto done;}}
 UF(ProDirectoryCurrentGet(original_directory));wcscpy_s(name,PRO_MDLNAME_SIZE,cmd->arg[2]);ProMdl exists;if(!ProMdlnameInit(name,PRO_MDL_PART,&exists)||!part_target_available(source,name,L"prt")){e=PRO_TK_E_FOUND;goto done;}
 for(int i=4;i<cmd->count;i++){swprintf_s(name,PRO_MDLNAME_SIZE,L"%ls_%02d",cmd->arg[2],i-3);if(!ProMdlnameInit(name,PRO_MDL_PART,&exists)){e=PRO_TK_E_FOUND;goto done;}}
 if(swprintf_s(output_directory,PRO_PATH_SIZE,L"%ls\\%ls_family",original_directory,cmd->arg[2])<0){e=PRO_TK_BAD_INPUTS;goto done;}
 /* Atomic new directory ownership prevents another session's output being overwritten. */
 if(!CreateDirectoryW(output_directory,NULL)){e=GetLastError()==ERROR_ALREADY_EXISTS?PRO_TK_E_FOUND:PRO_TK_CANT_WRITE;goto done;}directory_owned=1;UF(ProDirectoryChange(output_directory));directory_changed=1;wcscpy_s(name,PRO_MDLNAME_SIZE,cmd->arg[2]);
 UF(ProMdlnameCopy(source,name,&generic));UF(ux_dimension(generic,cmd->arg[3]+4,&dimension));ProBoolean related;UF(ProDimensionIsReldriven(&dimension.dim,&related));if(related){e=PRO_TK_BAD_CONTEXT;goto done;}
 UF(ProFamtableInit(generic,&table));e=ProFamtableCheck(&table);if(e!=PRO_TK_EMPTY&&e!=PRO_TK_E_NOT_FOUND){if(!e)e=PRO_TK_BAD_CONTEXT;goto done;}
 item=dimension.dim;UF(ProModelitemToFamtableItem(&item,&column));UF(ProFamtableItemAdd(&table,&column));
 for(int i=4;i<cmd->count;i++){
  swprintf_s(name,PRO_MDLNAME_SIZE,L"%ls_%02d",cmd->arg[2],i-3);if(!ProMdlnameInit(name,PRO_MDL_PART,&exists)||!part_target_available(source,name,L"prt")){e=PRO_TK_E_FOUND;goto done;}
  UF(ProFaminstanceInit(name,&table,&row));UF(ProFaminstanceAdd(&row));created++;value.type=PRO_PARAM_DOUBLE;value.value.d_val=as_arg(cmd,i);UF(ProFaminstanceValueSet(&row,&column,&value));UF(ProFaminstanceValueGet(&row,&column,&read));if(read.type!=PRO_PARAM_DOUBLE||!part_close(read.value.d_val,value.value.d_val)){e=PRO_TK_GENERAL_ERROR;goto done;}
  UF(ProFaminstanceRetrieve(&row,&instances[i-4]));UF(part_regenerate(instances[i-4]));UxDimension actual;UF(ux_dimension(instances[i-4],cmd->arg[3]+4,&actual));if(!part_close(actual.value,value.value.d_val)){e=PRO_TK_GENERAL_ERROR;goto done;}double v;UF(ux_solid(instances[i-4],&v));
 }
 UF(ux_solid(source,&after));if(!part_close(original,after)){e=PRO_TK_GENERAL_ERROR;goto done;}UF(ProMdlSave(generic));
 swprintf_s(report,capacity,L"已在 %ls 保存新母模型 %ls 和包含%d个规格的原生族表（实例从%ls_01起），按尺寸%ls分规格，实际实例重生成及尺寸读回通过。可从母模型打开各规格；源零件保持，未覆盖旧文件。",output_directory,cmd->arg[2],created,cmd->arg[2],cmd->arg[3]+4);
done:if(e){for(int i=0;i<8;i++)if(instances[i]&&ProMdlErase(instances[i]))ux_recovery_uncertain=1;if(generic){if(ProMdlErase(generic)||ux_copy_files_remove(cmd->arg[2]))ux_recovery_uncertain=1;}}
 if(directory_changed&&ProDirectoryChange(original_directory)){ux_recovery_uncertain=1;e=PRO_TK_GENERAL_ERROR;}
 if(e&&directory_owned)RemoveDirectoryW(output_directory);return e;
#undef UF
}
