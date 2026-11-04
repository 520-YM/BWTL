
        FdRef ref={0};ProError e;
        if(cmd->count==2)return foundation_references(model,report,capacity);
        e=fd_ref_resolve(model,cmd->arg[2],PRO_TYPE_UNUSED,&ref);if(e)return e;
        if(fd_selected_stamp)ProVerstampFree(&fd_selected_stamp);fd_selected_model=NULL;
        e=ProMdlVerstampGet(model,&fd_selected_stamp);
        if(!e){fd_selected_model=model;fd_selected_item=ref.item;swprintf_s(report,capacity,L"已核对参照 %ls：几何ID%d、类型%d、所属特征%d。此参照已记录用于下一次操作；模型变化后会失效。",cmd->arg[2],ref.item.id,ref.item.type,ref.owner_feature);}
        fd_ref_free(&ref);return e;
    
