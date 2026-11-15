static int replay_through_spline_same(const Pro2dSplinedef *a,const Pro2dSplinedef *b)
{
    if(a->n_points!=b->n_points||a->tangency_type!=b->tangency_type||
       !a->point_arr||!b->point_arr)return 0;
    if((a->tangency_type==PRO_2D_SPLINE_TAN_START||a->tangency_type==PRO_2D_SPLINE_TAN_BOTH)&&
       fabs(remainder(a->start_tang_angle-b->start_tang_angle,2*AS_PI))>1e-6)return 0;
    if((a->tangency_type==PRO_2D_SPLINE_TAN_END||a->tangency_type==PRO_2D_SPLINE_TAN_BOTH)&&
       fabs(remainder(a->end_tang_angle-b->end_tang_angle,2*AS_PI))>1e-6)return 0;
    for(unsigned int i=0;i<a->n_points;i++)
        if(!replay_point2_same(a->point_arr[i],b->point_arr[i]))return 0;
    return 1;
}

static int replay_bspline_same(const Pro2dBSplinedef *a,const Pro2dBSplinedef *b)
{
    if(a->degree!=b->degree||a->num_knots!=b->num_knots||a->num_c_points!=b->num_c_points||
       !a->params||!b->params||!a->c_pnts||!b->c_pnts)return 0;
    for(int i=0;i<a->num_knots;i++)
        if(fabs(a->params[i]-b->params[i])>1e-7)return 0;
    for(int i=0;i<a->num_c_points;i++){
        const double wa=a->weights?a->weights[i]:1,wb=b->weights?b->weights[i]:1;
        if(fabs(wa-wb)>1e-7||!replay_point2_same(a->c_pnts[i],b->c_pnts[i]))return 0;
    }
    return 1;
}

typedef struct {
    union {
        Pro2dEntdef entity;
        Pro2dEllipsedef ellipse;
        Pro2dConicdef conic;
        Pro2dSplinedef spline;
        Pro2dBSplinedef bspline;
    } definition;
    double *storage;
} ReplayEntitySnapshot;

static ProError replay_entity_snapshot(const Pro2dEntdef *entity,ReplayEntitySnapshot *snapshot)
{
    switch(entity->type){
    case PRO_2D_ELLIPSE:snapshot->definition.ellipse=*(const Pro2dEllipsedef *)entity;break;
    case PRO_2D_CONIC:snapshot->definition.conic=*(const Pro2dConicdef *)entity;break;
    case PRO_2D_SPLINE:{
        const Pro2dSplinedef *s=(const Pro2dSplinedef *)entity;
        if(!s->point_arr||!s->n_points||s->n_points>SIZE_MAX/sizeof(Pro2dPnt))return PRO_TK_BAD_INPUTS;
        const size_t bytes=(size_t)s->n_points*sizeof(Pro2dPnt);
        snapshot->storage=malloc(bytes);
        if(!snapshot->storage)return PRO_TK_OUT_OF_MEMORY;
        snapshot->definition.spline=*s;
        snapshot->definition.spline.point_arr=(Pro2dPnt *)snapshot->storage;
        memcpy(snapshot->storage,s->point_arr,bytes);
        break;
    }
    case PRO_2D_B_SPLINE:{
        const Pro2dBSplinedef *s=(const Pro2dBSplinedef *)entity;
        if(!s->params||!s->c_pnts||s->num_knots<=0||s->num_c_points<=0)return PRO_TK_BAD_INPUTS;
        const size_t points=(size_t)s->num_c_points,knots=(size_t)s->num_knots;
        if(points>(SIZE_MAX/sizeof(double)-knots)/3)return PRO_TK_OUT_OF_RANGE;
        snapshot->storage=malloc((2*points+knots+(s->weights?points:0))*sizeof(double));
        if(!snapshot->storage)return PRO_TK_OUT_OF_MEMORY;
        snapshot->definition.bspline=*s;
        snapshot->definition.bspline.c_pnts=(Pro2dPnt *)snapshot->storage;
        snapshot->definition.bspline.params=snapshot->storage+2*points;
        memcpy(snapshot->definition.bspline.c_pnts,s->c_pnts,points*sizeof(Pro2dPnt));
        memcpy(snapshot->definition.bspline.params,s->params,knots*sizeof(double));
        if(s->weights){
            snapshot->definition.bspline.weights=snapshot->storage+2*points+knots;
            memcpy(snapshot->definition.bspline.weights,s->weights,points*sizeof(double));
        }
        break;
    }
    default:return PRO_TK_UNSUPPORTED;
    }
    return PRO_TK_NO_ERROR;
}
