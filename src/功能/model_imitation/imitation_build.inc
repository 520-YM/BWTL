typedef struct { double a,b; } ImitationPoint;
typedef enum {
    IMITATION_MAIN, IMITATION_TAPER_LEFT, IMITATION_TAPER_RIGHT, IMITATION_EAR_CLEARANCE,
    IMITATION_EARS, IMITATION_EAR_CUTS, IMITATION_WINDOW
} ImitationShape;

typedef struct {
    const ImitationGeometry *geometry;
    ImitationShape shape;
    int a,b;
    double fixed;
    ProSection section;
    const double (*frame)[4];
    double scale;
} ImitationSketch;

static AsPoint imitation_project(const ImitationSketch *s,ImitationPoint p){
    ProVector point,delta;
    for(int i=0;i<3;i++)point[i]=(s->geometry->origin[i]+s->fixed)*s->scale;
    point[s->a]=(s->geometry->origin[s->a]+p.a)*s->scale;
    point[s->b]=(s->geometry->origin[s->b]+p.b)*s->scale;
    ad_sub(point,s->frame[3],delta);
    return as_point(ad_dot(delta,s->frame[0]),ad_dot(delta,s->frame[1]));
}

static ProError imitation_line(const ImitationSketch *s,ImitationPoint a,ImitationPoint b){
    int id;
    return as_add_line(s->section,imitation_project(s,a),imitation_project(s,b),&id);
}

static ProError imitation_arc(const ImitationSketch *s,ImitationPoint center,ImitationPoint a,ImitationPoint b,double r){
    int id;
    AsPoint c=imitation_project(s,center);
    AsPoint u=as_minus(imitation_project(s,a),c),v=as_minus(imitation_project(s,b),c);
    if(as_cross2(u,v)<0)u=v;
    double start=atan2(u.y,u.x);
    return as_add_arc(s->section,c,r*s->scale,start,start+AS_PI/2,&id);
}

