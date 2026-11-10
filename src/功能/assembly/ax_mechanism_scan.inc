static void ax_motion_pose(ProMatrix start,const AxGeometry *line,double angle,double shift,ProMatrix pose){
    double c=cos(angle*AS_PI/180),s=sin(angle*AS_PI/180);
    ProVector relative,rotated;
    memcpy(pose,start,sizeof(ProMatrix));
    for(int i=0;i<3;i++)ax_rotate_vector(start[i],line->direction,c,s,pose[i]);
    ad_sub(start[3],line->point,relative);
    ax_rotate_vector(relative,line->direction,c,s,rotated);
    for(int i=0;i<3;i++)pose[3][i]=line->point[i]+rotated[i]+shift*line->direction[i];
}

static ProError ax_motion_sample_fit(ProSelection moving,ProSelection fixed,double mm,double *gap,int *collision){
    ProBoolean interfering=PRO_B_FALSE;
    Pro3dPnt nearest[2];
    double distance=0;
    *gap=0;*collision=0;
    ProError error=ProFitClearanceCompute(moving,PRO_FIT_INCLUDE_NONE,fixed,PRO_FIT_INCLUDE_NONE,
        PRO_FIT_WHOLE_SURFACE,NULL,PRO_B_FALSE,&distance,NULL,&interfering,nearest);
    if(error)return error;
    if(interfering){
        double volume;
        error=ax_interference_volume(moving,fixed,&volume);
        /* A native positive interference flag is an obstruction even if
           the optional intersection-volume operation cannot resolve it. */
        *collision=error||volume*mm*mm*mm>0.001;
    }else{
        if(!isfinite(distance)||distance<0)return PRO_TK_BAD_CONTEXT;
        *gap=distance*mm;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_motion_sweep(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxComponent moving={0},fixed={0},a={0},b={0};
    ProMdl scratch=NULL;
    ProSolid transient=NULL;
    ProMdlName name;
    ProMatrix origin,obstacle,pose,actual;
    ProSelection sa=NULL,sb=NULL;
    AxGeometry line;
    ProWVerstamp stamp=NULL,moving_stamp=NULL,fixed_stamp=NULL;
    static unsigned serial;
    int general=kind==AX_MOTION_SWEEP;
    int rotate=kind==AX_DOOR_SWEEP||(general&&!wcscmp(cmd->arg[5],L"ROTATE"));
    int index=general?8:6,count=(int)as_arg(cmd,index);
    int collisions=0,failures=0,first_collision=-1,first_failed=-1;
    double low=general?as_arg(cmd,6):0,high=as_arg(cmd,general?7:5);
    double limit=as_arg(cmd,index+1),smallest=INFINITY;
    ProError error;
#define MS(call) do{if((error=(call))){trace("motion sweep " #call,error);goto done;}}while(0)
    MS(ax_hw_occurrence(assembly,cmd->arg[2],mm,&moving));
    MS(ax_hw_occurrence(assembly,cmd->arg[3],mm,&fixed));
    if(ax_same_occurrence(&moving,&fixed)||fabs(high-low)<1e-9||
       (rotate&&(fabs(low)>360||fabs(high)>360))){error=PRO_TK_BAD_INPUTS;goto done;}
    MS(dv_no_external_dependencies(moving.model));
    if(fixed.model!=moving.model)MS(dv_no_external_dependencies(fixed.model));
    MS(ax_device_line(&moving,cmd->arg[4],1,&line));
    MS(ax_device_pose(&moving,origin));
    MS(ax_device_pose(&fixed,obstacle));
    MS(ProMdlVerstampGet(assembly,&stamp));
    MS(ProMdlVerstampGet(moving.model,&moving_stamp));
    MS(ProMdlVerstampGet(fixed.model,&fixed_stamp));
    swprintf_s(name,_countof(name),L"bw_swp_%lx_%x",GetCurrentProcessId(),++serial);
    MS(ProSolidMdlnameCreate(name,PRO_MDLFILE_ASSEMBLY,&transient));
    scratch=(ProMdl)transient;
    double scratch_mm;
    MS(qa_mm_per_unit(scratch,&scratch_mm));
    if(!part_close(scratch_mm,mm)){
        part_text_add(out,L"临时装配与原装配的长度单位不一致，未开始检查。\n");
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    MS(ProAsmcompAssemble((ProAssembly)scratch,(ProSolid)moving.model,origin,&a.feature));
    MS(ProAsmcompAssemble((ProAssembly)scratch,(ProSolid)fixed.model,obstacle,&b.feature));
    MS(part_regenerate(scratch));
    a.model=moving.model;b.model=fixed.model;
    ProIdTable ai={a.feature.id},bi={b.feature.id};
    MS(ProAsmcomppathInit((ProSolid)scratch,ai,1,&a.path));
    MS(ProAsmcomppathInit((ProSolid)scratch,bi,1,&b.path));
    MS(ax_part_selection(&a,&sa));
    MS(ax_part_selection(&b,&sb));
    for(int i=0;i<count;i++){
        double position=low+(high-low)*i/(count-1),gap;
        int collision;
        ax_motion_pose(origin,&line,rotate?position:0,rotate?0:position/mm,pose);
        MS(ax_set_position(scratch,&a.feature,pose));
        MS(ax_actual_pose(scratch,&a.feature,actual));
        if(!ax_same_pose(pose,actual,mm)){error=PRO_TK_GENERAL_ERROR;goto done;}
        MS(ax_motion_sample_fit(sa,sb,mm,&gap,&collision));
        smallest=fmin(smallest,gap);
        if(collision){if(first_collision<0)first_collision=i;collisions++;}
        if(collision||gap+0.001<limit){if(first_failed<0)first_failed=i;failures++;}
        part_text_add(out,L"%.8g %ls：%ls，间隙 %.8g mm。\n",position,rotate?L"度":L"mm",
            collision?L"有干涉":gap+0.001<limit?L"间隙不足":L"通过",gap);
    }
    part_text_add(out,L"完成 %d 个离散姿态，干涉 %d 个、未满足间隙 %d 个，最小间隙 %.8g mm。\n",count,collisions,failures,smallest);
    if(first_collision>=0)part_text_add(out,L"首次采样干涉位置 %.8g %ls。\n",low+(high-low)*first_collision/(count-1),rotate?L"度":L"mm");
    if(first_failed>=0)part_text_add(out,L"首次未通过位置 %.8g %ls。\n",low+(high-low)*first_failed/(count-1),rotate?L"度":L"mm");
done:
    if(sa)ProSelectionFree(&sa);
    if(sb)ProSelectionFree(&sb);
    if(scratch){
        ProError erased=ProMdlErase(scratch);
        if(erased&&!error)error=erased;
    }
    ProWVerstamp before[]={stamp,moving_stamp,fixed_stamp};
    ProMdl originals[]={assembly,moving.model,fixed.model};
    for(int i=0;i<3;i++)if(before[i]){
        ProWVerstamp after=NULL;
        if(ProMdlVerstampGet(originals[i],&after)||!ProVerstampEqual(before[i],after)){
            error=PRO_TK_BAD_CONTEXT;
            ax_uncertain(out);
        }
        if(after)ProVerstampFree(&after);
        ProVerstampFree(&before[i]);
    }
    if(!error)part_text_add(out,L"已核对原装配及两零件的模型版本保持。\n");
    return error;
#undef MS
}
