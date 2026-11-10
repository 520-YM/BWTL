static void sx_compound_finish(FdTxn *tx,ProError e,int original_bodies,wchar_t *report,size_t capacity){
    if(e&&tx->root&&tx->before_count){
        FdTxn after={0};
        ProMassProperty mass;
        int bodies,modified=0;
        if(ProSolidFeatVisit((ProSolid)tx->model,fd_feature_ids,NULL,&after))tx->rollback_ok=0;
        else for(int i=after.before_count-1;i>=0;i--){
            if(fd_was_feature(tx,after.before_ids[i]))continue;
            modified=1;
            if(model_remove_feature((ProSolid)tx->model,after.before_ids[i]))tx->rollback_ok=0;
        }
        if((modified&&part_regenerate(tx->model))||part_health(tx->model)||sd_body_count(tx->model,&bodies)||
           bodies!=original_bodies||(original_bodies&&
           (ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)||
            !part_close(mass.volume,tx->volume))))tx->rollback_ok=0;
    }
    sd_profile_finish(tx,e,original_bodies,report,capacity);
}
