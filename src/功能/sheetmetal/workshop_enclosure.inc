typedef struct {
    double low[3],high[3];
    int count;
} SmyModuleExtent;

static int smy_module_row(wchar_t **f,void *context){
    SmyModuleExtent *modules=context;
    if(!dv_safe_leaf(f[0]))return 0;
    double lo[3],hi[3],reserve[3];
    for(int i=0;i<3;i++){
        if(!smx_number(f[1+i],-1e5,1e5)||!smx_number(f[4+i],-1e5,1e5)||!smx_number(f[7+i],0,1000))return 0;
        lo[i]=wcstod(f[1+i],NULL);hi[i]=wcstod(f[4+i],NULL);reserve[i]=wcstod(f[7+i],NULL);
        if(hi[i]<=lo[i])return 0;
    }
    for(int i=0;i<3;i++){
        modules->low[i]=fmin(modules->low[i],lo[i]-reserve[i]);
        modules->high[i]=fmax(modules->high[i],hi[i]+reserve[i]);
    }
    modules->count++;
    return 1;
}

static ProError smy_case_internal(ProMdl model,const wchar_t *name,double l,double w,double mm,double t,double actual[3]){
    ProName sketch_name;
    ProMatrix frame;
    FdSketch sketch={0};
    swprintf_s(sketch_name,_countof(sketch_name),L"%ls_SK",name);
    ProError error=fd_sketch_open(model,sketch_name,&sketch,0);
    if(!error)error=ProSectionLocationGet(sketch.section,frame);
    fd_sketch_free(&sketch);
    if(error)return error;
    ProVector middle;
    for(int i=0;i<3;i++)middle[i]=frame[3][i]+(l*frame[0][i]+w*frame[1][i])/(2*mm);
    double sides[2][2]={{INFINITY,INFINITY},{INFINITY,INFINITY}},height=INFINITY;
    for(int i=1;i<=4;i++){
        wchar_t wall_name[32];
        swprintf_s(wall_name,_countof(wall_name),L"%ls_W%d",name,i);
        ProFeature wall;
        CabinetFace face;
        error=cm_feature_find(model,wall_name,&wall);
        if(!error)error=cm_wall_face(&wall,&face);
        if(error)return error;
        ProVector delta;
        ad_sub(middle,face.point,delta);
        int axis=fabs(ad_dot(face.normal,frame[0]))>0.999999?0:fabs(ad_dot(face.normal,frame[1]))>0.999999?1:-1;
        if(axis<0)return PRO_TK_BAD_CONTEXT;
        double d=ad_dot(delta,frame[axis])*mm;
        int side=d<0;
        double inward=fabs(d)-(ad_dot(delta,face.normal)<0?t:0);
        if(inward<=0||isfinite(sides[axis][side]))return PRO_TK_BAD_CONTEXT;
        sides[axis][side]=inward;
        ProSurface surface;
        ProVector lo,hi;
        error=ProSurfaceInit((ProSolid)model,face.id,&surface);
        if(!error)error=ProSurfaceExtremesEval(surface,frame[2],lo,hi);
        if(error)return error;
        double start=ad_dot(frame[3],frame[2]);
        double top=fmax(fabs(ad_dot(lo,frame[2])-start),fabs(ad_dot(hi,frame[2])-start))*mm;
        height=fmin(height,top-t);
    }
    for(int i=0;i<2;i++){
        if(!isfinite(sides[i][0])||!isfinite(sides[i][1]))return PRO_TK_BAD_CONTEXT;
        actual[i]=sides[i][0]+sides[i][1];
    }
    actual[2]=height;
    return isfinite(height)&&height>0?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError smy_case_space(ProMdl model,const PartCommand *cmd,PartText *out){
    SmyModuleExtent modules={{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY},0};
    wchar_t leaf[64];
    swprintf_s(leaf,_countof(leaf),L"%ls.tsv",cmd->arg[9]);
    ProError error=smx_table(leaf,10,smy_module_row,&modules,out);
    if(error)return error;
    double target[3],size[3],actual[3],mm;
    double t=as_arg(cmd,5),radius=as_arg(cmd,6),retreat=as_arg(cmd,7);
    if(retreat<radius+t)return PRO_TK_BAD_INPUTS;
    for(int i=0;i<3;i++){
        double extra=as_arg(cmd,10+i);
        target[i]=modules.high[i]-modules.low[i]+2*extra;
        size[i]=target[i]+(i==2?t:2*t);
    }
    error=qa_mm_per_unit(model,&mm);
    FdTxn tx={0};
    if(!error)error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_WALL);
    if(!error&&!tx.was_empty){fd_cleanup(&tx,0);return PRO_TK_BAD_CONTEXT;}
    wchar_t protocol[1200],receipt[PART_TEXT_MAX]=L"";
    PartCommand tray;
    swprintf_s(protocol,_countof(protocol),L"CREO_OP CASE_TRAY %ls %ls %ls %ls %.12g %.12g %.12g %.12g %.12g %.12g %ls",
        cmd->arg[1],cmd->arg[2],cmd->arg[3],cmd->arg[4],size[0],size[1],t,size[2],radius,retreat,cmd->arg[8]);
    if(!error&&part_command_parse(protocol,&tray)!=1)error=PRO_TK_BAD_INPUTS;
    if(!error)error=cm_execute(&tray,model,receipt,_countof(receipt));
    if(!error)error=smy_case_internal(model,cmd->arg[2],size[0],size[1],mm,t,actual);
    if(!error){
        part_text_add(out,L"%d 个内装件按工作表留量及箱体四周余量合并，要求净空间 %.8g × %.8g × %.8g mm；原生壁面净空间保守值 %.8g × %.8g × %.8g mm。\n",
            modules.count,target[0],target[1],target[2],actual[0],actual[1],actual[2]);
        for(int i=0;i<3;i++)if(actual[i]+0.001<target[i])error=PRO_TK_BAD_CONTEXT;
    }
    if(!error){
        wchar_t layout[130];
        swprintf_s(layout,_countof(layout),L"%.9g %.9g %.9g / %.9g %.9g %.9g",modules.low[0],modules.low[1],
            modules.low[2],modules.high[0],modules.high[1],modules.high[2]);
        SmxParameter tag[]={{L"BWTL_CASE_MODULE_TABLE",cmd->arg[9]},{L"BWTL_CASE_MODULE_BOUNDS",layout}};
        error=smx_params(model,tag,_countof(tag),0,out);
    }
    if(error&&tx.root){
        wchar_t rollback[1000];
        am_finish(&tx,error,0,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n%ls\n",receipt,rollback);
    }else fd_cleanup(&tx,0);
    if(!error)part_text_add(out,L"已创建 %ls，保留内装件表标识和合并包络。托盘为开口箱体，未保存。\n",cmd->arg[2]);
    return error;
}

static ProError smy_case_display(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm,t;
    ProError error=fab_thickness(model,&mm,&t);
    if(error)return error;
    double view_w=as_arg(cmd,8),view_h=as_arg(cmd,9),glass_w=as_arg(cmd,10),glass_h=as_arg(cmd,11);
    double gap=as_arg(cmd,12),overlap=as_arg(cmd,13),border=as_arg(cmd,14);
    double mask_x=(glass_w-view_w)/2,mask_y=(glass_h-view_h)/2;
    if(mask_x<overlap+gap||mask_y<overlap+gap){
        part_text_add(out,L"玻璃在最大定位偏移下的遮边小于指定压框搭接，窗口未创建。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    double foot_w=glass_w+2*(gap+border),foot_h=glass_h+2*(gap+border);
    PartCommand display;
    wchar_t protocol[1200],receipt[PART_TEXT_MAX]=L"";
    swprintf_s(protocol,_countof(protocol),L"CREO_OP PN_DISPLAY %ls %ls %ls %ls %ls %.12g %.12g %.12g %.12g %.12g %.12g %.12g %.12g",
        cmd->arg[1],cmd->arg[2],cmd->arg[3],cmd->arg[4],cmd->arg[5],as_arg(cmd,6),as_arg(cmd,7),t,view_w,view_h,foot_w,foot_h,border);
    if(part_command_parse(protocol,&display)!=1)return PRO_TK_BAD_INPUTS;
    error=pn_execute(&display,model,1,receipt,_countof(receipt));
    part_text_add(out,L"%ls\n",receipt);
    if(!error)part_text_add(out,L"实际可视窗口 %.8g × %.8g mm；玻璃设计 %.8g × %.8g mm，单侧定位余量 %.8g mm，最差遮边 X/Y %.8g / %.8g mm，所需搭接 %.8g mm。已核对外周承托区。\n",
        view_w,view_h,glass_w,glass_h,gap,mask_x-gap,mask_y-gap,overlap);
    return error;
}
