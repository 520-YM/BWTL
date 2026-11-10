static ProError mp_deburr(FdTxn *tx,const PartCommand *cmd,PartText *out){
    ShopMouth mouths[6]={0};
    FdRef faces[6]={0};
    ProElement sets,set,refs,collection,radii;
    ProError error=PRO_TK_NO_ERROR;
    double leg=as_arg(cmd,3),volume=0;
    int count=(int)as_arg(cmd,4);
#define MP_CH(call) do{if((error=(call)))goto done;}while(0)
    for(int i=0;i<count;i++){
        ShopMouth *mouth=&mouths[i];
        MP_CH(mp_mouth(tx,cmd->arg[6+2*i],cmd->arg[5+2*i],mouth));
        MP_CH(md_solid_face(tx,cmd->arg[6+2*i],&faces[i]));
        for(int j=0;j<i;j++)for(int a=0;a<mouth->ring.count;a++)for(int b=0;b<mouths[j].ring.count;b++)
            if(mouth->ring.ids[a]==mouths[j].ring.ids[b]){error=PRO_TK_BAD_INPUTS;goto done;}
        if(leg>=mouth->radius){error=PRO_TK_BAD_INPUTS;goto done;}
        MP_CH(mp_circle_probes(tx,&mouth->frame,as_point(0,0),mouth->radius+0.5*leg,0,2*leg+0.1,1));
        MP_CH(mp_circle_probes(tx,&mouth->frame,as_point(0,0),mouth->radius+1.25*leg,0,2*leg+0.1,1));
        volume+=AS_PI*(mouth->radius*leg*leg+leg*leg*leg/3);
    }
    MP_CH(model_element(tx->root,PRO_E_RNDCH_SETS,0,0,&sets));
    for(int i=0;i<count;i++){
        MP_CH(model_element(sets,PRO_E_RNDCH_SET,0,0,&set));
        MP_CH(model_element(set,PRO_E_RNDCH_DIMENSIONAL_SCHEMA,1,PRO_CHM_D1_X_D2,NULL));
        MP_CH(model_element(set,PRO_E_RNDCH_CHAMFER_SHAPE,1,PRO_CHM_OFFSET_SURFACE,NULL));
        MP_CH(model_element(set,PRO_E_RNDCH_REFERENCES,0,0,&refs));
        MP_CH(model_element(refs,PRO_E_RNDCH_REFERENCE_TYPE,1,PRO_ROUND_REF_EDGE,NULL));
        MP_CH(fd_ref_element(tx,refs,PRO_E_RNDCH_REFERENCE_SURFACE,&faces[i]));
        MP_CH(model_element(refs,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&collection));
        MP_CH(cm_ring_collection(&mouths[i].ring,collection));
        MP_CH(model_element(set,PRO_E_RNDCH_RADII,0,0,&radii));
        MP_CH(am_radius(tx,radii,leg*tx->scale,leg*tx->scale,1,-1,NULL));
    }
    MP_CH(model_element(tx->root,PRO_E_RNDCH_ATTACH_TYPE,1,PRO_ROUND_ATTACHED,NULL));
    MP_CH(fd_create(tx,0));
    MP_CH(part_regenerate(tx->model));
    MP_CH(md_expected_volume(tx,-volume));
    for(int i=0;i<count;i++){
        ShopMouth *mouth=&mouths[i];
        MP_CH(mp_circle_probes(tx,&mouth->frame,as_point(0,0),mouth->radius+0.5*leg,0.5*leg,2*leg+0.1,0));
        MP_CH(mp_circle_probes(tx,&mouth->frame,as_point(0,0),mouth->radius+1.25*leg,0,2*leg+0.1,0));
    }
    part_text_add(out,L"已批量处理 %d 个圆孔口，核实等边倒角 C%.8g 及去料体积。",count,leg);
done:
    for(int i=0;i<count;i++)fd_ref_free(&faces[i]);
    return error;
#undef MP_CH
}
