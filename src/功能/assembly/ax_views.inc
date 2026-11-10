
static ProError ax_view_status(const char *stage,ProError error){
    if(error)trace(stage,error);
    return error;
}

static ProError ax_view_repaint(ProMdl assembly){
    int window;
    ProError error=ProMdlWindowGet(assembly,&window);
    return error?error:ProWindowRepaint(window);
}

typedef struct { const wchar_t *name; ProSimprep rep; int found; } AxRepFind;

static ProError ax_view_rep_find(ProGeomitem *item,ProError status,ProAppData data){
    AxRepFind *search=data;
    ProSimprepdata *rep_data=NULL;
    if(status)return status;
    ProError error=ProSimprepdataGet(item,&rep_data);
    if(!error&&!_wcsicmp(rep_data->name,search->name)){
        search->rep=*item;
        search->found=1;
    }
    if(rep_data)ProSimprepdataFree(&rep_data);
    return error;
}

static ProError ax_view_rep_lookup(ProMdl assembly,const wchar_t *name,ProSimprep *rep){
    AxRepFind search={name,{0},0};
    ProError error=ProSolidSimprepVisit((ProSolid)assembly,NULL,ax_view_rep_find,&search);
    if(error)return error;
    if(!search.found)return PRO_TK_E_NOT_FOUND;
    *rep=search.rep;
    return PRO_TK_NO_ERROR;
}

static ProError ax_view_rep(ProMdl assembly,const PartCommand *cmd,int activate,PartText *out){
    ProName name;
    ProSimprep rep={0},before={0},active={0};
    ProSimprepdata *data=NULL,*actual=NULL;
    ProBoolean dynamic;
    ProError error=ProAssemblyDynPosGet((ProAssembly)assembly,&dynamic);
    if(error||dynamic)return error?error:PRO_TK_BAD_CONTEXT;
    error=ProSimprepActiveGet((ProSolid)assembly,&before);
    if(error)return error;
    wcscpy_s(name,_countof(name),cmd->arg[2]);
    if(activate&&cmd->count==3)wcscpy_s(name,_countof(name),L"主表示");
    if(activate&&cmd->count<=4&&(cmd->count==3||!wcscmp(cmd->arg[2],L"APPLY"))){
        if(cmd->count==4)wcscpy_s(name,_countof(name),cmd->arg[3]);
        error=cmd->count==3?ProSimprepInit(NULL,-1,(ProSolid)assembly,&rep):ax_view_rep_lookup(assembly,name,&rep);
        ax_view_status("isolation lookup",error);
        if(!error)error=ProSimprepActivate((ProSolid)assembly,&rep);
        if(!error)error=ProSimprepActiveGet((ProSolid)assembly,&active);
        if(!error&&rep.id!=active.id)error=PRO_TK_GENERAL_ERROR;
        if(!error&&cmd->count==3)error=ax_master(assembly);
        if(!error)error=ProSolidDisplay((ProSolid)assembly);
        if(error==PRO_TK_E_FOUND)error=PRO_TK_NO_ERROR;
        if(!error)error=ax_view_repaint(assembly);
        if(error&&active.id==rep.id)ProSimprepActivate((ProSolid)assembly,&before);
        if(!error)part_text_add(out,L"当前表示：%ls。\n",name);
        return error;
    }
    if((error=ax_master(assembly)))return error;
    error=ax_view_rep_lookup(assembly,name,&rep);
    if(error!=PRO_TK_E_NOT_FOUND)return error?error:PRO_TK_E_FOUND;
    error=ax_view_status("rep alloc",ProSimprepdataAlloc(name,PRO_B_FALSE,PRO_SIMPREP_EXCLUDE,&data));
    int selected[PART_ARG_MAX]={0};
    for(int i=3;!error&&i<cmd->count;i++){
        AxComponent owner={0};
        ProSimprepAction action={0};
        ProSimprepitem item={0};
        error=ax_component_resolve(assembly,cmd->arg[i],&owner);
        if(error)break;
        selected[i]=owner.feature.id;
        action.type=PRO_SIMPREP_INCLUDE;
        error=ProSimprepdataitemInit(owner.path.comp_id_table,owner.path.table_num,
            PRO_VALUE_UNUSED,&action,&item);
        ax_view_status("rep item init",error);
        if(!error)error=ax_view_status("rep item add",ProSimprepdataitemAdd(data,&item));
    }
    int created=0;
    if(!error){
        error=ax_view_status("rep create",ProSimprepCreate((ProSolid)assembly,data,&rep));
        created=!error;
    }
    if(!error)error=ax_view_status("rep readback",ProSimprepdataGet(&rep,&actual));
    if(!error&&actual->deflt!=PRO_SIMPREP_EXCLUDE)error=PRO_TK_GENERAL_ERROR;
    int size=0;
    if(!error)error=ProArraySizeGet((ProArray)actual->items,&size);
    if(!error&&size!=cmd->count-3)error=PRO_TK_GENERAL_ERROR;
    for(int i=3;!error&&i<cmd->count;i++){
        int found=0;
        for(int j=0;j<size;j++){
            ProSimprepitem *item=&actual->items[j];
            if(item->item_path.path_size==1&&item->item_path.comp_path[0]==selected[i]&&
                item->action.type==PRO_SIMPREP_INCLUDE)found=1;
        }
        if(!found)error=PRO_TK_GENERAL_ERROR;
    }
    if(!error&&activate){
        error=ProSimprepActivate((ProSolid)assembly,&rep);
        if(!error)error=ProSimprepActiveGet((ProSolid)assembly,&active);
        if(!error&&active.id!=rep.id)error=PRO_TK_GENERAL_ERROR;
        if(!error)error=ProSolidDisplay((ProSolid)assembly);
        if(error==PRO_TK_E_FOUND)error=PRO_TK_NO_ERROR;
        if(!error)error=ax_view_repaint(assembly);
    }
    if(actual)ProSimprepdataFree(&actual);
    if(data)ProSimprepdataFree(&data);
    if(error&&created){
        if(activate&&ProSimprepActivate((ProSolid)assembly,&before))ax_uncertain(out);
        if(ProSimprepDelete(&rep))ax_uncertain(out);
    }
    if(!error)part_text_add(out,L"%ls简化表示 %ls，保留 %d 个顶层模块。装配未保存。\n",
        activate?L"已建立并激活":L"已建立",name,cmd->count-3);
    return error;
}

