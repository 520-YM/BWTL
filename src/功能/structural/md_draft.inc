static ProError md_draft(FdTxn *tx,const PartCommand *cmd,int kind){
    int split=kind==MD_DRAFT_SPLIT,variable=kind==MD_DRAFT_VARIABLE;
    int first=split?8:7,count=variable?1:cmd->count-first;
    int side_index=variable?10:split?7:6;
    FdRef face[8]={{0}},hinge={0},direction={0},station[2]={{0}};ProCollection collection=NULL;
    ProElement surfaces,dir,side,angles,entry;ProVector point,hn,dn,p,n,first_station={0};ProError e;
#define DR(call) do{if((e=(call)))goto done;}while(0)
    DR(fd_ref_resolve(tx->model,cmd->arg[variable?4:3],PRO_SURFACE,&hinge));
    DR(fd_ref_resolve(tx->model,cmd->arg[variable?5:4],PRO_SURFACE,&direction));
    DR(fd_plane_data(&hinge.item,point,hn));DR(fd_plane_data(&direction.item,p,dn));
    if(fabs(fabs(ad_dot(hn,dn))-1)>1e-6){e=PRO_TK_BAD_INPUTS;goto done;}
    DR(ProSrfcollectionAlloc(&collection));
    for(int i=0;i<count;i++){
        DR(md_solid_face(tx,cmd->arg[variable?3:first+i],&face[i]));DR(fd_plane_data(&face[i].item,p,n));
        if(fabs(ad_dot(n,dn))>1e-6){e=PRO_TK_BAD_INPUTS;goto done;}
        DR(rf_surface_add(tx,collection,&face[i]));
    }
    DR(model_element(tx->root,PRO_E_DRAFT_SPLIT,1,split?PRO_DRAFT_UI_SPLIT_NEUT:PRO_DRAFT_UI_SPLIT_NONE,NULL));
    DR(model_element(tx->root,PRO_E_STD_SURF_COLLECTION_APPL,0,0,&surfaces));DR(ProElementCollectionSet(surfaces,collection));
    DR(model_element(tx->root,PRO_E_DIRECTION_COMPOUND,0,0,&dir));DR(fd_ref_element(tx,dir,PRO_E_DIRECTION_REFERENCE,&direction));
    DR(model_element(dir,PRO_E_DIRECTION_FLIP,1,!wcscmp(cmd->arg[side_index],L"POS")?PRO_DIRECTION_FLIP_ALONG:PRO_DIRECTION_FLIP_AGAINST,NULL));
    DR(model_element(tx->root,PRO_E_DRAFT_CONSTANT_OR_VARIABLE,1,variable?PRO_DRAFT_UI_VARIABLE:PRO_DRAFT_UI_CONSTANT,NULL));
    DR(model_element(tx->root,PRO_E_DRAFT_INCLUDE_TANGENT,1,PRO_DRAFT_UI_NOT_INC_TANG,NULL));
    for(int i=0;i<(split?2:1);i++){
        DR(model_element(tx->root,i?PRO_E_DRAFT_SIDE_2:PRO_E_DRAFT_SIDE_1,0,0,&side));
        DR(model_element(side,i?PRO_E_DRAFT_NEUTRAL_OBJECT_TYPE_2:PRO_E_DRAFT_NEUTRAL_OBJECT_TYPE_1,1,PRO_DRAFT_UI_PLANE,NULL));
        DR(fd_ref_element(tx,side,i?PRO_E_DRAFT_NEUTRAL_PLANE_2:PRO_E_DRAFT_NEUTRAL_PLANE_1,&hinge));
        DR(model_element(side,i?PRO_E_DRAFT_DEPENDENT_2:PRO_E_DRAFT_DEPENDENT_1,1,PRO_DRAFT_UI_INDEPENDENT,NULL));
        if(variable){
            DR(model_element(side,PRO_E_DRAFT_ANGLES,0,0,&angles));
            for(int j=0;j<2;j++){
                ProVector xyz,delta;ProSurface surface;ProUvParam uv;ProVector actual;
                DR(fd_ref_resolve(tx->model,cmd->arg[6+2*j],PRO_POINT,&station[j]));DR(ad_point(&station[j],xyz));
                if(j&&ad_near(first_station,xyz,0.001*tx->scale)){e=PRO_TK_BAD_INPUTS;goto done;}
                if(!j)for(int d=0;d<3;d++)first_station[d]=xyz[d];
                ad_sub(xyz,point,delta);if(fabs(ad_dot(delta,hn))>0.001*tx->scale){e=PRO_TK_BAD_INPUTS;goto done;}
                DR(ProGeomitemToSurface(&face[0].item,&surface));DR(ProSurfaceParamEval((ProSolid)tx->model,surface,xyz,uv));
                DR(ProSurfaceXyzdataEval(surface,uv,actual,NULL,NULL,NULL));
                if(!ad_near(xyz,actual,0.001*tx->scale)){e=PRO_TK_BAD_INPUTS;goto done;}
                DR(model_element(angles,PRO_E_DRAFT_ANG_PNT,0,0,&entry));
                DR(fd_ref_element(tx,entry,PRO_E_STD_POINT_COLLECTION_APPL,&station[j]));
                DR(model_element(entry,PRO_E_DRAFT_ANGLE,2,as_arg(cmd,7+2*j),NULL));
            }
        }else DR(model_element(side,i?PRO_E_DRAFT_ANGLE_2:PRO_E_DRAFT_ANGLE_1,2,as_arg(cmd,5+i),NULL));
    }
    DR(fd_create(tx,0));
done:for(int i=0;i<8;i++)fd_ref_free(&face[i]);fd_ref_free(&hinge);fd_ref_free(&direction);for(int i=0;i<2;i++)fd_ref_free(&station[i]);
    if(collection)ProCollectionFree(&collection);return e;
#undef DR
}
