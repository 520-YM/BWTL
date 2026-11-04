static ProError part_dimension_visit(ProDimension *dim,ProError status,ProAppData data){
    PartSnapshot *s=data;PartDimension *d;ProFeature owner;
    if(status)return status;
    if(s->dimensions==PART_DIM_MAX){s->truncated=1;return 0;}
    d=&s->dimension[s->dimensions];d->handle=*dim;
    if(ProDimensionSymbolGet(dim,d->symbol)||ProDimensionValueGet(dim,&d->value)||
        ProDimensionTypeGet(dim,&d->type)||ProDimensionOwnerfeatureGet(dim,&owner)||
        ProDimensionIsReldriven(dim,&d->related)||ProDimensionIsRegenednegative(dim,&d->negative)){s->read_errors++;return 0;}
    d->feature=owner.id;s->dimensions++;return 0;
}
