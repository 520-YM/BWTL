static ProError quick_seed(ProMdlName name,ProMdl *model);

static ProError ax_layout_token_datum(ProMdl model,const wchar_t *line){
    PartCommand parsed;
    wchar_t report[PART_TEXT_MAX]=L"";
    if(part_command_parse(line,&parsed)!=1)return PRO_TK_BAD_INPUTS;
    return fd_create_datum(&parsed,model,report,_countof(report));
}

static ProError ax_layout_plane(ProMdl model,const wchar_t *name,const wchar_t *reference,double offset){
    wchar_t line[512];
    swprintf_s(line,_countof(line),L"CREO_OP DATUM_PLANE TARGET %ls %ls %.10g",name,reference,offset?offset:1);
    if(offset)return ax_layout_token_datum(model,line);
    PartCommand parsed;
    wchar_t report[PART_TEXT_MAX]=L"";
    if(part_command_parse(line,&parsed)!=1)return PRO_TK_BAD_INPUTS;
    wcscpy_s(parsed.arg[4],PART_TOKEN_MAX,L"0");
    return fd_create_datum(&parsed,model,report,_countof(report));
}

static ProError ax_layout_csys(ProMdl model,const wchar_t *name,const wchar_t *reference,double x,double y,double z){
    wchar_t line[512];
    swprintf_s(line,_countof(line),L"CREO_OP DATUM_CSYS TARGET %ls %ls %.10g %.10g %.10g 0 0 0",name,reference,x,y,z);
    return ax_layout_token_datum(model,line);
}

static ProError ax_layout_axis(ProMdl model,const wchar_t *name,const wchar_t *a,const wchar_t *b){
    wchar_t line[512];
    swprintf_s(line,_countof(line),L"CREO_OP DATUM_AXIS TARGET %ls PLANES %ls %ls",name,a,b);
    return ax_layout_token_datum(model,line);
}

static ProError ax_layout_point(ProMdl model,const wchar_t *name,const wchar_t *reference,double x,double y,double z){
    wchar_t line[512];
    swprintf_s(line,_countof(line),L"CREO_OP DATUM_POINT TARGET %ls %ls %.10g %.10g %.10g",name,reference,x,y,z);
    return ax_layout_token_datum(model,line);
}

