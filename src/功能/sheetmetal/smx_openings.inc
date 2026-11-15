
#define SMX_MAX_OPENINGS 48
typedef struct {
    ProMdl model;
    ProSurface surface;
    PanelFrame frame;
    PanelProfile profile;
    double mm,nearest;
    int edge,skip,bends_only,exterior,all_boundaries;
} SmxClearance;

static ProError smx_clearance_edge(ProEdge edge,ProError status,ProAppData context){
    if(status)return status;
    SmxClearance *measure=context;
    int id;
    ProEnttype type;
    ProVector a,b,d1,d2,d3;
    ProError error=ProEdgeIdGet(edge,&id);
    if(error||id==measure->skip)return error;
    if(measure->bends_only){
        ProSurface neighbors[2];
        error=ProEdgeNeighborsGet(edge,NULL,NULL,&neighbors[0],&neighbors[1]);
        if(error)return error;
        ProSurface opposite=neighbors[0]==measure->surface?neighbors[1]:neighbors[0];
        double angle=0;
        if(!opposite||ProSmtBendsrfInfoGet((ProPart)measure->model,opposite,NULL,NULL,&angle,NULL,NULL)||fabs(angle)<1)
            return PRO_TK_NO_ERROR;
    }else if(!measure->exterior&&!measure->all_boundaries)return PRO_TK_NO_ERROR;
    if((error=ProEdgeTypeGet(edge,&type))||
       (error=ProEdgeXyzdataEval(edge,0,a,d1,d2,d3))||
       (error=ProEdgeXyzdataEval(edge,1,b,d1,d2,d3)))return error;
    double distance;
    if(type==PRO_ENT_LINE)distance=fx_segment_distance(as_point(measure->profile.x,measure->profile.y),
        fx_local_xy(&measure->frame,a,1/measure->mm),fx_local_xy(&measure->frame,b,1/measure->mm));
    else if(type==PRO_ENT_ARC||type==PRO_ENT_CIRCLE){
        error=fx_arc_distance(edge,&measure->frame,1/measure->mm,a,b,&measure->profile,&distance);
        if(error)return error;
    }else return PRO_TK_INVALID_TYPE;
    if(distance<measure->nearest){measure->nearest=distance;measure->edge=id;}
    return PRO_TK_NO_ERROR;
}

static ProError smx_clearance_contour(ProContour contour,ProError status,ProAppData context){
    if(status)return status;
    SmxClearance *measure=context;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(contour,&traversal);
    if(error)return error;
    measure->exterior=traversal==PRO_CONTOUR_TRAV_EXTERNAL;
    return ProContourEdgeVisit(measure->surface,contour,smx_clearance_edge,NULL,measure);
}

static double smx_profile_bound(const PanelProfile *p){
    if(p->shape==PN_CIRCLE||p->shape==PN_FLAT)return p->width/2;
    if(p->shape==PN_CAPSULE)return p->width/2;
    if(p->shape==PN_KEY)return fmax(p->width/2,p->detail+p->height/2);
    if(p->shape==PN_NOTCH)return fmax(p->width/2,hypot(p->width/2+p->detail,p->height/2));
    return hypot(p->width,p->height)/2;
}

static ProError smx_outline_clearance(FdTxn *tx,const PanelFrame *frame,const PanelProfile *profile,double minimum){
    SmxClearance measure={0};
    measure.model=tx->model;measure.mm=1/tx->scale;measure.frame=*frame;measure.profile=*profile;
    measure.nearest=INFINITY;
    ProError error=ProSurfaceInit((ProSolid)tx->model,frame->face_id,&measure.surface);
    if(!error)error=ProSurfaceContourVisit(measure.surface,smx_clearance_contour,NULL,&measure);
    if(error)return error;
    return !isfinite(measure.nearest)||measure.nearest-smx_profile_bound(profile)<minimum-0.0001?PRO_TK_BAD_CONTEXT:PRO_TK_NO_ERROR;
}

static int smx_profile_type(const wchar_t *shape,double x,double y,double width,double height,double detail,PanelProfile *profile){
    if(!wcscmp(shape,L"ROUND")&&height==0&&detail==0)*profile=(PanelProfile){PN_CIRCLE,x,y,width,width};
    else if(!wcscmp(shape,L"SLOT")&&height>0&&width>height)*profile=(PanelProfile){PN_CAPSULE,x,y,width,height,0,detail*AS_PI/180};
    else if(!wcscmp(shape,L"D")&&height==0&&detail>0&&detail<width/2)*profile=(PanelProfile){PN_FLAT,x,y,width,width,detail};
    else if(!wcscmp(shape,L"ROUNDRECT")&&height>0&&detail>0&&2*detail<fmin(width,height))
        *profile=(PanelProfile){PN_ROUNDED,x,y,width,height,detail};
    else if(!wcscmp(shape,L"KEY")&&height>0&&height<width&&detail>width/2)
        *profile=(PanelProfile){PN_KEY,x,y,width,height,detail,0};
    else if(!wcscmp(shape,L"NOTCH")&&height>0&&height<width&&detail>0)
        *profile=(PanelProfile){PN_NOTCH,x,y,width,height,detail,0};
    else return 0;
    return width>0;
}