static ProError ax_view_named(ProMdl assembly,const PartCommand *cmd,PartText *out){
    ProLine name;
    ProView view=NULL;
    ProMatrix matrix;
    wcscpy_s(name,_countof(name),cmd->arg[2]);
    int save=!wcscmp(cmd->arg[3],L"SAVE");
    ProError error=ProViewNameToView(assembly,name,&view);
    if(save){
        if(error!=PRO_TK_E_NOT_FOUND)return error?error:PRO_TK_E_FOUND;
        error=ProViewCreate(assembly,name,&view);
        if(!error){
            error=ProViewNameToView(assembly,name,&view);
            if(!error)error=ProViewMatrixGet(assembly,view,matrix);
            if(error&&view&&ProViewDelete(assembly,view))ax_uncertain(out);
        }
    }else if(!error){
        error=ProViewRetrieve(assembly,name,&view);
        if(!error)error=ax_view_repaint(assembly);
    }
    if(!error)part_text_add(out,L"已%ls装配视角 %ls，装配未保存。\n",save?L"记录":L"恢复",name);
    return error;
}

typedef struct { const wchar_t *name; ProXsec value; int found; } AxSectionFind;
static ProError ax_view_section_find(ProXsec *section,ProAppData data){
    AxSectionFind *search=data;
    if(!_wcsicmp(search->name,section->xsec_name)){
        search->found=1;
        search->value=*section;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_view_section(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxSectionFind search={cmd->arg[2],{0},0};
    ProError error=ProSolidXsecVisit((ProSolid)assembly,ax_view_section_find,&search);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    ProXsec section={0};
    int created=0;
    if(cmd->count==4){
        if(!search.found)return PRO_TK_E_NOT_FOUND;
        section=search.value;
    }else{
        if(search.found)return PRO_TK_E_FOUND;
        AxComponent root={0};
        AxGeometry geometry;
        ProSelection selection=NULL;
        ProModelitem plane;
        root.model=assembly;
        error=ax_view_status("section plane",ax_ref_resolve(&root,cmd->arg[3],0,&selection,&geometry));
        if(!error)error=ProSelectionModelitemGet(selection,&plane);
        ProMdlName name;
        wcscpy_s(name,_countof(name),cmd->arg[2]);
        double offset=as_arg(cmd,4)/mm;
        ProDimension dimension;
        if(!error&&offset==0){
            ProXsecMdlname handle=NULL;
            error=ProXSectionPlanarCreate((ProSolid)assembly,name,selection,PRO_XSECTYPE_MODEL,
                NULL,0,PRO_XSEC_MODEL_EXCLUDE,NULL,NULL,&handle);
            if(!error){
                created=1;
                section.solid_owner=(ProSolid)assembly;
                wcscpy_s(section.xsec_name,_countof(section.xsec_name),name);
            }
            if(handle)ProXsecMdlnameFree(&handle);
        }else if(!error){
            error=ProXsecParallelCreate((ProSolid)assembly,name,plane.id,offset,&section,&dimension);
            created=!error;
        }
        if(selection)ProSelectionFree(&selection);
    }
    ax_view_status("section creation or lookup",error);
    if(!error)error=ax_view_status("section activate",ProXsecActiveSet(&section));
    if(!error)error=ax_view_status("section visible",ProXsecMakeVisible(&section));
    if(!error)error=ax_view_repaint(assembly);
    ProXsec active={0};
    if(!error)error=ax_view_status("section active readback",ProXsecActiveGet(&active));
    if(!error&&(active.solid_owner!=(ProSolid)assembly||_wcsicmp(active.xsec_name,cmd->arg[2])))
        error=PRO_TK_GENERAL_ERROR;
    if(error&&created&&ProXsecDelete(&section))ax_uncertain(out);
    if(!error)part_text_add(out,L"已显示装配剖切 %ls，装配未保存。\n",cmd->arg[2]);
    return error;
}

static ProError ax_view_explosion_display(ProMdl assembly,int exploded){
    ProError error=exploded?ProAssemblyExplode((ProAssembly)assembly):ProAssemblyUnexplode((ProAssembly)assembly);
    if(error==PRO_TK_NO_CHANGE)error=PRO_TK_NO_ERROR;
    return error?error:ax_view_repaint(assembly);
}

static ProError ax_view_explode(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    if(cmd->count==3){
        ProError error=ax_view_explosion_display(assembly,0);
        if(!error)part_text_add(out,L"已恢复装配显示。\n");
        return error;
    }
    ProExpldstate state={0};
    ProName name;
    wcscpy_s(name,_countof(name),cmd->arg[2]);
    ProError error=ProExpldstateInit(name,0,(ProSolid)assembly,&state);
    if(cmd->count==4){
        if(error==PRO_TK_E_FOUND)error=PRO_TK_NO_ERROR;
        if(!error)error=ProExpldstateActivate((ProSolid)assembly,&state);
        if(!error)error=ax_view_explosion_display(assembly,1);
        if(!error)part_text_add(out,L"已显示爆炸视图 %ls。\n",name);
        return error;
    }
    if(error!=PRO_TK_E_NOT_FOUND)return error?error:PRO_TK_E_FOUND;
    ProExpldAnimMoveData *moves=NULL,*readback=NULL;
    int count=(cmd->count-3)/4,actual=0,created=0;
    ProAsmcomppath *paths[7]={0};
    AxComponent owners[7]={0};
    ProMatrix positions[7],current;
    error=ProArrayAlloc(count,sizeof(*moves),1,(ProArray *)&moves);
    for(int i=0;!error&&i<count;i++){
        int n=3+4*i;
        ProVector translation={as_arg(cmd,n+1)/mm,as_arg(cmd,n+2)/mm,as_arg(cmd,n+3)/mm};
        error=ax_component_resolve(assembly,cmd->arg[n],&owners[i]);
        if(!error)error=ax_actual_pose(assembly,&owners[i].feature,positions[i]);
        if(!error)error=ProArrayAlloc(1,sizeof(*paths[i]),1,(ProArray *)&paths[i]);
        if(!error){
            paths[i][0]=owners[i].path;
            error=ax_view_status("explode init move",ProExpldAnimDataTranslatemoveInit(translation,paths[i],&moves[i]));
        }
    }
    if(!error){
        error=ax_view_status("explode create",ProExpldstateCreate((ProSolid)assembly,name,moves,&state));
        created=!error;
    }
    if(!error)error=ax_view_status("explode readback",ProExpldstateMovesGet(&state,&readback));
    if(!error)error=ProArraySizeGet((ProArray)readback,&actual);
    if(!error&&actual!=count){trace("explode readback count",actual);error=PRO_TK_GENERAL_ERROR;}
    if(!error)error=ax_view_status("explode activate",ProExpldstateActivate((ProSolid)assembly,&state));
    if(!error)error=ax_view_status("explode verify assembled pose",ax_view_explosion_display(assembly,0));
    for(int i=0;!error&&i<count;i++){
        error=ax_actual_pose(assembly,&owners[i].feature,current);
        if(!error&&!ax_same_pose(positions[i],current,mm))error=ax_view_status("explode assembled pose changed",PRO_TK_GENERAL_ERROR);
    }
    if(!error)error=ax_view_status("explode display",ax_view_explosion_display(assembly,1));
    if(error&&created){
        ProAssemblyUnexplode((ProAssembly)assembly);
        if(ProExpldstateDelete(&state))ax_uncertain(out);
    }
    if(readback)ProExpldanimmovedataProarrayFree(readback);
    if(moves)ProArrayFree((ProArray *)&moves);
    for(int i=0;i<count;i++)if(paths[i])ProArrayFree((ProArray *)&paths[i]);
    if(!error)part_text_add(out,L"已建立并显示爆炸视图 %ls（%d 个模块）。实际装配位置已核对，未保存。\n",name,count);
    return error;
}
