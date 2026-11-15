typedef struct {
    double origin[3];
    double width,half_flat,depth,thickness,radius,ear_half,ear_tangent,ear_end,corner;
    double window[2][2],hole_center[2][2],hole_radius;
    ProSurface top;
} ImitationGeometry;

static ProError imitation_active_body(ProMdl model,ProSolidBody *result){
    ProSolidBody *bodies=NULL;
    int count=0,active=0;
    ProError error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        ProBoolean construction;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(error||state!=PRO_BODY_STATE_ACTIVE)continue;
        error=ProSolidBodyIsConstruction(&bodies[i],&construction);
        if(error||construction)continue;
        if(result)*result=bodies[i];
        active++;
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    return error?error:active==1?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

typedef struct { ProSurface face; double range[2][2]; int inner,edges; } ImitationWindow;

static ProError imitation_window_edge(ProEdge edge,ProError status,ProAppData context){
    if(status)return status;
    ImitationWindow *window=context;
    ProEnttype type;
    ProVector a,b;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(!error)error=ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL);
    if(!error)error=ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL);
    if(error)return error;
    if(type!=PRO_ENT_LINE||(fabs(a[0]-b[0])>0.0001&&fabs(a[1]-b[1])>0.0001))return PRO_TK_INVALID_TYPE;
    for(int i=0;i<2;i++){
        window->range[0][i]=fmin(window->range[0][i],fmin(a[i],b[i]));
        window->range[1][i]=fmax(window->range[1][i],fmax(a[i],b[i]));
    }
    window->edges++;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_window_loop(ProContour loop,ProError status,ProAppData context){
    if(status)return status;
    ImitationWindow *window=context;
    ProContourTraversal side;
    ProError error=ProContourTraversalGet(loop,&side);
    if(error||side==PRO_CONTOUR_TRAV_EXTERNAL)return error;
    if(++window->inner!=1)return PRO_TK_INVALID_TYPE;
    return ProContourEdgeVisit(window->face,loop,imitation_window_edge,NULL,window);
}

static int imitation_near(double a,double b){return fabs(a-b)<0.002;}

