typedef struct {ProFeature feature;ProName old_name,new_name,temp;} OrganizeRename;
static int org_rename_uncertain;
#ifdef BWTL_ORGANIZE_FAULT_TEST
static int org_test_rename_fault,org_test_rename_fault_hit;
#endif

static ProError org_name_is(ProFeature *feature,const wchar_t *expected){
    ProName current;
    ProError error=ProModelitemNameGet((ProModelitem *)feature,current);
    return error?error:_wcsicmp(current,expected)?PRO_TK_BAD_CONTEXT:PRO_TK_NO_ERROR;
}

static ProError org_set_name(OrganizeRename *entry,ProName name){
    ProError error=ProModelitemNameSet((ProModelitem *)&entry->feature,name);
    return error?error:org_name_is(&entry->feature,name);
}

static ProError org_temp_name(ProMdl model,OrganizeRename *entries,int count,int at){
    ProModelitem existing;
    for(int tag=0;tag<100;tag++){
        swprintf_s(entries[at].temp,PRO_NAME_SIZE,L"BWTL_TMP_%d_%d",entries[at].feature.id,tag);
        int reserved=0;
        for(int i=0;i<count;i++)
            if(!_wcsicmp(entries[at].temp,entries[i].new_name)||!_wcsicmp(entries[at].temp,entries[i].old_name))reserved=1;
        if(reserved)continue;
        ProError error=ProModelitemByNameInit(model,PRO_FEATURE,entries[at].temp,&existing);
        if(error==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
        if(error)return error;
    }
    return PRO_TK_E_FOUND;
}

static int org_restore_names(Revision *initial,OrganizeRename *entries,int count){
    int ok=1;
    /* Free the original names first, including when the requested changes form a cycle. */
    for(int i=0;i<count;i++)
        if(org_name_is(&entries[i].feature,entries[i].temp)&&org_set_name(&entries[i],entries[i].temp))ok=0;
    for(int i=0;i<count;i++)if(org_set_name(&entries[i],entries[i].old_name))ok=0;
    if(part_regenerate(initial->model)||!rv_geometry_restored(initial))ok=0;
    for(int i=0;i<count;i++)if(org_name_is(&entries[i].feature,entries[i].old_name))ok=0;
    return ok;
}

static ProError org_rename(const OrganizeView *view,const PartCommand *cmd,PartText *out){
    if(org_rename_uncertain)return PRO_TK_BAD_CONTEXT;
    OrganizeRename entries[8]={0};
    int count=(int)as_arg(cmd,2),started=0;
    Revision initial;
    ProError error=rv_begin(&initial,view->model);
    if(error)return error;
    for(int i=0;i<count;i++){
        OrganizeRename *e=&entries[i];
        int at=3+i*3,number,id;
        ProFeatStatus status;
        unsigned flags;
        part_integer(cmd->arg[at]+3,&id);
        wcscpy_s(e->old_name,PRO_NAME_SIZE,cmd->arg[at+1]);
        wcscpy_s(e->new_name,PRO_NAME_SIZE,cmd->arg[at+2]);
        if((error=dv_feature(view->model,e->old_name,&e->feature,&number))||
           (error=org_read_status(&e->feature,&status,&flags)))return error;
        if(e->feature.id!=id||status!=PRO_FEAT_ACTIVE)return PRO_TK_BAD_CONTEXT;
        for(int j=0;j<i;j++)
            if(e->feature.id==entries[j].feature.id||!_wcsicmp(e->new_name,entries[j].new_name))return PRO_TK_BAD_INPUTS;
    }
    for(int i=0;i<count;i++){
        ProModelitem existing;
        error=ProModelitemByNameInit(view->model,PRO_FEATURE,entries[i].new_name,&existing);
        if(!error){
            int owned=0;
            for(int j=0;j<count;j++)if(existing.id==entries[j].feature.id)owned=1;
            if(!owned)return PRO_TK_E_FOUND;
        }else if(error!=PRO_TK_E_NOT_FOUND)return error;
        if((error=org_temp_name(view->model,entries,count,i)))return error;
    }
    for(int i=0;i<count;i++){
        started=1;
        if((error=org_set_name(&entries[i],entries[i].temp)))goto done;
    }
    for(int i=0;i<count;i++){
        if((error=org_set_name(&entries[i],entries[i].new_name)))goto done;
#ifdef BWTL_ORGANIZE_FAULT_TEST
        if(org_test_rename_fault){org_test_rename_fault_hit=1;error=PRO_TK_GENERAL_ERROR;goto done;}
#endif
    }
    if((error=part_regenerate(view->model)))goto done;
    if(!rv_geometry_restored(&initial)){error=PRO_TK_GENERAL_ERROR;goto done;}
    for(int i=0;i<count;i++)if((error=org_name_is(&entries[i].feature,entries[i].new_name)))goto done;
    part_text_add(out,L"已重命名 %d 个特征，原特征 ID 与实体几何已核对。\n",count);
    for(int i=0;i<count;i++)part_text_add(out,L"ID:%d %ls → %ls\n",entries[i].feature.id,entries[i].old_name,entries[i].new_name);
done:
    if(error&&started){
        int restored=org_restore_names(&initial,entries,count);
        if(!restored)org_rename_uncertain=1;
        part_text_add(out,restored?L"批量重命名未完成，整组原名称已恢复并读回。":L"批量重命名恢复未能核实，本会话已停止继续重命名。");
    }
    return error;
}
