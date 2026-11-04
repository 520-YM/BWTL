static ProError part_parameter_visit(ProParameter *param,ProError status,ProAppData data){
    PartSnapshot *s=data;PartParameter *p;
    if(status)return status;
    if(s->parameters==PART_PARAM_MAX){s->truncated=1;return 0;}
    p=&s->parameter[s->parameters];wcscpy_s(p->name,PRO_NAME_SIZE,param->id);
    if(ProParameterValueWithUnitsGet(param,&p->value,NULL)||ProParameterLockstatusGet(param,&p->lock)){s->read_errors++;return 0;}
    s->parameters++;return 0;
}
