enum RevisionHoleKind {RV_STRAIGHT,RV_BORE,RV_SINK};
typedef struct {
    ProFeature feature;
    ProDimension position[2],dia,depth_dim,head_dim,shape_dim;
    ProVector points[3],normals[3];
    int refs[3],kind,blind;
    double x,y,diameter,depth,head,shape;
} RevisionHole;

static ProError rv_tree_int(ProElement root,const ProElemId *path,int count,int *value){
    ProElement element;
    ProError error=mc_tree_element(root,path,count,&element);
    return error?error:ProElementIntegerGet(element,NULL,value);
}

static ProError rv_tree_number(ProElement root,const ProElemId *path,int count,double *value){
    ProElement element;
    ProError error=mc_tree_element(root,path,count,&element);
    return error?error:ProElementDoubleGet(element,NULL,value);
}

static ProError rv_hole_load(Revision *tx,const wchar_t *name,int kind,int positions,int depth,RevisionHole *hole){
    const ProElemId style[]={PRO_E_HLE_COM,PRO_E_HLE_TYPE_NEW};
    const ProElemId refids[]={PRO_E_HLE_PRIM_REF,PRO_E_HLE_DIM_REF1,PRO_E_HLE_DIM_REF2};
    const ProElemId offsets[]={PRO_E_HLE_DIM_DIST1,PRO_E_HLE_DIM_DIST2};
    const ProElemId align[]={PRO_E_HLE_PLC_ALIGN_OPT1,PRO_E_HLE_PLC_ALIGN_OPT2};
    const ProElemId diameter[]={PRO_E_HLE_COM,kind==RV_STRAIGHT?PRO_E_DIAMETER:PRO_E_HLE_HOLEDIAM};
    const ProElemId limit[]={PRO_E_HLE_COM,PRO_E_HOLE_STD_DEPTH,PRO_E_HOLE_DEPTH_TO,PRO_E_HOLE_DEPTH_TO_TYPE};
    const ProElemId blindval[]={PRO_E_HLE_COM,PRO_E_HOLE_STD_DEPTH,PRO_E_HOLE_DEPTH_TO,PRO_E_EXT_DEPTH_TO_VALUE};
    const ProElemId head[]={PRO_E_HLE_COM,kind==RV_BORE?PRO_E_HLE_CBOREDIAM:PRO_E_HLE_CSINKDIAM};
    const ProElemId shape[]={PRO_E_HLE_COM,kind==RV_BORE?PRO_E_HLE_CBOREDEPTH:PRO_E_HLE_CSINKANGLE};
    ProElement root=NULL,elem;
    ProError error;
    int mode,flag;
    memset(hole,0,sizeof(*hole));
    hole->kind=kind;
#define RH(call) do{if((error=(call))){trace("revision hole " #call,error);goto done;}}while(0)
    RH(rv_feature(tx->model,name,PRO_FEAT_HOLE,&hole->feature));
    RH(ProFeatureElemtreeExtract(&hole->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&root));
    RH(rv_tree_int(root,style,2,&mode));
    if(mode!=(kind==RV_STRAIGHT?PRO_HLE_NEW_TYPE_STRAIGHT:PRO_HLE_CUSTOM_TYPE)){error=PRO_TK_BAD_CONTEXT;goto done;}
    RH(rv_tree_int(root,(ProElemId[]){PRO_E_HLE_PLACEMENT,PRO_E_HLE_PL_TYPE},2,&mode));
    if(mode!=PRO_HLE_PL_TYPE_LIN){error=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<3;i++){
        ProModelitem item;
        RH(mc_tree_element(root,(ProElemId[]){PRO_E_HLE_PLACEMENT,refids[i]},2,&elem));
        RH(dv_reference_item(elem,&item));
        if(item.type!=PRO_SURFACE||item.owner!=tx->model){error=PRO_TK_BAD_CONTEXT;goto done;}
        hole->refs[i]=item.id;
        RH(fd_plane_data(&item,hole->points[i],hole->normals[i]));
        if(!ad_unit(hole->normals[i])){error=PRO_TK_BAD_CONTEXT;goto done;}
        if(i==0){
            FdRef solid={0};
            wchar_t token[32];
            swprintf_s(token,_countof(token),L"SURFACE:%d",item.id);
            error=md_solid_face(&(FdTxn){.model=tx->model},token,&solid);
            fd_ref_free(&solid);
            if(error)goto done;
        }
        for(int j=0;j<i;j++)if(fabs(ad_dot(hole->normals[i],hole->normals[j]))>1e-6){error=PRO_TK_BAD_CONTEXT;goto done;}
    }
    for(int i=0;i<2;i++){
        double *value=i?&hole->y:&hole->x;
        RH(rv_tree_int(root,(ProElemId[]){PRO_E_HLE_PLACEMENT,align[i]},2,&flag));
        if(flag==PRO_HLE_PLC_ALIGN){
            if(positions){error=PRO_TK_BAD_CONTEXT;goto done;}
        }else{
            const ProElemId path[]={PRO_E_HLE_PLACEMENT,offsets[i]};
            RH(rv_tree_number(root,path,2,value));
            if(positions){
                if(*value<=0){error=PRO_TK_BAD_CONTEXT;goto done;}
                RH(rv_elem_dim(&hole->feature,path,2,0,&hole->position[i]));
            }
        }
    }
    RH(rv_tree_number(root,diameter,2,&hole->diameter));
    if(kind==RV_STRAIGHT){
        RH(rv_tree_int(root,(ProElemId[]){PRO_E_HLE_COM,PRO_E_HOLE_STD_DEPTH,PRO_E_HOLE_DEPTH_FROM,PRO_E_HOLE_DEPTH_FROM_TYPE},4,&mode));
        if(mode!=PRO_HLE_STRGHT_NONE_DEPTH){error=PRO_TK_BAD_CONTEXT;goto done;}
        RH(rv_tree_int(root,limit,4,&mode));
        if(mode!=PRO_HLE_STRGHT_BLIND_DEPTH&&mode!=PRO_HLE_STRGHT_THRU_ALL_DEPTH){error=PRO_TK_BAD_CONTEXT;goto done;}
        hole->blind=mode==PRO_HLE_STRGHT_BLIND_DEPTH;
        if(hole->blind)RH(rv_tree_number(root,blindval,4,&hole->depth));
        if(depth){
            if(!hole->blind){error=PRO_TK_BAD_CONTEXT;goto done;}
            RH(rv_elem_dim(&hole->feature,blindval,4,0,&hole->depth_dim));
        }else if(!positions)RH(rv_elem_dim(&hole->feature,diameter,2,0,&hole->dia));
    }else{
        RH(rv_tree_int(root,(ProElemId[]){PRO_E_HLE_COM,PRO_E_HLE_DEPTH},2,&mode));
        if(mode!=PRO_HLE_STD_THRU_ALL_DEPTH){error=PRO_TK_BAD_CONTEXT;goto done;}
        RH(rv_tree_int(root,(ProElemId[]){PRO_E_HLE_COM,PRO_E_HLE_ADD_CBORE},2,&flag));
        if(flag!=(kind==RV_BORE?PRO_HLE_ADD_CBORE:PRO_HLE_NO_CBORE)){error=PRO_TK_BAD_CONTEXT;goto done;}
        RH(rv_tree_int(root,(ProElemId[]){PRO_E_HLE_COM,PRO_E_HLE_ADD_CSINK},2,&flag));
        if(flag!=(kind==RV_SINK?PRO_HLE_ADD_CSINK:PRO_HLE_NO_CSINK)){error=PRO_TK_BAD_CONTEXT;goto done;}
        RH(rv_tree_number(root,head,2,&hole->head));
        RH(rv_tree_number(root,shape,2,&hole->shape));
        RH(rv_elem_dim(&hole->feature,head,2,0,&hole->head_dim));
        RH(rv_elem_dim(&hole->feature,shape,2,0,&hole->shape_dim));
    }
done:
    if(root)ProFeatureElemtreeFree(&hole->feature,root);
    return error;
#undef RH
}

