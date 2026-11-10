typedef struct {
    AmFrame placement;
    ProVector inward;
    int face_id;
} PanelFrame;

static ProError pn_frame(FdTxn *tx,const PartCommand *cmd,PanelFrame *frame){
    FdRef face={0},csys={0};
    ProCsys geometry;
    ProGeomitemdata *raw=NULL;
    ProVector point,normal,offset;
    ProError error=md_solid_face(tx,cmd->arg[3],&face);
    if(!error)error=fd_plane_data(&face.item,point,normal);
    if(!error)error=fd_ref_resolve(tx->model,cmd->arg[4],PRO_CSYS,&csys);
    if(!error)error=ProGeomitemToCsys(&csys.item,&geometry);
    if(!error)error=ProCsysDataGet(geometry,&raw);
    if(!error){
        frame->placement.csys=*raw->data.p_csys_data;
        frame->face_id=face.item.id;
        ad_sub(frame->placement.csys.origin,point,offset);
        if(!ad_unit(normal)||fabs(ad_dot(offset,normal))>1e-5*tx->scale||
           ad_dot(normal,frame->placement.csys.z_vector)<1-1e-8)error=PRO_TK_BAD_CONTEXT;
        for(int i=0;i<3;i++)frame->inward[i]=-normal[i];
    }
    if(raw)ProGeomitemdataFree(&raw);
    fd_ref_free(&face);
    fd_ref_free(&csys);
    return error;
}

typedef struct {
    ProSection section;
    AsPoint origin,x_axis,y_axis;
    double scale;
} PanelSketch;

static AsPoint pn_project(const PanelSketch *sketch,double x,double y){
    return as_plus(sketch->origin,as_plus(as_mul(sketch->x_axis,x*sketch->scale),as_mul(sketch->y_axis,y*sketch->scale)));
}

static ProError pn_line(const PanelSketch *sketch,double x1,double y1,double x2,double y2){
    int id;
    return as_add_line(sketch->section,pn_project(sketch,x1,y1),pn_project(sketch,x2,y2),&id);
}

static ProError pn_arc(const PanelSketch *sketch,double x,double y,double radius,double start,double span){
    int id;
    if(as_cross2(sketch->x_axis,sketch->y_axis)<0)start+=span;
    AsPoint a=as_plus(as_mul(sketch->x_axis,cos(start)),as_mul(sketch->y_axis,sin(start)));
    start=atan2(a.y,a.x);
    return as_add_arc(sketch->section,pn_project(sketch,x,y),radius*sketch->scale,start,start+span,&id);
}

static ProError pn_rectangle(const PanelSketch *sketch,double x0,double x1,double y0,double y1,double radius){
    ProError error;
    if((error=pn_line(sketch,x0+radius,y0,x1-radius,y0))||
       (error=pn_line(sketch,x1,y0+radius,x1,y1-radius))||
       (error=pn_line(sketch,x1-radius,y1,x0+radius,y1))||
       (error=pn_line(sketch,x0,y1-radius,x0,y0+radius)))return error;
    if(!radius)return PRO_TK_NO_ERROR;
    if((error=pn_arc(sketch,x1-radius,y0+radius,radius,1.5*AS_PI,AS_PI/2))||
       (error=pn_arc(sketch,x1-radius,y1-radius,radius,0,AS_PI/2))||
       (error=pn_arc(sketch,x0+radius,y1-radius,radius,AS_PI/2,AS_PI/2)))return error;
    return pn_arc(sketch,x0+radius,y0+radius,radius,AS_PI,AS_PI/2);
}

#include "../fastener/fx_profile_draw.inc"
#include "../shop/mp_groove_profile.inc"

