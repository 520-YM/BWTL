static ProError fd_sketch_edit(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdSketch s={0},check={0};FdDimValue before[256];FdTxn state={0};
    int before_count=0,before_entities=0,*entities=NULL,entity_count=0,ids[4]={0},count=0,result_id=-1,i,dirty=0,restored=1;
    ProSectionPointType senses[3]={0};ProError e=0;const char *stage="open editable associated sketch";double value=0;ProSecdimType dimension=PRO_TK_DIM_NONE;
    ProConstraintType constraint=PRO_CONSTRAINT_SAME_POINT;
    if((e=fd_sketch_open(model,cmd->arg[2],&s,1)))goto done;
    {ProUnitsystem us;ProUnititem native,mm;ProUnitConversion conv;
        if((e=ProMdlPrincipalunitsystemGet(model,&us))||(e=ProUnitsystemUnitGet(&us,PRO_UNITTYPE_LENGTH,&native))||(e=ProUnitInit(model,L"mm",&mm))||(e=ProUnitConversionCalculate(&mm,&native,&conv)))goto done;state.scale=conv.scale;
    }
    if((e=fd_dimensions_capture(s.original,before,&before_count)))goto done;
    if((e=ProSectionEntityIdsGet(s.original,&entities,&before_entities)))goto done;ProArrayFree((ProArray *)&entities);entities=NULL;
    if(!wcscmp(cmd->arg[0],L"SKETCH_ADD")){
#include "sketch_add_edit.inc"
}else if(!wcscmp(cmd->arg[0],L"SKETCH_DIM")){
#include "sketch_dim_edit.inc"
}else{
#include "sketch_constraint.inc"
}
    stage="solve edited sketch";if((e=fd_section_solve(s.section,state.scale)))goto done;
    stage="redefine associated sketch";s.options[0]=PRO_FEAT_CR_NO_OPTS;dirty=1;
    e=ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,s.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&state.errors);if(e)goto done;
    stage="regenerate edited sketch";if((e=part_regenerate(model)))goto done;
    stage="read back committed sketch";if((e=fd_sketch_open(model,cmd->arg[2],&check,0)))goto done;
    if(!wcscmp(cmd->arg[0],L"SKETCH_DIM")){
        double actual;ProSecdimType actual_type;if((e=ProSecdimValueGet(check.section,result_id,&actual))||(e=ProSecdimTypeGet(check.section,result_id,&actual_type)))goto done;
        if(!part_close(actual,value)||actual_type!=dimension){e=PRO_TK_GENERAL_ERROR;goto done;}
    }else if(!wcscmp(cmd->arg[0],L"SKETCH_CONSTRAINT")){
        int actual_id=-1;e=fd_constraint_evidence(check.section,constraint,count,ids,senses,&actual_id);if(e)goto done;
    }else{
        if((e=ProSectionEntityIdsGet(check.section,&entities,&entity_count)))goto done;
        if(entity_count!=before_entities+count){e=PRO_TK_GENERAL_ERROR;goto done;}
        for(i=0;i<count;i++){Pro2dEntdef *ent=NULL;if((e=ProSectionEntityGet(check.section,ids[i],&ent)))goto done;}
    }
    swprintf_s(report,capacity,L"草绘 %ls 已%ls。已对实际提交的草绘读回验证并重生成，未保存。%ls",cmd->arg[2],!wcscmp(cmd->arg[0],L"SKETCH_ADD")?L"添加图元":!wcscmp(cmd->arg[0],L"SKETCH_DIM")?L"设置强尺寸":L"添加或核对几何约束",L"图元编号可通过读取该草绘查看。");
done:
    if(entities)ProArrayFree((ProArray *)&entities);fd_sketch_free(&check);
    if(e&&dirty){
        ProError restore=ProFeatureWithoptionsRedefine(NULL,&s.feature,s.backup,s.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&state.errors);
        if(restore||part_regenerate(model)||fd_sketch_open(model,cmd->arg[2],&check,0))restored=0;
        if(restored){
            if(ProSectionEntityIdsGet(check.section,&entities,&entity_count)||entity_count!=before_entities)restored=0;
            for(i=0;i<before_count;i++){double actual;ProSecdimType type;if(ProSecdimValueGet(check.section,before[i].id,&actual)||ProSecdimTypeGet(check.section,before[i].id,&type)||type!=before[i].type||!part_close(actual,before[i].value))restored=0;}
            if(entities)ProArrayFree((ProArray *)&entities);
        }fd_sketch_free(&check);
    }
    fd_sketch_free(&s);
    if(e)swprintf_s(report,capacity,L"草绘操作未完成：%hs，错误码%d。%ls；未保存。",stage,e,restored?(dirty?L"已恢复原草绘并核对原尺寸":L"未提交对模型的修改"):L"恢复状态未能验证，请检查模型");
    return e;
}
