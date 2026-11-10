typedef struct {int id,face,ref1,ref2,blind;double x,y,diameter,depth;} OrganizeHoleDefinition;
typedef struct {
    Revision view;
    OrganizeHoleDefinition rows[DV_BORE_MAX];
    int count,limited,skipped;
} OrganizeHoleDefinitions;

static ProError org_definition_visit(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    OrganizeHoleDefinitions *scan=data;
    ProFeattype type;
    ProFeatStatus state;
    ProError error=ProFeatureTypeGet(feature,&type);
    if(error||type!=PRO_FEAT_HOLE)return error;
    if((error=ProFeatureStatusGet(feature,&state)))return error;
    if(state!=PRO_FEAT_ACTIVE)return PRO_TK_NO_ERROR;
    ProName name;
    qa_feature_label(feature,name);
    RevisionHole hole;
    error=rv_hole_load(&scan->view,name,RV_STRAIGHT,0,0,&hole);
    if(error){scan->skipped++;return PRO_TK_NO_ERROR;}
    if(scan->count==_countof(scan->rows)){scan->limited=1;return PRO_TK_NO_ERROR;}
    scan->rows[scan->count++]=(OrganizeHoleDefinition){feature->id,hole.refs[0],hole.refs[1],hole.refs[2],hole.blind,hole.x,hole.y,hole.diameter,hole.depth};
    return PRO_TK_NO_ERROR;
}

static ProError org_overlap(const OrganizeView *view,const PartCommand *cmd,PartText *out){
    DeliveryBores *scan=NULL;
    ProError error=org_bores(view,&scan);
    if(error)return error;
    OrganizeHoleDefinitions definitions={0};
    definitions.view.model=view->model;
    definitions.view.mm=view->mm;
    error=ProSolidFeatVisit((ProSolid)view->model,org_definition_visit,NULL,&definitions);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(error){free(scan);return error;}
    int limit=(int)as_arg(cmd,3),duplicates=0,overlaps=0,coaxial=0,shown=0;
    double tol=as_arg(cmd,2)/view->mm;
    /* The feature definitions provide duplicate evidence even if Creo merges the resulting faces. */
    for(int i=0;i<definitions.count;i++)for(int j=0;j<i;j++){
        const OrganizeHoleDefinition *a=&definitions.rows[i],*b=&definitions.rows[j];
        if(a->face!=b->face||a->blind!=b->blind||fabs(a->diameter-b->diameter)>tol||
           (a->blind&&fabs(a->depth-b->depth)>tol))continue;
        if(a->ref1!=b->ref1||a->ref2!=b->ref2||hypot(a->x-b->x,a->y-b->y)>tol)continue;
        duplicates++;
        if(shown<limit){
            shown++;
            part_text_add(out,L"重复定义候选 ID:%d ↔ ID:%d，同入口、孔位及规格。\n",a->id,b->id);
        }
    }
    for(int i=0;i<scan->count;i++)for(int j=0;j<i;j++){
        const DeliveryBore *a=&scan->bores[i],*b=&scan->bores[j];
        if(ad_dot(a->axis,b->axis)<1-1e-8)continue;
        ProVector bl,bh,offset;
        cm_point(b->centerline,b->axis,b->low,bl);
        cm_point(b->centerline,b->axis,b->high,bh);
        if(fmin(a->high,ad_dot(bh,a->axis))-fmax(a->low,ad_dot(bl,a->axis))<=tol)continue;
        ad_sub(b->centerline,a->centerline,offset);
        double axial=ad_dot(offset,a->axis);
        for(int k=0;k<3;k++)offset[k]-=axial*a->axis[k];
        double distance=sqrt(ad_dot(offset,offset));
        if(distance<=tol){coaxial++;continue;}
        double intrusion=a->radius+b->radius-distance;
        if(intrusion<=tol)continue;
        overlaps++;
        if(shown<limit){
            shown++;
            part_text_add(out,L"重叠候选 SURFACE:%d ↔ SURFACE:%d，平行轴距 %.8g mm，圆轮廓投影重叠 %.8g mm。\n",
                a->faces[0],b->faces[0],distance*view->mm,intrusion*view->mm);
        }
    }
    part_text_add(out,L"重复定义候选 %d 对，圆柱孔段重叠候选 %d 对，同轴交叠孔段另计 %d 对。",duplicates,overlaps,coaxial);
    if(duplicates+overlaps>shown)part_text_add(out,L"已列出前 %d 对。",shown);
    if(scan->limited||definitions.limited)part_text_add(out,L"扫描达到上限。");
    if(definitions.skipped)part_text_add(out,L"%d 个原生孔定义超出当前直孔读取范围。",definitions.skipped);
    free(scan);
    return PRO_TK_NO_ERROR;
}
