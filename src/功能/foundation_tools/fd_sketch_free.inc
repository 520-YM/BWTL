static void fd_sketch_free(FdSketch *s){
    if(s->tree)ProFeatureElemtreeFree(&s->feature,s->tree);
    if(s->backup)ProFeatureElemtreeFree(&s->feature,s->backup);
    if(s->options)ProArrayFree((ProArray *)&s->options);memset(s,0,sizeof(*s));
}
