typedef enum {MC_COUNTERBORE,MC_COUNTERSINK,MC_THREAD} McHoleKind;
typedef struct {
    McHoleGeometry geometry;
    McHoleKind kind;
    double diameter,outer,recess,angle,depth,thread_depth,stock;
} McProfiledHole;

static ProError mc_hole_stock(ProMdl model,const McHoleGeometry *geometry,int entrance,double *stock){
    ProRay ray;
    ProSelection *hits=NULL;
    int count,i,entry_found=0,exit_found=0;
    ProError e;
    memcpy(ray.start_point,geometry->center,sizeof(ray.start_point));
    memcpy(ray.dir_vector,geometry->normal,sizeof(ray.dir_vector));
    if((e=ProSolidRayIntersectionCompute(model,0.1,&ray,&hits,&count)))return e;
    for(i=0;i<count;i++){
        ProModelitem item;
        ProSurface surface;
        ProQuilt quilt;
        ProBoolean datum;
        Pro3dPnt point;
        double distance=0;
        int j;
        if((e=ProSelectionModelitemGet(hits[i],&item)))return e;
        if(item.type!=PRO_SURFACE)continue;
        if((e=ProGeomitemToSurface(&item,&surface))||(e=ProSurfaceIsDatumPlane((ProSolid)model,surface,&datum)))return e;
        if(datum)continue;
        e=ProSurfaceQuiltGet((ProSolid)model,surface,&quilt);
        if(!e)continue;
        if(e!=PRO_TK_E_NOT_FOUND)return e;
        if((e=ProSelectionPoint3dGet(hits[i],point)))return e;
        for(j=0;j<3;j++)distance+=(point[j]-geometry->center[j])*geometry->normal[j];
        if(item.id==entrance&&fabs(distance)<1e-5){entry_found=1;continue;}
        if(exit_found||fabs(distance)<1e-5)return PRO_TK_BAD_CONTEXT;
        *stock=fabs(distance);
        exit_found=1;
    }
    return entry_found&&exit_found?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError mc_profiled_hole(FdTxn *tx,const PartCommand *cmd,McProfiledHole *hole){
    ProElement com=NULL;
    int entrance;
    const int thread=hole->kind==MC_THREAD;
    const int bore=hole->kind==MC_COUNTERBORE;
    const int sink=hole->kind==MC_COUNTERSINK;
    ProError e;
#define MC_PROFILE(call) do{e=(call);if(e)return e;}while(0)
    if(thread){
        part_number(cmd->arg[10],&hole->thread_depth);
        part_number(cmd->arg[11],&hole->depth);
        hole->thread_depth*=tx->scale;
    }else{
        part_number(cmd->arg[8],&hole->diameter);
        part_number(cmd->arg[9],&hole->outer);
        part_number(cmd->arg[10],bore?&hole->recess:&hole->angle);
        part_number(cmd->arg[12],&hole->depth);
        hole->diameter*=tx->scale;
        hole->outer*=tx->scale;
        hole->recess*=tx->scale;
        if(sink)hole->recess=(hole->outer-hole->diameter)/2/tan(hole->angle*3.141592653589793/360);
    }
    hole->depth*=tx->scale;
    tx->stage="build native profiled hole";
    MC_PROFILE(model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_HLE_TYPE_SKETCHED,NULL));
    MC_PROFILE(model_element(tx->root,PRO_E_HLE_COM,0,0,&com));
    MC_PROFILE(model_element(com,PRO_E_HLE_TYPE_NEW,1,thread?PRO_HLE_NEW_TYPE_STANDARD:PRO_HLE_CUSTOM_TYPE,NULL));
    if(thread)MC_PROFILE(mc_thread_fields(com,hole->thread_depth));
    MC_PROFILE(model_element(com,PRO_E_HLE_ADD_CBORE,1,bore?PRO_HLE_ADD_CBORE:PRO_HLE_NO_CBORE,NULL));
    MC_PROFILE(model_element(com,PRO_E_HLE_ADD_CSINK,1,sink?PRO_HLE_ADD_CSINK:PRO_HLE_NO_CSINK,NULL));
    if(!thread)MC_PROFILE(model_element(com,PRO_E_HLE_HOLEDIAM,2,hole->diameter,NULL));
    MC_PROFILE(model_element(com,PRO_E_HLE_MAKE_LIGHTWT,1,PRO_HLE_REGULAR,NULL));
    if(bore){
        MC_PROFILE(model_element(com,PRO_E_HLE_CBOREDIAM,2,hole->outer,NULL));
        MC_PROFILE(model_element(com,PRO_E_HLE_CBOREDEPTH,2,hole->recess,NULL));
    }
    if(sink){
        MC_PROFILE(model_element(com,PRO_E_HLE_CSINKDIAM,2,hole->outer,NULL));
        MC_PROFILE(model_element(com,PRO_E_HLE_CSINKANGLE,2,hole->angle,NULL));
    }
    if(hole->depth>0){
        MC_PROFILE(model_element(com,PRO_E_HLE_DRILLDEPTH,2,hole->depth,NULL));
        MC_PROFILE(model_element(com,PRO_E_HLE_DRILLANGLE,2,118,NULL));
        MC_PROFILE(model_element(com,PRO_E_HLE_DEPTH_DIM_TYPE,1,PRO_HLE_DEP_SHOULDER_DIM_SCHEME,NULL));
    }else MC_PROFILE(model_element(com,PRO_E_HLE_ADD_EXIT_CSINK,1,PRO_HLE_NO_EXIT_CSINK,NULL));
    MC_PROFILE(model_element(com,PRO_E_HLE_DEPTH,1,hole->depth>0?PRO_HLE_STD_VAR_DEPTH:PRO_HLE_STD_THRU_ALL_DEPTH,NULL));
    MC_PROFILE(model_element(com,PRO_E_HLE_CRDIR_FLIP,1,PRO_HLE_CR_IN_SIDE_ONE,NULL));
    MC_PROFILE(model_element(com,PRO_E_HLE_TOP_CLEARANCE,1,PRO_HOLE_GEN_CLRNCE,NULL));
    MC_PROFILE(model_element(com,PRO_E_HLE_ADD_PARAMETERS,1,thread?PRO_HOLE_ADD_PARAMETERS_FLAG:PRO_HOLE_NO_PARAMETERS_FLAG,NULL));
    MC_PROFILE(model_element(com,PRO_E_HLE_ADD_NOTE,1,thread?PRO_HOLE_ADD_NOTE_FLAG:PRO_HOLE_NO_NOTE_FLAG,NULL));
    MC_PROFILE(mc_hole_place(tx,cmd,&hole->geometry,&entrance));
    if(thread)MC_PROFILE(mc_thread_size(tx,cmd));
    tx->stage="check continuous material at advanced hole axis";
    MC_PROFILE(mc_hole_stock(tx->model,&hole->geometry,entrance,&hole->stock));
    if(hole->recess>=hole->stock||(hole->depth>=hole->stock))return PRO_TK_BAD_CONTEXT;
    return fd_create(tx,0);
#undef MC_PROFILE
}

