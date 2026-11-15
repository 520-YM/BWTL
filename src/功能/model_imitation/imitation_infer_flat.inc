#include <ProIntfimport.h>

#define IMITATION_FLAT_MAX 256

typedef struct {
    ProEnttype type;
    ProVector a,b,center,middle;
    double radius;
} ImitationInferredEdge;

typedef struct {
    ProMdl source;
    ProSurface face;
    ProOutline bounds;
    int axis,count,outer,loops;
    double mm,area,thickness;
    ImitationInferredEdge edge[IMITATION_FLAT_MAX];
} ImitationFlatPlan;

static ProError imitation_infer_face(ProSurface face,ProError status,ProAppData data)
{
    if(status)return status;
    ImitationFlatPlan *plan=data;
    ProGeomitem item;
    ProVector point,normal;
    if(ProSurfaceToGeomitem((ProSolid)plan->source,face,&item)||fd_plane_data(&item,point,normal))
        return PRO_TK_NO_ERROR;
    if(normal[plan->axis]<0.999999||fabs(point[plan->axis]*plan->mm-plan->bounds[1][plan->axis])>0.001)
        return PRO_TK_NO_ERROR;
    double area;
    ProError error=ProSurfaceAreaEval(face,&area);
    if(!error&&area>plan->area){plan->area=area;plan->face=face;}
    return error;
}

