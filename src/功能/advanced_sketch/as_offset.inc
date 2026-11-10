static AsLine as_shift_line(AsLine line,double distance){
    AsPoint shift=as_mul(as_left(as_direction(line)),distance/as_dist(line.a,line.b));
    AsLine result={as_plus(line.a,shift),as_plus(line.b,shift)};
    return result;
}

static ProError as_offset_one(ProSection section,int id,double distance,int *created){
    Pro2dEntdef *entity=NULL;
    ProError e=as_regular(section,id,&entity);
    if(e)return e;
    if(entity->type==PRO_2D_LINE){
        Pro2dLinedef *def=(Pro2dLinedef *)entity;
        AsLine line={as_p(def->end1),as_p(def->end2)};
        if(as_dist(line.a,line.b)<=1e-9)return PRO_TK_BAD_INPUTS;
        line=as_shift_line(line,distance);
        return as_add_line(section,line.a,line.b,created);
    }
    if(entity->type==PRO_2D_CIRCLE){
        Pro2dCircledef circle=*(Pro2dCircledef *)entity;
        circle.radius+=distance;
        if(circle.radius<=0)return PRO_TK_BAD_INPUTS;
        return ProSectionEntityAdd(section,(Pro2dEntdef *)&circle,created);
    }
    if(entity->type==PRO_2D_ARC){
        Pro2dArcdef arc=*(Pro2dArcdef *)entity;
        arc.radius+=distance;
        if(arc.radius<=0)return PRO_TK_BAD_INPUTS;
        return ProSectionEntityAdd(section,(Pro2dEntdef *)&arc,created);
    }
    return PRO_TK_UNSUPPORTED;
}

typedef struct {AsLine ends;AsPoint tangent[2],center;double radius,start,span;int reversed;} AsOffsetPiece;

static void as_reverse_offset_piece(AsOffsetPiece *piece){
    AsPoint original=piece->tangent[0];
    piece->ends=as_reverse(piece->ends);
    piece->tangent[0]=as_mul(piece->tangent[1],-1);
    piece->tangent[1]=as_mul(original,-1);
    piece->reversed=1;
}

static ProError as_offset_tangent_loop(ProSection section,const PartCommand *cmd,double distance,double tol,AsEdit *edit){
    AsOffsetPiece path[AS_LIMIT]={0};
    int i,n=edit->count,orientation=0;
    double total=0;
    ProError e;
    for(i=0;i<n;i++){
        AsOffsetPiece *p=&path[i];
        int id=as_id(cmd,4+i);
        Pro2dEntdef *entity=NULL;
        if((e=as_regular(section,id,&entity))||
           (e=as_point_at(section,id,entity,0,&p->ends.a))||(e=as_point_at(section,id,entity,1,&p->ends.b)))return e;
        if(entity->type==PRO_2D_LINE){
            double length=as_dist(p->ends.a,p->ends.b);
            if(length<=tol)return PRO_TK_BAD_INPUTS;
            p->tangent[0]=p->tangent[1]=as_mul(as_direction(p->ends),1/length);
        }else if(entity->type==PRO_2D_ARC){
            Pro2dArcdef *arc=(Pro2dArcdef *)entity;
            p->center=as_p(arc->center);p->radius=arc->radius;p->start=arc->start_angle;p->span=arc->end_angle-arc->start_angle;
            if(p->span<0)p->span+=2*AS_PI;
            p->tangent[0]=as_left(as_ray(p->start));p->tangent[1]=as_left(as_ray(p->start+p->span));
        }else return PRO_TK_UNSUPPORTED;
    }
    if(!as_near(path[0].ends.b,path[1].ends.a,tol)&&!as_near(path[0].ends.b,path[1].ends.b,tol))as_reverse_offset_piece(&path[0]);
    for(i=1;i<n;i++)if(as_near(path[i-1].ends.b,path[i].ends.b,tol))as_reverse_offset_piece(&path[i]);
    for(i=0;i<n;i++){
        AsOffsetPiece *p=&path[i],*next=&path[(i+1)%n];
        if(!as_near(p->ends.b,next->ends.a,tol)||as_dot2(p->tangent[1],next->tangent[0])<1-1e-8)return PRO_TK_UNSUPPORTED;
        if(p->radius>0){
            int sign=p->reversed?-1:1;
            if((orientation&&orientation!=sign)||p->radius+distance<=tol)return PRO_TK_BAD_INPUTS;
            orientation=sign;total+=p->span;
        }
    }
    if(fabs(total-2*AS_PI)>1e-6)return PRO_TK_UNSUPPORTED;
    for(i=0;i<n;i++){
        AsOffsetPiece *p=&path[i];
        if(p->radius>0)e=as_add_arc(section,p->center,p->radius+distance,p->start,p->start+p->span,&edit->ids[i]);
        else{
            AsLine shifted=as_shift_line(p->ends,-orientation*distance);
            e=as_add_line(section,shifted.a,shifted.b,&edit->ids[i]);
        }
        if(e)return e;
    }
    return PRO_TK_NO_ERROR;
}