typedef struct {ProFeature *feature;McHoleGeometry *geometry;} McProfileWall;

static ProError mc_profile_wall(ProSurface surface,ProError status,ProAppData data){
    McProfileWall *wall=data;
    ProGeomitem item;
    ProFeature owner;
    ProError e;
    if(status)return status;
    if((e=ProSurfaceToGeomitem((ProSolid)wall->feature->owner,surface,&item))||
       (e=ProGeomitemFeatureGet(&item,&owner)))return e;
    if(owner.id!=wall->feature->id)return PRO_TK_NO_ERROR;
    return mc_hole_wall(&item,PRO_TK_NO_ERROR,wall->geometry);
}

static ProError mc_verify_profiled_hole(FdTxn *tx,const PartCommand *cmd,McProfiledHole *hole,double volume){
    const double pi=3.141592653589793;
    const int thread=hole->kind==MC_THREAD,bore=hole->kind==MC_COUNTERBORE,sink=hole->kind==MC_COUNTERSINK;
    const ProElemId dia[]={PRO_E_HLE_COM,PRO_E_HLE_HOLEDIAM};
    const ProElemId depth[]={PRO_E_HLE_COM,PRO_E_HLE_DEPTH},drill[]={PRO_E_HLE_COM,PRO_E_HLE_DRILLDEPTH};
    const ProElemId drill_angle[]={PRO_E_HLE_COM,PRO_E_HLE_DRILLANGLE};
    const ProElemId style[]={PRO_E_HLE_COM,PRO_E_HLE_TYPE_NEW};
    const ProElemId flag[]={PRO_E_HLE_COM,bore?PRO_E_HLE_ADD_CBORE:PRO_E_HLE_ADD_CSINK};
    const ProElemId outer[]={PRO_E_HLE_COM,bore?PRO_E_HLE_CBOREDIAM:PRO_E_HLE_CSINKDIAM};
    const ProElemId shape[]={PRO_E_HLE_COM,bore?PRO_E_HLE_CBOREDEPTH:PRO_E_HLE_CSINKANGLE};
    McProfileWall wall={&tx->feature,&hole->geometry};
    ProError e;
    double radius,head,tip=0,expected,cylinder;
    tx->stage="verify advanced hole profile and depths";
    if((e=mc_tree_value(&tx->feature,style,2,thread?PRO_HLE_NEW_TYPE_STANDARD:PRO_HLE_CUSTOM_TYPE,1))||
       (e=mc_tree_value(&tx->feature,depth,2,hole->depth>0?PRO_HLE_STD_VAR_DEPTH:PRO_HLE_STD_THRU_ALL_DEPTH,1))||
       (hole->depth>0&&((e=mc_tree_value(&tx->feature,drill,2,hole->depth,0))||
                       (e=mc_tree_value(&tx->feature,drill_angle,2,118,0)))))return e;
    if(thread){
        if((e=mc_verify_thread(tx,cmd,hole->thread_depth,&hole->diameter)))return e;
    }else if((e=mc_tree_value(&tx->feature,dia,2,hole->diameter,0))||
             (e=mc_tree_value(&tx->feature,flag,2,bore?PRO_HLE_ADD_CBORE:PRO_HLE_ADD_CSINK,1))||
             (e=mc_tree_value(&tx->feature,outer,2,hole->outer,0))||
             (e=mc_tree_value(&tx->feature,shape,2,bore?hole->recess:hole->angle,0)))return e;
    tx->stage="verify complete profiled hole and removed volume";
    radius=hole->diameter/2;
    head=hole->outer/2;
    if(!isfinite(radius)||radius<=0)return PRO_TK_BAD_CONTEXT;
    if(hole->depth>0){
        tip=radius/tan(118*pi/360);
        if(hole->depth+tip>=hole->stock)return PRO_TK_BAD_CONTEXT;
    }
    expected=pi*radius*radius*(hole->depth>0?hole->depth+tip/3:hole->stock);
    if(bore)expected+=pi*(head*head-radius*radius)*hole->recess;
    if(sink)expected+=pi*(head*head+head*radius-2*radius*radius)*hole->recess/3;
    if(!isfinite(expected)||fabs(tx->volume-volume-expected)>fmax(1e-7,expected*1e-5))return PRO_TK_BAD_CONTEXT;
    tx->stage="verify actual profiled hole axis and cylindrical wall";
    hole->geometry.radius=radius;
    hole->geometry.profiled=1;
    /* Inspect final solid faces so cosmetic thread surfaces are excluded. */
    if((e=ProSolidSurfaceVisit((ProSolid)tx->model,mc_profile_wall,NULL,&wall)))return e;
    cylinder=(hole->depth>0?hole->depth:hole->stock)-hole->recess;
    if(hole->geometry.cylinders<1||fabs(hole->geometry.angle-2*pi)>1e-5||!part_close(hole->geometry.length,cylinder))return PRO_TK_BAD_CONTEXT;
    return PRO_TK_NO_ERROR;
}
