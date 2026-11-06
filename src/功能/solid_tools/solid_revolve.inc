
        SD_TRY("resolve revolve axis",fd_ref_resolve(model,cmd->arg[4],PRO_AXIS,&axis));part_number(cmd->arg[5],&amount);
        SD_TRY("revolve external axis",model_element(tx.root,PRO_E_REVOLVE_AXIS_OPT,1,PRO_REV_AXIS_EXT_REF,NULL));
        SD_TRY("revolve axis reference",fd_ref_element(&tx,tx.root,PRO_E_REVOLVE_AXIS,&axis));
        if(cut)SD_TRY("remove material inside revolved sketch",model_element(tx.root,PRO_E_STD_MATRLSIDE,1,PRO_REV_MATERIAL_SIDE_TWO,NULL));
        SD_TRY("revolve angle",model_element(tx.root,PRO_E_REV_ANGLE,0,0,&depth));
        SD_TRY("revolve from",model_element(depth,PRO_E_REV_ANGLE_FROM,0,0,&from));
        SD_TRY("revolve from type",model_element(from,PRO_E_REV_ANGLE_FROM_TYPE,1,PRO_REV_ANG_FROM_NONE,NULL));
        SD_TRY("revolve to",model_element(depth,PRO_E_REV_ANGLE_TO,0,0,&to));
        SD_TRY("revolve to type",model_element(to,PRO_E_REV_ANGLE_TO_TYPE,1,PRO_REV_ANG_TO_ANGLE,NULL));
        SD_TRY("revolve angle value",model_element(to,PRO_E_REV_ANGLE_TO_VAL,2,amount,NULL));
    
