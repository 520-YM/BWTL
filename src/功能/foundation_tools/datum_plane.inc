
        FD_TRY("resolve plane",fd_ref_resolve(model,cmd->arg[3],PRO_SURFACE,&ref[0]));
        FD_TRY("verify plane geometry",fd_plane_data(&ref[0].item,p0,n0));part_number(cmd->arg[4],&v[0]);
        FD_TRY("plane constraints",model_element(tx.root,PRO_E_DTMPLN_CONSTRAINTS,0,0,&array));
        FD_TRY("plane constraint",model_element(array,PRO_E_DTMPLN_CONSTRAINT,0,0,&entry));
        FD_TRY("plane offset type",model_element(entry,PRO_E_DTMPLN_CONSTR_TYPE,1,PRO_DTMPLN_OFFS,NULL));
        FD_TRY("plane reference",fd_ref_element(&tx,entry,PRO_E_DTMPLN_CONSTR_REF,&ref[0]));
        FD_TRY("plane offset",model_element(entry,PRO_E_DTMPLN_CONSTR_REF_OFFSET,2,v[0]*scale,NULL));
    
