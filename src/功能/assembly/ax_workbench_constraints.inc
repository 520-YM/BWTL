#define AW_MAX_REFS 192
typedef struct {int id;ProType type;AxGeometry geometry;} AwReference;
typedef struct {int count,limited;AwReference refs[AW_MAX_REFS];} AwReferences;

static ProError aw_collect_geometry(ProGeomitem *item,ProError status,ProAppData context){
    AwReferences *list=context;
    if(status)return status;
    if(list->count>=AW_MAX_REFS){list->limited=1;return PRO_TK_NO_ERROR;}
    AwReference *ref=&list->refs[list->count];
    if(ax_geometry(item,&ref->geometry))return PRO_TK_NO_ERROR;
    ref->id=item->id;
    ref->type=item->type;
    list->count++;
    return PRO_TK_NO_ERROR;
}
static ProError aw_collect_feature(ProFeature *feature,ProError status,ProAppData context){
    if(status)return status;
    ProFeatStatus state;
    ProError error=ProFeatureStatusGet(feature,&state);
    if(error||state!=PRO_FEAT_ACTIVE)return error;
    error=ProFeatureGeomitemVisit(feature,PRO_SURFACE,aw_collect_geometry,NULL,context);
    if(!error||error==PRO_TK_E_NOT_FOUND)error=ProFeatureGeomitemVisit(feature,PRO_AXIS,aw_collect_geometry,NULL,context);
    return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}
static ProError aw_collect_world(AxComponent *component,AwReferences *refs){
    ProMatrix pose;
    ProError error=ax_device_pose(component,pose);
    if(!error)error=ProSolidFeatVisit((ProSolid)component->model,aw_collect_feature,NULL,refs);
    if(error)return error;
    for(int i=0;i<refs->count;i++){
        AxGeometry *shape=&refs->refs[i].geometry;
        ProVector point,direction;
        if((error=ProPntTrfEval(shape->point,pose,point))||
           (error=ProVectorTrfEval(shape->direction,pose,direction)))return error;
        if(!ad_unit(direction))return PRO_TK_BAD_CONTEXT;
        memcpy(shape->point,point,sizeof(point));
        memcpy(shape->direction,direction,sizeof(direction));
    }
    return PRO_TK_NO_ERROR;
}
static ProError aw_pair_references(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent a={0},b={0};
    AwReferences first={0},second={0};
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&a);
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[3],mm,&b);
    if(!error)error=aw_collect_world(&a,&first);
    if(!error)error=aw_collect_world(&b,&second);
    if(error)return error;
    int planes=0,coaxial=0;
    const double tolerance=as_arg(cmd,4),angle=as_arg(cmd,5);
    for(int i=0;i<first.count;i++)for(int j=0;j<second.count;j++){
        const AwReference *x=&first.refs[i],*y=&second.refs[j];
        const AxGeometry *u=&x->geometry,*v=&y->geometry;
        const int plane=u->type==AX_GEOM_PLANE;
        if(plane!=(v->type==AX_GEOM_PLANE)||ax_q_angle(u->direction,v->direction)>angle)continue;
        ProVector delta;
        ad_sub(v->point,u->point,delta);
        double projection=ad_dot(delta,u->direction);
        double distance=fabs(projection)*mm;
        if(!plane){
            for(int k=0;k<3;k++)delta[k]-=projection*u->direction[k];
            distance=sqrt(ad_dot(delta,delta))*mm;
        }
        if(distance>tolerance)continue;
        if(plane)planes++;else coaxial++;
        if(planes+coaxial<=48){
            part_text_add(out,L"%ls %ls:%d ↔ %ls %ls:%d，%ls，%ls %.8g mm。\n",cmd->arg[2],
                x->type==PRO_AXIS?L"AXIS_ID":L"SURFACE",x->id,cmd->arg[3],y->type==PRO_AXIS?L"AXIS_ID":L"SURFACE",y->id,
                plane?L"平面":L"轴线",plane?L"平面偏移":L"轴线偏心",distance);
        }
    }
    part_text_add(out,L"找到 %d 对平面和 %d 对轴线候选。公差依据当前位姿，装配前须核对接触范围及功能面。\n",planes,coaxial);
    if(first.limited||second.limited||planes+coaxial>48)part_text_add(out,L"参照或候选明细达到上限。\n");
    return PRO_TK_NO_ERROR;
}

static int aw_same_reference(ProSelection a,ProSelection b){
    ProModelitem x,y;
    ProAsmcomppath ap={0},bp={0};
    if(!a||!b||ProSelectionModelitemGet(a,&x)||ProSelectionModelitemGet(b,&y))return 0;
    if(x.owner!=y.owner||x.type!=y.type||x.id!=y.id)return 0;
    const ProError ea=ProSelectionAsmcomppathGet(a,&ap),eb=ProSelectionAsmcomppathGet(b,&bp);
    if(ea||eb)return ea==eb;
    return ap.table_num==bp.table_num&&!memcmp(ap.comp_id_table,bp.comp_id_table,ap.table_num*sizeof(int));
}

