static ProError sd_edit(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdSketch s={0},check={0};ProError e;ProErrorlist errors={0};FdRef ref={0};int dirty=0,restore_ok=1,id=0,created=-1,i,*ids=NULL,n=0,original_count=0;
    ProSectionPointType sense;Pro2dEntdef original={0};Pro2dLinedef line={0};Pro2dCircledef circle={0};Pro2dArcdef arc={0};ProEnttype type;double dx=0,dy=0,scale=1;ProVector vector={0};const char *stage="open existing sketch";
    if((e=fd_sketch_open(model,cmd->arg[2],&s,1)))goto done;
    {ProUnitsystem us;ProUnititem native,mm;ProUnitConversion cv;if((e=ProMdlPrincipalunitsystemGet(model,&us))||(e=ProUnitsystemUnitGet(&us,PRO_UNITTYPE_LENGTH,&native))||(e=ProUnitInit(model,L"mm",&mm))||(e=ProUnitConversionCalculate(&mm,&native,&cv)))goto done;scale=cv.scale;}
    if((e=ProSectionEntityIdsGet(s.original,&ids,&original_count)))goto done;ProArrayFree((ProArray *)&ids);ids=NULL;
    /* EntityTranslate is a legacy geometry operation. Modify the legacy
       representation first, then convert/resolve with Intent Manager. */
    if((e=ProSectionIntentManagerModeSet(s.section,(!wcscmp(cmd->arg[0],L"SKETCH_MOVE")||!wcscmp(cmd->arg[0],L"SKETCH_PROJECT"))?PRO_B_FALSE:PRO_B_TRUE)))goto done;
    if(!wcscmp(cmd->arg[0],L"SKETCH_MOVE")){
#include "sketch_move.inc"
}else if(!wcscmp(cmd->arg[0],L"SKETCH_PROJECT")){
#include "sketch_project.inc"
}else{
#include "sketch_remove.inc"
}
    stage="resolve edited sketch";if((e=fd_section_solve(s.section,scale)))goto done;
    stage="mark changed sketch geometry";if((e=sd_mark_section(&s)))goto done;
    s.options[0]=PRO_FEAT_CR_NO_OPTS;dirty=1;stage="commit edited sketch";
    e=ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,s.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors);if(e)goto done;
    stage="regenerate edited sketch and children";if((e=part_regenerate(model)))goto done;
    stage="read committed sketch";if((e=fd_sketch_open(model,cmd->arg[2],&check,0)))goto done;
    if(!wcscmp(cmd->arg[0],L"SKETCH_MOVE")){
        Pro2dEntdef *ent=NULL;if((e=ProSectionEntityGet(check.section,id,&ent)))goto done;if(ent->type!=type){e=PRO_TK_GENERAL_ERROR;goto done;}
        if(type==PRO_2D_CIRCLE){trace("committed circle x*1000",(int)lround(((Pro2dCircledef *)ent)->center[0]*1000));trace("committed circle y*1000",(int)lround(((Pro2dCircledef *)ent)->center[1]*1000));}
        if(type==PRO_2D_LINE){Pro2dLinedef *p=(Pro2dLinedef *)ent;for(i=0;i<2;i++)if(!part_close(p->end1[i],line.end1[i]+vector[i])||!part_close(p->end2[i],line.end2[i]+vector[i])){e=PRO_TK_GENERAL_ERROR;goto done;}}
        else {double *actual=type==PRO_2D_CIRCLE?((Pro2dCircledef *)ent)->center:((Pro2dArcdef *)ent)->center;double *center=type==PRO_2D_CIRCLE?circle.center:arc.center;for(i=0;i<2;i++)if(!part_close(actual[i],center[i]+vector[i])){e=PRO_TK_GENERAL_ERROR;goto done;}
            if(type==PRO_2D_CIRCLE&&!part_close(((Pro2dCircledef *)ent)->radius,circle.radius)){e=PRO_TK_GENERAL_ERROR;goto done;}
            if(type==PRO_2D_ARC){Pro2dArcdef *p=(Pro2dArcdef *)ent;if(!part_close(p->radius,arc.radius)||!part_close(p->start_angle,arc.start_angle)||!part_close(p->end_angle,arc.end_angle)){e=PRO_TK_GENERAL_ERROR;goto done;}}
        }
    }else if(!wcscmp(cmd->arg[0],L"SKETCH_PROJECT")){
        Pro2dEntdef *entity=NULL;stage="verify real projection association";
        if((e=ProSectionEntityGet(check.section,created,&entity)))goto done;
        if((e=sd_projection_reference(model,&check.feature,&ref)))goto done;
    }else if(!wcscmp(cmd->arg[3],L"ENTITY")){
        e=ProSectionEntityIdsGet(check.section,&ids,&n);if(e)goto done;if(n!=original_count-1){e=PRO_TK_GENERAL_ERROR;goto done;}for(i=0;i<n;i++)if(ids[i]==id){e=PRO_TK_GENERAL_ERROR;goto done;}
    }else if(!wcscmp(cmd->arg[3],L"CONSTRAINT")){
        e=ProSectionConstraintsIdsGet(check.section,&ids,&n);if(e==PRO_TK_E_NOT_FOUND)e=0;if(e)goto done;for(i=0;i<n;i++)if(ids[i]==id){e=PRO_TK_GENERAL_ERROR;goto done;}
    }else{
        /* A removed strong dimension may be replaced with a weak one. Probe
           strength only on the extracted COPY, never mutate the live model. */
        e=ProSecdimStrengthen(check.section,id);if(e==PRO_TK_E_FOUND){e=PRO_TK_GENERAL_ERROR;goto done;}if(e&&e!=PRO_TK_E_NOT_FOUND&&e!=PRO_TK_BAD_INPUTS)goto done;e=0;
    }
    swprintf_s(report,capacity,L"已在原草绘 %ls 完成%ls；原生读回与模型重生成通过，未保存。%ls",cmd->arg[2],!wcscmp(cmd->arg[0],L"SKETCH_MOVE")?L"图元平移":!wcscmp(cmd->arg[0],L"SKETCH_PROJECT")?L"关联投影":L"指定项目删除",created>=0?L"投影参照已核对。":L"");
done:
    if(ids)ProArrayFree((ProArray *)&ids);fd_sketch_free(&check);
    if(e&&dirty){restore_ok=!ProFeatureWithoptionsRedefine(NULL,&s.feature,s.backup,s.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors);if(part_regenerate(model))restore_ok=0;}
    fd_ref_free(&ref);fd_sketch_free(&s);
    if(e)swprintf_s(report,capacity,L"草绘编辑未完成：%hs，错误码%d。%ls，未保存。",stage,e,dirty?(restore_ok?L"已请求恢复原草绘，重生成通过":L"恢复未通过，请检查模型"):L"未提交修改");return e;
}
