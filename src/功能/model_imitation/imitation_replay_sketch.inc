#define REPLAY_SECTION_MAX 512

typedef struct {int source,target,localized,generated_axis;} ReplaySectionEntity;
typedef enum {REPLAY_ASSOCIATIVE_SECTION,REPLAY_INDEPENDENT_SECTION,REPLAY_HOLE_SECTION,REPLAY_SWEEP_SECTION} ReplaySectionMode;
#include "imitation_replay_constraints.inc"

static int replay_point2_same(const double *a,const double *b){
    return hypot(a[0]-b[0],a[1]-b[1])<=0.0002;
}

#include "imitation_replay_sketch_curves.inc"

static int replay_entity_same(const Pro2dEntdef *a,const Pro2dEntdef *b){
    if(a->type!=b->type)return 0;
    if(a->type==PRO_2D_LINE||a->type==PRO_2D_CENTER_LINE){
        const Pro2dLinedef *x=(const Pro2dLinedef *)a,*y=(const Pro2dLinedef *)b;
        return (replay_point2_same(x->end1,y->end1)&&replay_point2_same(x->end2,y->end2))||
               (replay_point2_same(x->end1,y->end2)&&replay_point2_same(x->end2,y->end1));
    }
    if(a->type==PRO_2D_CIRCLE){
        const Pro2dCircledef *x=(const Pro2dCircledef *)a,*y=(const Pro2dCircledef *)b;
        return replay_point2_same(x->center,y->center)&&fabs(x->radius-y->radius)<0.0002;
    }
    if(a->type==PRO_2D_ARC){
        const Pro2dArcdef *x=(const Pro2dArcdef *)a,*y=(const Pro2dArcdef *)b;
        return replay_point2_same(x->center,y->center)&&fabs(x->radius-y->radius)<0.0002&&
            fabs(remainder(x->start_angle-y->start_angle,2*AS_PI))<1e-6&&
            fabs(remainder(x->end_angle-y->end_angle,2*AS_PI))<1e-6;
    }
    if(a->type==PRO_2D_ELLIPSE){
        const Pro2dEllipsedef *x=(const Pro2dEllipsedef *)a,*y=(const Pro2dEllipsedef *)b;
        return replay_point2_same(x->origin,y->origin)&&fabs(x->x_radius-y->x_radius)<0.0002&&
            fabs(x->y_radius-y->y_radius)<0.0002;
    }
    if(a->type==PRO_2D_CONIC){
        const Pro2dConicdef *x=(const Pro2dConicdef *)a,*y=(const Pro2dConicdef *)b;
        return replay_point2_same(x->first_end_point,y->first_end_point)&&
            replay_point2_same(x->second_end_point,y->second_end_point)&&
            replay_point2_same(x->shoulder_point,y->shoulder_point)&&fabs(x->parameter-y->parameter)<1e-7;
    }
    if(a->type==PRO_2D_SPLINE)return replay_through_spline_same((const Pro2dSplinedef *)a,(const Pro2dSplinedef *)b);
    if(a->type==PRO_2D_B_SPLINE)return replay_bspline_same((const Pro2dBSplinedef *)a,(const Pro2dBSplinedef *)b);
    if(a->type!=PRO_2D_POINT&&a->type!=PRO_2D_COORD_SYS)return 0;
    const Pro2dPointdef *x=(const Pro2dPointdef *)a,*y=(const Pro2dPointdef *)b;
    return replay_point2_same(x->pnt,y->pnt);
}

static ProError replay_section_entity_compare(ProSection source,int source_id,ProSection target,int target_id)
{
    Pro2dEntdef *entity=NULL;
    ReplayEntitySnapshot original={0};
    ProError error=ProSectionEntityGet(source,source_id,&entity);
    if(error)return error;
    Pro2dEntdef *from=entity;
    error=replay_entity_snapshot(entity,&original);
    if(!error)from=&original.definition.entity;
    else if(error==PRO_TK_UNSUPPORTED)error=PRO_TK_NO_ERROR;
    if(!error)error=ProSectionEntityGet(target,target_id,&entity);
    if(!error&&!replay_entity_same(from,entity))error=PRO_TK_INCOMPLETE;
    free(original.storage);
    return error;
}

