#ifndef BWTL_ADVANCED_DATUM_TOOLS_H
#define BWTL_ADVANCED_DATUM_TOOLS_H
#include <ProImportfeat.h>

typedef struct {ProVector point,normal,axis_y;ProCurve curve;double length;} AdGeometry;
static double ad_dot(const double *a,const double *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static void ad_sub(const double *a,const double *b,double *c){int i;for(i=0;i<3;i++)c[i]=a[i]-b[i];}
static void ad_cross(const double *a,const double *b,double *c){
    c[0]=a[1]*b[2]-a[2]*b[1];c[1]=a[2]*b[0]-a[0]*b[2];c[2]=a[0]*b[1]-a[1]*b[0];
}
static int ad_unit(double *v){double n=sqrt(ad_dot(v,v));int i;if(!isfinite(n)||n<1e-12)return 0;for(i=0;i<3;i++)v[i]/=n;return 1;}
static int ad_near(const double *a,const double *b,double tolerance){ProVector d;ad_sub(a,b,d);return ad_dot(d,d)<=tolerance*tolerance;}

static ProError ad_point(FdRef *ref,ProVector xyz){
    ProPoint p;
    ProError e=ProGeomitemToPoint(&ref->item,&p);
    return e?e:ProPointCoordGet(p,xyz);
}

static ProError ad_plane_constraint(FdTxn *tx,ProElement array,int type,FdRef *ref,double angle){
    ProElement entry=NULL;
    ProError e;
    if((e=model_element(array,PRO_E_DTMPLN_CONSTRAINT,0,0,&entry))||
       (e=model_element(entry,PRO_E_DTMPLN_CONSTR_TYPE,1,type,NULL))||
       (e=fd_ref_element(tx,entry,PRO_E_DTMPLN_CONSTR_REF,ref)))return e;
    return type==PRO_DTMPLN_ANG?model_element(entry,PRO_E_DTMPLN_CONSTR_REF_ANGLE,2,angle,NULL):PRO_TK_NO_ERROR;
}

static ProError ad_curve_item(ProGeomitem *item,ProError status,ProAppData data){
    FdCollect *set=data;
    ProCurve curve;
    ProEnttype type;
    ProError e;
    if(status)return status;
    if((e=ProGeomitemToCurve(item,&curve))||(e=ProCurveTypeGet(curve,&type)))return e;
    if(type!=PRO_ENT_CMP_CRV){set->item=*item;set->count++;}
    return PRO_TK_NO_ERROR;
}

static ProError ad_single_curve(ProFeature *feature,ProGeomitem *item,ProCurve *curve){
    FdCollect set={0};
    ProError e=ProFeatureGeomitemVisit(feature,PRO_CURVE,ad_curve_item,NULL,&set);
    if(e)return e;
    if(set.count!=1)return set.count?PRO_TK_E_AMBIGUOUS:PRO_TK_E_NOT_FOUND;
    *item=set.item;
    return ProGeomitemToCurve(item,curve);
}

static ProError ad_curve_ref(ProMdl model,const wchar_t *token,FdRef *ref,ProCurve *curve){
    ProFeature f;
    ProError e;
    if((e=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)token+6,(ProModelitem *)&f))||
       (e=ad_single_curve(&f,&ref->item,curve)))return e;
    ref->owner_feature=f.id;
    return ProSelectionAlloc(NULL,(ProModelitem *)&ref->item,&ref->selection);
}

static ProError ad_line(const FdRef *ref,ProVector point,ProVector direction){
    ProVector end,d1,d2,d3;
    ProError e;
    if(ref->item.type==PRO_AXIS){
        ProAxis axis;
        ProGeomitemdata *raw=NULL;
        if((e=ProGeomitemToAxis((ProGeomitem *)&ref->item,&axis))||(e=ProAxisDataGet(axis,&raw)))return e;
        e=ProLinedataGet(raw->data.p_curve_data,point,end);
        ProGeomitemdataFree(&raw);
        if(e)return e;
    }else{
        ProEdge edge;
        ProEnttype type;
        if((e=ProGeomitemToEdge((ProGeomitem *)&ref->item,&edge))||(e=ProEdgeTypeGet(edge,&type)))return e;
        if(type!=PRO_ENT_LINE)return PRO_TK_INVALID_TYPE;
        if((e=ProEdgeXyzdataEval(edge,0,point,d1,d2,d3))||(e=ProEdgeXyzdataEval(edge,1,end,d1,d2,d3)))return e;
    }
    ad_sub(end,point,direction);
    return ad_unit(direction)?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
}

#include "功能/advanced_datum/ad_angle.inc"
#include "功能/advanced_datum/ad_three.inc"
#include "功能/advanced_datum/ad_mid.inc"
#include "功能/advanced_datum/ad_tangent.inc"
#include "功能/advanced_datum/ad_normal.inc"
#include "功能/advanced_datum/ad_axis_points.inc"
#include "功能/advanced_datum/ad_point_curve.inc"
#include "功能/advanced_datum/ad_intersection.inc"
#include "功能/advanced_datum/ad_spline.inc"
#include "功能/advanced_datum/ad_sym_csys.inc"
#include "功能/advanced_datum/ad_execute.inc"
#endif