static ProError ax_layout_single_use(ProMdl assembly,ProMdl model){
    AxScan *scan=NULL;
    ProError error=ax_scan(assembly,0,&scan);
    if(error)return error;
    int count=0;
    for(int i=0;i<scan->count;i++)if(scan->occurrences[i].model==model)count++;
    int incomplete=scan->limited||scan->unavailable||scan->suppressed;
    free(scan);
    return count==1&&!incomplete?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_layout_datums(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    ProMdl model=assembly;
    AxComponent owner={0};
    ProName names[7];
    const wchar_t *suffix[]={L"ORIGIN",L"PLANE",L"AXIS",L"H1",L"H2",L"H3",L"H4"};
    int module_interface=kind==AX_LAYOUT_INTERFACE,created=0,count=module_interface?7:3;
    const wchar_t *prefix=cmd->arg[module_interface?3:2];
    const wchar_t *csys=cmd->arg[module_interface?4:3];
    const wchar_t *a=cmd->arg[module_interface?5:4],*b=cmd->arg[module_interface?6:5];
    const wchar_t *plane=cmd->arg[module_interface?7:6];
    double z=as_arg(cmd,module_interface?10:7),plane_direction=1;
    ProError error;
    if(module_interface){
        if((error=ax_component_resolve(assembly,cmd->arg[2],&owner))||
           (error=ax_ordinary_direct(&owner.feature))||(error=ax_layout_single_use(assembly,owner.model)))return error;
        model=owner.model;
    }
    for(int i=0;i<count;i++){
        swprintf_s(names[i],_countof(names[i]),L"%ls_%ls",prefix,suffix[i]);
        if((error=ux_new_name(model,names[i])))return error;
    }
    /* Check that the datum axes and plane meet at the coordinate origin before applying the shared offset. */
    ProSelection selected=NULL;
    ProCsysdata frame;
    error=ax_csys_selection(model,csys,0,&selected,&frame);
    if(selected)ProSelectionFree(&selected);
    if(error)return error;
    const wchar_t *refs[]={a,b,plane};
    ProVector axis_normals[2];
    for(int i=0;i<3;i++){
        FdRef ref={0};
        ProVector point,normal,delta;
        error=fd_ref_resolve(model,refs[i],PRO_SURFACE,&ref);
        if(!error)error=fd_plane_data(&ref.item,point,normal);
        fd_ref_free(&ref);
        if(error)return error;
        ad_sub(point,frame.origin,delta);
        double dot=ad_dot(normal,frame.z_vector);
        if(fabs(ad_dot(delta,normal))>1e-5/mm)return PRO_TK_BAD_CONTEXT;
        if(i<2){
            if(fabs(dot)>1e-7)return PRO_TK_BAD_CONTEXT;
            memcpy(axis_normals[i],normal,sizeof(ProVector));
        }else{
            if(fabs(dot)<1-1e-7)return PRO_TK_BAD_CONTEXT;
            plane_direction=dot<0?-1:1;
        }
    }
    if(fabs(ad_dot(axis_normals[0],axis_normals[1]))>1e-7)return PRO_TK_BAD_CONTEXT;
    if((error=ax_layout_csys(model,names[0],csys,0,0,z)))goto done;
    created++;
    if((error=ax_layout_plane(model,names[1],plane,z*plane_direction)))goto done;
    created++;
    if((error=ax_layout_axis(model,names[2],a,b)))goto done;
    created++;
    if(module_interface){
        wchar_t origin[PRO_NAME_SIZE+6];
        swprintf_s(origin,_countof(origin),L"CSYS:%ls",names[0]);
        for(int i=0;i<4;i++){
            error=ax_layout_point(model,names[i+3],origin,(i&1?0.5:-0.5)*as_arg(cmd,8),
                (i&2?0.5:-0.5)*as_arg(cmd,9),0);
            if(error)goto done;
            created++;
        }
        if((error=part_regenerate(assembly)))goto done;
    }
    part_text_add(out,L"已建立%ls %ls。原点 %ls，安装面 %ls，中心轴 %ls。",
        module_interface?L"模块接口":L"总装安装基准",prefix,names[0],names[1],names[2]);
    if(module_interface)part_text_add(out,L"已创建 %ls_H1～H4 四个孔位基准点。",prefix);
    part_text_add(out,L"重生成通过，未保存。\n");
done:
    if(error&&created){
        int failed=0;
        for(int i=created-1;i>=0;i--){
            ProFeature feature;
            if(ux_named_feature(model,names[i],&feature)||model_remove_feature((ProSolid)model,feature.id))failed=1;
        }
        if(part_regenerate(model)||part_regenerate(assembly))failed=1;
        for(int i=0;i<count;i++)if(ux_new_name(model,names[i]))failed=1;
        if(failed)ax_uncertain(out);
    }
    return error;
}

typedef struct {ProDimension dim;double wanted;int count;} AxLayoutDimension;
static ProError ax_layout_linear_dimension(ProDimension *dim,ProError status,ProAppData context){
    AxLayoutDimension *result=context;
    if(status)return status;
    ProDimensiontype type;
    double value;
    if(!ProDimensionTypeGet(dim,&type)&&type==PRODIMTYPE_LINEAR&&!ProDimensionValueGet(dim,&value)&&
       part_close(value,result->wanted)){
        result->dim=*dim;
        result->count++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_layout_station(ProMdl seed,int index,double value,double mm){
    const wchar_t *names[]={L"BWTL_WIDTH",L"BWTL_DEPTH",L"BWTL_HEIGHT"};
    const wchar_t *symbols[]={L"BW_WIDTH",L"BW_DEPTH",L"BW_HEIGHT"};
    ProError error=ax_layout_csys(seed,names[index],L"CSYS:PRT_CSYS_DEF",index==0?value:0,index==1?value:0,index==2?value:0);
    ProFeature feature;
    AxLayoutDimension dim={.wanted=value/mm};
    if(!error)error=ux_named_feature(seed,names[index],&feature);
    if(!error)error=ProFeatureDimensionVisit(&feature,ax_layout_linear_dimension,NULL,&dim);
    if(error)return error;
    return dim.count==1?ProDimensionSymbolSet(&dim.dim,(wchar_t *)symbols[index]):PRO_TK_BAD_CONTEXT;
}

static ProError ax_layout_build(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    ProMdl model=NULL,seed=NULL,existing=NULL;
    ProMdlName name,seed_name;
    ProBoolean skeleton=PRO_B_FALSE,construction=PRO_B_FALSE;
    ProError error;
    int native_created=0;
    AxBatchItem item={0};
#define LB(call) do{if((error=(call))){trace("layout build " #call,error);goto done;}}while(0)
    if(!part_close(mm,1))return PRO_TK_BAD_CONTEXT;
    wcscpy_s(name,_countof(name),cmd->arg[2]);
    if(!part_target_available(NULL,name,L"prt")||!ProMdlnameInit(name,PRO_MDL_PART,&existing))return PRO_TK_E_FOUND;
    if(kind==AX_LAYOUT_ENVELOPE){
        const char *stage="construction envelope";
        ModelPlan plan={0,as_arg(cmd,7),as_arg(cmd,8),as_arg(cmd,9),0,1};
        ProSolidBody body;
        LB(quick_seed(name,&model));
        LB(model_build_internal(&plan,name,&stage,model,NULL,NULL));
        LB(ProSolidDefaultBodyGet((ProSolid)model,&body));
        LB(ProSolidBodyConstructionSet(&body));
        LB(ProSolidBodyIsConstruction(&body,&construction));
        if(!construction){error=PRO_TK_BAD_CONTEXT;goto done;}
        ProModelitem owner;
        ProParameter parameter;
        ProParamvalue role={.type=PRO_PARAM_STRING};
        wcscpy_s(role.value.s_val,PRO_LINE_SIZE,cmd->arg[3]);
        LB(ProMdlToModelitem(model,&owner));
        LB(ProParameterWithUnitsCreate(&owner,L"BWTL_LAYOUT_ROLE",&role,NULL,&parameter));
        item.model=model;
        for(int i=0;i<4;i++)item.pose[i][i]=1;
        for(int i=0;i<3;i++)item.pose[3][i]=as_arg(cmd,4+i)/mm;
        LB(ax_batch_create(assembly,&item,1,mm,1,out));
        part_text_add(out,L"已建立%ls %ls，尺寸 %.8g × %.8g × %.8g mm，采用 Creo 构造主体。\n",
            !wcscmp(cmd->arg[3],L"SPACE")?L"空间包络":L"禁入包络",name,plan.a,plan.b,plan.height);
    }else{
        error=ProAsmSkeletonGet((ProAssembly)assembly,&existing);
        if(!error)return PRO_TK_E_FOUND;
        if(error!=PRO_TK_E_NOT_FOUND)return error;
        swprintf_s(seed_name,_countof(seed_name),L"bw_seed_%lx_%lx",GetCurrentProcessId(),GetTickCount());
        if(!part_target_available(NULL,seed_name,L"prt"))return PRO_TK_E_FOUND;
        LB(quick_seed(seed_name,&seed));
        for(int i=0;i<3;i++)LB(ax_layout_station(seed,i,as_arg(cmd,3+i),mm));
        LB(ax_layout_plane(seed,L"BWTL_MOUNT_PLANE",L"PLANE:FRONT",0));
        LB(ax_layout_point(seed,L"BWTL_H1",L"CSYS:BWTL_WIDTH",-10,10,0));
        LB(ax_layout_point(seed,L"BWTL_H2",L"CSYS:BWTL_WIDTH",-10,as_arg(cmd,4)-10,0));
        LB(ax_layout_point(seed,L"BWTL_H3",L"CSYS:BWTL_DEPTH",10,-10,0));
        LB(ax_layout_point(seed,L"BWTL_H4",L"CSYS:BWTL_HEIGHT",10,10,0));
        LB(ProAsmSkeletonMdlnameCreate((ProAssembly)assembly,name,seed,&model));
        native_created=1;
        LB(ProMdlIsSkeleton(model,&skeleton));
        LB(part_regenerate(assembly));
        if(!skeleton){error=PRO_TK_BAD_CONTEXT;goto done;}
        AxScan *scan=NULL;
        LB(ax_scan(assembly,0,&scan));
        int id=0;
        for(int i=0;i<scan->count;i++)if(scan->occurrences[i].depth==1&&scan->occurrences[i].model==model)id=scan->occurrences[i].ids[0];
        free(scan);
        if(!id){error=PRO_TK_E_NOT_FOUND;goto done;}
        const wchar_t *symbols[]={L"BW_WIDTH",L"BW_DEPTH",L"BW_HEIGHT"};
        for(int i=0;i<3;i++){
            UxDimension check;
            LB(ux_dimension(model,symbols[i],&check));
            if(!part_close(check.value*mm,as_arg(cmd,3+i))){error=PRO_TK_GENERAL_ERROR;goto done;}
        }
        part_text_add(out,L"已建立 Creo 原生骨架 %ls，ID:%d。宽/深/高控制尺寸 BW_WIDTH、BW_DEPTH、BW_HEIGHT 已读回，三个定位站、安装面和四个孔位基准点已建立。\n",name,id);
    }
done:
    if(error&&native_created&&(ProAsmSkeletonDelete((ProAssembly)assembly)||part_regenerate(assembly)))ax_uncertain(out);
    if(error&&model&&!ax_recovery_uncertain)ProMdlErase(model);
    if(seed)ProMdlErase(seed);
    return error;
#undef LB
}