static ProError imitation_infer_edge(ProEdge edge,ProError status,ProAppData data)
{
    if(status)return status;
    ImitationFlatPlan *plan=data;
    if(plan->count==IMITATION_FLAT_MAX)return PRO_TK_OUT_OF_RANGE;
    ImitationInferredEdge *entity=&plan->edge[plan->count];
    ProEnttype type;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(error)return error;
    if(type!=PRO_ENT_LINE&&type!=PRO_ENT_ARC&&type!=PRO_ENT_CIRCLE){
        trace("Inferred contour unsupported edge type",type);
        return PRO_TK_UNSUPPORTED;
    }
    error=ProEdgeXyzdataEval(edge,0,entity->a,NULL,NULL,NULL);
    if(!error)error=ProEdgeXyzdataEval(edge,1,entity->b,NULL,NULL,NULL);
    if(error)return error;
    entity->type=type;
    if(type!=PRO_ENT_LINE){
        ProModelitem item;
        if(!error)error=ProEdgeToGeomitem((ProSolid)plan->source,edge,&item);
        if(!error)error=replay_circle_geometry(&item,entity->center,&entity->radius);
        if(!error)error=ProEdgeXyzdataEval(edge,0.5,entity->middle,NULL,NULL,NULL);
        if(error)return error;
        entity->radius*=plan->mm;
        if(ad_near(entity->a,entity->b,0.0002))entity->type=PRO_ENT_CIRCLE;
    }
    for(int i=0;i<3;i++){
        entity->a[i]*=plan->mm;entity->b[i]*=plan->mm;
        entity->center[i]*=plan->mm;entity->middle[i]*=plan->mm;
    }
    plan->count++;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_infer_loop(ProContour contour,ProError status,ProAppData data)
{
    if(status)return status;
    ImitationFlatPlan *plan=data;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(contour,&traversal);
    if(!error&&traversal==PRO_CONTOUR_TRAV_EXTERNAL)plan->outer++;
    const int start=plan->count;
    if(!error)error=ProContourEdgeVisit(plan->face,contour,imitation_infer_edge,NULL,plan);
    /* STEP often represents a cylindrical through-hole with two half arcs. */
    if(!error&&plan->count-start==2){
        ImitationInferredEdge *a=&plan->edge[start],*b=a+1;
        if(a->type==PRO_ENT_ARC&&b->type==PRO_ENT_ARC&&
           imitation_metric_same(a->radius,b->radius)&&ad_near(a->center,b->center,0.0002)&&
           ad_near(a->a,b->b,0.0002)&&ad_near(a->b,b->a,0.0002)&&
           cm_distance(a->middle,b->middle)>0.01){
            a->type=PRO_ENT_CIRCLE;
            plan->count--;
        }
    }
    if(!error)plan->loops++;
    return error;
}

static ProError imitation_flat_identify(ProMdl source,const ImitationShapeMetrics *shape,ImitationFlatPlan *plan)
{
    if(shape->bodies!=1||shape->quilts)return PRO_TK_UNSUPPORTED;
    plan->source=source;
    plan->mm=shape->mm;
    memcpy(plan->bounds,shape->bounds,sizeof(plan->bounds));
    int axis=0;
    for(int i=1;i<3;i++)if(shape->bounds[1][i]-shape->bounds[0][i]<
        shape->bounds[1][axis]-shape->bounds[0][axis])axis=i;
    plan->axis=axis;
    plan->thickness=shape->bounds[1][axis]-shape->bounds[0][axis];
    if(plan->thickness<=0.001)return PRO_TK_UNSUPPORTED;
    ProError error=ProSolidSurfaceVisit((ProSolid)source,imitation_infer_face,NULL,plan);
    if(!error&&!plan->face)error=PRO_TK_UNSUPPORTED;
    if(!error&&!imitation_metric_same(shape->volume,plan->area*shape->mm*shape->mm*plan->thickness)){
        trace("Inferred stock volume mismatch",0);
        error=PRO_TK_UNSUPPORTED;
    }
    if(!error)error=ProSurfaceContourVisit(plan->face,imitation_infer_loop,NULL,plan);
    if(!error&&(plan->outer!=1||plan->count<1))error=PRO_TK_UNSUPPORTED;
    return error;
}

static AsPoint imitation_inferred_point(const double *point,const ProMatrix frame,double scale)
{
    ProVector local;
    for(int i=0;i<3;i++)local[i]=point[i]*scale-frame[3][i];
    return as_point(ad_dot(local,frame[0]),ad_dot(local,frame[1]));
}

static ProError imitation_draw_flat_plan(ProSection section,const ProMatrix frame,double scale,void *data)
{
    const ImitationFlatPlan *plan=data;
    for(int i=0;i<plan->count;i++){
        const ImitationInferredEdge *edge=&plan->edge[i];
        const AsPoint start=imitation_inferred_point(edge->a,frame,scale);
        int id;
        ProError error;
        const AsPoint end=imitation_inferred_point(edge->b,frame,scale);
        if(edge->type==PRO_ENT_LINE)error=as_add_line(section,start,end,&id);
        else{
            const AsPoint center=imitation_inferred_point(edge->center,frame,scale);
            if(edge->type==PRO_ENT_CIRCLE){
                error=model_circle(section,center.x,center.y,edge->radius*scale,&id);
                if(!error)error=model_section_dimension(section,id,PRO_TK_DIM_DIA,2*edge->radius*scale,
                    center.x+4*edge->radius*scale,center.y+4*edge->radius*scale);
            }else{
                const AsPoint mid=imitation_inferred_point(edge->middle,frame,scale);
                double a=atan2(start.y-center.y,start.x-center.x);
                double b=atan2(end.y-center.y,end.x-center.x);
                const double m=atan2(mid.y-center.y,mid.x-center.x);
                double sweep=fmod(b-a+2*AS_PI,2*AS_PI);
                if(fmod(m-a+2*AS_PI,2*AS_PI)>sweep){a=b;sweep=2*AS_PI-sweep;}
                error=as_add_arc(section,center,edge->radius*scale,a,a+sweep,&id);
            }
        }
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError imitation_build_flat_plan(const ImitationFlatPlan *plan,ProMdl *model,PartText *out)
{
    ProMdlName name;
    const char *stage="blank part for inferred geometry";
    ProError error=model_create_empty(1,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,model);
    const int axis=plan->axis;
    const wchar_t *basis=NULL,*orient=NULL;
    if(!error){
        basis=imitation_template_plane(*model,axis);
        orient=imitation_template_plane(*model,(axis+1)%3);
        if(!basis||!orient)error=PRO_TK_E_NOT_FOUND;
    }
    const wchar_t *plane=basis;
    if(!error&&fabs(plan->bounds[1][axis])>1e-7){
        error=imitation_offset(*model,name,out,L"PROFILE_LEVEL",basis,axis,plan->bounds[1][axis]);
        plane=L"PLANE:PROFILE_LEVEL";
    }
    ProMatrix frame;
    if(!error)error=smx_custom_sketch(*model,L"INFERRED_PROFILE",plane,orient,imitation_draw_flat_plan,(void *)plan,frame);
    if(!error)error=imitation_operation(*model,out,1,L"CREO_OP SOLID_EXTRUDE %ls INFERRED_EXTRUSION INFERRED_PROFILE ADD BLIND %.12g %ls",
        name,plan->thickness,frame[2][axis]>0?L"POS":L"NEG");
    if(!error)part_text_add(out,L"几何推断：等截面板件，厚度 %.8g mm，%d 个轮廓（含 %d 个内孔或窗口），已创建可编辑草绘和原生拉伸。\n",
        plan->thickness,plan->loops,plan->loops-1);
    return error;
}

static ProError imitation_neutral_load(ProPath path,ProMdl *model,PartText *out)
{
    static LONG serial;
    const wchar_t *suffix=wcsrchr(path,L'.');
    if(!suffix||!dv_file_nonempty(path))return PRO_TK_CANT_OPEN;
    ProIntfImportType type;
    if(!_wcsicmp(suffix,L".step")||!_wcsicmp(suffix,L".stp"))type=PRO_INTF_IMPORT_STEP;
    else if(!_wcsicmp(suffix,L".iges")||!_wcsicmp(suffix,L".igs"))type=PRO_INTF_IMPORT_IGES;
    else return PRO_TK_BAD_INPUTS;
    ProMdlName name;
    swprintf_s(name,_countof(name),L"bwtl_input_%08lx_%08lx",GetCurrentProcessId(),InterlockedIncrement(&serial));
    DeliveryOutput work={0};
    ProPath input;
    ProError error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error&&swprintf_s(input,_countof(input),L"%ls\\input%ls",work.stage,suffix)<0)error=PRO_TK_BAD_INPUTS;
    if(!error&&!CopyFileW(path,input,TRUE))error=PRO_TK_CANT_OPEN;
    if(!error)error=ProIntfimportModelWithOptionsMdlnameCreate(input,NULL,type,PRO_MDL_PART,
        PRO_IMPORTREP_MASTER,name,NULL,NULL,model);
    error=fab_discard(&work,error,out);
    if(error&&*model){ProMdlErase(*model);*model=NULL;}
    return error;
}
