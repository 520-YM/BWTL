typedef struct {
    ProSolid model;
    ProVector origin,direction;
    double distance,tolerance;
    int id;
} SxNext;

static ProError sx_next_surface(ProSurface surface,ProError status,ProAppData data){
    SxNext *hit=data;
    ProGeomitem item;
    ProVector origin,normal,point;
    ProUvParam uv;
    ProUvStatus inside;
    double denominator=0,numerator=0,distance;
    ProError e;
    if(status)return status;
    if((e=ProSurfaceToGeomitem(hit->model,surface,&item))||(e=fd_plane_data(&item,origin,normal)))return e;
    for(int i=0;i<3;i++){
        denominator+=normal[i]*hit->direction[i];
        numerator+=normal[i]*(origin[i]-hit->origin[i]);
    }
    if(fabs(denominator)<1e-8)return PRO_TK_NO_ERROR;
    distance=numerator/denominator;
    if(distance<=hit->tolerance||distance>hit->distance+hit->tolerance)return PRO_TK_NO_ERROR;
    for(int i=0;i<3;i++)point[i]=hit->origin[i]+distance*hit->direction[i];
    if((e=ProSurfaceParamEval(hit->model,surface,point,uv))||
       (e=ProSurfaceUvpntVerify(hit->model,surface,uv,&inside)))return e;
    if(inside!=PRO_UV_INSIDE)return PRO_TK_NO_ERROR;
    if(fabs(distance-hit->distance)<=hit->tolerance)return PRO_TK_E_AMBIGUOUS;
    hit->id=item.id;
    hit->distance=distance;
    return PRO_TK_NO_ERROR;
}

static ProError sx_next_hit(ProMdl model,ProMatrix frame,const Pro2dPnt point,int sign,double tolerance,int *id){
    SxNext hit={0};
    ProError e;
    hit.model=(ProSolid)model;hit.distance=HUGE_VAL;hit.tolerance=tolerance;
    for(int i=0;i<3;i++){
        hit.origin[i]=frame[3][i]+point[0]*frame[0][i]+point[1]*frame[1][i];
        hit.direction[i]=sign*frame[2][i];
    }
    e=ProSolidSurfaceVisit(hit.model,sx_next_surface,NULL,&hit);
    if(e)return e;
    if(!hit.id)return PRO_TK_E_NOT_FOUND;
    if(*id&&*id!=hit.id)return PRO_TK_E_AMBIGUOUS;
    *id=hit.id;
    return PRO_TK_NO_ERROR;
}

static ProError sx_next_face(FdTxn *tx,const PartCommand *cmd,FdRef *target){
    FdSketch sketch={0};
    ProMatrix frame;
    ProSectionShape shape;
    Pro2dPnt center={0};
    ProVector origin,normal;
    int *ids=NULL,n=0,id=0,sign=!wcscmp(cmd->arg[6],L"POS")?1:-1;
    double tolerance=tx->scale*0.001;
    wchar_t token[32];
    ProError e;
    if((e=fd_sketch_open(tx->model,cmd->arg[3],&sketch,0))||
       (e=ProSectionLocationGet(sketch.section,frame))||(e=ProSectionShapeGet(sketch.section,&shape))||
       (e=ProSectionEntityIdsGet(sketch.section,&ids,&n)))goto done;
    if(shape!=PRO_SECSHAPE_1_CLOSED_LOOP||n<1||n>16){e=PRO_TK_BAD_INPUTS;goto done;}
    for(int i=0;i<n;i++){
        Pro2dEntdef *entity;
        if((e=ProSectionEntityGet(sketch.section,ids[i],&entity)))goto done;
        if(entity->type==PRO_2D_LINE){
            Pro2dLinedef *line=(Pro2dLinedef *)entity;
            for(int j=0;j<2;j++)center[j]+=line->end1[j]/n;
        }else if(n==1&&entity->type==PRO_2D_CIRCLE)memcpy(center,((Pro2dCircledef *)entity)->center,sizeof(center));
        else {e=PRO_TK_UNSUPPORTED;goto done;}
        for(int j=0;j<8;j++){
            Pro2dPnt point;
            double t=j/8.0;
            if(entity->type==PRO_2D_LINE){
                Pro2dLinedef *line=(Pro2dLinedef *)entity;
                for(int k=0;k<2;k++)point[k]=line->end1[k]+t*(line->end2[k]-line->end1[k]);
            }else{
                Pro2dCircledef *circle=(Pro2dCircledef *)entity;
                point[0]=circle->center[0]+circle->radius*cos(2*AS_PI*t);
                point[1]=circle->center[1]+circle->radius*sin(2*AS_PI*t);
            }
            if((e=sx_next_hit(tx->model,frame,point,sign,tolerance,&id)))goto done;
        }
    }
    if((e=sx_next_hit(tx->model,frame,center,sign,tolerance,&id)))goto done;
    swprintf_s(token,_countof(token),L"SURFACE:%d",id);
    if((e=fd_ref_resolve(tx->model,token,PRO_SURFACE,target)))goto done;
    e=fd_plane_data(&target->item,origin,normal);
done:
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&sketch);
    return e;
}

