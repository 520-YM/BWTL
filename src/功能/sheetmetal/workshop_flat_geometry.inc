typedef struct { int duplicates,zero,loose,branches,crossings,islands; } SmyFlatIssues;

static int smy_zero_edge(const FabEntity *edge){
    return edge->type==1&&fab_close_xy(edge->a,edge->b,FAB_JOIN);
}

static int smy_endpoint_count(const FabDxf *d,int source,const double *point){
    const FabEntity *edge=&d->entities[source];
    int count=0;
    for(int i=0;i<d->count;i++){
        const FabEntity *other=&d->entities[i];
        if(i==source||other->layer!=edge->layer||other->type==3||smy_zero_edge(other))continue;
        count+=fab_close_xy(point,other->a,FAB_JOIN)+fab_close_xy(point,other->b,FAB_JOIN);
    }
    return count;
}

static int smy_inside_flat(const FabDxf *d,double x,double y,int layer,int skip){
    int crossings=0;
    for(int attempt=0;attempt<d->count*2+1;attempt++){
        int ties=0;
        for(int i=0;i<d->count;i++){
            const FabEntity *edge=&d->entities[i];
            if(edge->layer!=layer||i==skip)continue;
            if(edge->type!=3&&(fabs(y-edge->a[1])<1e-9||fabs(y-edge->b[1])<1e-9))ties=1;
            if(edge->type!=1&&fabs(fabs(y-edge->center[1])-edge->radius)<1e-9)ties=1;
        }
        if(!ties)break;
        y+=1e-8;
    }
    for(int i=0;i<d->count;i++){
        const FabEntity *edge=&d->entities[i];
        if(edge->layer!=layer||i==skip)continue;
        if(edge->type==1){
            if((edge->a[1]>y)==(edge->b[1]>y))continue;
            double where=edge->a[0]+(y-edge->a[1])*(edge->b[0]-edge->a[0])/(edge->b[1]-edge->a[1]);
            if(where>x)crossings++;
        }else{
            double dy=y-edge->center[1],disc=edge->radius*edge->radius-dy*dy;
            if(disc<=0)continue;
            for(int sign=-1;sign<=1;sign+=2){
                double dx=sign*sqrt(disc);
                if(edge->center[0]+dx>x&&smx_arc_contains(edge,as_point(dx,dy)))crossings++;
            }
        }
    }
    return crossings%2;
}

static SmyFlatIssues smy_flat_issues(const FabDxf *d,PartText *out){
    SmyFlatIssues issues={0};
    for(int i=0;i<d->count;i++){
        const FabEntity *a=&d->entities[i];
        if(smy_zero_edge(a)){issues.zero++;continue;}
        for(int j=i+1;j<d->count;j++){
            const FabEntity *b=&d->entities[j];
            if(smy_zero_edge(b))continue;
            if(fab_same_entity(a,b,FAB_JOIN)){issues.duplicates++;continue;}
            if(a->layer==3||b->layer==3)continue;
            int joined=a->layer==b->layer&&a->type!=3&&b->type!=3&&
                (fab_close_xy(a->a,b->a,FAB_JOIN)||fab_close_xy(a->a,b->b,FAB_JOIN)||
                 fab_close_xy(a->b,b->a,FAB_JOIN)||fab_close_xy(a->b,b->b,FAB_JOIN));
            if(!joined&&smx_curve_gap(a,b)<=FAB_JOIN){
                issues.crossings++;
                if(out&&issues.crossings<=20)part_text_add(out,L"图元 %d 与 %d 接触、相交或部分重合。\n",i+1,j+1);
            }
        }
        if(a->layer==3)continue;
        if(a->type!=3){
            int ca=smy_endpoint_count(d,i,a->a),cb=smy_endpoint_count(d,i,a->b);
            issues.loose+=(ca==0)+(cb==0);
            issues.branches+=(ca>1)+(cb>1);
        }
        if(a->layer==2){
            double p[2];
            if(a->type==3)fab_arc_point(a,17,p);
            else if(a->type==2)fab_arc_point(a,a->start+a->sweep/2,p);
            else{p[0]=(a->a[0]+a->b[0])/2;p[1]=(a->a[1]+a->b[1])/2;}
            if(!smy_inside_flat(d,p[0],p[1],1,-1))issues.islands++;
        }
    }
    if(out)part_text_add(out,L"图元 %d，外轮廓 %d，内轮廓 %d，折弯线 %d；重复 %d，短到零 %d，悬空端点 %d，分叉 %d，非相邻交点 %d，外轮廓外的内轮廓图元 %d。\n",
        d->count,d->outline,d->holes,d->bends,issues.duplicates,issues.zero,issues.loose,issues.branches,issues.crossings,issues.islands);
    return issues;
}

