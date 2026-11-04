static ProError fd_section_solve(ProSection sec,double scale){
    ProBoolean im=PRO_B_FALSE;ProError e;int *ids=NULL,count=0,i;
    if((e=ProSectionIntentManagerModeGet(sec,&im)))return e;
    e=ProSectionEpsilonSet(sec,1e-6*scale);trace("foundation epsilon",e);if(e)return e;
    /* Toolkit documents that mode conversion resolves the section. Owned
       associative sketches use Intent Manager; Solve/Autodim are forbidden
       in that mode. Unlike SolveRigid this keeps editable dimensions. */
    if(!im){e=ProSectionIntentManagerModeSet(sec,PRO_B_TRUE);trace("foundation resolve with intent manager",e);if(e)return e;}
    if((e=ProSectionEntityIdsGet(sec,&ids,&count)))return e;
    for(i=0;i<count;i++){
        Pro2dEntdef *entity=NULL;if((e=ProSectionEntityGet(sec,ids[i],&entity)))break;
        if(entity->type==PRO_2D_LINE||entity->type==PRO_2D_CENTER_LINE){Pro2dLinedef *l=(Pro2dLinedef *)entity;if(!isfinite(l->end1[0])||!isfinite(l->end1[1])||!isfinite(l->end2[0])||!isfinite(l->end2[1])||hypot(l->end2[0]-l->end1[0],l->end2[1]-l->end1[1])<scale*1e-6){e=PRO_TK_GENERAL_ERROR;break;}}
        if(entity->type==PRO_2D_CIRCLE){Pro2dCircledef *c=(Pro2dCircledef *)entity;if(!isfinite(c->radius)||c->radius<=0){e=PRO_TK_GENERAL_ERROR;break;}}
        if(entity->type==PRO_2D_ARC){Pro2dArcdef *a=(Pro2dArcdef *)entity;if(!isfinite(a->radius)||a->radius<=0||fabs(a->end_angle-a->start_angle)<1e-9){e=PRO_TK_GENERAL_ERROR;break;}}
    }if(ids)ProArrayFree((ProArray *)&ids);return e;
}