static ProError rv_hole_check(Revision *tx,RevisionHole *hole){
    McHoleGeometry g={0};
    ProSolidBody body;
    double d[]={ad_dot(hole->points[0],hole->normals[0]),
        ad_dot(hole->points[1],hole->normals[1])+hole->x,ad_dot(hole->points[2],hole->normals[2])+hole->y};
    for(int i=0;i<3;i++){
        g.normal[i]=hole->normals[0][i];
        for(int j=0;j<3;j++)g.center[i]+=d[j]*hole->normals[j][i];
    }
    g.radius=hole->diameter/2;
    g.profiled=hole->kind!=RV_STRAIGHT;
    McProfileWall wall={&hole->feature,&g};
    ProError error=qa_single_body(tx->model,&body);
    if(!error){
        /* The visitor resolves ownership on the final geometry after regeneration. */
        error=ProSolidBodySurfaceVisit(&body,mc_profile_wall,&wall);
    }
    if(error)return error;
    if(g.cylinders<1||fabs(g.angle-2*AS_PI)>1e-5||(hole->blind&&!part_close(g.length,hole->depth)))
        return PRO_TK_BAD_CONTEXT;
    double front=0,roof=0,r=g.radius;
    if(hole->kind!=RV_STRAIGHT){
        r=(hole->head+hole->diameter)/4;
        front=hole->kind==RV_BORE?hole->shape:(hole->head/2-r)/tan(hole->shape*AS_PI/360);
        roof=hole->kind==RV_BORE?hole->shape:(hole->head-hole->diameter)/2/tan(hole->shape*AS_PI/360);
    }
    for(int i=0;i<9;i++){
        ProRay ray;
        QualityRay hits;
        double theta=(i+0.25)*AS_PI/4,rr=i==8?0:r;
        if(i<8&&hole->kind==RV_STRAIGHT)rr+=0.02/tx->mm;
        for(int j=0;j<3;j++){
            ray.start_point[j]=g.center[j]+rr*(cos(theta)*hole->normals[1][j]+sin(theta)*hole->normals[2][j]);
            ray.dir_vector[j]=-g.normal[j];
        }
        if((error=qa_ray(tx->model,&ray,0.0001/tx->mm,&hits)))return error;
        if(i==8&&!hole->blind){if(hits.count)return PRO_TK_BAD_CONTEXT;continue;}
        double first=i==8?hole->depth:front;
        if(hits.count!=2||fabs(hits.crossing[0].depth-first)>0.001/tx->mm)return PRO_TK_BAD_CONTEXT;
        if(!hole->blind&&fabs(hits.crossing[1].depth-(g.length+roof))>0.001/tx->mm)return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}

static ProError rv_hole_position(Revision *tx,RevisionHole *hole,double x,double y){
    if(x<=0.01/tx->mm||y<=0.01/tx->mm)return PRO_TK_BAD_INPUTS;
    ProError error=rv_add(tx,hole->position[0],hole->feature.id,hole->x,x);
    if(!error)error=rv_add(tx,hole->position[1],hole->feature.id,hole->y,y);
    hole->x=x;hole->y=y;
    return error;
}

static ProError rv_finish_holes(Revision *tx,RevisionHole *holes,int count){
    ProError error=rv_apply(tx);
    for(int i=0;!error&&i<count;i++)error=rv_hole_check(tx,&holes[i]);
    return error;
}

static ProError rv_load_group(Revision *tx,const PartCommand *cmd,int at,int count,RevisionHole *holes){
    for(int i=0;i<count;i++){
        ProError error=rv_hole_load(tx,cmd->arg[at+i],RV_STRAIGHT,1,0,&holes[i]);
        if(!error)error=rv_hole_check(tx,&holes[i]);
        if(error)return error;
        for(int j=0;j<3;j++)if(i&&holes[i].refs[j]!=holes[0].refs[j])return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}
