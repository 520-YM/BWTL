
#include "std_catalog.inc"
#include "std_trace_catalog.inc"
#include "std_effective_rules.inc"
#include "std_case_scope.inc"
#include "std_where_used.inc"
#include "std_variant_matrix.inc"
#include "std_intake.inc"

static int std_chat(const wchar_t *text){
    static const wchar_t *const labels[]={L"登记标准接口",L"查找标准接口",L"查找等同标准接口",L"比较标准件接口",L"登记标准件别名",L"查询标准件别名",L"登记标准件来源",L"查询标准件来源",L"导入公司工艺规则",L"查询公司工艺规则",L"登记方法适用范围",L"查询方法适用范围",L"追踪标准件引用",L"标准件规格试改",L"登记公司标准件"};
    size_t offset=0;
    int action=-1;
    for(int i=0;i<_countof(labels);i++){
        size_t length=wcslen(labels[i]);
        if(!wcsncmp(text,labels[i],length)&&(!text[length]||iswspace(text[length]))){action=i;offset=length;break;}
    }
    if(action<0)return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    ProMdl current=NULL;
    ProMdlCurrentGet(&current);
    wchar_t args[9][MAX_PATH]={0},details[PART_TEXT_MAX]=L"";
    PartText out={details,_countof(details),0};
    int n=ax_study_words(text+offset,args);
    ProError error=PRO_TK_BAD_INPUTS;
    double tolerance=0;
    if(action==0&&n==5)error=std_record(current,args[0],args[1],args[2],args[3],args[4],&out);
    else if(action==1&&n==6){
        double pitch,limit[3];
        if(or_number(args[0],&pitch)&&or_number(args[2],&limit[0])&&or_number(args[3],&limit[1])&&
           or_number(args[4],&limit[2])&&or_number(args[5],&tolerance)&&tolerance>=0.001&&tolerance<=5)
            error=std_search(args[1],limit,pitch,tolerance,NULL,&out);
    }else if(action==2&&n==3&&or_number(args[2],&tolerance)&&tolerance>=0.001&&tolerance<=5){
        StdRecord target={0};
        error=std_lookup(args[0],args[1],&target);
        if(!error)error=std_search(NULL,NULL,0,tolerance,&target,&out);
    }else if(action==3&&n==5&&or_number(args[4],&tolerance)&&tolerance>=0.001&&tolerance<=5)
        error=std_compare(args[0],args[1],args[2],args[3],tolerance,&out);
    else if(action==4&&n==3)error=std_alias_add(args[0],args[1],args[2],&out);
    else if(action==5&&n==1)error=std_alias_query(args[0],&out);
    else if(action==6&&n==5)error=std_source_add(args[0],args[1],args[2],args[3],args[4],&out);
    else if(action==7&&n==2)error=std_source_query(args[0],args[1],&out);
    else if(action==8&&n==1)error=std_rule_import(args[0],&out);
    else if(action==9&&n==4)error=std_rule_query(args[0],args[1],args[2],args[3],&out);
    else if(action==10&&n==4)error=std_scope_add(args[0],args[1],args[2],args[3],&out);
    else if(action==11&&n==3&&or_number(args[2],&tolerance))error=std_scope_query(args[0],args[1],tolerance,&out);
    else if(action==12&&n==3)error=std_where_used(args[0],args[1],args[2],&out);
    else if(action==13&&n==3)error=std_variant_matrix(current,args[0],args[1],args[2],&out);
    else if(action==14&&n==5)error=std_intake(args[0],args[1],args[2],args[3],args[4],&out);
    if(error&&!details[0])part_text_add(&out,L"没有读到适用的标准接口。请核对编号、修订和配合面的实际参照。\n");
    if(error)part_text_add(&out,L"标准接口命令未完成（Creo %d）。\n",error);
    native_prompt_set(L"");
    append_card(L"你",text);save_turn(L"用户",text);
    append_card(L"标准接口",details);save_turn(L"标准接口",details);
    return 1;
}
