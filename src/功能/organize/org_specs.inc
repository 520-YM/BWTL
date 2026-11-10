typedef struct {double min_d,max_d,min_l,max_l;int kind,count;} OrganizeSpec;

static int org_spec_order(const void *a,const void *b){
    const OrganizeSpec *x=a,*y=b;
    if(x->kind!=y->kind)return x->kind-y->kind;
    if(x->min_d!=y->min_d)return x->min_d<y->min_d?-1:1;
    return (x->min_l>y->min_l)-(x->min_l<y->min_l);
}

static ProError org_specs(const OrganizeView *view,const PartCommand *cmd,PartText *out){
    DeliveryBores *scan=NULL;
    ProError error=org_bores(view,&scan);
    if(error)return error;
    OrganizeSpec samples[DV_BORE_MAX],groups[DV_BORE_MAX];
    int count=0,n=0;
    for(int i=0;i<scan->count;i++){
        const DeliveryBore *b=&scan->bores[i];
        if(!b->classification)continue;
        double d=2*b->radius*view->mm,l=(b->high-b->low)*view->mm;
        samples[count++]=(OrganizeSpec){d,d,l,l,b->classification,1};
    }
    qsort(samples,count,sizeof(*samples),org_spec_order);
    double diameter_tol=as_arg(cmd,2),length_tol=as_arg(cmd,3);
    for(int i=0;i<count;i++){
        OrganizeSpec sample=samples[i];
        int j=0;
        for(;j<n;j++){
            OrganizeSpec *g=&groups[j];
            if(g->kind!=sample.kind||sample.min_d-g->min_d>diameter_tol||
               fmax(g->max_l,sample.min_l)-fmin(g->min_l,sample.min_l)>length_tol)continue;
            g->max_d=sample.max_d;g->min_l=fmin(g->min_l,sample.min_l);
            g->max_l=fmax(g->max_l,sample.max_l);g->count++;
            break;
        }
        if(j==n)groups[n++]=sample;
    }
    part_text_add(out,L"孔规格归类。已读 %d 个完整圆柱孔段，分为 %d 组。\n",count,n);
    for(int i=0;i<n;i++){
        const OrganizeSpec *g=&groups[i];
        part_text_add(out,L"%ls｜直径 %.8g–%.8g mm｜轴向跨度 %.8g–%.8g mm｜%d 个\n",
            org_bore_class(g->kind),g->min_d,g->max_d,g->min_l,g->max_l,g->count);
    }
    if(scan->limited)part_text_add(out,L"扫描达到上限，结果仅覆盖已读取部分。");
    part_text_add(out,L"按端型及实测尺寸归类，沉孔或台阶孔的不同圆柱段分别列出。");
    free(scan);
    return PRO_TK_NO_ERROR;
}