static ProError pn_draw(const PanelSketch *sketch,const PanelProfile *p){
    double x=p->x,y=p->y,r=p->width/2,h=p->height/2;
    ProError error;
    if(p->shape==PN_SHOP_GROOVE)return mp_groove_profile(sketch,p);
    if(p->shape>=PN_KEY)return fx_profile_draw(sketch,p);
    if(p->shape==PN_CIRCLE){
        AsPoint center=pn_project(sketch,x,y);
        int id;
        return model_circle(sketch->section,center.x,center.y,r*sketch->scale,&id);
    }
    if(p->shape==PN_FLAT){
        double angle=acos(p->detail/r),end_y=r*sin(angle);
        if((error=pn_arc(sketch,x,y,r,angle,2*(AS_PI-angle))))return error;
        return pn_line(sketch,x+p->detail,y-end_y,x+p->detail,y+end_y);
    }
    if(p->shape==PN_NOTCH){
        double angle=asin(h/r),join=r*cos(angle),end=r+p->detail;
        if((error=pn_arc(sketch,x,y,r,angle,2*(AS_PI-angle)))||
           (error=pn_line(sketch,x+join,y-h,x+end,y-h))||
           (error=pn_line(sketch,x+end,y-h,x+end,y+h)))return error;
        return pn_line(sketch,x+end,y+h,x+join,y+h);
    }
    if(p->shape==PN_EDGE_SLOT){
        double overhang=fmax(0.1,fmin(p->width,p->height)/4);
        return pn_rectangle(sketch,x-overhang,x+p->width,y-h,y+h,0);
    }
    return pn_rectangle(sketch,x-r,x+r,y-h,y+h,p->detail);
}

static ProError pn_sketch(ProMdl model,const PartCommand *cmd,const PanelFrame *panel,const wchar_t *name,
    const PanelProfile *profiles,int count,int sheet,ProMatrix frame){
    FdTxn tx={0};
    FdSketch sketch={0};
    ProVector offset;
    ProError error;
#define PN_SKETCH(call) do{if((error=(call)))goto done;}while(0)
    PN_SKETCH(sheet?sh_begin(&tx,model,name,PRO_FEAT_CURVE):fd_begin(&tx,model,name,PRO_FEAT_CURVE));
    PN_SKETCH(fd_sketch_setup(&tx,cmd->arg[3],cmd->arg[5]));
    PN_SKETCH(fd_create(&tx,1));
    sketch.feature=tx.feature;
    PN_SKETCH(ProFeatureElemtreeExtract(&sketch.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&sketch.tree));
    PN_SKETCH(fd_section_from_tree(sketch.tree,&sketch.section));
    PN_SKETCH(ProSectionLocationGet(sketch.section,frame));
    ad_sub(panel->placement.csys.origin,frame[3],offset);
    PanelSketch section={sketch.section,as_point(ad_dot(offset,frame[0]),ad_dot(offset,frame[1])),
        as_point(ad_dot(panel->placement.csys.x_vector,frame[0]),ad_dot(panel->placement.csys.x_vector,frame[1])),
        as_point(ad_dot(panel->placement.csys.y_vector,frame[0]),ad_dot(panel->placement.csys.y_vector,frame[1])),tx.scale};
    if(fabs(as_cross2(section.x_axis,section.y_axis))<1-1e-8){error=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<count;i++)PN_SKETCH(pn_draw(&section,&profiles[i]));
    PN_SKETCH(fd_section_solve(sketch.section,tx.scale));
    tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
    PN_SKETCH(ProFeatureWithoptionsRedefine(NULL,&sketch.feature,sketch.tree,tx.options,
        PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors));
    PN_SKETCH(fd_verify(&tx));
done:
    fd_sketch_free(&sketch);
    fd_cleanup(&tx,error);
    return error;
#undef PN_SKETCH
}

static ProError pn_cut(FdTxn *outer,const PanelFrame *panel,const wchar_t *name,const wchar_t *sketch,
    const ProMatrix frame,double depth,int sheet){
    if(!sheet)return md_extrude(outer,name,sketch,frame,panel->inward,depth,1);
    FdTxn tx={0};
    PartCommand cmd={0};
    swprintf_s(cmd.arg[3],PART_TOKEN_MAX,L"SKETCH:%ls",sketch);
    wcscpy_s(cmd.arg[4],PART_TOKEN_MAX,ad_dot(frame[2],panel->inward)<0?L"POS":L"NEG");
    ProError error=sh_begin(&tx,outer->model,name,PRO_FEAT_CUT);
    if(!error)error=sh_cut(&tx,&cmd);
    if(!error)error=part_regenerate(outer->model);
    if(!error)error=sh_verify(&tx,&cmd,SH_CUT);
    fd_cleanup(&tx,error);
    return error;
}