static ProError replay_section_dimensions(ProSection source,ProSection target,const ReplaySectionEntity *map,int count,
                                      int *dimensions){
    int *dims=NULL,total=0;
    if(ProSecdimIdsGet(source,&dims,&total))return PRO_TK_NO_ERROR;
    ProError lock_error=PRO_TK_NO_ERROR;
    for(int i=0;!lock_error&&i<total;i++){
        int *refs=NULL,n=0;
        ProSectionPointType *points=NULL;
        ProSecdimType type;
        Pro2dPnt place;
        double original,actual;
        int created;
        ProError error=ProSecdimReferencesGet(source,dims[i],&refs,&points,&n);
        if(!error&&n<=0)error=PRO_TK_UNSUPPORTED;
        for(int j=0;!error&&j<n;j++){
            int k=0;
            while(k<count&&map[k].source!=refs[j])k++;
            if(k==count)error=PRO_TK_E_NOT_FOUND;
            else refs[j]=map[k].target;
        }
        if(!error)error=ProSecdimTypeGet(source,dims[i],&type);
        if(!error)error=ProSecdimLocationGet(source,dims[i],place);
        if(!error)error=ProSecdimValueGet(source,dims[i],&original);
        ProBoolean locked=PRO_B_FALSE;
        ProSecdimIsLocked(source,dims[i],&locked);
        if(!error)error=ProSecdimCreate(target,refs,points,n,type,place,&created);
        if(!error){
            error=ProSecdimValueGet(target,created,&actual);
            if(!error&&!part_close(actual,original))error=PRO_TK_INCOMPLETE;
            if(error)ProSecdimDelete(target,created);
            else{
                (*dimensions)++;
                if(locked)lock_error=ProSecdimLockSet(target,created,PRO_B_TRUE);
            }
        }
        if(locked&&error)lock_error=error;
        if(refs)ProArrayFree((ProArray *)&refs);
        if(points)ProArrayFree((ProArray *)&points);
    }
    if(dims)ProArrayFree((ProArray *)&dims);
    return lock_error;
}

static ProError replay_project_entity(ImitationReplay *replay,ProSection source,ProSection target,int id,int *created,int *localized){
    ProSelection reference=NULL,selected=NULL;
    ProReference original=NULL;
    ProModelitem item;
    ProError error=ProSectionEntityReferenceGet(source,id,&reference);
    if(!error)error=ProSelectionToReference(reference,&original);
    if(!error&&replay->external_planes){
        ProMdl owner=NULL;
        if(!ProReferenceOwnerGet(original,&owner)&&owner&&owner!=replay->source&&owner!=replay->definition_owner){
            ReplayExternalReference external={0};
            error=replay_external_reference(replay,original,&external);
            if(!error&&replay->reference_pattern){
                ProError projection=replay_pattern_project(replay,&external,target,created);
                if(projection!=PRO_TK_E_NOT_FOUND){
                    if(!projection)replay->external_used[external.index]=1;
                    ProReferenceFree(original);
                    return projection;
                }
            }
            Pro2dEntdef *entity=NULL;
            if(!error)error=ProSectionEntityGet(source,id,&entity);
            if(!error&&(entity->type!=PRO_2D_LINE&&entity->type!=PRO_2D_CIRCLE&&
                entity->type!=PRO_2D_ARC&&entity->type!=PRO_2D_POINT))error=PRO_TK_UNSUPPORTED;
            if(!error)error=ProSectionEntityAdd(target,entity,created);
            if(!error)error=ProSectionEntityConstructionSet(target,*created,PRO_B_TRUE);
            if(!error){
                if(external.index<0||external.index>=replay->external_total){
                    ProReferenceFree(original);
                    return PRO_TK_OUT_OF_RANGE;
                }
                replay->external_used[external.index]=1;
                *localized=1;
                part_text_add(replay->out,L"源特征 %d 的投影图元 %d 已重画为内嵌构造几何。\n",
                    replay->current_feature,id);
            }
            ProReferenceFree(original);
            return error;
        }
    }

    if(!error)error=replay_reference_item(replay,original,&item,0);
    if(!error)error=ProSelectionAlloc(NULL,&item,&selected);
    if(!error)error=ProSectionEntityFromProjection(target,selected,created);
    if(original)ProReferenceFree(original);
    if(selected)ProSelectionFree(&selected);
    return error;
}