static ProError aw_diagnose(ProMdl assembly,const wchar_t *token,int kind,double mm,PartText *out){
    AxComponent component={0};
    ProError error=ax_component_resolve(assembly,token,&component);
    if(error)return error;
    ProAsmcomppath owner;
    if((error=ProAsmcomppathInit((ProSolid)assembly,component.path.comp_id_table,component.path.table_num-1,&owner)))return error;
    ProAsmcompconstraint *rows=NULL;
    int count=0;
    error=ax_read_constraints(&component.feature,&owner,&rows,&count);
    if(error)goto done;
    ProBoolean under=PRO_B_FALSE,packaged=PRO_B_FALSE;
    if((error=ProAsmcompIsUnderconstrained(&component.feature,&under))||
       (error=ProAsmcompIsPackaged(&component.feature,&packaged)))goto done;
    if(kind==AW_FREEDOM){
        if(!count&&(under||packaged))part_text_add(out,L"%ls 无定位约束，可沿三轴平移并绕三轴旋转。\n",token);
        else if(!under)part_text_add(out,L"%ls 有 %d 条约束，Creo 未报告刚体欠约束。\n",token,count);
        else if(count==1){
            ProAsmcompConstrType type;
            ProSelection own=NULL;
            ProModelitem item;
            ProDatumside side;
            if((error=ProAsmcompconstraintTypeGet(rows[0],&type)))goto done;
            if((error=ProAsmcompconstraintCompreferenceGet(rows[0],&own,&side)))goto done;
            AxGeometry geometry;
            ProMatrix pose;
            ProVector direction;
            if(!own){error=PRO_TK_E_NOT_FOUND;goto done;}
            if((error=ProSelectionModelitemGet(own,&item))||(error=ax_geometry(&item,&geometry))||
               (error=ax_device_pose(&component,pose))||(error=ProVectorTrfEval(geometry.direction,pose,direction)))goto done;
            if((type==PRO_ASM_INSERT||type==PRO_ASM_ALIGN)&&geometry.type!=AX_GEOM_PLANE)
                part_text_add(out,L"%ls 可沿同轴方向 (%.6g, %.6g, %.6g) 平移并绕其转动。端面可限制轴向移动，侧面可限制转动。\n",
                    token,direction[0],direction[1],direction[2]);
            else if((type==PRO_ASM_MATE||type==PRO_ASM_ALIGN||ax_offset_type(type))&&geometry.type==AX_GEOM_PLANE)
                part_text_add(out,L"%ls 可在定位面内滑动并绕法向 (%.6g, %.6g, %.6g) 转动。加不平行的定位面或定位轴可进一步限制。\n",
                    token,direction[0],direction[1],direction[2]);
            else part_text_add(out,L"Creo 报告 %ls 欠约束，现有约束类型的自由度需要进一步分析。\n",token);
        }else part_text_add(out,L"Creo 报告 %ls 欠约束，%d 条约束的组合自由度尚不能逐轴判定。\n",token,count);
        if(!error)error=ax_placement_status(assembly,token,mm,out);
        goto done;
    }

    int missing=0,conflicts=0,duplicates=0;
    for(int i=0;i<count;i++){
        ProAsmcompConstrType type;
        ProSelection base=NULL,part=NULL;
        ProDatumside base_side,part_side;
        if((error=ProAsmcompconstraintTypeGet(rows[i],&type)))goto done;
        if(type==PRO_ASM_FIX)continue;
        if(ProAsmcompconstraintAsmreferenceGet(rows[i],&base,&base_side)||
           ProAsmcompconstraintCompreferenceGet(rows[i],&part,&part_side)||
           !base||!part||ProSelectionVerify(base)||ProSelectionVerify(part)){
            part_text_add(out,L"约束 %d（%ls）至少一侧参照失效或不可读。\n",i+1,ax_constraint_label(type));
            missing++;
            continue;
        }
        for(int j=0;j<i;j++){
            ProAsmcompConstrType other;
            ProSelection previous_base=NULL,previous_part=NULL;
            ProDatumside previous_base_side,previous_part_side;
            if(ProAsmcompconstraintTypeGet(rows[j],&other)||other!=type||
               ProAsmcompconstraintAsmreferenceGet(rows[j],&previous_base,&previous_base_side)||
               ProAsmcompconstraintCompreferenceGet(rows[j],&previous_part,&previous_part_side)||
               !aw_same_reference(base,previous_base)||!aw_same_reference(part,previous_part))continue;
            double a=0,b=0;
            if(ax_offset_type(type)&&(ProAsmcompconstraintOffsetGet(rows[i],&a)||ProAsmcompconstraintOffsetGet(rows[j],&b)))continue;
            const int conflict=fabs((a-b)*mm)>1e-6||base_side!=previous_base_side||part_side!=previous_part_side;
            conflicts+=conflict;
            duplicates+=!conflict;
            part_text_add(out,L"约束 %d ↔ %d 使用同对参照，%ls（偏距 %.8g / %.8g mm）。\n",j+1,i+1,
                conflict?L"偏距或参照侧不一致":L"重复定义",b*mm,a*mm);
        }
    }
    part_text_add(out,L"%ls 已核对 %d 条约束，参照失效或不可读 %d 条，不一致 %d 对，重复 %d 对。检查覆盖同类型、同对参照的约束。\n",
        token,count,missing,conflicts,duplicates);
done:
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return error;
}
