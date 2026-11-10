typedef struct {const wchar_t *type,*sketch;double first,second;} SxProfile;

static SxProfile sx_profile_command(const PartCommand *cmd,int first,double scale){
    SxProfile p={cmd->arg[first],NULL,0,0};
    if(!wcscmp(p.type,L"SKETCH"))p.sketch=cmd->arg[first+1];
    else{
        p.first=as_arg(cmd,first+1)*scale;
        if(cmd->count>first+2)p.second=as_arg(cmd,first+2)*scale;
    }
    return p;
}

static ProError sx_copy_profile(ProMdl model,const wchar_t *name,ProSection target){
    FdSketch source={0};
    ProSectionShape shape;
    int *ids=NULL,n=0,added=0,id;
    ProError e=fd_sketch_open(model,name,&source,0);
    if(e)goto done;
    if((e=ProSectionShapeGet(source.section,&shape))||(e=ProSectionEntityIdsGet(source.section,&ids,&n)))goto done;
    if(shape!=PRO_SECSHAPE_1_CLOSED_LOOP||n>AS_LIMIT){e=PRO_TK_BAD_INPUTS;goto done;}
    for(int i=0;i<n;i++){
        ProBoolean reference=PRO_B_FALSE,construction=PRO_B_FALSE;
        Pro2dEntdef *entity=NULL;
        if((e=ProSectionEntityGet(source.section,ids[i],&entity))||
           (e=ProSectionEntityIsProjection(source.section,ids[i],&reference))||
           (e=ProSectionEntityIsConstruction(source.section,ids[i],&construction)))goto done;
        if(reference||construction||entity->type==PRO_2D_CENTER_LINE)continue;
        if(entity->type!=PRO_2D_LINE&&entity->type!=PRO_2D_ARC&&entity->type!=PRO_2D_CIRCLE&&
           entity->type!=PRO_2D_B_SPLINE&&entity->type!=PRO_2D_SPLINE&&entity->type!=PRO_2D_CONIC&&entity->type!=PRO_2D_ELLIPSE){
            e=PRO_TK_UNSUPPORTED;goto done;
        }
        if((e=ProSectionEntityAdd(target,entity,&id)))goto done;
        added++;
    }
    if(!added)e=PRO_TK_BAD_INPUTS;
done:
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&source);
    return e;
}

static ProError sx_sweep_draw(ProMdl model,ProSection section,const SxProfile *p){
    ProError e;
    int id;
    if(p->sketch)return sx_copy_profile(model,p->sketch,section);
    if(!wcscmp(p->type,L"CIRCLE")){
        if((e=model_circle(section,0,0,p->first/2,&id)))return e;
        return model_section_dimension(section,id,PRO_TK_DIM_DIA,p->first,p->first,p->first);
    }
    if(!wcscmp(p->type,L"ELLIPSE")){
        Pro2dEllipsedef ellipse={0};
        ellipse.type=PRO_2D_ELLIPSE;ellipse.x_radius=p->first/2;ellipse.y_radius=p->second/2;
        return ProSectionEntityAdd(section,(Pro2dEntdef *)&ellipse,&id);
    }
    {
        const AsPoint corners[]={{-p->first/2,-p->second/2},{p->first/2,-p->second/2},{p->first/2,p->second/2},{-p->first/2,p->second/2}};
        for(int i=0;i<4;i++)if((e=as_add_line(section,corners[i],corners[(i+1)%4],&id)))return e;
    }
    return PRO_TK_NO_ERROR;
}