typedef struct {
    const wchar_t *key;
    double x,y;
    wchar_t revision[32];
    PanelProfile profiles[SMX_MAX_OPENINGS];
    int count;
} SmxMount;

static int smx_mount_row(wchar_t **f,void *context){
    SmxMount *mount=context;
    if(!smx_word(f[0])||!smx_word(f[1])||!smx_number(f[3],-1e5,1e5)||!smx_number(f[4],-1e5,1e5)||
       !smx_number(f[5],0.01,10000)||!smx_number(f[6],0,10000)||!smx_number(f[7],-360,10000))return 0;
    double x,y,w,h,detail;
    part_number(f[3],&x);part_number(f[4],&y);part_number(f[5],&w);part_number(f[6],&h);part_number(f[7],&detail);
    PanelProfile profile;
    if(!smx_profile_type(f[2],x+mount->x,y+mount->y,w,h,detail,&profile))return 0;
    if(_wcsicmp(f[0],mount->key))return 1;
    if(mount->count==_countof(mount->profiles)||(mount->count&&_wcsicmp(mount->revision,f[1])))return 0;
    if(!mount->count)wcscpy_s(mount->revision,_countof(mount->revision),f[1]);
    mount->profiles[mount->count++]=profile;
    return 1;
}

static ProError smx_openings_apply(ProMdl model,const PartCommand *cmd,const PanelProfile *rows,int count,
    double margin,int clip,PartText *out){
    FdTxn tx={0};
    PanelFrame frame={0};
    PanelProfile accepted[SMX_MAX_OPENINGS];
    ProMatrix sketch_frame;
    ProName sketch;
    double mm,thickness,area=0;
    int used=0,skipped=0,mutated=0;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_CUT);
    if(!error)error=pn_frame(&tx,cmd,&frame);
    for(int i=0;!error&&i<count;i++){
        ProError eligibility=pn_profile_probes(&tx,&frame,&rows[i],thickness,0);
        if(!eligibility)eligibility=smx_outline_clearance(&tx,&frame,&rows[i],margin);
        for(int j=0;!eligibility&&j<used;j++)
            if(hypot(rows[i].x-accepted[j].x,rows[i].y-accepted[j].y)<
                smx_profile_bound(&rows[i])+smx_profile_bound(&accepted[j])+margin)
                eligibility=PRO_TK_BAD_CONTEXT;
        if(eligibility){
            if(clip&&eligibility==PRO_TK_BAD_CONTEXT){
                skipped++;
                if(skipped<=20)part_text_add(out,L"跳过 (%.8g, %.8g) mm：未满足实体、边界或料桥条件。\n",rows[i].x,rows[i].y);
                continue;
            }
            part_text_add(out,L"孔位 (%.8g, %.8g) mm 未通过边界、材料或料桥检查，整组未切。\n",rows[i].x,rows[i].y);
            error=eligibility;
            break;
        }
        accepted[used++]=rows[i];
        area+=pn_area(&rows[i]);
    }
    if(!error&&!used)error=PRO_TK_E_NOT_FOUND;
    if(!error){
        swprintf_s(sketch,_countof(sketch),L"%.25ls_SK",tx.name);
        mutated=1;
        error=pn_sketch(model,cmd,&frame,sketch,accepted,used,1,sketch_frame);
    }
    if(!error)error=pn_cut(&tx,&frame,tx.name,sketch,sketch_frame,thickness,1);
    if(!error)error=md_expected_volume(&tx,-area*thickness);
    for(int i=0;!error&&i<used;i++)error=pn_profile_probes(&tx,&frame,&accepted[i],thickness,1);
    if(error&&mutated){
        wchar_t rollback[768];
        am_finish(&tx,error,1,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",rollback);
    }else fd_cleanup(&tx,0);
    if(!error)part_text_add(out,L"已创建 %d 个贯穿轮廓，跳过 %d 个。板厚、去料体积和逐孔穿透已回读，未保存。\n",used,skipped);
    return error;
}

