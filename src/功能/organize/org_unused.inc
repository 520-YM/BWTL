typedef struct {PartText *out;int count,unknown;} OrganizeUnused;

static ProError org_unused_visit(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    OrganizeUnused *scan=data;
    ProFeattype type;
    ProFeatStatus state;
    ProBoolean visible;
    unsigned flags;
    ProError error=ProFeatureTypeGet(feature,&type);
    if(error)return error;
    if(type!=PRO_FEAT_DATUM&&type!=PRO_FEAT_DATUM_AXIS&&type!=PRO_FEAT_DATUM_POINT&&type!=PRO_FEAT_CSYS)return PRO_TK_NO_ERROR;
    if((error=ProFeatureVisibilityGet(feature,&visible))||(error=org_read_status(feature,&state,&flags)))return error;
    if(!visible||state!=PRO_FEAT_ACTIVE)return PRO_TK_NO_ERROR;
    int *children=NULL,count=0;
    error=ProFeatureChildrenGet(feature,&children,&count);
    if(children)ProArrayFree((ProArray *)&children);
    if(error&&error!=PRO_TK_E_NOT_FOUND){scan->unknown++;return PRO_TK_NO_ERROR;}
    if(!count){
        if(scan->count<80){
            ProName name;
            qa_feature_label(feature,name);
            part_text_add(scan->out,L"ID:%d｜%ls｜本零件原生树未查到下游引用\n",feature->id,name);
        }
        scan->count++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError org_unused(const OrganizeView *view,PartText *out){
    OrganizeUnused scan={out,0,0};
    ProError error=ProSolidFeatVisit((ProSolid)view->model,org_unused_visit,NULL,&scan);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error)part_text_add(out,L"找到 %d 个未引用基准候选。依赖不可读 %d 个。%ls检查范围为当前零件原生父子树。",
        scan.count,scan.unknown,scan.count>80?L"已列出前 80 个。":L"");
    return error;
}
