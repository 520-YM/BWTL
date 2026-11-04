static void fd_ref_free(FdRef *r){if(r->selection)ProSelectionFree(&r->selection);}
