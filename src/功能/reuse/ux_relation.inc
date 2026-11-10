typedef struct {ProMdl model;const wchar_t *next;int error,depth;} UxExpression;
static double ux_expression_sum(UxExpression *s);
static double ux_expression_atom(UxExpression *s){
 double value=0;wchar_t symbol[PRO_NAME_SIZE];int n=0;if(++s->depth>24){s->error=1;return 0;}
 if(*s->next==L'+'||*s->next==L'-'){int negative=*s->next++==L'-';value=ux_expression_atom(s);if(negative)value=-value;}
 else if(*s->next==L'('){s->next++;value=ux_expression_sum(s);if(*s->next!=L')')s->error=1;else s->next++;}
 else if(iswdigit(*s->next)||*s->next==L'.'){wchar_t *end;value=wcstod(s->next,&end);if(end==s->next)s->error=1;s->next=end;}
 else {while(iswalnum(*s->next)||*s->next==L'_'){if(n>=PRO_NAME_SIZE-1){s->error=1;break;}symbol[n++]=*s->next++;}symbol[n]=0;UxDimension dim;
  if(!n)s->error=1;else if(!ux_dimension(s->model,symbol,&dim))value=dim.value;
  else {ProModelitem owner;ProParameter param;ProParamvalue v;if(ProMdlToModelitem(s->model,&owner)||ProParameterInit(&owner,symbol,&param)||ProParameterValueWithUnitsGet(&param,&v,NULL))s->error=1;else if(v.type==PRO_PARAM_DOUBLE)value=v.value.d_val;else if(v.type==PRO_PARAM_INTEGER)value=v.value.i_val;else s->error=1;}
 }s->depth--;if(!isfinite(value))s->error=1;return value;
}
static double ux_expression_product(UxExpression *s){double v=ux_expression_atom(s);while(!s->error&&(*s->next==L'*'||*s->next==L'/')){int divide=*s->next++==L'/';double other=ux_expression_atom(s);if(divide&&fabs(other)<1e-15){s->error=1;break;}v=divide?v/other:v*other;}return v;}
static double ux_expression_sum(UxExpression *s){double v=ux_expression_product(s);while(!s->error&&(*s->next==L'+'||*s->next==L'-')){int subtract=*s->next++==L'-';double other=ux_expression_product(s);v=subtract?v-other:v+other;}return v;}
static ProError ux_relation_apply(ProMdl model,const PartCommand *cmd,int probe,wchar_t *report,size_t capacity){
 ProModelitem owner;ProRelset set;ProWstring *old=NULL,*lines=NULL;ProError e;int n=0,newset=0,changed=0;wchar_t formula[320];UxDimension target;ProParameter param;ProParamvalue oldparam,readparam;int dimension=0;double oldvalue,expected,actual;
#define UR(call) do{if((e=(call))){trace("reuse relation " #call,e);goto done;}}while(0)
 UR(ProMdlToModelitem(model,&owner));if(!ux_dimension(model,cmd->arg[2],&target)){dimension=1;oldvalue=target.value;ProBoolean driven;UR(ProDimensionIsReldriven(&target.dim,&driven));if(driven){e=PRO_TK_BAD_CONTEXT;goto done;}}
 else {UR(ProParameterInit(&owner,(wchar_t *)cmd->arg[2],&param));UR(ProParameterValueWithUnitsGet(&param,&oldparam,NULL));if(oldparam.type!=PRO_PARAM_DOUBLE){e=PRO_TK_BAD_INPUTS;goto done;}oldvalue=oldparam.value.d_val;}
 {UxExpression expression={model,cmd->arg[3],0,0};expected=ux_expression_sum(&expression);if(expression.error||*expression.next||!isfinite(expected)||fabs(expected)>1e9||(dimension&&expected<=0)){e=PRO_TK_BAD_INPUTS;goto done;}}
 e=ProModelitemToRelset(&owner,&set);if(e==PRO_TK_E_NOT_FOUND){UR(ProRelsetCreate(&owner,&set));newset=1;}else if(e)goto done;
 UR(ProArrayAlloc(0,sizeof(ProWstring),1,(ProArray *)&old));UR(ProRelsetRelationsGet(&set,&old));UR(ProArraySizeGet((ProArray)old,&n));if(n>1000){e=PRO_TK_BAD_CONTEXT;goto done;}
 for(int i=0;i<n;i++){const wchar_t *s=old[i];while(iswspace(*s))s++;size_t len=wcslen(cmd->arg[2]);if(!_wcsnicmp(s,cmd->arg[2],len)){s+=len;while(iswspace(*s))s++;if(*s==L'='){e=PRO_TK_E_FOUND;goto done;}}}
 UR(ProArrayAlloc(n+1,sizeof(ProWstring),1,(ProArray *)&lines));for(int i=0;i<n;i++)lines[i]=old[i];swprintf_s(formula,_countof(formula),L"%ls=%ls",cmd->arg[2],cmd->arg[3]);lines[n]=formula;changed=1;
 UR(ProRelsetRelationsSet(&set,lines,n+1));UR(ProRelsetRegenerate(&set));UR(part_regenerate(model));
 if(dimension)UR(ProDimensionValueGet(&target.dim,&actual));else{UR(ProParameterValueWithUnitsGet(&param,&readparam,NULL));actual=readparam.value.d_val;}
 if(!part_close(actual,expected)){e=PRO_TK_GENERAL_ERROR;goto done;}
 swprintf_s(report,capacity,L"已添加并验证关系式 %ls；目标实际值 %.9g，原有关系保留，重生成通过，未保存。",formula,actual);
done:
 if(e&&(changed||newset)){
  int bad=newset?ProRelsetDelete(&set):ProRelsetRelationsSet(&set,old,n);
  if(changed){if(dimension){if(ProDimensionValueSet(&target.dim,oldvalue))bad=1;}else if(ProParameterValueWithUnitsSet(&param,&oldparam,NULL))bad=1;}
  if(part_regenerate(model))bad=1;if(bad&&!probe)ux_recovery_uncertain=1;
 }
 if(lines)ProArrayFree((ProArray *)&lines);if(old)ProWstringproarrayFree(old);return e;
#undef UR
}
static ProError ux_relation(ProMdl model,const PartCommand *cmd,wchar_t *report,size_t capacity){
 ProMdl copy=NULL;ProMdlName name;ProError e;swprintf_s(name,PRO_MDLNAME_SIZE,L"ux_rel_%08lx_%08lx",GetCurrentProcessId(),GetTickCount());
 if(!part_target_available(model,name,L"prt"))return PRO_TK_E_FOUND;
 if((e=ProMdlnameCopy(model,name,&copy)))return e;e=ux_relation_apply(copy,cmd,1,report,capacity);if(ProMdlErase(copy)||ux_copy_files_remove(name)){ux_recovery_uncertain=1;return PRO_TK_GENERAL_ERROR;}if(e)return e;return ux_relation_apply(model,cmd,0,report,capacity);
}
