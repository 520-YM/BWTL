static ProError smy_version_compare(ProMdl model,const PartCommand *cmd,PartText *out){
    ProMdl old=NULL;
    int loaded=0;
    double mm,thick,old_mm,old_thick;
    ProError error=ax_load_part(cmd->arg[2],&old,&loaded);
    if(!error)error=fab_thickness(model,&mm,&thick);
    if(!error)error=fab_thickness(old,&old_mm,&old_thick);
    ProOutline recent,prior;
    if(!error)error=ProSolidOutlineGet((ProSolid)model,recent);
    if(!error)error=ProSolidOutlineGet((ProSolid)old,prior);
    if(!error){
        part_text_add(out,L"材料按本轮指定标识：旧 %ls、新 %ls；实测板厚 %.8g → %.8g mm。\n",cmd->arg[7],cmd->arg[6],old_thick,thick);
        for(int i=0;i<3;i++)part_text_add(out,L"%lc 向包络 %.8g → %.8g mm。\n",L'X'+i,(prior[1][i]-prior[0][i])*old_mm,(recent[1][i]-recent[0][i])*mm);
    }
    FabBends bend[2]={{0},{0}};
    if(!error)error=smy_bends_load(old,&bend[0]);
    if(!error)error=smy_bends_load(model,&bend[1]);
    double tolerance=as_arg(cmd,5);
    int old_used[256]={0},reported=0;
    for(int i=0;!error&&i<bend[1].count;i++){
        const FabBend *current=&bend[1].rows[i];
        int match=-1;
        for(int j=0;j<bend[0].count;j++){
            const FabBend *previous=&bend[0].rows[j];
            if(!old_used[j]&&current->surface==previous->surface&&!_wcsicmp(current->name,previous->name)){match=j;break;}
        }
        if(match<0){
            if(++reported<=20)part_text_add(out,L"当前折弯 %ls / SURFACE:%d 无法按关联面和特征名匹配旧版。\n",current->name,current->surface);
            continue;
        }
        old_used[match]=1;
        const FabBend *previous=&bend[0].rows[match];
        if(fabs(current->angle-previous->angle)>0.001||fabs(current->radius-previous->radius)>tolerance||
           fabs(current->length-previous->length)>tolerance)
            if(++reported<=20)part_text_add(out,L"%ls / SURFACE:%d：角度 %.8g → %.8g°，R %.8g → %.8g mm，折弯长 %.8g → %.8g mm。\n",
                current->name,current->surface,previous->angle,current->angle,previous->radius,current->radius,previous->length,current->length);
    }
    for(int i=0;!error&&i<bend[0].count;i++)if(!old_used[i]&&++reported<=20)
        part_text_add(out,L"旧版折弯 %ls / SURFACE:%d 未匹配当前版。\n",bend[0].rows[i].name,bend[0].rows[i].surface);
    DeliveryOutput work={0};
    FabDxf *d[2]={calloc(1,sizeof(FabDxf)),calloc(1,sizeof(FabDxf))};
    ProPath path;
    if(!error&&(!d[0]||!d[1]))error=PRO_TK_OUT_OF_MEMORY;
    if(!error)error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=fab_flat_read(model,work.stage,L"current_version",cmd->arg[3],d[1],path);
    if(!error)error=fab_flat_read(old,work.stage,L"prior_version",cmd->arg[4],d[0],path);
    if(!error){
        unsigned char matched[FAB_DXF_MAX]={0};
        int changes=0;
        for(int i=0;i<d[1]->count;i++){
            const FabEntity *current=&d[1]->entities[i];
            int match=-1;
            for(int j=0;j<d[0]->count;j++)
                if(!matched[j]&&fab_same_entity(current,&d[0]->entities[j],tolerance)){match=j;break;}
            if(match>=0){matched[match]=1;continue;}
            const double *p=current->type==1?current->a:current->center;
            if(++changes<=20)part_text_add(out,L"新展开图元 #%d（%ls）位置 %.8g / %.8g mm 未匹配旧版。\n",i+1,
                current->layer==2?L"孔槽":current->layer==3?L"折弯线":L"外轮廓",p[0],p[1]);
        }
        for(int i=0;i<d[0]->count;i++)if(!matched[i]){
            const FabEntity *e=&d[0]->entities[i];
            const double *p=e->type==1?e->a:e->center;
            if(++changes<=20)part_text_add(out,L"旧展开图元 #%d（图层 %d）位置 %.8g / %.8g mm 未匹配新版。\n",i+1,e->layer,p[0],p[1]);
        }
        part_text_add(out,L"折弯差异或待核实 %d 条，展开差异 %d 个；采用指定固定面的坐标，立体接缝需对应面另行核查。\n",
            reported,changes);
    }
    free(d[0]);free(d[1]);
    error=fab_discard(&work,error,out);
    if(loaded&&old){
        ProError erased=ProMdlErase(old);
        if(!error)error=erased;
    }
    return error;
}
