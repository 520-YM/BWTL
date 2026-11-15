
#define SMX_FACE_LIMIT 256
typedef struct {
    ProSurface surface;
    int id,type,body,side,owner;
    double area,radius,angle,length;
    ProVector point,axis,samples[9];
    int sampled;
} SmxFace;
typedef struct {
    ProMdl model;
    int body,count;
    double mm;
    SmxFace faces[SMX_FACE_LIMIT];
} SmxGeometry;

static ProError smx_face_collect(ProSurface surface,ProError status,ProAppData data){
    SmxGeometry *list=data;
    if(status)return status;
    if(list->count==_countof(list->faces))return PRO_TK_OUT_OF_RANGE;
    SmxFace *face=&list->faces[list->count];
    ProSrftype type;
    ProGeomitem item;
    ProFeature owner;
    ProGeomitemdata *raw=NULL;
    ProError error=ProSurfaceTypeGet(surface,&type);
    if(!error)error=ProSurfaceToGeomitem((ProSolid)list->model,surface,&item);
    if(!error)error=ProGeomitemFeatureGet(&item,&owner);
    if(!error)error=ProSurfaceAreaEval(surface,&face->area);
    if(!error)error=ProSurfaceDataGet(surface,&raw);
    if(error)return error;
    face->surface=surface;face->id=item.id;face->type=type;face->body=list->body;face->owner=owner.id;
    face->area*=list->mm*list->mm;
    ProSmtSurfType side;
    if(!ProSmtSurfaceTypeGet((ProPart)list->model,surface,&side))face->side=side;
    const ProSurfacedata *info=raw->data.p_surface_data;
    if(type==PRO_SRF_PLANE||type==PRO_SRF_CYL){
        for(int i=0;i<3;i++)for(int j=0;j<3;j++){
            ProVector normal;
            ProError sample=qa_interior_sample(list->model,surface,info,i,j,3,face->samples[face->sampled],normal);
            if(sample==PRO_TK_E_NOT_FOUND)continue;
            if(sample){error=sample;goto done;}
            if(!face->sampled&&type==PRO_SRF_PLANE){
                memcpy(face->axis,normal,sizeof(face->axis));
                memcpy(face->point,face->samples[0],sizeof(face->point));
            }
            face->sampled++;
        }
    }
    if(type==PRO_SRF_CYL){
        const ProCylinderdata *cylinder=&info->srf_shape.cylinder;
        memcpy(face->axis,cylinder->e3,sizeof(face->axis));
        memcpy(face->point,cylinder->origin,sizeof(face->point));
        if(!ad_unit(face->axis)){error=PRO_TK_BAD_CONTEXT;goto done;}
        face->radius=cylinder->radius*list->mm;
        ProVector low,high,delta;
        if((error=ProSurfaceExtremesEval(surface,face->axis,low,high)))goto done;
        ad_sub(high,low,delta);
        face->length=fabs(ad_dot(delta,face->axis))*list->mm;
        if(face->radius>0&&face->length>0)face->angle=face->area/(face->radius*face->length)*180/AS_PI;
    }
    list->count++;
done:
    ProGeomitemdataFree(&raw);
    return error;
}

static ProError smx_geometry(ProMdl model,SmxGeometry *list){
    ProSolidBody *bodies=NULL;
    int count=0;
    list->model=model;
    ProError error=qa_mm_per_unit(model,&list->mm);
    if(!error)error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error&&bodies)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        ProBoolean construction;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(!error)error=ProSolidBodyIsConstruction(&bodies[i],&construction);
        if(!error&&state==PRO_BODY_STATE_ACTIVE&&!construction){
            list->body=bodies[i].id;
            error=ProSolidBodySurfaceVisit(&bodies[i],smx_face_collect,&list[0]);
            if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
        }
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    return error?error:list->count?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static int smx_planar_partner(const SmxGeometry *list,const SmxFace *a,const SmxFace *b,double *thickness){
    if(a->body!=b->body||a->type!=PRO_SRF_PLANE||b->type!=PRO_SRF_PLANE||
        !a->sampled||!b->sampled||ad_dot(a->axis,b->axis)>-0.99999)return 0;
    ProVector delta;
    ad_sub(b->point,a->point,delta);
    double depth=ad_dot(delta,a->axis);
    *thickness=fabs(depth)*list->mm;
    if(*thickness<0.01||*thickness>100||depth>=0)return 0;
    InspectionPlane plane={0};plane.handle=b->surface;
    int hits=0;
    for(int i=0;i<a->sampled;i++){
        ProVector target;
        cm_point(a->samples[i],a->axis,depth,target);
        hits+=in_on_plane(list->model,&plane,target);
    }
    return hits;
}

