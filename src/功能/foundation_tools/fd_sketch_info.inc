static ProError fd_sketch_info(ProMdl model,const wchar_t *name,wchar_t *report,size_t capacity){
    FdSketch s={0};PartText out={report,capacity,0};ProError e;int *ids=NULL,count=0,i;double mm=1;
    ProUnitsystem us;ProUnititem native,unit;ProUnitConversion conv;report[0]=0;
    if((e=fd_sketch_open(model,name,&s,0)))goto done;
    if(!ProMdlPrincipalunitsystemGet(model,&us)&&!ProUnitsystemUnitGet(&us,PRO_UNITTYPE_LENGTH,&native)&&!ProUnitInit(model,L"mm",&unit)&&!ProUnitConversionCalculate(&native,&unit,&conv))mm=conv.scale;
    part_text_add(&out,L"草绘 %ls（坐标和线性尺寸已换算毫米）：\n",name);
    e=ProSectionEntityIdsGet(s.section,&ids,&count);if(e)goto done;
    for(i=0;i<count;i++){
        Pro2dEntdef *ent=NULL;if(ProSectionEntityGet(s.section,ids[i],&ent))continue;
        if(ent->type==PRO_2D_LINE||ent->type==PRO_2D_CENTER_LINE){Pro2dLinedef *l=(Pro2dLinedef *)ent;part_text_add(&out,L"图元%d %ls：(%.9g,%.9g) → (%.9g,%.9g)\n",ids[i],ent->type==PRO_2D_LINE?L"直线":L"中心线",l->end1[0]*mm,l->end1[1]*mm,l->end2[0]*mm,l->end2[1]*mm);}
        else if(ent->type==PRO_2D_CIRCLE){Pro2dCircledef *c=(Pro2dCircledef *)ent;part_text_add(&out,L"图元%d 圆：圆心(%.9g,%.9g)，半径%.9g\n",ids[i],c->center[0]*mm,c->center[1]*mm,c->radius*mm);}
        else if(ent->type==PRO_2D_ARC){Pro2dArcdef *a=(Pro2dArcdef *)ent;part_text_add(&out,L"图元%d 圆弧：圆心(%.9g,%.9g)，半径%.9g，角度%.9g至%.9g度\n",ids[i],a->center[0]*mm,a->center[1]*mm,a->radius*mm,a->start_angle*180/3.141592653589793,a->end_angle*180/3.141592653589793);}
        else part_text_add(&out,L"图元%d：原生类型%d（引用或其他图元）\n",ids[i],ent->type);
    }ProArrayFree((ProArray *)&ids);ids=NULL;
    e=ProSecdimIdsGet(s.section,&ids,&count);if(e&&e!=PRO_TK_E_NOT_FOUND)goto done;e=0;
    for(i=0;i<count;i++){ProSecdimType type;double value;if(!ProSecdimTypeGet(s.section,ids[i],&type)&&!ProSecdimValueGet(s.section,ids[i],&value))part_text_add(&out,L"尺寸%d：类型%d，值%.9g%ls\n",ids[i],type,(type==PRO_TK_DIM_LINES_ANGLE||type==PRO_TK_DIM_ARC_ANGLE)?value:value*mm,(type==PRO_TK_DIM_LINES_ANGLE||type==PRO_TK_DIM_ARC_ANGLE)?L"度":L"毫米");}
    if(ids)ProArrayFree((ProArray *)&ids);ids=NULL;count=0;
    e=ProSectionConstraintsIdsGet(s.section,&ids,&count);if(e==PRO_TK_E_NOT_FOUND)e=0;if(e)goto done;
    for(i=0;i<count;i++){ProConstraintType type;ProConstraintStatus status;int refs,*entities=NULL;ProSectionPointType *senses=NULL;
        if(!ProSectionConstraintsGet(s.section,ids[i],&type,&status,&refs,&entities,&senses))part_text_add(&out,L"约束%d：类型%d，%ls\n",ids[i],type,status==PRO_TK_CONSTRAINT_ENABLED?L"有效":L"禁用");
        if(entities)ProArrayFree((ProArray *)&entities);if(senses)ProArrayFree((ProArray *)&senses);
    }
done:if(ids)ProArrayFree((ProArray *)&ids);fd_sketch_free(&s);return e;
}
