static const PartFeature *part_find_feature(const PartSnapshot *s,int id){int i;for(i=0;i<s->features;i++)if(s->feature[i].id==id)return &s->feature[i];return NULL;}
