
static ProError cbp_guard_envelope(const wchar_t *part_name,const wchar_t *axis_name,double size[3]){
    if(!ax_study_key(part_name)||wcslen(axis_name)!=1||axis_name[0]<L'X'||axis_name[0]>L'Z')return PRO_TK_BAD_INPUTS;
    ProMdl hinge=NULL;
    int loaded=0;
    ProError status=ax_load_part(part_name,&hinge,&loaded);
    ProOutline bounds;
    double mm;
    if(!status)status=or_outline(hinge,bounds);
    if(!status)status=qa_mm_per_unit(hinge,&mm);
    if(!status){
        int axis=axis_name[0]-L'X';
        for(int i=0;i<3;i++)size[i]=(bounds[1][(axis+i)%3]-bounds[0][(axis+i)%3])*mm;
    }
    if(loaded&&hinge)ProMdlErase(hinge);
    return status;
}

static ProError cbp_guard_clearance(CabinetPart *part,ProFeature walls[2],double expected_width,double required_height){
    CabinetFace front,back;
    ProError status=cm_wall_face(&walls[0],&front);
    if(!status)status=cm_wall_face(&walls[1],&back);
    if(status)return status;
    if(fabs(front.normal[1])<1-1e-8||fabs(back.normal[1])<1-1e-8)return PRO_TK_INCOMPLETE;
    ProVector separation;
    ad_sub(back.point,front.point,separation);
    const double mm=1/part->txn.scale;
    double width=fabs(separation[1])*mm;
    if(ad_dot(separation,front.normal)<0)width-=part->thickness;
    if(ad_dot(separation,back.normal)>0)width-=part->thickness;
    if(fabs(width-expected_width)>0.001)return PRO_TK_INCOMPLETE;

    ProSolidBody solid;
    ProOutline bounds;
    status=qa_single_body(part->txn.model,&solid);
    if(!status)status=ProSolidBodyOutlineGet(&solid,bounds);
    if(!status&&(bounds[1][2]*mm-part->thickness<required_height-0.001))status=PRO_TK_INCOMPLETE;
    if(status)return status;

    ProRay ray={{-part->txn.scale,part->depth*part->txn.scale/2,(part->thickness+required_height/2)*part->txn.scale},
        {1,0,0}};
    QualityRay hits;
    status=qa_ray(part->txn.model,&ray,0.0001*part->txn.scale,&hits);
    return status?status:hits.count?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static ProError cbp_hinge_guard(const wchar_t *name,const wchar_t *hinge,const wchar_t *axis,const double values[9],
    ProMdl *result,PartText *out){
    const double pin=values[0],side=values[1],deep=values[2],screw_l=values[3],screw_w=values[4],
        end_hole=values[5],dia=values[6],t=values[7],radius=values[8];
    *result=NULL;
    if(!ax_study_key(name)||!_wcsicmp(name,hinge)||pin<=0||side<=0||deep<=0||screw_l<=0||screw_w<=0||
        end_hole<=0||dia<=0||t<=0||radius<=0)return PRO_TK_BAD_INPUTS;
    double envelope[3];
    ProError status=cbp_guard_envelope(hinge,axis,envelope);
    if(status){part_text_add(out,L"无法测量指定铰链。请核对件号、工作目录和铰链轴向。\n");return status;}
    double w=envelope[0]+2*pin,inside=envelope[1]+2*side,d=inside+2*t,height=envelope[2]+deep;
    double hole_y=t+side/2;
    if(end_hole<=dia/2||end_hole+dia/2>=w/2||screw_l>=w||screw_w>=inside||
       hole_y-dia/2<=radius+t||hole_y+dia/2>=(d-screw_w)/2){
        part_text_add(out,L"拆装窗口或安装孔会进入折弯区、相互冲突或穿出板边。请核对窗口、侧净隙和孔边距。\n");
        return PRO_TK_BAD_INPUTS;
    }
    CabinetPart part={0};
    const wchar_t *stage=L"护架底板";
    status=cbp_seed(&part,name,w,d,t,1);
    ProFeature walls[2];
    for(int i=0;!status&&i<2;i++){
        stage=L"两侧防护折弯";
        CabinetEdge base,tip;
        status=cbp_stock_edge(&part,0,i,&base);
        if(!status)status=cbp_fold_up(&part,&base,i?L"GUARD_WALL_RIGHT":L"GUARD_WALL_LEFT",0,w,height+radius+t,radius,
            &walls[i],&tip);
    }
    PanelProfile window=pn_rect(w/2,d/2,screw_l,screw_w);
    if(!status){stage=L"螺钉拆装窗口";status=cbp_top_openings(&part,L"SCREW_ACCESS_PROFILE",L"SCREW_SERVICE_WINDOW",&window,1,1);}
    PanelProfile holes[]={cbp_circle(end_hole,hole_y,dia),cbp_circle(w-end_hole,hole_y,dia),
        cbp_circle(end_hole,d-hole_y,dia),cbp_circle(w-end_hole,d-hole_y,dia)};
    if(!status){stage=L"独立安装孔";status=cbp_top_openings(&part,L"GUARD_MOUNTS_PROFILE",L"GUARD_MOUNTING_HOLES",holes,4,1);}
    if(!status){stage=L"内净宽和拔销通道";status=cbp_guard_clearance(&part,walls,inside,height);}
    if(!status){stage=L"双折弯和板厚";status=cbp_finish(&part,2,radius);}
    if(!status)part_text_add(out,L"已生成 %ls。铰链 %ls 沿 %ls 轴包络 %.6g × %.6g × %.6g mm，两端各留 %.6g mm。螺钉窗口 %.6g × %.6g mm，四个独立安装孔同组，轴向拔销通道贯通。待保存。\n",
        name,hinge,axis,envelope[0],envelope[1],envelope[2],pin,screw_l,screw_w);
    return cbp_end(&part,status,stage,L"窄门快拆铰链护架",result,out);
}
