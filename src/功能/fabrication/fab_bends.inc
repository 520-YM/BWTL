typedef struct {
    int number,id,surface,method;
    ProName name;
    double angle,radius,length,developed;
} FabBend;
typedef struct {ProMdl model;double mm,thickness;int count;FabBend rows[256];} FabBends;

static ProError fab_bend_visit(ProSurface surface,ProError status,ProAppData data){
    FabBends *b=data;
    if(status)return status;
    ProSmtSurfType side;
    ProError error=ProSmtSurfaceTypeGet((ProPart)b->model,surface,&side);
    if(error||side!=PRO_SMT_SURF_FACE)return error;
    double radius,angle,developed;
    ProBool inside;
    ProSmtDvlLenCalcInfo calc;
    error=ProSmtBendsrfInfoGet((ProPart)b->model,surface,&radius,&inside,&angle,&developed,&calc);
    if(error==PRO_TK_BAD_CONTEXT||error==PRO_TK_E_NOT_FOUND||error==PRO_TK_BAD_INPUTS)return PRO_TK_NO_ERROR;
    if(error||fabs(angle)<0.01)return error;
    ProSrftype type;
    if((error=ProSurfaceTypeGet(surface,&type)))return error;
    if(type==PRO_SRF_PLANE)return PRO_TK_NO_ERROR;
    if(type!=PRO_SRF_CYL||b->count==_countof(b->rows))return PRO_TK_BAD_CONTEXT;
    ProGeomitemdata *raw=NULL;
    if((error=ProSurfaceDataGet(surface,&raw)))return error;
    ProVector axis;
    memcpy(axis,raw->data.p_surface_data->srf_shape.cylinder.e3,sizeof(axis));
    ProGeomitemdataFree(&raw);
    ProVector low,high,span;
    ProGeomitem item;
    ProFeature owner;
    FabBend *row=&b->rows[b->count];
    if(!ad_unit(axis))return PRO_TK_BAD_CONTEXT;
    if((error=ProSurfaceExtremesEval(surface,axis,low,high))||
       (error=ProSurfaceToGeomitem((ProSolid)b->model,surface,&item))||
       (error=ProGeomitemFeatureGet(&item,&owner))||
       (error=ProFeatureNumberGet(&owner,&row->number)))return error;
    ad_sub(high,low,span);
    row->length=fabs(ad_dot(span,axis))*b->mm;
    row->radius=radius*b->mm-(inside?0:b->thickness);
    row->angle=angle;row->developed=developed*b->mm;
    if(row->radius<-0.0001||row->length<=0||row->developed<=0)return PRO_TK_BAD_CONTEXT;
    row->radius=fmax(0,row->radius);
    row->method=calc.method;row->id=owner.id;row->surface=item.id;
    qa_feature_label(&owner,row->name);
    b->count++;
    return PRO_TK_NO_ERROR;
}

static int fab_bend_order(const void *a,const void *b){
    const FabBend *x=a,*y=b;
    if(x->number!=y->number)return x->number<y->number?-1:1;
    return (x->surface>y->surface)-(x->surface<y->surface);
}

static ProError fab_bends(ProMdl model,const PartCommand *cmd,PartText *out){
    FabBends bends={0};
    bends.model=model;
    ProError error=fab_thickness(model,&bends.mm,&bends.thickness);
    if(!error)error=ProSolidSurfaceVisit((ProSolid)model,fab_bend_visit,NULL,&bends);
    DeliveryOutput work={0};
    FILE *file=NULL;
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,L"bend-sequence.csv",&file);
    if(!error){
        qsort(bends.rows,bends.count,sizeof(bends.rows[0]),fab_bend_order);
        fwprintf(file,L"模型顺序,特征ID,特征名称,折弯面ID,角度_deg,内R_mm,轴向长度_mm,展开弧长_mm,Creo展开算法编号\n");
        for(int i=0;i<bends.count;i++){
            const FabBend *r=&bends.rows[i];
            fwprintf(file,L"%d,%d,",r->number,r->id);
            if(!dv_csv_text(file,r->name))error=PRO_TK_CANT_WRITE;
            fwprintf(file,L",%d,%.10g,%.10g,%.10g,%.10g,%d\n",r->surface,r->angle,r->radius,r->length,r->developed,r->method);
        }
    }
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"已按模型特征顺序生成 %d 条折弯记录。机床工序和模具由工艺确认。\n%ls\\bend-sequence.csv",bends.count,work.target);
    return error;
}
