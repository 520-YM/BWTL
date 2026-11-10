static ProError sd_sketch_ref_shape(ProMdl model,const wchar_t *name,FdRef *out,ProSectionShape *shape,int open){
    FdSketch sketch={0};ProError e;memset(out,0,sizeof(*out));
    e=fd_sketch_open(model,name,&sketch,0);if(e)goto done;
    e=ProSectionShapeGet(sketch.section,shape);if(e)goto done;
    if(open?*shape!=PRO_SECSHAPE_1_OPEN_LOOP:
       (*shape!=PRO_SECSHAPE_1_CLOSED_LOOP&&*shape!=PRO_SECSHAPE_MULTI_CLOSED_LOOPS)){e=PRO_TK_BAD_INPUTS;goto done;}
    out->item=*(ProGeomitem *)&sketch.feature;out->owner_feature=sketch.feature.id;
    e=ProSelectionAlloc(NULL,(ProModelitem *)&sketch.feature,&out->selection);
done:fd_sketch_free(&sketch);return e;
}
static ProError sd_sketch_ref(ProMdl model,const wchar_t *name,FdRef *out,ProSectionShape *shape){
    return sd_sketch_ref_shape(model,name,out,shape,0);
}
