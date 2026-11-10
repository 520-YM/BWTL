static ProError ux_udf_path(const wchar_t *name,int library,ProPath path){ProPath dir;if(ProDirectoryCurrentGet(dir))return PRO_TK_BAD_CONTEXT;return swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls%ls.gph",dir,library?L"BWTL_UDF\\":L"",name)<0?PRO_TK_BAD_INPUTS:0;}
static ProError ux_udf_data(const wchar_t *name,ProUdfdata *data,ProUdfRequiredRef **refs,int *count){ProPath path;ProError e;if((e=ux_udf_path(name,1,path))||(e=ProUdfdataAlloc(data))||(e=ProUdfdataPathSet(*data,path))||(e=ProUdfdataRequiredreferencesGet(*data,refs)))return e;return ProArraySizeGet((ProArray)*refs,count);}
static ProError ux_udf(ProMdl model,const PartCommand *cmd,FdTxn *tx,wchar_t *report,size_t capacity){
 ProUdfdata data=NULL;ProUdfRequiredRef *refs=NULL;ProUdfreference bindings[8]={0};FdRef resolved[8]={{0}};ProGroup group={0};ProFeature *features=NULL;ProError e;int count=0,n=0,kind=ux_kind(cmd->arg[0]);ProPath source,target,dir;int copied=0;
#define UU(call) do{if((e=(call))){trace("reuse udf " #call,e);goto done;}}while(0)
 if(kind==UX_UDF_SAVE){
  UU(ux_udf_path(cmd->arg[3],0,source));
  if(GetFileAttributesW(source)==INVALID_FILE_ATTRIBUTES){ProPath mask;WIN32_FIND_DATAW found;swprintf_s(mask,PRO_PATH_SIZE,L"%ls.*",source);HANDLE search=FindFirstFileW(mask,&found);int latest=0;if(search!=INVALID_HANDLE_VALUE){do{wchar_t *dot=wcsrchr(found.cFileName,L'.');int version=0;if(dot&&part_integer(dot+1,&version)&&version>latest&&!(found.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))latest=version;}while(FindNextFileW(search,&found));FindClose(search);}if(!latest){e=PRO_TK_CANT_OPEN;goto done;}size_t length=wcslen(source);swprintf_s(source+length,PRO_PATH_SIZE-length,L".%d",latest);}
  UU(ux_udf_path(cmd->arg[2],1,target));UU(ProDirectoryCurrentGet(dir));wcscat_s(dir,PRO_PATH_SIZE,L"\\BWTL_UDF");DWORD attrs=GetFileAttributesW(dir);if(attrs!=INVALID_FILE_ATTRIBUTES&&(!(attrs&FILE_ATTRIBUTE_DIRECTORY)||(attrs&FILE_ATTRIBUTE_REPARSE_POINT))){e=PRO_TK_BAD_CONTEXT;goto done;}
  if(!CreateDirectoryW(dir,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){e=PRO_TK_CANT_WRITE;goto done;}if(!CopyFileW(source,target,TRUE)){e=PRO_TK_CANT_WRITE;goto done;}copied=1;UU(ux_udf_data(cmd->arg[2],&data,&refs,&count));
 }else UU(ux_udf_data(cmd->arg[kind==UX_UDF_PLACE?3:2],&data,&refs,&count));
 if(kind!=UX_UDF_PLACE){PartText text={report,capacity,0};part_text_add(&text,L"%lsUDF模板 %ls，原生定义可读取，共%d个必需参照。\n",copied?L"已入库保存":L"已读取",cmd->arg[2],count);for(int i=0;i<count;i++){ProLine prompt;ProType type;UU(ProUdfrequiredrefPromptGet(refs[i],prompt));UU(ProUdfrequiredrefTypeGet(refs[i],&type));part_text_add(&text,L"参照%d：%ls，类型%d。\n",i+1,prompt,type);}goto done;}
 if(count!=cmd->count-4||count>8){e=PRO_TK_BAD_INPUTS;goto done;}
 UU(ProUdfdataDependencySet(data,PROUDFDEPENDENCY_INDEPENDENT));UU(ProUdfdataScaleSet(data,PROUDFSCALETYPE_SAME_DIMS,1));
 for(int i=0;i<count;i++){ProLine prompt;ProType type;UU(ProUdfrequiredrefPromptGet(refs[i],prompt));UU(ProUdfrequiredrefTypeGet(refs[i],&type));
  if(type==PRO_FEATURE){const wchar_t *colon=wcschr(cmd->arg[i+4],L':');if(!colon){e=PRO_TK_BAD_INPUTS;goto done;}UU(ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)colon+1,(ProModelitem *)&resolved[i].item));resolved[i].owner_feature=resolved[i].item.id;UU(ProSelectionAlloc(NULL,(ProModelitem *)&resolved[i].item,&resolved[i].selection));}
  else UU(fd_ref_resolve(model,cmd->arg[i+4],type,&resolved[i]));
  UU(ProUdfreferenceAlloc(prompt,resolved[i].selection,PRO_B_FALSE,&bindings[i]));UU(ProUdfdataReferenceAdd(data,bindings[i]));}
 {ProUdfCreateOption options[]={PROUDFOPT_NO_REDEFINE,PROUDFOPT_FIX_MODEL_UI_OFF};e=ProUdfCreate((ProSolid)model,data,NULL,options,2,&group);if(group.owner==model&&group.id>0&&!fd_was_feature(tx,group.id)){tx->feature=group;tx->created=1;}if(e)goto done;}
 if(!tx->created){e=PRO_TK_GENERAL_ERROR;goto done;}{ProFeature header;UU(ProFeatureInit((ProSolid)model,group.id,&header));UU(ProModelitemNameSet((ProModelitem *)&header,tx->name));tx->feature=header;}UU(ProGroupFeaturesCollect(&group,&features));UU(ProArraySizeGet((ProArray)features,&n));if(n<2||n>64){e=PRO_TK_GENERAL_ERROR;goto done;}
 for(int i=0;i<n;i++){ProFeatStatus status;UU(ProFeatureStatusGet(&features[i],&status));if(status!=PRO_FEAT_ACTIVE||fd_was_feature(tx,features[i].id)){e=PRO_TK_BAD_CONTEXT;goto done;}}
 UU(part_regenerate(model));swprintf_s(report,capacity,L"已放置原生UDF %ls，组名%ls，包含%d个特征。逐项参照绑定和重生成通过，未保存。",cmd->arg[3],tx->name,n);
done:for(int i=0;i<8;i++){if(bindings[i])ProUdfreferenceFree(bindings[i]);fd_ref_free(&resolved[i]);}if(features)ProArrayFree((ProArray *)&features);if(refs)ProUdfrequiredrefProarrayFree(refs);if(data)ProUdfdataFree(data);if(e&&copied)DeleteFileW(target);return e;
#undef UU
}
