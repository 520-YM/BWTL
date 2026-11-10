#ifndef BWTL_ADVANCED_SKETCH_EDIT_H
#define BWTL_ADVANCED_SKETCH_EDIT_H

#define AS_AUDIT_LIMIT 96
typedef struct {AsPoint a,b;} AsLine;
typedef struct {int id;Pro2dEntType type;int sampled;AsPoint points[5];} AsEntity;
typedef struct {AsEntity items[AS_AUDIT_LIMIT];int count;} AsSnapshot;
typedef struct {int ids[AS_LIMIT],count;FdRef references[AS_LIMIT];int ref_count;int changed[AS_AUDIT_LIMIT],changed_count;} AsEdit;

static int as_id(const PartCommand *cmd,int i){int id=0;part_integer(cmd->arg[i],&id);return id;}
static AsPoint as_direction(AsLine line){return as_minus(line.b,line.a);}
static AsLine as_reverse(AsLine line){AsLine result={line.b,line.a};return result;}
static int as_near(AsPoint a,AsPoint b,double tol){return as_dist(a,b)<=tol;}

static ProError as_model_scale(ProMdl model,double *scale){
    ProUnitsystem units;
    ProUnititem mm,native;
    ProUnitConversion conversion;
    ProError e;
    if((e=ProMdlPrincipalunitsystemGet(model,&units))||(e=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&native))||
       (e=ProUnitInit(model,L"mm",&mm))||(e=ProUnitConversionCalculate(&mm,&native,&conversion)))return e;
    if(!isfinite(conversion.scale)||conversion.scale<=0||conversion.offset!=0)return PRO_TK_BAD_CONTEXT;
    *scale=conversion.scale;
    return PRO_TK_NO_ERROR;
}

static ProError as_regular(ProSection section,int id,Pro2dEntdef **entity){
    ProBoolean reference=PRO_B_FALSE,construction=PRO_B_FALSE;
    ProError e;
    if((e=ProSectionEntityGet(section,id,entity))||(e=ProSectionEntityIsProjection(section,id,&reference))||
       (e=ProSectionEntityIsConstruction(section,id,&construction)))return e;
    return reference||construction?PRO_TK_INVALID_TYPE:PRO_TK_NO_ERROR;
}

static ProError as_line_get(ProSection section,int id,AsLine *line){
    Pro2dEntdef *entity=NULL;
    ProError e=as_regular(section,id,&entity);
    if(e)return e;
    if(entity->type!=PRO_2D_LINE)return PRO_TK_INVALID_TYPE;
    line->a=as_p(((Pro2dLinedef *)entity)->end1);line->b=as_p(((Pro2dLinedef *)entity)->end2);
    return PRO_TK_NO_ERROR;
}

static int as_line_intersect(AsLine a,AsLine b,double *ta,double *tb){
    AsPoint da=as_direction(a),db=as_direction(b),delta=as_minus(b.a,a.a);
    double den=as_cross2(da,db);
    if(fabs(den)<=1e-10*as_dist(a.a,a.b)*as_dist(b.a,b.b))return 0;
    *ta=as_cross2(delta,db)/den;*tb=as_cross2(delta,da)/den;
    return 1;
}

static ProError as_point_at(ProSection section,int id,Pro2dEntdef *entity,double t,AsPoint *p){
    ProVector point,d1,d2;
    ProError e;
    if(entity->type==PRO_2D_LINE||entity->type==PRO_2D_CENTER_LINE){
        Pro2dLinedef *line=(Pro2dLinedef *)entity;
        *p=as_plus(as_p(line->end1),as_mul(as_minus(as_p(line->end2),as_p(line->end1)),t));
        return PRO_TK_NO_ERROR;
    }
    if(entity->type==PRO_2D_ARC){
        Pro2dArcdef *arc=(Pro2dArcdef *)entity;
        double span=arc->end_angle-arc->start_angle;
        if(span<0)span+=2*AS_PI;
        *p=as_plus(as_p(arc->center),as_mul(as_ray(arc->start_angle+t*span),arc->radius));
        return PRO_TK_NO_ERROR;
    }
    if(entity->type==PRO_2D_CIRCLE){
        Pro2dCircledef *circle=(Pro2dCircledef *)entity;
        *p=as_plus(as_p(circle->center),as_mul(as_ray(t*2*AS_PI),circle->radius));
        return PRO_TK_NO_ERROR;
    }
    if((e=ProSectionEntityEval(section,id,t,point,d1,d2)))return e;
    *p=as_p(point);
    return PRO_TK_NO_ERROR;
}

