static void foundation_reset(void){
    if(fd_draft.stamp)ProVerstampFree(&fd_draft.stamp);memset(&fd_draft,0,sizeof(fd_draft));
    if(fd_selected_stamp)ProVerstampFree(&fd_selected_stamp);fd_selected_model=NULL;
    fd_last_sketch_model=NULL;fd_last_sketch[0]=0;
}
