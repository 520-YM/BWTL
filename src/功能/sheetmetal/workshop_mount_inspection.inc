typedef struct {
    double x,y,min_d,max_d,max_offset,min_web;
} SmyMountHole;

typedef struct {
    const wchar_t *role,*key;
    SmyMountHole holes[12];
    int count;
    wchar_t revision[32];
} SmyMountInspection;

static int smy_mount_inspection_row(wchar_t **fields,void *context){
    SmyMountInspection *rule=context;
    if(!smy_mount_inspection_role(fields[0])||!dv_safe_leaf(fields[1])||!dv_safe_leaf(fields[8]))return 0;
    double v[6];
    for(int i=0;i<6;i++){
        if(!smx_number(fields[i+2],i<2?-1e5:i<5?0.001:0,i<2?1e5:10000))return 0;
        v[i]=wcstod(fields[i+2],NULL);
    }
    if(v[2]>v[3])return 0;
    if(_wcsicmp(rule->role,fields[0])||_wcsicmp(rule->key,fields[1]))return 1;
    if(rule->count==_countof(rule->holes)||(rule->count&&_wcsicmp(rule->revision,fields[8])))return 0;
    for(int i=0;i<rule->count;i++)if(hypot(rule->holes[i].x-v[0],rule->holes[i].y-v[1])<0.001)return 0;
    if(!rule->count)wcscpy_s(rule->revision,_countof(rule->revision),fields[8]);
    rule->holes[rule->count++]=(SmyMountHole){v[0],v[1],v[2],v[3],v[4],v[5]};
    return 1;
}

static ProError smy_mount_inspection(ProMdl model,const PartCommand *cmd,PartText *out){
    SmyMountInspection rule={cmd->arg[2],cmd->arg[3]};
    ProError error=smx_table(L"BWTL_SMT_MOUNT_CHECKS.tsv",9,smy_mount_inspection_row,&rule,out);
    if(error)return error;
    if(rule.count<2||cmd->count-6!=rule.count){
        part_text_add(out,L"模板须登记至少两个定位孔，并按模板顺序指定全部圆孔壁。模板孔数 %d，提供 %d。\n",rule.count,cmd->count-6);
        return PRO_TK_BAD_INPUTS;
    }
    ProCsysdata frame;
    error=in_box_frame(model,cmd->arg[5],&frame);
    if(error)return error;
    int failed=0;
    for(int i=0;i<rule.count;i++){
        ShopMouth mouth={0};
        double mm,t;
        error=smy_access_mouth(model,cmd->arg[4],cmd->arg[6+i],&mouth,&mm,&t);
        if(error)return error;
        ProVector p,axis;
        smy_align_local(&mouth,&frame,mm,p,axis);
        if(axis[2]<0.999999||fabs(p[2])>0.001){
            part_text_add(out,L"安装坐标系原点应位于实际安装面上，Z 轴沿该面外法向。\n");
            return PRO_TK_BAD_CONTEXT;
        }
        InspectionSeat scan={&mouth,1/mm,INFINITY};
        error=ProSurfaceContourVisit(mouth.ring.face,in_seat_contour,NULL,&scan);
        if(error)return error;
        if(!isfinite(scan.nearest))return PRO_TK_E_NOT_FOUND;
        const SmyMountHole *target=&rule.holes[i];
        double deviation=hypot(p[0]-target->x,p[1]-target->y);
        double diameter=2*mouth.radius,web=scan.nearest-mouth.radius;
        int pass=deviation<=target->max_offset+0.000001&&diameter+0.000001>=target->min_d&&diameter<=target->max_d+0.000001&&
            web+0.000001>=target->min_web;
        failed+=!pass;
        part_text_add(out,L"孔 %d %ls：中心 (%.8g, %.8g)，目标 (%.8g, %.8g)，中心偏差 %.8g / %.8g mm，孔径 %.8g mm，最小料桥 %.8g mm，%ls。\n",
            i+1,cmd->arg[6+i],p[0],p[1],target->x,target->y,deviation,target->max_offset,diameter,web,pass?L"符合":L"超限");
    }
    part_text_add(out,L"%ls / %ls（%ls），共 %d 孔，超限 %d。检查安装几何。\n",rule.role,rule.key,rule.revision,rule.count,failed);
    return PRO_TK_NO_ERROR;
}
