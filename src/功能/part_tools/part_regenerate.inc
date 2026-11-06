static ProError part_regenerate(ProMdl model){
    ProError e;int attempt;
    /* Creo may request another regeneration pass without reporting a failed
       feature. Retry only that documented status, and keep the retry bounded.
       Geometry/value readback and rollback remain with the calling operation. */
    for(attempt=0;attempt<3;attempt++){
        e=ProSolidRegenerate((ProSolid)model,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);
        if(e!=PRO_TK_REGEN_AGAIN)break;
    }
    return e?e:part_health(model);
}
