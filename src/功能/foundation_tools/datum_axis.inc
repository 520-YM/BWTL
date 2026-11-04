
        int planes=!wcscmp(cmd->arg[3],L"PLANES"),count=planes?2:1;
        for(i=0;i<count;i++)FD_TRY("resolve axis reference",fd_ref_resolve(model,cmd->arg[4+i],PRO_SURFACE,&ref[i]));
        if(planes){
            double dot;FD_TRY("first axis plane",fd_plane_data(&ref[0].item,p0,n0));FD_TRY("second axis plane",fd_plane_data(&ref[1].item,p1,n1));
            dot=n0[0]*n1[0]+n0[1]*n1[1]+n0[2]*n1[2];if(fabs(dot)>1-1e-9){e=PRO_TK_BAD_INPUTS;goto done;}
        }else{ProSurface surface;ProSrftype st;FD_TRY("axis cylinder handle",ProGeomitemToSurface(&ref[0].item,&surface));FD_TRY("axis cylinder type",ProSurfaceTypeGet(surface,&st));if(st!=PRO_SRF_CYL){e=PRO_TK_INVALID_TYPE;goto done;}}
        FD_TRY("axis constraints",model_element(tx.root,PRO_E_DTMAXIS_CONSTRAINTS,0,0,&array));
        for(i=0;i<count;i++){
            FD_TRY("axis constraint",model_element(array,PRO_E_DTMAXIS_CONSTRAINT,0,0,&entry));
            FD_TRY("axis through type",model_element(entry,PRO_E_DTMAXIS_CONSTR_TYPE,1,PRO_DTMAXIS_CONSTR_TYPE_THRU,NULL));
            FD_TRY("axis reference",fd_ref_element(&tx,entry,PRO_E_DTMAXIS_CONSTR_REF,&ref[i]));
        }
    
