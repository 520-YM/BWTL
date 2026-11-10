#define FAB_DXF_MAX 4096
#define FAB_JOIN 0.00001
typedef struct {
    int type,layer;
    unsigned fields;
    double a[2],b[2],center[2],radius,start,sweep;
} FabEntity;
typedef struct {
    int count,outline,holes,bends;
    double low[2],high[2];
    FabEntity entities[FAB_DXF_MAX];
} FabDxf;

static int fab_line_value(FILE *f,char *line,size_t capacity){
    if(!fgets(line,(int)capacity,f))return 0;
    if(!strchr(line,'\n')&&!feof(f))return -1;
    char *start=line;
    while(isspace((unsigned char)*start))start++;
    size_t end=strlen(start);
    while(end&&isspace((unsigned char)start[end-1]))end--;
    memmove(line,start,end);
    line[end]=0;
    return 1;
}

static int fab_close_xy(const double a[2],const double b[2],double tol){
    return hypot(a[0]-b[0],a[1]-b[1])<=tol;
}

static double fab_degrees(double value){
    double angle=fmod(value,360);
    return angle<0?angle+360:angle;
}

static void fab_arc_point(const FabEntity *e,double degrees,double xy[2]){
    const double angle=degrees*AS_PI/180;
    xy[0]=e->center[0]+e->radius*cos(angle);
    xy[1]=e->center[1]+e->radius*sin(angle);
}

static int fab_same_entity(const FabEntity *a,const FabEntity *b,double tol){
    if(a->type!=b->type||a->layer!=b->layer)return 0;
    if(a->type==1)return (fab_close_xy(a->a,b->a,tol)&&fab_close_xy(a->b,b->b,tol))||
        (fab_close_xy(a->a,b->b,tol)&&fab_close_xy(a->b,b->a,tol));
    if(!fab_close_xy(a->center,b->center,tol)||fabs(a->radius-b->radius)>tol)return 0;
    if(a->type==3)return 1;
    double mid_a[2],mid_b[2];
    fab_arc_point(a,a->start+a->sweep/2,mid_a);
    fab_arc_point(b,b->start+b->sweep/2,mid_b);
    return fab_close_xy(a->a,b->a,tol)&&fab_close_xy(a->b,b->b,tol)&&fab_close_xy(mid_a,mid_b,tol);
}

static void fab_expand(FabDxf *d,const double point[2]){
    for(int i=0;i<2;i++){d->low[i]=fmin(d->low[i],point[i]);d->high[i]=fmax(d->high[i],point[i]);}
}

static int fab_entity_finish(FabDxf *d,FabEntity *e){
    if(!e->type)return 1;
    if(d->count==FAB_DXF_MAX||!e->layer||(e->layer==3&&e->type!=1))return 0;
    const unsigned required=e->type==1?15:e->type==2?115:19;
    if(e->fields!=required)return 0;
    if(e->type!=1){
        if(e->radius<=FAB_JOIN)return 0;
        if(e->type==2){
            e->sweep=fab_degrees(e->sweep-e->start);
            if(e->sweep<1e-8||e->sweep>360-1e-8)return 0;
            fab_arc_point(e,e->start,e->a);
            fab_arc_point(e,e->start+e->sweep,e->b);
        }
    }
    if(e->type==1&&fab_close_xy(e->a,e->b,FAB_JOIN))return 0;
    for(int i=0;i<d->count;i++)if(fab_same_entity(e,&d->entities[i],FAB_JOIN))return 0;
    d->entities[d->count++]=*e;
    if(e->layer==3)d->bends++;
    if(e->layer==2)d->holes++;
    if(e->layer==1){
        d->outline++;
        if(e->type!=3){fab_expand(d,e->a);fab_expand(d,e->b);}
        if(e->type!=1)for(int i=0;i<4;i++){
            double p[2];
            if(e->type==2&&fab_degrees(90*i-e->start)>e->sweep)continue;
            fab_arc_point(e,90*i,p);fab_expand(d,p);
        }
    }
    *e=(FabEntity){0};
    return 1;
}

