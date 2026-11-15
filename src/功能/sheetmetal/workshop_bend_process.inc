typedef struct {
    const wchar_t *grade,*die;
    const FabBend *bend;
    double thickness;
    int matches,passed;
    PartText *out;
} SmyForce;

static int smy_force_row(wchar_t **f,void *context){
    SmyForce *rule=context;
    if(!dv_safe_leaf(f[0])||!dv_safe_leaf(f[1])||!dv_safe_leaf(f[2])||!dv_safe_leaf(f[9]))return 0;
    for(int i=3;i<9;i++)if(!smx_number(f[i],0.001,100000))return 0;
    double thickness=wcstod(f[3],NULL),angle=wcstod(f[4],NULL),unit=wcstod(f[5],NULL),
        machine=wcstod(f[6],NULL),tool=wcstod(f[7],NULL),length=wcstod(f[8],NULL);
    if(angle>180||thickness>100)return 0;
    if(_wcsicmp(f[0],rule->grade)||_wcsicmp(f[1],rule->die)||fabs(thickness-rule->thickness)>0.0001||
       fabs(angle-fabs(rule->bend->angle))>0.001)return 1;
    double m=rule->bend->length/1000,need=unit*m,tool_limit=tool*m;
    int pass=need<=fmin(machine,tool_limit)&&rule->bend->length<=length;
    rule->matches++;
    rule->passed+=pass;
    part_text_add(rule->out,L"%ls / %ls（%ls）：规则 %.8g kN/m，按实测折弯长 %.8g mm 估算 %.8g kN（%.8g 吨力）。设备限值 %.8g kN，按有效长度折算的模具限值 %.8g kN，最大加工长 %.8g mm，%ls。\n",
        f[2],f[1],f[9],unit,rule->bend->length,need,need/9.80665,machine,tool_limit,length,pass?L"满足表列条件":L"至少一项超限");
    return 1;
}

static ProError smy_bend_process(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    SmyBendOrder plan={0};
    ProError error=smy_bends_load(model,&plan.bends);
    if(error)return error;
    int i=smy_bend_row(&plan,cmd->arg[kind==SMY_BEND_FORCE?2:3]);
    if(i<0)return PRO_TK_E_NOT_FOUND;
    const FabBend *row=&plan.bends.rows[i];
    if(kind==SMY_BEND_FORCE){
        SmyForce rule={cmd->arg[3],cmd->arg[4],row,plan.bends.thickness,0,0,out};
        error=smx_table(L"BWTL_SMT_PRESS_FORCE.tsv",10,smy_force_row,&rule,out);
        if(error)return error;
        if(!rule.matches){
            part_text_add(out,L"公司表缺少对应材料、板厚、角度及模具规则。\n");
            return PRO_TK_E_NOT_FOUND;
        }
        part_text_add(out,L"表列 %d 台设备，%d 台满足记录条件。以设备和模具厂家对实际设置的允许载荷为准。\n",rule.matches,rule.passed);
        return PRO_TK_NO_ERROR;
    }
    double nominal=fabs(row->angle),measured=as_arg(cmd,7),delta=nominal-measured;
    DeliveryOutput work={0};
    FILE *file=NULL;
    error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,L"bend-first-trial.csv",&file);
    if(!error){
        fwprintf(file,L"模型,弯面,材料,板厚_mm,模具,试折批号,目标角_deg,实测角_deg,需修正角差_deg\n");
        const wchar_t *text[]={cmd->arg[1],cmd->arg[3],cmd->arg[4]};
        for(int j=0;!error&&j<3;j++)if(!dv_csv_text(file,text[j])||fputwc(L',',file)==WEOF)error=PRO_TK_CANT_WRITE;
        if(!error&&fwprintf(file,L"%.10g,",plan.bends.thickness)<0)error=PRO_TK_CANT_WRITE;
        for(int j=5;!error&&j<=6;j++)if(!dv_csv_text(file,cmd->arg[j])||fputwc(L',',file)==WEOF)error=PRO_TK_CANT_WRITE;
        if(!error&&fwprintf(file,L"%.10g,%.10g,%+.10g\n",nominal,measured,delta)<0)error=PRO_TK_CANT_WRITE;
    }
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"模型目标 %.8g°，输入试折实测 %.8g°，角差 %+.8g°。已按材料、板厚、模具和批号登记：%ls。\n",
        nominal,measured,delta,work.target);
    return error;
}