static double smx_gauge(const SmxGeometry *list,int body,PartText *out){
    double best_area=0,best=0;
    int paired=0;
    for(int i=0;i<list->count;i++){
        const SmxFace *a=&list->faces[i];
        if(a->body!=body||a->type!=PRO_SRF_PLANE)continue;
        double closest=INFINITY;
        int partner=-1,valid=0;
        for(int j=0;j<list->count;j++){
            double thickness;
            int hits=smx_planar_partner(list,a,&list->faces[j],&thickness);
            if(hits>=3&&hits*3>=a->sampled*2&&thickness<closest){
                closest=thickness;partner=j;valid=hits;
            }
        }
        if(partner>=0&&a->id<list->faces[partner].id){
            if(paired<32&&out)part_text_add(out,L"主体 %d，面 %d/%d：%.8g mm（%d/%d 采样落在对面）。\n",
                body,a->id,list->faces[partner].id,closest,valid,a->sampled);
            paired++;
            double area=fmin(a->area,list->faces[partner].area);
            if(area>best_area){best_area=area;best=closest;}
        }
    }
    return best;
}

static int smx_cyl_pair(const SmxGeometry *list,const SmxFace *a,const SmxFace *b,double gauge){
    if(a->type!=PRO_SRF_CYL||b->type!=PRO_SRF_CYL||a->body!=b->body||!a->sampled||!b->sampled||
       fabs(ad_dot(a->axis,b->axis))<0.99999||fabs(a->radius-b->radius)<0.009||
       fabs(a->angle-b->angle)>1||a->angle<1||a->angle>178)return 0;
    if(gauge>0&&fabs(fabs(a->radius-b->radius)-gauge)>0.005)return 0;
    ProVector delta,cross;
    ad_sub(b->point,a->point,delta);
    ad_cross(delta,a->axis,cross);
    if(sqrt(ad_dot(cross,cross))*list->mm>0.005||fabs(a->length-b->length)>0.02)return 0;
    ProVector radial;
    ad_sub(a->samples[a->sampled/2],a->point,radial);
    double axial=ad_dot(radial,a->axis);
    for(int i=0;i<3;i++)radial[i]-=axial*a->axis[i];
    if(!ad_unit(radial))return 0;
    ProVector corresponding;
    cm_point(a->samples[a->sampled/2],radial,(b->radius-a->radius)/list->mm,corresponding);
    InspectionPlane probe={0};probe.handle=b->surface;
    return in_on_plane(list->model,&probe,corresponding);
}

static ProError smx_geo_report(ProMdl model,int kind,PartText *out){
    SmxGeometry *list=calloc(1,sizeof(*list));
    if(!list)return PRO_TK_OUT_OF_MEMORY;
    ProError error=smx_geometry(model,list);
    if(error){free(list);return error;}
    int bodies[SMX_FACE_LIMIT],body_count=0;
    double thickness[SMX_FACE_LIMIT]={0};
    for(int i=0;i<list->count;i++){
        int b=0;
        while(b<body_count&&bodies[b]!=list->faces[i].body)b++;
        if(b==body_count)bodies[body_count++]=list->faces[i].body;
    }
    for(int i=0;i<body_count;i++)thickness[i]=smx_gauge(list,bodies[i],kind==SMX_GAUGE?out:NULL);
    double largest=0;
    int stock=-1;
    for(int i=0;i<body_count;i++)if(thickness[i]>0){
        double area=0;
        for(int j=0;j<list->count;j++)if(list->faces[j].body==bodies[i]&&list->faces[j].type==PRO_SRF_PLANE)
            area=fmax(area,list->faces[j].area);
        if(area>largest){largest=area;stock=i;}
    }
    if(kind==SMX_GAUGE||kind==SMX_ATTACH||kind==SMX_BODIES){
        for(int i=0;i<body_count;i++){
            part_text_add(out,L"主体 %d：",bodies[i]);
            if(thickness[i]>0)part_text_add(out,L"最大匹配平面区的候选厚度 %.8g mm",thickness[i]);
            else part_text_add(out,L"等厚平面未核实");
            if(kind==SMX_ATTACH)part_text_add(out,L"；%ls",i==stock?L"主板候选":L"其他主体，可能是独立板件或附着件");
            if(kind==SMX_BODIES){
                int drive=0;
                for(int j=0;j<list->count;j++)if(list->faces[j].body==bodies[i]&&list->faces[j].side==PRO_SMT_SURF_FACE&&
                    list->faces[j].type==PRO_SRF_PLANE)drive++;
                part_text_add(out,L"；%d 个平面驱动面候选，独立展开需要另行验证",drive);
            }
            part_text_add(out,L"。\n");
        }
    }else{
        int matched=0,alone=0;
        for(int i=0;i<list->count;i++){
            const SmxFace *a=&list->faces[i];
            if(a->type!=PRO_SRF_CYL)continue;
            int found=0,body=0;
            while(body<body_count&&bodies[body]!=a->body)body++;
            for(int j=0;j<list->count;j++)if(i!=j&&smx_cyl_pair(list,a,&list->faces[j],thickness[body])){
                found=1;
                if(i>j)continue;
                const SmxFace *b=&list->faces[j];
                matched++;
                if(matched<=42)part_text_add(out,L"面 %d/%d：%ls；候选内 R %.8g mm，角度 %.8g°，轴向长度 %.8g mm，半径差 %.8g mm。\n",
                    a->id,b->id,thickness[body]>0?L"同轴且与平面厚度吻合":L"同轴，板厚待核实",
                    fmin(a->radius,b->radius),a->angle,a->length,fabs(a->radius-b->radius));
            }
            if(!found){
                alone++;
                if(kind==SMX_CONFIDENCE&&alone<=42)part_text_add(out,L"面 %d：未找到满足条件的伴随圆柱面，保留为待判定圆柱/圆角。\n",a->id);
            }
        }
        part_text_add(out,L"候选折弯面配对 %d 对，未配对圆柱面 %d 个。识别以真实曲面和采样为依据。\n",matched,alone);
    }
    free(list);
    return PRO_TK_NO_ERROR;
}

