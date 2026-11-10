typedef struct {int id,end;AsPoint point;} AsEnd;
typedef struct {AsEnd ends[AS_AUDIT_LIMIT*2];int gaps,branches,duplicates,crossings,curves,unsupported;ProSectionShape shape;} AsAudit;

static int as_pair_end(const AsEntity *a,AsPoint p,double tol){
    if(as_near(a->points[0],a->points[4],tol))return 0;
    return as_near(p,a->points[0],tol)||as_near(p,a->points[4],tol);
}

static int as_line_overlap(const AsEntity *a,const AsEntity *b,double tol){
    AsPoint da=as_minus(a->points[4],a->points[0]);
    double len=as_dist(a->points[0],a->points[4]),t[2];
    int i;
    if(len<=tol)return 0;
    da=as_mul(da,1/len);
    for(i=0;i<2;i++){
        AsPoint delta=as_minus(b->points[i*4],a->points[0]);
        if(fabs(as_cross2(da,delta))>tol)return 0;
        t[i]=as_dot2(da,delta);
    }
    return fmin(len,fmax(t[0],t[1]))-fmax(0,fmin(t[0],t[1]))>tol;
}

/* Toolkit also returns intersections of the unbounded supporting geometry. */
static int as_contains_point(ProSection section,const AsEntity *entity,AsPoint p,double tol){
    Pro2dEntdef *def=NULL;
    AsPoint sample;
    Pro2dPnt xy;
    double t;
    if(as_near(p,entity->points[0],tol)||as_near(p,entity->points[4],tol))return 1;
    if(entity->type==PRO_2D_LINE){
        AsPoint direction=as_minus(entity->points[4],entity->points[0]),delta=as_minus(p,entity->points[0]);
        double len=as_dist(entity->points[0],entity->points[4]);
        if(len<=tol)return 0;
        t=as_dot2(direction,delta)/(len*len);
        return t>=0&&t<=1&&fabs(as_cross2(direction,delta))<=tol*len;
    }
    if(ProSectionEntityGet(section,entity->id,&def))return 0;
    if(entity->type==PRO_2D_ARC){
        Pro2dArcdef *arc=(Pro2dArcdef *)def;
        AsPoint delta=as_minus(p,as_p(arc->center));
        double angle=atan2(delta.y,delta.x)-arc->start_angle,span=arc->end_angle-arc->start_angle;
        if(span<0)span+=2*AS_PI;
        angle=fmod(angle+4*AS_PI,2*AS_PI);
        return fabs(hypot(delta.x,delta.y)-arc->radius)<=tol&&angle<=span;
    }
    if(entity->type==PRO_2D_CIRCLE){
        Pro2dCircledef *circle=(Pro2dCircledef *)def;
        return fabs(as_dist(p,as_p(circle->center))-circle->radius)<=tol;
    }
    as_xy(xy,p);
    return !ProSectionEntityParamEval(section,entity->id,xy,&t)&&t>=0&&t<=1&&
        !as_point_at(section,entity->id,def,t,&sample)&&as_near(p,sample,tol);
}

static ProError as_audit(ProSection section,double scale,AsAudit *audit,PartText *details){
    AsSnapshot profile={0};
    AsEnd ends[AS_AUDIT_LIMIT*2];
    double tol=1e-5*scale;
    int i,j,k,end_count=0;
    ProError e;
    memset(audit,0,sizeof(*audit));
    if((e=as_snapshot(section,&profile)))return e;
    for(i=0;i<profile.count;i++){
        AsEntity *item=&profile.items[i];
        Pro2dEntdef *entity=NULL;
        e=as_regular(section,item->id,&entity);
        if(e==PRO_TK_INVALID_TYPE){item->sampled=0;continue;}
        if(e)return e;
        if(!as_profile_type(item->type)){
            if(item->type!=PRO_2D_CENTER_LINE&&item->type!=PRO_2D_POINT&&item->type!=PRO_2D_COORD_SYS)audit->unsupported++;
            item->sampled=0;continue;
        }
        audit->curves++;
        if(as_near(item->points[0],item->points[4],tol))continue;
        ends[end_count++]=(AsEnd){item->id,0,item->points[0]};
        ends[end_count++]=(AsEnd){item->id,1,item->points[4]};
    }
    for(i=0;i<end_count;i++){
        int links=0;
        for(j=0;j<end_count;j++)if(i!=j&&as_near(ends[i].point,ends[j].point,tol))links++;
        if(!links){
            audit->ends[audit->gaps++]=ends[i];
            if(details)part_text_add(details,L"断点 图元%d %ls (%.6g, %.6g) mm\n",ends[i].id,ends[i].end?L"终点":L"起点",ends[i].point.x/scale,ends[i].point.y/scale);
        }else if(links>1)audit->branches++;
    }
    for(i=0;i<profile.count;i++)if(profile.items[i].sampled)for(j=i+1;j<profile.count;j++)if(profile.items[j].sampled){
        const AsEntity *a=&profile.items[i],*b=&profile.items[j];
        Pro2dPnt *hits=NULL;
        int count=0;
        if(as_same_entity(a,b,tol)||(a->type==PRO_2D_LINE&&b->type==PRO_2D_LINE&&as_line_overlap(a,b,tol))){
            audit->duplicates++;
            if(details)part_text_add(details,L"重复或重叠 图元%d 与 %d\n",a->id,b->id);
            continue;
        }
        e=ProSectionEntityIntersectionGet(section,a->id,b->id,&hits);
        if(!e&&hits)e=ProArraySizeGet((ProArray)hits,&count);
        if(!e)for(k=0;k<count;k++){
            AsPoint p=as_p(hits[k]);
            if(as_pair_end(a,p,tol)&&as_pair_end(b,p,tol))continue;
            if(!as_contains_point(section,a,p,tol)||!as_contains_point(section,b,p,tol))continue;
            audit->crossings++;
            if(details)part_text_add(details,L"交点 图元%d 与 %d (%.6g, %.6g) mm\n",a->id,b->id,p.x/scale,p.y/scale);
        }
        if(hits)ProArrayFree((ProArray *)&hits);
        if(e&&e!=PRO_TK_E_NOT_FOUND&&e!=PRO_TK_NOT_EXIST)return e;
    }
    e=ProSectionShapeGet(section,&audit->shape);
    if(e)return e;
    if(audit->shape==PRO_SECSHAPE_INTERSECTING&&!audit->crossings&&!audit->duplicates){
        audit->crossings=1;
        if(details)part_text_add(details,L"Creo 检测到轮廓自交。单个曲线内部的交点未返回坐标。\n");
    }
    return PRO_TK_NO_ERROR;
}

static int as_closed(const AsAudit *a){
    return a->curves&&!a->gaps&&!a->branches&&!a->duplicates&&!a->crossings&&!a->unsupported&&
        (a->shape==PRO_SECSHAPE_1_CLOSED_LOOP||a->shape==PRO_SECSHAPE_MULTI_CLOSED_LOOPS);
}