static int as_profile_type(Pro2dEntType type){
    return type==PRO_2D_LINE||type==PRO_2D_ARC||type==PRO_2D_CIRCLE||type==PRO_2D_SPLINE||
        type==PRO_2D_B_SPLINE||type==PRO_2D_ELLIPSE||type==PRO_2D_CONIC;
}

static ProError as_snapshot(ProSection section,AsSnapshot *snapshot){
    int *ids=NULL,n=0,i,k;
    ProError e=ProSectionEntityIdsGet(section,&ids,&n);
    snapshot->count=0;
    if(e==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
    if(e)return e;
    if(n>AS_AUDIT_LIMIT){e=PRO_TK_BAD_INPUTS;goto done;}
    for(i=0;i<n;i++){
        Pro2dEntdef *entity=NULL;
        AsEntity *out=&snapshot->items[snapshot->count++];
        if((e=ProSectionEntityGet(section,ids[i],&entity)))goto done;
        out->id=ids[i];out->type=entity->type;out->sampled=as_profile_type(entity->type)||entity->type==PRO_2D_CENTER_LINE;
        if(out->sampled)for(k=0;k<5;k++)if((e=as_point_at(section,ids[i],entity,k/4.0,&out->points[k])))goto done;
    }
done:
    if(ids)ProArrayFree((ProArray *)&ids);
    return e;
}

static const AsEntity *as_find(const AsSnapshot *snapshot,int id){
    int i;
    for(i=0;i<snapshot->count;i++)if(snapshot->items[i].id==id)return &snapshot->items[i];
    return NULL;
}

static int as_same_entity(const AsEntity *a,const AsEntity *b,double tol){
    int i,direct=1,reverse=1;
    if(!b||a->type!=b->type||a->sampled!=b->sampled)return 0;
    if(!a->sampled)return 1;
    for(i=0;i<5;i++){
        direct&=as_near(a->points[i],b->points[i],tol);
        reverse&=as_near(a->points[i],b->points[4-i],tol);
    }
    return direct||reverse;
}

static int as_snapshot_match(const AsSnapshot *before,const AsSnapshot *after,const AsEdit *edit,double tol){
    int i,j;
    if(!edit&&before->count!=after->count)return 0;
    for(i=0;i<before->count;i++){
        if(edit){
            for(j=0;j<edit->changed_count;j++)if(before->items[i].id==edit->changed[j])break;
            if(j<edit->changed_count)continue;
        }
        if(!as_same_entity(&before->items[i],as_find(after,before->items[i].id),tol))return 0;
    }
    return 1;
}

static ProError as_replace_line(ProSection section,int id,AsLine value,AsEdit *edit){
    int new_id;
    ProError e;
    if(edit->changed_count>=AS_AUDIT_LIMIT)return PRO_TK_BAD_INPUTS;
    if((e=as_add_line(section,value.a,value.b,&new_id))||(e=ProSectionEntityReplace(section,id,new_id)))return e;
    edit->changed[edit->changed_count++]=id;
    return PRO_TK_NO_ERROR;
}

#include "功能/advanced_sketch/as_offset.inc"
#include "功能/advanced_sketch/as_mirror.inc"
#include "功能/advanced_sketch/as_trim.inc"
#include "功能/advanced_sketch/as_corner.inc"
#include "功能/advanced_sketch/as_chain.inc"
#include "功能/advanced_sketch/as_check.inc"
#include "功能/advanced_sketch/as_repair.inc"
#include "功能/advanced_sketch/as_execute.inc"
#endif
