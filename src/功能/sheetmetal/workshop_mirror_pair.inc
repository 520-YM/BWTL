static int smy_pair_diff(const FabDxf *reference,const FabDxf *other,int axis,double datum,double tolerance){
    unsigned char used[FAB_DXF_MAX]={0};
    int matches=0;
    for(int i=0;i<reference->count;i++){
        FabEntity expected=reference->entities[i];
        if(axis>=0){
            if(expected.type==1){
                expected.a[axis]=2*datum-expected.a[axis];
                expected.b[axis]=2*datum-expected.b[axis];
            }else{
                expected.center[axis]=2*datum-expected.center[axis];
                if(expected.type==2){
                    expected.start=fab_degrees((axis==0?180:0)-expected.start-expected.sweep);
                    fab_arc_point(&expected,expected.start,expected.a);
                    fab_arc_point(&expected,expected.start+expected.sweep,expected.b);
                }
            }
        }
        for(int j=0;j<other->count;j++){
            if(used[j]||!fab_same_entity(&expected,&other->entities[j],tolerance))continue;
            used[j]=1;
            matches++;
            break;
        }
    }
    return reference->count+other->count-2*matches;
}

static ProError smy_mirror_pair(ProMdl model,const PartCommand *cmd,PartText *out){
    ProMdl other=NULL;
    int loaded=0;
    ProError error=ax_load_part(cmd->arg[2],&other,&loaded);
    double mm[2],t[2];
    if(!error)error=fab_thickness(model,&mm[0],&t[0]);
    if(!error)error=fab_thickness(other,&mm[1],&t[1]);
    FabDxf *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));
    if((!a||!b)&&!error)error=PRO_TK_OUT_OF_MEMORY;
    DeliveryOutput scratch={0};
    if(!error)error=fab_scratch(&scratch);
    if(!error)error=dv_stage_enter(&scratch);
    ProPath path;
    if(!error)error=fab_flat_read(model,scratch.stage,L"pair_first",cmd->arg[3],a,path);
    if(!error)error=fab_flat_read(other,scratch.stage,L"pair_second",cmd->arg[4],b,path);
    if(!error){
        double datum=as_arg(cmd,6),tolerance=as_arg(cmd,7);
        int axis=!wcscmp(cmd->arg[5],L"Y");
        int mirrored=smy_pair_diff(a,b,axis,datum,tolerance);
        int same=smy_pair_diff(a,b,-1,0,tolerance);
        int equal_thickness=fabs(t[0]-t[1])<=tolerance;
        const wchar_t *state=!equal_thickness?L"板厚不符":
            !mirrored&&!same?L"几何对称，无法区分左右":
            !mirrored?L"展开互为镜像":!same?L"同向件，疑似重复发放":
            L"不是指定基准下的镜像配对";
        part_text_add(out,L"实测板厚 %.8g / %.8g mm，按 %ls=%.8g mm 比较。镜像差异 %d 个，同向差异 %d 个。%ls。\n",
            t[0],t[1],cmd->arg[5],datum,mirrored,same,state);
        if(a->bends||b->bends)part_text_add(out,L"当前结论基于展开图。需另核对折后朝向和装配参照。\n");
    }
    free(a);free(b);
    error=fab_discard(&scratch,error,out);
    if(loaded&&other){
        ProError erased=ProMdlErase(other);
        if(!error)error=erased;
    }
    return error;
}