static ProError sx_extrude_depth(FdTxn *tx,const PartCommand *cmd,int kind,FdRef *target){
    ProElement depth=NULL,from=NULL,to=NULL;
    int from_type=PRO_EXT_DEPTH_FROM_NONE,to_type=PRO_EXT_DEPTH_TO_BLIND;
    double to_value=0;
    ProError e;
    switch(kind){
    case SX_SYMMETRIC:to_type=PRO_EXT_DEPTH_SYMMETRIC;to_value=2*as_arg(cmd,5);break;
    case SX_REFERENCE:
        to_type=PRO_EXT_DEPTH_TO_REF;
        e=!wcscmp(cmd->arg[5],L"NEXT")?sx_next_face(tx,cmd,target):fd_ref_resolve(tx->model,cmd->arg[5],PRO_SURFACE,target);
        if(e)return e;
        break;
    case SX_TWO_SIDE:from_type=PRO_EXT_DEPTH_FROM_BLIND;to_value=as_arg(cmd,5);break;
    case SX_THIN:to_value=as_arg(cmd,4);break;
    default:return PRO_TK_BAD_INPUTS;
    }
    if((e=model_element(tx->root,PRO_E_STD_EXT_DEPTH,0,0,&depth))||
       (e=model_element(depth,PRO_E_EXT_DEPTH_FROM,0,0,&from))||
       (e=model_element(from,PRO_E_EXT_DEPTH_FROM_TYPE,1,from_type,NULL))||
       (e=model_element(depth,PRO_E_EXT_DEPTH_TO,0,0,&to))||
       (e=model_element(to,PRO_E_EXT_DEPTH_TO_TYPE,1,to_type,NULL)))return e;
    if(to_value&&(e=model_element(to,PRO_E_EXT_DEPTH_TO_VALUE,2,to_value*tx->scale,NULL)))return e;
    if(from_type==PRO_EXT_DEPTH_FROM_BLIND&&(e=model_element(from,PRO_E_EXT_DEPTH_FROM_VALUE,2,as_arg(cmd,6)*tx->scale,NULL)))return e;
    if(to_type==PRO_EXT_DEPTH_TO_REF){
        if((e=fd_ref_element(tx,to,PRO_E_EXT_DEPTH_TO_REF,target)))return e;
        return model_element(to,PRO_E_EXT_DEPTH_TO_REF_TRF,1,PRO_EXT_DEPTH_REF_NONE,NULL);
    }
    return PRO_TK_NO_ERROR;
}

static ProError sx_thin_parameters(FdTxn *tx,const PartCommand *cmd){
    ProElement thin=NULL,start=NULL,end=NULL;
    int side=!wcscmp(cmd->arg[6],L"SYM")?PRO_EXT_MATERIAL_BOTH_SIDES:
        !wcscmp(cmd->arg[6],L"POS")?PRO_EXT_MATERIAL_SIDE_ONE:PRO_EXT_MATERIAL_SIDE_TWO;
    ProError e;
    if((e=model_element(tx->root,PRO_E_STD_MATRLSIDE,1,side,NULL))||
       (e=model_element(tx->root,PRO_E_THICKNESS,2,as_arg(cmd,5)*tx->scale,NULL))||
       (e=model_element(tx->root,PRO_E_FEAT_THIN,0,0,&thin))||
       (e=model_element(thin,PRO_E_FEAT_THIN_STRT,0,0,&start))||
       (e=model_element(start,PRO_E_FEAT_THIN_STRT_OPT,1,PRO_FEAT_THIN_IGNORE,NULL))||
       (e=model_element(thin,PRO_E_FEAT_THIN_END,0,0,&end)))return e;
    return model_element(end,PRO_E_FEAT_THIN_END_OPT,1,PRO_FEAT_THIN_IGNORE,NULL);
}