static int smy_flat_clean_geometry(FabDxf *d,double tolerance,int *removed,int *joins){
    int count=0;
    for(int i=0;i<d->count;i++){
        const FabEntity *edge=&d->entities[i];
        int duplicate=0;
        for(int j=0;j<count;j++)if(fab_same_entity(edge,&d->entities[j],FAB_JOIN)){duplicate=1;break;}
        if(!duplicate&&!smy_zero_edge(edge))d->entities[count++]=*edge;
        else (*removed)++;
    }
    d->count=count;
    for(int i=0;i<d->count;i++){
        FabEntity *edge=&d->entities[i];
        if(edge->layer==3||edge->type!=1)continue;
        double *points[]={edge->a,edge->b};
        for(int k=0;k<2;k++){
            double *point=points[k];
            if(smy_endpoint_count(d,i,point))continue;
            double *match=NULL;
            int matches=0;
            for(int j=0;j<d->count;j++){
                FabEntity *other=&d->entities[j];
                if(j==i||other->layer!=edge->layer||other->type!=1)continue;
                double *candidates[]={other->a,other->b};
                for(int q=0;q<2;q++)if(fab_close_xy(candidates[q],point,tolerance)&&!smy_endpoint_count(d,j,candidates[q])){
                    match=candidates[q];matches++;
                }
            }
            if(matches>1)return 0;
            if(matches==1){
                for(int v=0;v<2;v++)point[v]=match[v]=(point[v]+match[v])/2;
                (*joins)++;
            }
        }
    }
    SmyFlatIssues issues=smy_flat_issues(d,NULL);
    return !issues.duplicates&&!issues.zero&&!issues.loose&&!issues.branches&&!issues.crossings&&!issues.islands;
}

static void smy_flat_xy(double p[2],double x,double y,double cos_a,double sin_a){
    double dx=p[0]-x,dy=p[1]-y;
    p[0]=dx*cos_a+dy*sin_a;
    p[1]=dy*cos_a-dx*sin_a;
}

static void smy_flat_frame(FabDxf *d,double x,double y,double angle){
    double a=angle*AS_PI/180;
    for(int i=0;i<d->count;i++){
        FabEntity *e=&d->entities[i];
        if(e->type!=3){
            smy_flat_xy(e->a,x,y,cos(a),sin(a));
            smy_flat_xy(e->b,x,y,cos(a),sin(a));
        }
        if(e->type!=1){
            smy_flat_xy(e->center,x,y,cos(a),sin(a));
            e->start=fab_degrees(e->start-angle);
        }
    }
}

static ProError smy_flat_write(const wchar_t *path,const FabDxf *d){
    FILE *file=_wfopen(path,L"wt");
    if(!file)return PRO_TK_CANT_WRITE;
    fprintf(file,"0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n");
    const char *layers[]={"OUTLINE","HOLES","BEND"};
    for(int i=0;i<d->count;i++){
        const FabEntity *e=&d->entities[i];
        const char *layer=layers[e->layer-1];
        if(e->type==1)sh_dxf_line(file,layer,e->a,e->b);
        else{
            fprintf(file,"0\n%s\n8\n%s\n10\n%.15g\n20\n%.15g\n30\n0\n40\n%.15g\n",
                e->type==2?"ARC":"CIRCLE",layer,e->center[0],e->center[1],e->radius);
            if(e->type==2)fprintf(file,"50\n%.15g\n51\n%.15g\n",e->start,fab_degrees(e->start+e->sweep));
        }
    }
    fprintf(file,"0\nENDSEC\n0\nEOF\n");
    return fab_close(file,PRO_TK_NO_ERROR);
}