static ProError as_offset(ProSection section,const PartCommand *cmd,double scale,AsEdit *edit){
    const int n=cmd->count-4;
    const double distance=as_arg(cmd,3)*scale,tol=1e-5*scale;
    AsLine edges[AS_LIMIT],shifted[AS_LIMIT];
    AsPoint vertices[AS_LIMIT];
    double turn,orientation=0,t,u;
    int i,prev;
    ProError e;
    edit->count=n;
    if(n==1)return as_offset_one(section,as_id(cmd,4),distance,edit->ids);
    if(n<3)return PRO_TK_BAD_INPUTS;
    for(i=0;i<n;i++){
        e=as_line_get(section,as_id(cmd,4+i),&edges[i]);
        if(e==PRO_TK_INVALID_TYPE)return as_offset_tangent_loop(section,cmd,distance,tol,edit);
        if(e)return e;
        if(as_dist(edges[i].a,edges[i].b)<=tol)return PRO_TK_BAD_INPUTS;
    }
    if(!as_near(edges[0].b,edges[1].a,tol)&&!as_near(edges[0].b,edges[1].b,tol))edges[0]=as_reverse(edges[0]);
    for(i=1;i<n;i++){
        if(as_near(edges[i-1].b,edges[i].b,tol))edges[i]=as_reverse(edges[i]);
        if(!as_near(edges[i-1].b,edges[i].a,tol))return PRO_TK_BAD_INPUTS;
    }
    if(!as_near(edges[n-1].b,edges[0].a,tol))return PRO_TK_BAD_INPUTS;
    for(i=0;i<n;i++){
        turn=as_cross2(as_direction(edges[i]),as_direction(edges[(i+1)%n]));
        if(fabs(turn)<tol*tol||turn*orientation<0)return PRO_TK_UNSUPPORTED;
        orientation=turn;
    }
    /* Require all original vertices on the inner side of every edge. */
    for(i=0;i<n;i++)for(int j=0;j<n;j++)
        if(as_cross2(as_direction(edges[i]),as_minus(edges[j].a,edges[i].a))*copysign(1,orientation)<-tol*as_dist(edges[i].a,edges[i].b))return PRO_TK_UNSUPPORTED;
    for(i=0;i<n;i++)shifted[i]=as_shift_line(edges[i],-copysign(1,orientation)*distance);
    for(i=0;i<n;i++){
        prev=(i+n-1)%n;
        if(!as_line_intersect(shifted[prev],shifted[i],&t,&u))return PRO_TK_BAD_INPUTS;
        vertices[i]=as_plus(shifted[i].a,as_mul(as_direction(shifted[i]),u));
    }
    for(i=0;i<n;i++){
        AsPoint next=vertices[(i+1)%n];
        if(as_dot2(as_minus(next,vertices[i]),as_direction(edges[i]))<=tol*tol)return PRO_TK_BAD_INPUTS;
        if((e=as_add_line(section,vertices[i],next,&edit->ids[i])))return e;
    }
    return PRO_TK_NO_ERROR;
}
