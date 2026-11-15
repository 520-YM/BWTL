
typedef struct {
    int kind,axis;
    double wanted,angle,radius,tolerance;
    const wchar_t *edge,*side,*reference,*name;
} SmxFlangeSpec;

static ProError smx_flange_measure(ProMdl model,const SmxFlangeSpec *spec,const CabinetEdge *edge,
    ProFeature *wall,double mm,double *value){
    if(spec->kind==SMX_OUTER){
        ProOutline outline;
        ProError error=ProSolidOutlineGet((ProSolid)model,outline);
        if(!error)*value=(outline[1][spec->axis]-outline[0][spec->axis])*mm;
        return error;
    }
    FdRef ref={0};
    ProVector point,normal;
    ProError error=fd_ref_resolve(model,spec->reference,PRO_SURFACE,&ref);
    if(!error)error=fd_plane_data(&ref.item,point,normal);
    if(!error&&!ad_unit(normal))error=PRO_TK_BAD_CONTEXT;
    if(!error&&spec->kind==SMX_INNER){
        CabinetFace face;
        double thickness;
        error=cm_wall_face(wall,&face);
        if(!error&&fabs(ad_dot(face.normal,normal))<0.99999)error=PRO_TK_BAD_CONTEXT;
        if(!error)error=fab_thickness(model,&mm,&thickness);
        if(!error){
            ProVector offset;
            ad_sub(face.point,point,offset);
            *value=fabs(ad_dot(offset,normal))*mm;
            ProSurface opposing;
            ProSmtSurfType opposite_side;
            error=ProGeomitemToSurface(&ref.item,&opposing);
            if(!error)error=ProSmtSurfaceTypeGet((ProPart)model,opposing,&opposite_side);
            if(!error&&opposite_side!=PRO_SMT_SURF_FACE&&opposite_side!=PRO_SMT_SURF_OFFSET)error=PRO_TK_BAD_CONTEXT;
            if(!error){
                /* Measure clear distance between the two facing sheet surfaces. */
                ProVector between;
                ad_sub(point,face.point,between);
                if(ad_dot(between,face.normal)<0)*value-=thickness;
                if(ad_dot(offset,normal)<0)*value-=thickness;
                if(*value<=0)error=PRO_TK_BAD_CONTEXT;
            }
        }
    }else if(!error){
        CabinetEdge tip;
        error=cm_wall_tip(model,wall,edge,&tip);
        if(!error){
            ProVector d1,d2;
            ad_sub(tip.a,point,d1);ad_sub(tip.b,point,d2);
            *value=ad_dot(d1,normal)*mm;
            if(fabs(*value-ad_dot(d2,normal)*mm)>spec->tolerance)error=PRO_TK_BAD_CONTEXT;
        }
    }
    fd_ref_free(&ref);
    return error;
}

static ProError smx_flange_add(ProMdl model,const SmxFlangeSpec *spec,double initial,FdTxn *tx,
    CabinetEdge *edge,ProFeature *wall){
    ProError error=sh_begin(tx,model,spec->name,PRO_FEAT_WALL);
    if(!error)error=cm_edge_get(model,spec->edge,edge);
    if(!error)error=cm_attach(tx,spec->name,edge,0,edge->length,initial,spec->angle,spec->radius,spec->side,wall);
    return error;
}

static ProError smx_flange_inset(ProFeature *wall,double inset,double scale){
    ProElement tree=NULL,position,type,value;
    ProError error=ProFeatureElemtreeExtract(wall,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error)error=sh_child(tree,PRO_E_SMT_WALL_HEIGHT,&position);
    if(!error)error=sh_child(position,PRO_E_SMT_WALL_HEIGHT_TYPE,&type);
    if(!error)error=ProElementIntegerSet(type,PRO_BEND_POSITION_OFFSET_BEND_START);
    if(!error){
        error=sh_child(position,PRO_E_SMT_WALL_HEIGHT_VALUE,&value);
        if(error==PRO_TK_E_NOT_FOUND)error=model_element(position,PRO_E_SMT_WALL_HEIGHT_VALUE,2,inset*scale,&value);
        else if(!error)error=ProElementDoubleSet(value,inset*scale);
    }
    if(!error)error=cm_apply_tree(wall,tree);
    if(tree)ProFeatureElemtreeFree(wall,tree);
    return error;
}