static ProError imitation_polygon(const ImitationSketch *s,const ImitationPoint *p,int n){
    for(int i=0;i<n;i++){
        ProError error=imitation_line(s,p[i],p[(i+1)%n]);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError imitation_main_profile(const ImitationSketch *s){
    const ImitationGeometry *m=s->geometry;
    double w=m->half_flat,t=m->thickness,r=m->radius,d=m->depth;
    ProError error;
#define IMITATE_DRAW(call) do{error=(call);if(error)return error;}while(0)
    IMITATE_DRAW(imitation_line(s,(ImitationPoint){-w,t},(ImitationPoint){w,t}));
    IMITATE_DRAW(imitation_line(s,(ImitationPoint){-w,0},(ImitationPoint){w,0}));
    for(int sign=-1;sign<=1;sign+=2){
        double y=sign*w,inside=sign*(w+r),outside=sign*(w+r+t);
        ImitationPoint center={y,-r};
        IMITATE_DRAW(imitation_arc(s,center,(ImitationPoint){y,t},(ImitationPoint){outside,-r},r+t));
        IMITATE_DRAW(imitation_arc(s,center,(ImitationPoint){y,0},(ImitationPoint){inside,-r},r));
        IMITATE_DRAW(imitation_line(s,(ImitationPoint){outside,-r},(ImitationPoint){outside,-d}));
        IMITATE_DRAW(imitation_line(s,(ImitationPoint){inside,-r},(ImitationPoint){inside,-d}));
        IMITATE_DRAW(imitation_line(s,(ImitationPoint){inside,-d},(ImitationPoint){outside,-d}));
    }
    return PRO_TK_NO_ERROR;
}

static ProError imitation_ear_profile(const ImitationSketch *s){
    const ImitationGeometry *m=s->geometry;
    double w=m->half_flat,t=m->thickness,r=m->radius,d=m->depth,z=-d+r+t;
    double overlap=fmin(t,(d-2*r-t)/2);
    ProError error;
    for(int sign=-1;sign<=1;sign+=2){
        double inside=sign*(w+r),outside=sign*(w+r+t),y=sign*m->ear_tangent,end=sign*m->ear_end;
        ImitationPoint center={y,z};
        IMITATE_DRAW(imitation_arc(s,center,(ImitationPoint){inside,z},(ImitationPoint){y,-d},r+t));
        IMITATE_DRAW(imitation_arc(s,center,(ImitationPoint){outside,z},(ImitationPoint){y,-d+t},r));
        IMITATE_DRAW(imitation_line(s,(ImitationPoint){inside,z},(ImitationPoint){inside,z+overlap}));
        IMITATE_DRAW(imitation_line(s,(ImitationPoint){outside,z},(ImitationPoint){outside,z+overlap}));
        IMITATE_DRAW(imitation_line(s,(ImitationPoint){inside,z+overlap},(ImitationPoint){outside,z+overlap}));
        IMITATE_DRAW(imitation_line(s,(ImitationPoint){y,-d},(ImitationPoint){end,-d}));
        IMITATE_DRAW(imitation_line(s,(ImitationPoint){y,-d+t},(ImitationPoint){end,-d+t}));
        IMITATE_DRAW(imitation_line(s,(ImitationPoint){end,-d},(ImitationPoint){end,-d+t}));
    }
    return PRO_TK_NO_ERROR;
}

static ProError imitation_ear_cuts(const ImitationSketch *s){
    const ImitationGeometry *m=s->geometry;
    double e=m->ear_end,w=m->ear_half,r=m->corner,margin=fmax(1,m->thickness*3);
    ProError error;
    for(int side=0;side<2;side++){
        int y=side?1:-1;
        AsPoint c=imitation_project(s,(ImitationPoint){m->hole_center[side][0],m->hole_center[side][1]});
        Pro2dCircledef circle={PRO_2D_CIRCLE,{c.x,c.y},m->hole_radius*s->scale};
        int id;
        IMITATE_DRAW(ProSectionEntityAdd(s->section,(Pro2dEntdef *)&circle,&id));
        for(int x=-1;x<=1;x+=2){
            ImitationPoint p[]={{w*x,(e-r)*y},{(w+margin)*x,(e-r)*y},{(w+margin)*x,(e+margin)*y},
                {(w-r)*x,(e+margin)*y},{(w-r)*x,e*y}};
            for(int i=0;i<4;i++)IMITATE_DRAW(imitation_line(s,p[i],p[i+1]));
            IMITATE_DRAW(imitation_arc(s,(ImitationPoint){(w-r)*x,(e-r)*y},p[0],p[4],r));
        }
    }
    return PRO_TK_NO_ERROR;
}
#undef IMITATE_DRAW

static ProError imitation_draw(ProSection section,const ProMatrix frame,double scale,void *context){
    ImitationSketch s=*(const ImitationSketch *)context;
    s.section=section;s.frame=frame;s.scale=scale;
    const ImitationGeometry *m=s.geometry;
    int fixed=3-s.a-s.b;
    if(!imitation_near(frame[3][fixed],(m->origin[fixed]+s.fixed)*scale)||fabs(frame[2][fixed])<0.99999)
        return PRO_TK_BAD_CONTEXT;
    if(s.shape==IMITATION_MAIN)return imitation_main_profile(&s);
    if(s.shape==IMITATION_EARS)return imitation_ear_profile(&s);
    if(s.shape==IMITATION_EAR_CUTS)return imitation_ear_cuts(&s);
    if(s.shape==IMITATION_WINDOW){
        const ImitationPoint p[]={{m->window[0][0],m->window[0][1]},{m->window[1][0],m->window[0][1]},
            {m->window[1][0],m->window[1][1]},{m->window[0][0],m->window[1][1]}};
        return imitation_polygon(&s,p,_countof(p));
    }
    double d=m->depth,w=m->ear_half,t=m->thickness,r=m->radius,margin=fmax(2,t*5);
    if(s.shape==IMITATION_EAR_CLEARANCE){
        const ImitationPoint p[]={{-w,-d+r+t},{w,-d+r+t},{w,-d-margin},{-w,-d-margin}};
        return imitation_polygon(&s,p,_countof(p));
    }
    double edge=m->width/2,extended=edge+margin;
    double z=-r+margin*(d-r)/(edge-w);
    int sign=s.shape==IMITATION_TAPER_LEFT?-1:1;
    const ImitationPoint p[]={{sign*extended,z},{sign*w,-d},{sign*w,-d-margin},{sign*extended,-d-margin}};
    return imitation_polygon(&s,p,_countof(p));
}

static ProError imitation_operation(ProMdl model,PartText *out,int first,const wchar_t *format,...){
    wchar_t command[1200],report[PART_TEXT_MAX]=L"";
    PartCommand parsed;
    va_list args;
    va_start(args,format);
    int size=vswprintf_s(command,_countof(command),format,args);
    va_end(args);
    ProError error=PRO_TK_BAD_INPUTS;
    if(size>0&&part_command_parse(command,&parsed)==1)
        error=first?sd_create_on_body(&parsed,model,report,_countof(report),1):
            foundation_execute(&parsed,model,report,_countof(report));
    if(error)part_text_add(out,L"%ls\n",report[0]?report:L"原生操作参数未通过校验。");
    return error;
}

static ProError imitation_plane(ProMdl model,const wchar_t *token,ProVector point,ProVector normal){
    FdRef reference={0};
    ProError error=fd_ref_resolve(model,token,PRO_SURFACE,&reference);
    if(!error)error=fd_plane_data(&reference.item,point,normal);
    fd_ref_free(&reference);
    return error;
}

static const wchar_t *imitation_template_plane(ProMdl model,int axis){
    static const wchar_t *const names[]={L"PLANE:RIGHT",L"PLANE:TOP",L"PLANE:FRONT"};
    for(int i=0;i<_countof(names);i++){
        ProVector point,normal;
        if(!imitation_plane(model,names[i],point,normal)&&fabs(normal[axis])>0.99999)return names[i];
    }
    return NULL;
}

static ProError imitation_offset(ProMdl model,const wchar_t *name,PartText *out,const wchar_t *feature,const wchar_t *basis,int axis,double value){
    ProVector point,normal;
    if(!basis)basis=imitation_template_plane(model,axis);
    if(!basis)return PRO_TK_E_NOT_FOUND;
    ProError error=imitation_plane(model,basis,point,normal);
    if(error||fabs(normal[axis])<0.99999)return error?error:PRO_TK_BAD_CONTEXT;
    double offset=(value-point[axis])/normal[axis];
    if(fabs(offset)<0.0001)return PRO_TK_BAD_CONTEXT;
    return imitation_operation(model,out,0,L"CREO_OP DATUM_PLANE %ls %ls %ls %.12g",name,feature,basis,offset);
}

static ProError imitation_extrude(ProMdl model,const wchar_t *name,PartText *out,const wchar_t *feature,const wchar_t *profile,
    const wchar_t *plane,ImitationSketch s,int direction,int add,double depth){
    ProMatrix frame;
    const wchar_t *orient=imitation_template_plane(model,s.a);
    if(!orient)return PRO_TK_E_NOT_FOUND;
    ProError error=smx_custom_sketch(model,profile,plane,orient,imitation_draw,&s,frame);
    if(error)return error;
    int axis=3-s.a-s.b;
    const wchar_t *sense=frame[2][axis]*direction>0?L"NEG":L"POS";
    return imitation_operation(model,out,s.shape==IMITATION_MAIN,L"CREO_OP SOLID_EXTRUDE %ls %ls %ls %ls BLIND %.12g %ls",
        name,feature,profile,add?L"ADD":L"CUT",depth,sense);
}

static ProError imitation_find_plane(ProMdl model,int axis,double location,int sign,wchar_t token[32]){
    SmxGeometry *g=calloc(1,sizeof(*g));
    if(!g)return PRO_TK_OUT_OF_MEMORY;
    ProError error=smx_geometry(model,g);
    int id=0;
    double area=0;
    for(int i=0;!error&&i<g->count;i++){
        const SmxFace *face=&g->faces[i];
        if(face->type!=PRO_SRF_PLANE||face->area<=area)continue;
        ProGeomitem item;
        ProVector point,normal;
        if(ProSurfaceToGeomitem((ProSolid)model,face->surface,&item)||fd_plane_data(&item,point,normal))continue;
        if(sign*normal[axis]>0.99999&&imitation_near(point[axis],location)){id=face->id;area=face->area;}
    }
    free(g);
    if(error||!id)return error?error:PRO_TK_E_NOT_FOUND;
    swprintf_s(token,32,L"SURFACE:%d",id);
    return PRO_TK_NO_ERROR;
}

static ProError imitation_geometry_build(ProMdl model,const wchar_t *name,const ImitationGeometry *m,PartText *out){
    ProError error;
    double margin=fmax(3,m->thickness*5),back=-m->ear_end-margin,above=m->thickness+margin;
#define IMITATE_STEP(call) do{error=(call);if(error){trace(#call,error);return error;}}while(0)
    IMITATE_STEP(imitation_offset(model,name,out,L"START_X",NULL,0,m->origin[0]-m->width/2));
    IMITATE_STEP(imitation_offset(model,name,out,L"EAR_X",NULL,0,m->origin[0]-m->ear_half));
    IMITATE_STEP(imitation_offset(model,name,out,L"CUT_Y",NULL,1,m->origin[1]+back));
    IMITATE_STEP(imitation_offset(model,name,out,L"CUT_Z",NULL,2,m->origin[2]+above));
    const ImitationSketch side={m,IMITATION_MAIN,1,2,-m->width/2};
    const ImitationSketch left={m,IMITATION_TAPER_LEFT,0,2,back};
    const ImitationSketch right={m,IMITATION_TAPER_RIGHT,0,2,back};
    const ImitationSketch clear={m,IMITATION_EAR_CLEARANCE,0,2,back};
    const ImitationSketch ears={m,IMITATION_EARS,1,2,-m->ear_half};
    const ImitationSketch trim={m,IMITATION_EAR_CUTS,0,1,above};
    const ImitationSketch window={m,IMITATION_WINDOW,0,1,above};
    IMITATE_STEP(imitation_extrude(model,name,out,L"FOLDED_MAIN",L"MAIN_SECTION",L"PLANE:START_X",side,1,1,m->width));
    IMITATE_STEP(imitation_extrude(model,name,out,L"FLANGE_LEFT_TAPER",L"LEFT_TAPER_PROFILE",L"PLANE:CUT_Y",left,1,0,-2*back));
    IMITATE_STEP(imitation_extrude(model,name,out,L"FLANGE_RIGHT_TAPER",L"RIGHT_TAPER_PROFILE",L"PLANE:CUT_Y",right,1,0,-2*back));
    IMITATE_STEP(imitation_extrude(model,name,out,L"EAR_BEND_CLEARANCE",L"CLEAR_EARS_PROFILE",L"PLANE:CUT_Y",clear,1,0,-2*back));
    IMITATE_STEP(imitation_extrude(model,name,out,L"FOLDED_EARS",L"EAR_SECTION",L"PLANE:EAR_X",ears,1,1,2*m->ear_half));
    IMITATE_STEP(imitation_extrude(model,name,out,L"EAR_CORNERS_AND_HOLES",L"EAR_PROFILE",L"PLANE:CUT_Z",trim,-1,0,above+m->depth+margin));
    IMITATE_STEP(imitation_extrude(model,name,out,L"CENTER_WINDOW",L"WINDOW_PROFILE",L"PLANE:CUT_Z",window,-1,0,above+m->thickness));
    return PRO_TK_NO_ERROR;
#undef IMITATE_STEP
}