static ProError imitation_measure(ProMdl source,ImitationGeometry *m){
    ProSolidBody body;
    ProOutline box;
    SmxGeometry *geometry=calloc(1,sizeof(*geometry));
    if(!geometry)return PRO_TK_OUT_OF_MEMORY;
    ProError error=imitation_active_body(source,&body);
    if(!error)error=ProSolidBodyOutlineGet(&body,box);
    if(!error)error=smx_geometry(source,geometry);
    if(error)goto done;
    if(!imitation_near(geometry->mm,1)){error=PRO_TK_BAD_INPUTS;goto done;}
    wchar_t details[1024]=L"";
    PartText gauge={details,_countof(details),0};
    m->thickness=smx_gauge(geometry,geometry->body,&gauge);
    if(m->thickness<0.1||m->thickness>20){error=PRO_TK_INVALID_TYPE;goto done;}
    m->origin[0]=(box[1][0]+box[0][0])/2;
    m->origin[1]=(box[1][1]+box[0][1])/2;
    m->origin[2]=box[1][2]-m->thickness;
    m->width=box[1][0]-box[0][0];
    m->depth=m->origin[2]-box[0][2];
    m->ear_end=box[1][1]-m->origin[1];
    double top_area=0;
    for(int i=0;i<geometry->count;i++){
        const SmxFace *face=&geometry->faces[i];
        if(face->type!=PRO_SRF_PLANE||face->area<=top_area)continue;
        ProGeomitem item;
        ProVector point,normal;
        if(!ProSurfaceToGeomitem((ProSolid)source,face->surface,&item)&&!fd_plane_data(&item,point,normal)&&
           normal[2]>0.99999&&imitation_near(point[2],box[1][2])){
            top_area=face->area;
            m->top=face->surface;
        }
    }
    if(!m->top){error=PRO_TK_INVALID_TYPE;goto done;}
    ProVector direction={0,1,0},low,high;
    error=ProSurfaceExtremesEval(m->top,direction,low,high);
    if(error)goto done;
    m->half_flat=(high[1]-low[1])/2;
    if(!imitation_near((high[1]+low[1])/2,m->origin[1])){error=PRO_TK_INVALID_TYPE;goto done;}
    ImitationWindow window={m->top,{{INFINITY,INFINITY},{-INFINITY,-INFINITY}},0,0};
    error=ProSurfaceContourVisit(m->top,imitation_window_loop,NULL,&window);
    if(error)goto done;
    if(window.inner!=1||window.edges!=4){error=PRO_TK_INVALID_TYPE;goto done;}
    for(int i=0;i<2;i++)for(int j=0;j<2;j++)m->window[i][j]=window.range[i][j]-m->origin[j];

    double outer_y[2]={0},ear_y[2]={0},ear_depth[2]={0},ear_width[2]={0},hole_angle[2]={0};
    double inner_r=INFINITY;
    int main_pair[2]={0},ear_pair[2]={0},corners=0;
    for(int i=0;i<geometry->count;i++){
        const SmxFace *f=&geometry->faces[i];
        if(f->type!=PRO_SRF_PLANE&&f->type!=PRO_SRF_CYL){error=PRO_TK_INVALID_TYPE;goto done;}
        if(f->type!=PRO_SRF_CYL)continue;
        double x=f->point[0]-m->origin[0],y=f->point[1]-m->origin[1],z=f->point[2]-m->origin[2];
        int side=y>0;
        if(fabs(f->axis[0])>0.99999&&imitation_near(f->angle,90)){
            if(imitation_near(f->length,m->width)){
                outer_y[side]=fabs(y);
                main_pair[side]++;
                inner_r=fmin(inner_r,f->radius);
            }else{
                ear_y[side]=fabs(y);
                ear_depth[side]=z;
                ear_width[side]=f->length;
                ear_pair[side]++;
            }
        }else if(fabs(f->axis[2])>0.99999&&imitation_near(f->length,m->thickness)){
            if(imitation_near(f->angle,90)){
                if(corners&&!imitation_near(m->corner,f->radius)){error=PRO_TK_INVALID_TYPE;goto done;}
                m->corner=f->radius;
                corners++;
            }else if(f->angle>100&&f->angle<=360.002){
                if(m->hole_radius&&!imitation_near(m->hole_radius,f->radius)){error=PRO_TK_INVALID_TYPE;goto done;}
                if(hole_angle[side]&&(!imitation_near(m->hole_center[side][0],x)||!imitation_near(m->hole_center[side][1],y))){
                    error=PRO_TK_INVALID_TYPE;goto done;
                }
                m->hole_radius=f->radius;
                m->hole_center[side][0]=x;
                m->hole_center[side][1]=y;
                hole_angle[side]+=f->angle;
            }else{error=PRO_TK_INVALID_TYPE;goto done;}
        }else{error=PRO_TK_INVALID_TYPE;goto done;}
    }
    m->radius=inner_r;
    m->ear_half=ear_width[0]/2;
    m->ear_tangent=ear_y[0];
    for(int i=0;i<2;i++){
        if(main_pair[i]!=2||ear_pair[i]!=2||!imitation_near(outer_y[i],m->half_flat)||
           !imitation_near(ear_y[i],m->half_flat+2*m->radius+m->thickness)||
           !imitation_near(ear_width[i],2*m->ear_half)||
           !imitation_near(ear_depth[i],-m->depth+m->radius+m->thickness)||
           !imitation_near(hole_angle[i],360)){
            error=PRO_TK_INVALID_TYPE;goto done;
        }
    }
    if(corners!=4||m->radius<0.05||m->radius>=m->depth||m->ear_half>=m->width/2||
       m->corner>=m->ear_half||m->ear_end-m->corner<=m->ear_tangent||
       !imitation_near(top_area,m->width*2*m->half_flat-
           (m->window[1][0]-m->window[0][0])*(m->window[1][1]-m->window[0][1])))
        error=PRO_TK_INVALID_TYPE;
done:
    free(geometry);
    return error;
}