static ProError smx_sized_flange(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    SmxFlangeSpec spec={0};
    spec.kind=kind;spec.name=cmd->arg[2];spec.edge=cmd->arg[3];spec.side=cmd->arg[8];
    spec.axis=kind==SMX_OUTER?(int)(cmd->arg[4][0]-L'X'):0;
    spec.reference=kind==SMX_OUTER?NULL:cmd->arg[4];
    spec.wanted=as_arg(cmd,5);spec.angle=as_arg(cmd,6);spec.radius=as_arg(cmd,7);spec.tolerance=as_arg(cmd,9);
    if(kind==SMX_TO_PLANE)spec.wanted=0;
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(error)return error;
    double lower=fmax(2*spec.radius+thickness,3),upper=lower+fmax(thickness*2,4);
    DeliveryOutput work={0};
    FdTxn trial={0};
    CabinetEdge edge;
    ProFeature wall;
    ProMdl copy=NULL;
    double a=0,b=0,height=0,inset=0,measured=0;
    error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=smx_copy_scratch(model,&copy);
    if(!error)error=smx_flange_add(copy,&spec,lower,&trial,&edge,&wall);
    if(!error)error=smx_flange_measure(copy,&spec,&edge,&wall,mm,&a);
    if(!error&&fabs(a-spec.wanted)<=spec.tolerance)height=lower;
    if(!error&&!height&&kind==SMX_INNER){
        error=smx_flange_inset(&wall,thickness,1/mm);
        if(!error)error=smx_flange_measure(copy,&spec,&edge,&wall,mm,&a);
        if(!error)error=smx_flange_inset(&wall,2*thickness,1/mm);
        if(!error)error=smx_flange_measure(copy,&spec,&edge,&wall,mm,&b);
        if(!error&&fabs(b-a)<0.00001)error=PRO_TK_BAD_CONTEXT;
        if(!error)inset=thickness+thickness*(spec.wanted-a)/(b-a);
        if(!error&&(!isfinite(inset)||inset<0.0001||inset>1000))error=PRO_TK_BAD_CONTEXT;
        if(!error)error=smx_flange_inset(&wall,inset,1/mm);
        if(!error)error=smx_flange_measure(copy,&spec,&edge,&wall,mm,&measured);
        if(!error&&fabs(measured-spec.wanted)>spec.tolerance)error=PRO_TK_BAD_CONTEXT;
        if(!error)height=lower;
    }
    if(!error&&!height){
        error=cm_resize_wall(&trial,&wall,upper);
        if(!error)error=smx_flange_measure(copy,&spec,&edge,&wall,mm,&b);
        if(!error&&fabs(b-a)<0.00001)error=PRO_TK_BAD_CONTEXT;
        if(!error)height=lower+(upper-lower)*(spec.wanted-a)/(b-a);
        if(!error&&(!isfinite(height)||height<=spec.radius||height>10000))error=PRO_TK_BAD_INPUTS;
        if(!error)error=cm_resize_wall(&trial,&wall,height);
        if(!error)error=smx_flange_measure(copy,&spec,&edge,&wall,mm,&measured);
        if(!error&&fabs(measured-spec.wanted)>spec.tolerance)error=PRO_TK_BAD_CONTEXT;
    }
    fd_cleanup(&trial,0);
    if(copy){
        ProError erase=ProMdlErase(copy);
        if(!error)error=erase;
    }
    error=fab_discard(&work,error,out);
    if(error){
        part_text_add(out,L"副本未达到指定尺寸或参照不可用（Creo %d），原件保持原状。\n",error);
        return error;
    }
    FdTxn tx={0};
    error=smx_flange_add(model,&spec,height,&tx,&edge,&wall);
    if(!error&&inset)error=smx_flange_inset(&wall,inset,1/mm);
    if(!error)error=smx_flange_measure(model,&spec,&edge,&wall,mm,&measured);
    if(!error&&fabs(measured-spec.wanted)>spec.tolerance)error=PRO_TK_GENERAL_ERROR;
    if(!error)error=cm_bend_verified(&wall,spec.angle,spec.radius,1/mm);
    if(error&&tx.root){
        wchar_t rollback[768];
        am_finish(&tx,error,1,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",rollback);
    }else fd_cleanup(&tx,0);
    if(!error)part_text_add(out,L"翻边 %ls 已创建。直段输入 %.8g mm，成品%ls %.8g mm，允许偏差 %.8g mm，折角和内 R 已核对。未保存。\n",
        spec.name,height,kind==SMX_OUTER?L"外包":kind==SMX_INNER?L"净间距":L"端边到目标面距离",measured,spec.tolerance);
    return error;
}

static ProError smx_profile_chain(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(error)return error;
    FdTxn tx={0};
    error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_WALL);
    CabinetEdge current;
    if(!error)error=cm_edge_get(model,cmd->arg[3],&current);
    int segments=(cmd->count-4)/4;
    for(int i=0;!error&&i<segments;i++){
        int offset=4+i*4;
        ProName name;
        ProFeature wall;
        swprintf_s(name,_countof(name),L"%.24ls_%02d",cmd->arg[2],i+1);
        error=cm_attach(&tx,name,&current,0,current.length,as_arg(cmd,offset),as_arg(cmd,offset+1),
            as_arg(cmd,offset+2),cmd->arg[offset+3],&wall);
        if(!error)error=cm_bend_verified(&wall,as_arg(cmd,offset+1),as_arg(cmd,offset+2),1/mm);
        if(!error){
            CabinetEdge tip;
            error=cm_wall_tip(model,&wall,&current,&tip);
            if(!error)current=tip;
        }
    }
    if(error&&tx.root){
        wchar_t rollback[768];
        am_finish(&tx,error,1,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",rollback);
    }else fd_cleanup(&tx,0);
    if(!error)part_text_add(out,L"已建立 %d 段关联翻边，各段折角和内 R 已回读，末端边 EDGE:%d。未保存。\n",segments,current.id);
    return error;
}

static ProError smx_bend_values(ProMdl model,const wchar_t *token,ProSmtDvlLenCalcInfo *calc,
    double *radius,double *angle,double *developed,double *thickness,double *mm){
    FdRef ref={0};
    ProSurface surface;
    ProBool inside;
    ProError error=fab_thickness(model,mm,thickness);
    if(!error)error=qa_surface(model,token,1,&ref,&surface);
    if(!error)error=ProSmtBendsrfInfoGet((ProPart)model,surface,radius,&inside,angle,developed,calc);
    if(!error){
        *radius=*radius**mm-(inside?0:*thickness);
        *angle=fabs(*angle);
        *developed*=*mm;
        if(*radius<0||*angle<1||*developed<=0)error=PRO_TK_BAD_CONTEXT;
    }
    fd_ref_free(&ref);
    return error;
}

static ProError smx_bend_dimension(ProMdl model,const PartCommand *cmd,PartText *out){
    double r,angle,developed,t,mm;
    ProSmtDvlLenCalcInfo calc;
    ProError error=smx_bend_values(model,cmd->arg[2],&calc,&r,&angle,&developed,&t,&mm);
    if(error)return error;
    if(angle>=175)return PRO_TK_BAD_CONTEXT;
    double half=angle*AS_PI/360,inner=r*tan(half),outer=(r+t)*tan(half);
    part_text_add(out,L"折弯角 %.8g°，板厚 %.8g mm，内/外 R %.8g/%.8g mm，原生展开弧长 %.8g mm。\n",angle,t,r,r+t,developed);
    part_text_add(out,L"从切点到各自虚交点的退距：内侧 %.8g mm，外侧 %.8g mm；外尺寸口径折弯扣除 %.8g mm。\n",
        inner,outer,2*outer-developed);
    return PRO_TK_NO_ERROR;
}

static ProError smx_direction_compare(ProMdl model,const PartCommand *cmd,PartText *out){
    const wchar_t *directions[]={L"POS",L"NEG"};
    SmxMeasures m[2]={{0},{0}};
    for(int i=0;i<2;i++){
        DeliveryOutput work={0};
        ProMdl copy=NULL;
        ProError error=fab_scratch(&work);
        if(!error)error=dv_stage_enter(&work);
        if(!error)error=smx_copy_scratch(model,&copy);
        ProMdlName name;
        if(!error)error=ProMdlMdlnameGet(copy,name);
        if(!error)error=cm_native_command(copy,L"CREO_OP SMT_FLANGE %ls BWTL_DIR %ls %.12g %.12g %.12g %ls",
            name,cmd->arg[2],as_arg(cmd,3),as_arg(cmd,4),as_arg(cmd,5),directions[i]);
        if(!error)error=smx_measures(copy,cmd->arg[6],&m[i]);
        if(copy){
            ProError erase=ProMdlErase(copy);
            if(!error)error=erase;
        }
        error=fab_discard(&work,error,out);
        if(error)return error;
        part_text_add(out,L"%ls：成品 X/Y/Z %.8g/%.8g/%.8g mm，Z 最低/最高 %.8g/%.8g mm，展开 %.8g × %.8g mm。\n",
            directions[i],m[i].bounds[0],m[i].bounds[1],m[i].bounds[2],m[i].low[2],m[i].high[2],m[i].flat[0],m[i].flat[1]);
    }
    part_text_add(out,L"两种折向均在临时副本生成并回读，原件未改。\n");
    return PRO_TK_NO_ERROR;
}

static ProError smx_radius_check(ProMdl model,const PartCommand *cmd,PartText *out){
    FabBends bends={0};
    bends.model=model;
    ProError error=fab_thickness(model,&bends.mm,&bends.thickness);
    if(!error)error=ProSolidSurfaceVisit((ProSolid)model,fab_bend_visit,NULL,&bends);
    if(error)return error;
    SmxMaterial material={0};
    error=smx_material_get(cmd->arg[2],bends.thickness,&material,out);
    if(error)return error;
    ProLine stored;
    error=smx_text_get(model,L"BWTL_GRAIN_VECTOR",stored);
    if(error){part_text_add(out,L"请先登记可核实的板材纹向。\n");return error;}
    ProVector grain;
    if(swscanf_s(stored,L"%lf %lf %lf",&grain[0],&grain[1],&grain[2])!=3||!ad_unit(grain))return PRO_TK_BAD_CONTEXT;
    int fail=0;
    for(int i=0;i<bends.count;i++){
        const FabBend *bend=&bends.rows[i];
        ProSurface surface;
        ProGeomitemdata *raw=NULL;
        error=ProSurfaceInit((ProSolid)model,bend->surface,&surface);
        if(!error)error=ProSurfaceDataGet(surface,&raw);
        if(error)return error;
        ProVector axis;
        memcpy(axis,raw->data.p_surface_data->srf_shape.cylinder.e3,sizeof(axis));
        ProGeomitemdataFree(&raw);
        if(!ad_unit(axis))return PRO_TK_BAD_CONTEXT;
        double degrees=acos(qa_clamp(fabs(ad_dot(axis,grain))))*180/AS_PI;
        double limit=degrees<=as_arg(cmd,4)?material.parallel:degrees>=90-as_arg(cmd,4)?material.cross:fmax(material.parallel,material.cross);
        limit=fmax(limit,as_arg(cmd,5));
        int outside=bend->radius+0.0001<limit;
        fail+=outside;
        part_text_add(out,L"%ls，面 %d，轴与纹向 %.8g°，内 R %.8g，要求至少 %.8g，差 %+.8g mm，%ls。\n",
            bend->name,bend->surface,degrees,bend->radius,limit,bend->radius-limit,outside?L"不符合":L"符合");
    }
    part_text_add(out,L"共核对 %d 道，%d 道低于规则下限。规则版本 %ls。\n",bends.count,fail,material.version);
    return PRO_TK_NO_ERROR;
}

static ProError smx_allowance_report(ProMdl model,PartText *out){
    FabBends bends={0};
    bends.model=model;
    ProError error=fab_thickness(model,&bends.mm,&bends.thickness);
    if(!error)error=ProSolidSurfaceVisit((ProSolid)model,fab_bend_visit,NULL,&bends);
    if(error)return error;
    for(int i=0;i<bends.count;i++){
        const FabBend *bend=&bends.rows[i];
        ProSurface surface;
        ProSmtDvlLenCalcInfo calc={0};
        error=ProSurfaceInit((ProSolid)model,bend->surface,&surface);
        if(!error)error=ProSmtBendsrfInfoGet((ProPart)model,surface,NULL,NULL,NULL,NULL,&calc);
        if(error)return error;
        const wchar_t *scope=calc.driven_by_part_settings?L"零件设置":L"特征覆盖";
        part_text_add(out,L"%ls / 面 %d：%ls，算法编号 %d，实际展开弧长 %.8g mm。",
            bend->name,bend->surface,scope,calc.method,bend->developed);
        if(calc.method==PRO_DVL_LEN_DRIVEN_BY_BEND_TABLE)part_text_add(out,L"表 %ls。",calc.bend_table);
        if(calc.method==PRO_DVL_LEN_DRIVEN_BY_Y_FACTOR)part_text_add(out,L"Y 因子 %.8g。",calc.y_factor_value);
        part_text_add(out,L"\n");
    }
    part_text_add(out,L"读取 %d 道原生折弯的补偿来源。\n",bends.count);
    return PRO_TK_NO_ERROR;
}

static ProError smx_coupon(const PartCommand *cmd,PartText *out){
    double t=as_arg(cmd,2),r=as_arg(cmd,3),angle=as_arg(cmd,4)*AS_PI/180;
    double flat=as_arg(cmd,5),outside=as_arg(cmd,6);
    double setback=(r+t)*tan(angle/2),bd=outside-flat,ba=2*setback-bd;
    double factor=(ba/angle-r)/t;
    if(!isfinite(factor)||factor<0||factor>1||ba<=0){
        part_text_add(out,L"试片数据反算 K=%.8g 超出 0–1，或展开弧长不为正。请核对板厚、折角和两外尺寸之和。\n",factor);
        return PRO_TK_BAD_INPUTS;
    }
    part_text_add(out,L"试片输入：板厚 %.8g mm，内 R %.8g mm，转角 %.8g°，实测下料 %.8g mm，折后两侧外尺寸之和 %.8g mm。\n",
        t,r,as_arg(cmd,4),flat,outside);
    part_text_add(out,L"外侧退距 %.8g mm，候选 BD %.8g mm，BA %.8g mm，K %.8g。此结果供试片记录复核，未改公司表或零件。\n",
        setback,bd,ba,factor);
    return PRO_TK_NO_ERROR;
}
