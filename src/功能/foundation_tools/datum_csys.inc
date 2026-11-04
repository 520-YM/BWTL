
            FD_TRY("csys origin array",model_element(tx.root,PRO_E_CSYS_ORIGIN_CONSTRS,0,0,&array));
            FD_TRY("csys origin",model_element(array,PRO_E_CSYS_ORIGIN_CONSTR,0,0,&entry));
            FD_TRY("csys origin reference",fd_ref_element(&tx,entry,PRO_E_CSYS_ORIGIN_CONSTR_REF,&ref[0]));
            FD_TRY("csys offset type",model_element(tx.root,PRO_E_CSYS_OFFSET_TYPE,1,PRO_CSYS_OFFSET_CARTESIAN,NULL));
            FD_TRY("csys moves",model_element(tx.root,PRO_E_CSYS_ORIENTMOVES,0,0,&array));
            /* Rotations then translations match the SDK's general csys sequence. */
            for(i=0;i<6;i++){int index=i<3?i+3:i-3;
                FD_TRY("csys move",model_element(array,PRO_E_CSYS_ORIENTMOVE,0,0,&entry));
                FD_TRY("csys move kind",model_element(entry,PRO_E_CSYS_ORIENTMOVE_MOVE_TYPE,1,index,NULL));
                FD_TRY("csys move value",model_element(entry,PRO_E_CSYS_ORIENTMOVE_MOVE_VAL,2,v[index]*(index<3?scale:1),NULL));
            }
            FD_TRY("csys orientation method",model_element(tx.root,PRO_E_CSYS_ORIENT_BY_METHOD,1,PRO_CSYS_ORIENT_BY_SEL_CSYS_AXES,NULL));
        