static ProError smx_geometry_compare(ProMdl model,const PartCommand *cmd,PartText *out){
    ProMdl prior=NULL;
    int loaded=0;
    ProError error=ax_load_part(cmd->arg[2],&prior,&loaded);
    if(error)return error;
    ProOutline now,old;
    double mm,old_mm,tol=as_arg(cmd,3);
    error=qa_mm_per_unit(model,&mm);
    if(!error)error=qa_mm_per_unit(prior,&old_mm);
    if(!error)error=ProSolidOutlineGet((ProSolid)model,now);
    if(!error)error=ProSolidOutlineGet((ProSolid)prior,old);
    int outside=0;
    for(int i=0;!error&&i<3;i++){
        double current=(now[1][i]-now[0][i])*mm,previous=(old[1][i]-old[0][i])*old_mm;
        int fail=fabs(current-previous)>tol;
        outside+=fail;
        part_text_add(out,L"%lc 包络：旧 %.8g，新 %.8g，差 %+.8g mm，%ls。\n",L'X'+i,previous,current,current-previous,fail?L"超差":L"公差内");
    }
    SmxGeometry *first=calloc(1,sizeof(*first)),*second=calloc(1,sizeof(*second));
    if(!error&&(!first||!second))error=PRO_TK_OUT_OF_MEMORY;
    if(!error)error=smx_geometry(model,first);
    if(!error)error=smx_geometry(prior,second);
    if(!error){
        double a=smx_gauge(first,first->faces[0].body,NULL),b=smx_gauge(second,second->faces[0].body,NULL);
        if(a&&b)part_text_add(out,L"首主体主板候选厚度：旧 %.8g、新 %.8g mm，%ls。\n",b,a,fabs(a-b)>tol?L"超差":L"公差内");
        else part_text_add(out,L"首主体厚度未能完整配对。\n");
        int used[SMX_FACE_LIMIT]={0},unknown=0;
        for(int i=0;i<first->count;i++){
            const SmxFace *left=&first->faces[i];
            if(left->type!=PRO_SRF_CYL)continue;
            int match=-1,hits=0;
            ProVector center;
            for(int k=0;k<3;k++)center[k]=left->point[k]*mm;
            for(int j=0;j<second->count;j++){
                const SmxFace *right=&second->faces[j];
                if(used[j]||right->type!=PRO_SRF_CYL||fabs(ad_dot(left->axis,right->axis))<0.9999)continue;
                ProVector delta,cross;
                for(int k=0;k<3;k++)delta[k]=right->point[k]*old_mm-center[k];
                ad_cross(delta,left->axis,cross);
                if(sqrt(ad_dot(cross,cross))<=tol&&fabs(left->angle-right->angle)<=1){match=j;hits++;}
            }
            if(hits==1){
                used[match]=1;
                double d=left->radius-second->faces[match].radius;
                if(fabs(d)>tol)part_text_add(out,L"同轴圆柱面 %d/%d 半径差 %+.8g mm。\n",left->id,second->faces[match].id,d);
            }else unknown++;
        }
        part_text_add(out,L"包络超差 %d 个方向，当前有 %d 个圆柱面无法唯一配对；异形孔与接缝需另做几何复核。\n",outside,unknown);
    }
    free(first);free(second);
    if(loaded&&prior)ProMdlErase(prior);
    return error;
}