static ProError smx_opening_patterns(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    SmxMount mount={0};
    double margin;
    if(kind==SMX_MOUNT){
        mount.key=cmd->arg[6];mount.x=as_arg(cmd,7);mount.y=as_arg(cmd,8);margin=as_arg(cmd,9);
        ProError error=smx_table(L"BWTL_SMT_MOUNTS.tsv",8,smx_mount_row,&mount,out);
        if(error)return error;
        if(!mount.count){part_text_add(out,L"安装孔模板 %ls 未登记。\n",mount.key);return PRO_TK_E_NOT_FOUND;}
        part_text_add(out,L"安装孔模板 %ls，版本 %ls。\n",mount.key,mount.revision);
    }else if(kind==SMX_INTERFACE){
        margin=as_arg(cmd,12);
        mount.count=1;
        if(!smx_profile_type(cmd->arg[6],as_arg(cmd,7),as_arg(cmd,8),as_arg(cmd,9),as_arg(cmd,10),as_arg(cmd,11),
            &mount.profiles[0]))return PRO_TK_BAD_INPUTS;
    }else{
        int nx,ny;
        part_integer(cmd->arg[8],&nx);part_integer(cmd->arg[9],&ny);
        double x=as_arg(cmd,6),y=as_arg(cmd,7),dx=as_arg(cmd,10),dy=as_arg(cmd,11);
        margin=as_arg(cmd,16);
        for(int j=0;j<ny;j++)for(int i=0;i<nx;i++)
            if(!smx_profile_type(cmd->arg[12],x+i*dx,y+j*dy,as_arg(cmd,13),as_arg(cmd,14),as_arg(cmd,15),
                &mount.profiles[mount.count++]))return PRO_TK_BAD_INPUTS;
    }
    return smx_openings_apply(model,cmd,mount.profiles,mount.count,margin,kind==SMX_CLIP_GRID,out);
}

static ProError smx_bore_center(ProMdl model,const InspectionPlane *plane,const wchar_t *token,
    QualityBore *bore,ProVector center){
    FdRef ref={0};
    ProError error=qa_bore(model,token,&ref,bore);
    if(!error){
        double projected=ad_dot(plane->normal,bore->axis);
        if(fabs(projected)<1e-8)error=PRO_TK_BAD_CONTEXT;
        else{
            ProVector delta;
            ad_sub(plane->point,bore->origin,delta);
            cm_point(bore->origin,bore->axis,ad_dot(delta,plane->normal)/projected,center);
        }
    }
    fd_ref_free(&ref);
    return error;
}

static ProError smx_hole_check(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    InspectionPlane plane;
    double mm;
    ProError error=in_plane(model,cmd->arg[2],&plane);
    if(!error)error=qa_mm_per_unit(model,&mm);
    QualityBore bore;
    ProVector center;
    if(!error)error=smx_bore_center(model,&plane,cmd->arg[3],&bore,center);
    if(error)return error;
    double angle=acos(qa_clamp(fabs(ad_dot(plane.normal,bore.axis))))*180/AS_PI;
    if(kind==SMX_NORMAL){
        double max=as_arg(cmd,4);
        part_text_add(out,L"圆孔轴与指定板面的法向夹角 %.8g°，要求 ≤ %.8g°，%ls。交点 (%.8g, %.8g, %.8g) mm。\n",
            angle,max,angle<=max?L"符合":L"不符合",center[0]*mm,center[1]*mm,center[2]*mm);
        return PRO_TK_NO_ERROR;
    }
    if(angle>0.01)return PRO_TK_BAD_CONTEXT;
    SmxClearance measure={0};
    measure.model=model;measure.mm=mm;measure.surface=plane.handle;measure.nearest=INFINITY;
    measure.bends_only=kind==SMX_BEND_CLEARANCE;
    memcpy(measure.frame.placement.csys.origin,center,sizeof(ProVector));
    memcpy(measure.frame.placement.csys.z_vector,plane.normal,sizeof(ProVector));
    ProVector seed={1,0,0};
    if(fabs(plane.normal[0])>0.9){seed[0]=0;seed[1]=1;}
    ad_cross(seed,plane.normal,measure.frame.placement.csys.x_vector);
    if(!ad_unit(measure.frame.placement.csys.x_vector))return PRO_TK_BAD_CONTEXT;
    ad_cross(plane.normal,measure.frame.placement.csys.x_vector,measure.frame.placement.csys.y_vector);
    error=ProSurfaceContourVisit(plane.handle,smx_clearance_contour,NULL,&measure);
    if(error)return error;
    double minimum=as_arg(cmd,4),distance=measure.nearest-bore.radius*mm;
    if(isfinite(measure.nearest))part_text_add(out,L"到最近%ls EDGE:%d 的净距 %.8g mm，下限 %.8g mm，%ls。\n",
        measure.bends_only?L"折弯切边":L"外轮廓",measure.edge,distance,minimum,distance>=minimum?L"符合":L"不符合");
    else part_text_add(out,L"所选板面未找到相应边界，结果待核实。\n");
    if(kind==SMX_WEB_CLEARANCE&&cmd->count==6){
        QualityBore other;
        ProVector other_center;
        error=smx_bore_center(model,&plane,cmd->arg[5],&other,other_center);
        if(error)return error;
        if(fabs(ad_dot(plane.normal,other.axis))<0.999999)return PRO_TK_BAD_CONTEXT;
        double separation=cm_distance(center,other_center);
        if(separation*mm<0.0001){
            part_text_add(out,L"两处选中的圆柱面属于同一孔轴，请重新指定另一个孔。\n");
            return PRO_TK_BAD_CONTEXT;
        }
        double web=(separation-bore.radius-other.radius)*mm;
        part_text_add(out,L"指定两个圆孔之间最薄料桥 %.8g mm，%ls。\n",web,web>=minimum?L"符合":L"不符合");
    }
    return PRO_TK_NO_ERROR;
}
