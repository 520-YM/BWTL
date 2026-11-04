
        int through=!wcscmp(cmd->arg[5],L"THROUGH");part_number(cmd->arg[6],&amount);
        SD_TRY("extrude depth",model_element(tx.root,PRO_E_STD_EXT_DEPTH,0,0,&depth));
        SD_TRY("extrude from",model_element(depth,PRO_E_EXT_DEPTH_FROM,0,0,&from));
        SD_TRY("extrude from type",model_element(from,PRO_E_EXT_DEPTH_FROM_TYPE,1,PRO_EXT_DEPTH_FROM_NONE,NULL));
        SD_TRY("extrude to",model_element(depth,PRO_E_EXT_DEPTH_TO,0,0,&to));
        SD_TRY("extrude to type",model_element(to,PRO_E_EXT_DEPTH_TO_TYPE,1,through?PRO_EXT_DEPTH_TO_ALL:PRO_EXT_DEPTH_TO_BLIND,NULL));
        if(!through)SD_TRY("extrude depth value",model_element(to,PRO_E_EXT_DEPTH_TO_VALUE,2,amount*tx.scale,NULL));
    