static ProError replay_pattern_seed_dimensions(ProSection section){
    int *ids=NULL,count=0,*dims=NULL,dim_count=0;
    unsigned char sized[REPLAY_SECTION_MAX]={0};
    ProError error=ProSectionEntityIdsGet(section,&ids,&count);
    if(!error&&count>REPLAY_SECTION_MAX)error=PRO_TK_OUT_OF_RANGE;
    if(!error){
        error=ProSecdimIdsGet(section,&dims,&dim_count);
        if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    }
    for(int i=0;!error&&i<dim_count;i++){
        ProSecdimType type;
        error=ProSecdimTypeGet(section,dims[i],&type);
        if(error||(type!=PRO_TK_DIM_DIA&&type!=PRO_TK_DIM_RAD&&type!=PRO_TK_DIM_DIA_LINEAR))continue;
        int *refs=NULL,n=0;
        ProSectionPointType *points=NULL;
        error=ProSecdimReferencesGet(section,dims[i],&refs,&points,&n);
        for(int j=0;!error&&j<n;j++)for(int k=0;k<count;k++)if(ids[k]==refs[j])sized[k]=1;
        if(refs)ProArrayFree((ProArray *)&refs);
        if(points)ProArrayFree((ProArray *)&points);
    }
    for(int i=0;!error&&i<count;i++){
        if(sized[i])continue;
        ProBoolean projection,construction;
        Pro2dEntdef *entity=NULL;
        error=ProSectionEntityIsProjection(section,ids[i],&projection);
        if(!error)error=ProSectionEntityIsConstruction(section,ids[i],&construction);
        if(error||projection||construction)continue;
        error=ProSectionEntityGet(section,ids[i],&entity);
        if(error==PRO_TK_INVALID_TYPE){error=PRO_TK_NO_ERROR;continue;}
        if(error||entity->type!=PRO_2D_CIRCLE)continue;
        const Pro2dCircledef *circle=(const Pro2dCircledef *)entity;
        error=model_section_dimension(section,ids[i],PRO_TK_DIM_DIA,2*circle->radius,
            circle->center[0]+4*circle->radius,circle->center[1]+4*circle->radius);
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    if(dims)ProArrayFree((ProArray *)&dims);
    return error;
}

static ProError replay_draw_section(ImitationReplay *replay,ProSection source,ProSection target,
                                    ReplaySectionMode mode,int *drawn,int *dimensions){
    const int independent=mode!=REPLAY_ASSOCIATIVE_SECTION;
    ProBoolean intent=PRO_B_FALSE;
    ProError error=ProSectionIntentManagerModeGet(target,&intent);
    if(error)trace("Replay sketch read intent",error);
    if(!error&&intent){
        error=ProSectionIntentManagerModeSet(target,PRO_B_FALSE);
        if(error)trace("Replay sketch disable intent",error);
    }
    ReplaySectionEntity map[REPLAY_SECTION_MAX];
    int *ids=NULL,count=0,mapped=0;
    if(!error){
        error=ProSectionEntityIdsGet(source,&ids,&count);
        if(error)trace("Replay source sketch entity list",error);
    }
    if(!error&&count>REPLAY_SECTION_MAX)error=PRO_TK_OUT_OF_RANGE;
    for(int i=0;!error&&i<count;i++){
        Pro2dEntdef *entity=NULL;
        ProBoolean projection=PRO_B_FALSE,construction=PRO_B_FALSE;
        error=ProSectionEntityIsProjection(source,ids[i],&projection);
        if(!error)error=ProSectionEntityIsConstruction(source,ids[i],&construction);
        if(error){trace("Replay source entity metadata ID",ids[i]);trace("Replay source entity metadata",error);}
        if(error)continue;
        if(projection){
            if(!independent){
                int id,localized=0;
                ProError projected=replay_project_entity(replay,source,target,ids[i],&id,&localized);
                if(!projected){
                    map[mapped++]=(ReplaySectionEntity){ids[i],id,localized};
                }else trace("Replay unavailable section projection",projected);
            }
            continue;
        }
        error=ProSectionEntityGet(source,ids[i],&entity);
        if(error==PRO_TK_INVALID_TYPE){
            /* Some embedded sections expose internal records without a
               public 2D entity. The rebuilt result still needs full shape verification. */
            error=PRO_TK_NO_ERROR;
            continue;
        }
        if(error){trace("Replay source geometry ID",ids[i]);trace("Replay source geometry",error);break;}
        if(mode==REPLAY_SWEEP_SECTION&&entity->type==PRO_2D_POINT){
            const Pro2dPointdef saved=*(const Pro2dPointdef *)entity;
            Pro2dEntdef *existing=NULL;
            if(!ProSectionEntityGet(target,ids[i],&existing)&&replay_entity_same((const Pro2dEntdef *)&saved,existing)){
                map[mapped++]=(ReplaySectionEntity){ids[i],ids[i],0,1};
                continue;
            }
        }
        /* A Creo 2D section creates its horizontal and vertical coordinate
           axes when the coordinate-system entity is inserted. Reuse those
           axes for the original sketch dimensions. */
        if(mode==REPLAY_HOLE_SECTION&&(ids[i]==1||ids[i]==2)){
            Pro2dEntdef *existing=NULL;
            error=ProSectionEntityGet(target,ids[i],&existing);
            if(!error&&(entity->type!=PRO_2D_CENTER_LINE||existing->type!=PRO_2D_CENTER_LINE))
                error=PRO_TK_UNSUPPORTED;
            if(!error){
                const Pro2dLinedef *original=(const Pro2dLinedef *)entity;
                const Pro2dLinedef *axis=(const Pro2dLinedef *)existing;
                const int normal=ids[i]==1?1:0;
                if(fabs(original->end1[normal]-axis->end1[normal])>0.0002||
                   fabs(original->end2[normal]-axis->end2[normal])>0.0002)error=PRO_TK_UNSUPPORTED;
                else map[mapped++]=(ReplaySectionEntity){ids[i],ids[i],0,1};
            }
            continue;
        }
        switch(entity->type){
        case PRO_2D_POINT:case PRO_2D_COORD_SYS:case PRO_2D_LINE:case PRO_2D_CENTER_LINE:
        case PRO_2D_ARC:case PRO_2D_CIRCLE:case PRO_2D_ELLIPSE:case PRO_2D_CONIC:
        case PRO_2D_SPLINE:case PRO_2D_B_SPLINE:
            break;
        default:error=PRO_TK_UNSUPPORTED;
        }
        int id;
        if(!error)error=ProSectionEntityAdd(target,entity,&id);
        if(error){trace("Replay draw entity type",entity->type);trace("Replay draw entity",error);}
        if(!error)map[mapped++]=(ReplaySectionEntity){ids[i],id};
        if(!error&&construction&&entity->type!=PRO_2D_CENTER_LINE&&entity->type!=PRO_2D_COORD_SYS&&
           entity->type!=PRO_2D_POINT)
            error=ProSectionEntityConstructionSet(target,id,PRO_B_TRUE);
        if(error)trace("Replay mark construction geometry",error);
        if(!error&&!construction)(*drawn)++;
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    if(!error&&!*drawn)error=PRO_TK_EMPTY;
    if(!error)error=replay_section_dimensions(source,target,map,mapped,dimensions);
    if(!error&&replay->reference_pattern)error=replay_pattern_seed_dimensions(target);
    if(!error&&independent){
        ProWSecerror diagnostics=NULL;
        error=ProSecerrorAlloc(&diagnostics);
        if(!error&&mode==REPLAY_HOLE_SECTION)error=ProSectionEpsilonSet(target,1e-5);
        if(!error&&mode==REPLAY_INDEPENDENT_SECTION)error=ProSectionSolve(target,&diagnostics);
        if(!error)error=ProSectionAutodim(target,&diagnostics);
        if(!error&&(mode==REPLAY_HOLE_SECTION||mode==REPLAY_SWEEP_SECTION))error=ProSectionRegenerate(target,&diagnostics);
        if(diagnostics)ProSecerrorFree(&diagnostics);
        if(error)trace("Replay solve attached profile",error);
    }else if(!error){
        error=fd_section_solve(target,1);
        if(!error)replay_section_constraints(source,target,map,mapped);
    }
    for(int i=0;!error&&i<mapped;i++){
        ProBoolean projection=PRO_B_FALSE;
        error=ProSectionEntityIsProjection(source,map[i].source,&projection);
        if(!error&&!map[i].generated_axis&&(!projection||map[i].localized||!replay->external_total))
            error=replay_section_entity_compare(source,map[i].source,target,map[i].target);
        if(!error&&!map[i].generated_axis&&!projection){
            ProBoolean original=PRO_B_FALSE,rebuilt=PRO_B_FALSE;
            error=ProSectionEntityIsConstruction(source,map[i].source,&original);
            if(!error)error=ProSectionEntityIsConstruction(target,map[i].target,&rebuilt);
            if(!error&&original!=rebuilt)error=PRO_TK_INCOMPLETE;
        }
        if(error){
            trace("Replay changed solved entity",map[i].source);
            trace("Replay entity verification",error);
        }
    }
    if(!error&&!independent){
        ProSectionShape a,b;
        error=ProSectionShapeGet(source,&a);
        if(!error)error=ProSectionShapeGet(target,&b);
        if(!error&&a!=b){
            trace("Replay source section topology",a);
            trace("Replay result section topology",b);
            error=PRO_TK_INCOMPLETE;
        }
    }
    return error;
}

static ProError replay_section_frame(ProSection source,ProSection target){
    ProMatrix a,b;
    ProError error=ProSectionLocationGet(source,a);
    if(!error)error=ProSectionLocationGet(target,b);
    for(int i=0;!error&&i<4;i++)if(!ad_near(a[i],b[i],0.0002))error=PRO_TK_INCOMPLETE;
    return error;
}