static int fab_entity_value(FabEntity *e,long code,const char *value){
    if(code==8){
        if(e->layer)return 0;
        e->layer=!strcmp(value,"OUTLINE")?1:!strcmp(value,"HOLES")?2:!strcmp(value,"BEND")?3:0;
        return e->layer!=0;
    }
    unsigned bit=0;
    double *dest=NULL;
    if(code==10){bit=1;dest=e->type==1?e->a:e->center;}
    if(code==20){bit=2;dest=(e->type==1?e->a:e->center)+1;}
    if(code==11){bit=4;dest=e->b;}
    if(code==21){bit=8;dest=e->b+1;}
    if(code==40){bit=16;dest=&e->radius;}
    if(code==50){bit=32;dest=&e->start;}
    if(code==51){bit=64;dest=&e->sweep;}
    if(!bit&&code!=30&&code!=31)return code==5||code==100||code==330;
    char *end;
    double number=strtod(value,&end);
    if(end==value||*end||!isfinite(number)||fabs(number)>1e8)return 0;
    if(!bit)return fabs(number)<=FAB_JOIN;
    if(e->fields&bit)return 0;
    e->fields|=bit;*dest=number;
    return 1;
}

static int fab_dxf_contours(const FabDxf *d){
    if(!d->outline||d->high[0]-d->low[0]<=FAB_JOIN||d->high[1]-d->low[1]<=FAB_JOIN)return 0;
    for(int i=0;i<d->count;i++){
        const FabEntity *e=&d->entities[i];
        if(e->layer==3||e->type==3)continue;
        int joined_a=0,joined_b=0;
        for(int j=0;j<d->count;j++){
            const FabEntity *f=&d->entities[j];
            if(j==i||f->layer!=e->layer||f->type==3)continue;
            joined_a+=fab_close_xy(e->a,f->a,FAB_JOIN)+fab_close_xy(e->a,f->b,FAB_JOIN);
            joined_b+=fab_close_xy(e->b,f->a,FAB_JOIN)+fab_close_xy(e->b,f->b,FAB_JOIN);
        }
        if(joined_a!=1||joined_b!=1)return 0;
    }
    return 1;
}

static ProError fab_dxf_read(const wchar_t *path,FabDxf *d){
    if(!dv_file_nonempty(path))return PRO_TK_CANT_OPEN;
    FILE *f=_wfopen(path,L"rt");
    if(!f)return PRO_TK_CANT_OPEN;
    memset(d,0,sizeof(*d));
    d->low[0]=d->low[1]=HUGE_VAL;d->high[0]=d->high[1]=-HUGE_VAL;
    FabEntity entity={0};
    int section=0,units=0,unit_field=0,eof=0,seen_entities=0,seen_header=0;
    char group[128],value[256];
    while(fab_line_value(f,group,sizeof(group))==1&&fab_line_value(f,value,sizeof(value))==1){
        char *end;
        const long code=strtol(group,&end,10);
        if(end==group||*end||code<0||code>1071)break;
        if(!code&&!strcmp(value,"SECTION")){
            if(section||fab_line_value(f,group,sizeof(group))!=1||strcmp(group,"2")||
               fab_line_value(f,value,sizeof(value))!=1)break;
            section=!strcmp(value,"HEADER")?1:!strcmp(value,"ENTITIES")?2:3;
            if(section==1&&seen_header++)break;
            if(section==2&&seen_entities++)break;
        }else if(!code&&!strcmp(value,"ENDSEC")){
            if(!section||unit_field||!fab_entity_finish(d,&entity))break;
            section=0;
        }else if(!code&&!strcmp(value,"EOF")){
            if(!section&&fab_line_value(f,group,sizeof(group))==0)eof=1;
            break;
        }else if(section==1){
            if(unit_field){
                if(code!=70||strcmp(value,"4"))break;
                unit_field=0;units=1;
            }else if(code==9&&!strcmp(value,"$INSUNITS")){
                if(units)break;
                unit_field=1;
            }
        }else if(section==2){
            if(!code){
                if(!fab_entity_finish(d,&entity))break;
                entity.type=!strcmp(value,"LINE")?1:!strcmp(value,"ARC")?2:!strcmp(value,"CIRCLE")?3:0;
                if(!entity.type)break;
            }else if(!entity.type||!fab_entity_value(&entity,code,value))break;
        }else if(!section)break;
    }
    const int valid=eof&&units&&seen_entities&&!ferror(f)&&fab_dxf_contours(d);
    fclose(f);
    return valid?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError fab_flat_read(ProMdl model,const wchar_t *directory,const wchar_t *base,
                             const wchar_t *face,FabDxf *d,ProPath path){
    ProError error=fab_leaf_path(directory,base,L".dxf",path);
    if(!error)error=fab_native_flat(model,base,face);
    return error?error:fab_dxf_read(path,d);
}
