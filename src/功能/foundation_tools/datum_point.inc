
            FD_TRY("point type",model_element(tx.root,PRO_E_DPOINT_TYPE,1,PRO_DPOINT_TYPE_OFFSET_CSYS,NULL));
            FD_TRY("point Cartesian",model_element(tx.root,PRO_E_DPOINT_OFST_CSYS_TYPE,1,PRO_DTMPNT_OFFCSYS_CARTESIAN,NULL));
            FD_TRY("point csys",fd_ref_element(&tx,tx.root,PRO_E_DPOINT_OFST_CSYS_REF,&ref[0]));
            FD_TRY("point dimensions",model_element(tx.root,PRO_E_DPOINT_OFST_CSYS_WITH_DIMS,1,PRO_B_TRUE,NULL));
            FD_TRY("point array",model_element(tx.root,PRO_E_DPOINT_OFST_CSYS_PNTS_ARRAY,0,0,&array));
            FD_TRY("point entry",model_element(array,PRO_E_DPOINT_OFST_CSYS_PNT,0,0,&entry));
            FD_TRY("point name",model_element(entry,PRO_E_DPOINT_OFST_CSYS_PNT_NAME,0,0,&el));FD_TRY("set point name",ProElementWstringSet(el,(wchar_t *)cmd->arg[2]));
            for(i=0;i<3;i++)FD_TRY("point coordinate",model_element(entry,(ProElemId)(i==0?PRO_E_DPOINT_OFST_CSYS_DIR1_VAL:i==1?PRO_E_DPOINT_OFST_CSYS_DIR2_VAL:PRO_E_DPOINT_OFST_CSYS_DIR3_VAL),2,v[i]*scale,NULL));
        
