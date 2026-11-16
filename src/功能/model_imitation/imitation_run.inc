#include "imitation_geometry.inc"
#include "imitation_build.inc"
#include "imitation_general.inc"
#include "imitation_sheet_rebuild.inc"
#include "imitation_acceptance.inc"
#include "imitation_replay.inc"
#include "imitation_infer_flat.inc"
#include "imitation_learning_store.inc"
#include "imitation_learning_capture.inc"

typedef struct {
    int features,imports,datums,dimensions;
    ProName csys;
} ImitationSourceInfo;

static ProError imitation_source_feature(ProFeature *f,ProError status,ProAppData data){
    if(status)return status;
    ImitationSourceInfo *info=data;
    ProFeattype type;
    if(ProFeatureTypeGet(f,&type))return PRO_TK_NO_ERROR;
    info->features++;
    info->imports+=type==PRO_FEAT_IMPORT;
    info->datums+=type==PRO_FEAT_DATUM;
    if(type==PRO_FEAT_CSYS&&!info->csys[0])ProModelitemNameGet((ProModelitem *)f,info->csys);
    return PRO_TK_NO_ERROR;
}

static ProError imitation_source_dimension(ProDimension *d,ProError status,ProAppData data){
    if(status)return status;
    double value;
    if(!ProDimensionValueGet(d,&value))((ImitationSourceInfo *)data)->dimensions++;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_read_source(ProMdl source,ImitationSourceInfo *info){
    ProMdlType type;
    ProError error=ProMdlTypeGet(source,&type);
    if(error||type!=PRO_MDL_PART)return error?error:PRO_TK_INVALID_TYPE;
    error=part_health(source);
    if(!error)error=ProSolidFeatVisit((ProSolid)source,imitation_source_feature,NULL,info);
    if(!error)error=ProSolidDimensionVisit((ProSolid)source,PRO_B_FALSE,imitation_source_dimension,NULL,info);
    return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}

static ProError imitation_datums(ProMdl source,ProMdl result,const wchar_t *name,const ImitationGeometry *m,
    const ImitationSourceInfo *info,int *copied,PartText *out){
    const wchar_t *names[]={L"DTM1",L"DTM2"};
    for(int i=0;i<2;i++){
        wchar_t token[32];
        swprintf_s(token,_countof(token),L"PLANE:%ls",names[i]);
        ProVector point,normal;
        if(imitation_plane(source,token,point,normal))continue;
        int axis=i?0:1;
        if(fabs(normal[axis])<0.99999)continue;
        double half=axis==0?m->width/2:m->ear_end;
        ProError error=imitation_find_plane(result,axis,m->origin[axis]-half,-1,token);
        if(!error)error=imitation_offset(result,name,out,names[i],token,axis,point[axis]);
        if(error)return error;
        (*copied)++;
    }
    if(!info->csys[0]||!fd_name(info->csys)||!_wcsicmp(info->csys,L"PRT_CSYS_DEF"))return PRO_TK_NO_ERROR;
    wchar_t token[64];
    swprintf_s(token,_countof(token),L"CSYS:%ls",info->csys);
    ProCsysdata a,b;
    if(in_box_frame(source,token,&a)||in_box_frame(result,L"CSYS:PRT_CSYS_DEF",&b)||
       cm_distance(a.origin,b.origin)>0.0001||ad_dot(a.x_vector,b.x_vector)<0.999999||
       ad_dot(a.y_vector,b.y_vector)<0.999999||ad_dot(a.z_vector,b.z_vector)<0.999999)return PRO_TK_NO_ERROR;
    return imitation_operation(result,out,0,L"CREO_OP DATUM_CSYS %ls %ls CSYS:PRT_CSYS_DEF 0 0 0 0 0 0",name,info->csys);
}

static ProError imitation_verify_shape(ProMdl source,ProMdl model,const ImitationGeometry *m,PartText *out){
    ProMassProperty a,b;
    ProSolidBody original,result;
    ProOutline x,y;
    ProError error=qa_geometry_mass(source,&a);
    if(!error)error=qa_geometry_mass(model,&b);
    if(!error)error=imitation_active_body(source,&original);
    if(!error)error=imitation_active_body(model,&result);
    if(!error)error=ProSolidBodyOutlineGet(&original,x);
    if(!error)error=ProSolidBodyOutlineGet(&result,y);
    if(error)return error;
    if(fabs(a.volume-b.volume)>fmax(0.005,1e-6*a.volume)||fabs(a.surface_area-b.surface_area)>fmax(0.005,1e-6*a.surface_area)){
        part_text_add(out,L"实体未通过体积或面积验收，原件/重建体积 %.9g / %.9g mm³，面积 %.9g / %.9g mm²。\n",
            a.volume,b.volume,a.surface_area,b.surface_area);
        return PRO_TK_GENERAL_ERROR;
    }
    for(int i=0;i<2;i++)for(int j=0;j<3;j++)if(!imitation_near(x[i][j],y[i][j]))return PRO_TK_GENERAL_ERROR;

    ImitationGeometry readback={0};
    error=imitation_measure(model,&readback);
    if(error)return error;
    if(!imitation_near(m->thickness,readback.thickness)||!imitation_near(m->radius,readback.radius)||
       !imitation_near(m->corner,readback.corner)||!imitation_near(m->hole_radius,readback.hole_radius)||
       !imitation_near(m->ear_half,readback.ear_half)||!imitation_near(m->half_flat,readback.half_flat))
        return PRO_TK_GENERAL_ERROR;
    for(int i=0;i<2;i++)for(int j=0;j<2;j++)
        if(!imitation_near(m->hole_center[i][j],readback.hole_center[i][j])||!imitation_near(m->window[i][j],readback.window[i][j]))
            return PRO_TK_GENERAL_ERROR;

    /* Probe both solids on the same grid, away from face seams. */
    int rays=0,occupied=0;
    for(int k=0;k<3;k++)for(int i=0;i<16;i++)for(int j=0;j<16;j++){
        int u=(k+1)%3,v=(k+2)%3;
        ProRay ray={0};
        ray.dir_vector[k]=1;
        ray.start_point[k]=x[0][k]-5;
        ray.start_point[u]=x[0][u]+(x[1][u]-x[0][u])*(i+0.361)/16;
        ray.start_point[v]=x[0][v]+(x[1][v]-x[0][v])*(j+0.719)/16;
        QualityRay before,after;
        if(qa_ray(source,&ray,0.0001,&before)||qa_ray(model,&ray,0.0001,&after))continue;
        rays++;
        occupied+=before.count>0;
        if(before.count!=after.count)return PRO_TK_GENERAL_ERROR;
        for(int q=0;q<before.count;q++)
            if(!imitation_near(before.crossing[q].depth,after.crossing[q].depth))return PRO_TK_GENERAL_ERROR;
    }
    if(rays<700||occupied<100)return PRO_TK_INCOMPLETE;
    part_text_add(out,L"实体包络 %.8g × %.8g × %.8g mm，体积与面积一致；%d 条有效实体射线一致。\n",
        y[1][0]-y[0][0],y[1][1]-y[0][1],y[1][2]-y[0][2],rays);
    return PRO_TK_NO_ERROR;
}

static ProError imitation_verify_bends(ProMdl model,const ImitationGeometry *m,PartText *out){
    double mm,t;
    ProError error=fab_thickness(model,&mm,&t);
    if(error||!imitation_near(t,m->thickness))return error?error:PRO_TK_GENERAL_ERROR;
    FabBends bends={0};
    error=smy_bends_load(model,&bends);
    if(error||bends.count!=4)return error?error:PRO_TK_GENERAL_ERROR;
    int main=0,ears=0;
    for(int i=0;i<bends.count;i++){
        const FabBend *bend=&bends.rows[i];
        if(!imitation_near(fabs(bend->angle),90)||!imitation_near(bend->radius,m->radius))return PRO_TK_GENERAL_ERROR;
        main+=imitation_near(bend->length,m->width);
        ears+=imitation_near(bend->length,2*m->ear_half);
    }
    if(main!=2||ears!=2)return PRO_TK_GENERAL_ERROR;
    wchar_t face[32];
    error=imitation_find_plane(model,2,m->origin[2]+t,1,face);
    if(error)return error;
    FabDxf *flat=calloc(1,sizeof(*flat));
    if(!flat)return PRO_TK_OUT_OF_MEMORY;
    ProPath directory,path;
    error=ProDirectoryCurrentGet(directory);
    if(!error)error=fab_flat_read(model,directory,L"imitation_check",face,flat,path);
    if(!error&&(flat->bends!=4||flat->holes<6))error=PRO_TK_GENERAL_ERROR;
    free(flat);
    if(!error)part_text_add(out,L"板厚 %.8g mm，4 道 90° 折弯，内 R%.8g mm。窗口 %.8g × %.8g mm，两个 Ø%.8g mm 安装孔，耳端 R%.8g mm。展开轮廓闭合，折弯线与孔槽回读通过。\n",
        t,m->radius,m->window[1][0]-m->window[0][0],m->window[1][1]-m->window[0][1],2*m->hole_radius,m->corner);
    return error;
}

static ProError imitation_result_name(ProMdl source,const wchar_t *directory,ProMdlName result,ProPath path){
    ProMdlName original;
    ProError error=ProMdlMdlnameGet(source,original);
    if(error)return error;
    wchar_t stem[20];
    int used=0;
    for(int i=0;original[i]&&used<18;i++){
        wchar_t c=original[i];
        stem[used++]=(c>=L'0'&&c<=L'9')||(c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')?c:L'_';
    }
    stem[used]=0;
    if(!used)return PRO_TK_BAD_INPUTS;
    for(int i=0;i<99;i++){
        const wchar_t *prefix=stem[0]>=L'0'&&stem[0]<=L'9'?L"m_":L"";
        if(i)swprintf_s(result,PRO_MDLNAME_SIZE,L"%ls%ls_bwtl_%02d",prefix,stem,i+1);
        else swprintf_s(result,PRO_MDLNAME_SIZE,L"%ls%ls_bwtl",prefix,stem);
        if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls.prt.1",directory,result)<0)return PRO_TK_BAD_INPUTS;
        ProMdl loaded;
        error=ProMdlnameInit(result,PRO_MDL_PART,&loaded);
        if(error==PRO_TK_E_NOT_FOUND&&GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES&&GetLastError()==ERROR_FILE_NOT_FOUND){
            ProPath old;
            if(dv_native_revision(directory,result,old)==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
        }
    }
    return PRO_TK_E_FOUND;
}

static ProError imitation_ears_new(ProMdl source, const ImitationGeometry *m, const ImitationSourceInfo *info,
                                   ProMdl *model, ProMdlName result_name, PartText *out) {
    ProMdlName name;
    wchar_t face[32];
    const char *stage = "imitation blank part";
    int copied = 0;
    ProError error = model_create_empty(1, name, &stage);
    if (!error) error = ProMdlnameInit(name, PRO_MDL_PART, model);
    if (!error) error = imitation_geometry_build(*model, name, m, out);
    if (!error) error = imitation_datums(source, *model, name, m, info, &copied, out);
    if (!error) error = imitation_find_plane(*model, 2, m->origin[2] + m->thickness, 1, face);
    if (!error) error = imitation_operation(*model, out, 0, L"CREO_OP SMT_CONVERT %ls NATIVE_SHEET %ls %.12g",
                                            name, face, m->thickness);
    if (!error) error = part_regenerate(*model);
    if (!error) error = imitation_verify_shape(source, *model, m, out);
    if (!error) error = imitation_verify_bends(*model, m, out);
    if (!error) error = ProMdlnameRename(*model, result_name);
    if (!error) part_text_add(out, L"按实测尺寸重新建钣金特征，复建 %d 个适配的命名基准面。\n", copied);
    return error;
}


typedef struct {
    const wchar_t *stage;
    int dependency_block,edit_verified,partial_displayed,manual;
    ReplayFailure replay;
} ImitationOutcome;

#include "imitation_replay_material.inc"

static ProMdlName imitation_partial_target=L"";
static ProMdlName imitation_last_source=L"";
static int imitation_last_completed,imitation_last_failed_step;
static ProError imitation_last_failure_code;

static void imitation_write_progress(FILE *prompt,ProMdl active){
    if(!imitation_last_failure_code||!active)return;
    ProMdlName name;
    if(ProMdlMdlnameGet(active,name))return;
    if(imitation_partial_target[0]&&!_wcsicmp(name,imitation_partial_target)){
        fwprintf(prompt,L"\n<本次会话的仿照回执>当前活动零件是已核验并保留的独立新件。"
            L"源件 %ls，已完成 %d 道，第 %d 步返回 Creo %d。"
            L"用户要求继续时，使用本轮新件参照继续剩余操作，保留已完成特征。"
            L"使用有依据的替代路径，无法继续时报告保留结果和具体阻断原因。</本次会话的仿照回执>\n",
            imitation_last_source,imitation_last_completed,imitation_last_failed_step,imitation_last_failure_code);
    }else if(!_wcsicmp(name,imitation_last_source)){
        fwprintf(prompt,L"\n<本次会话的仿照回执>活动模型是源件 %ls。最近一次仿照第 %d 步返回 Creo %d。"
            L"%ls 本轮若明确要求继续仿照，先依据实际会话状态选择可行的独立重建路径。"
            L"不得将源件作为仿照结果修改。</本次会话的仿照回执>\n",
            imitation_last_source,imitation_last_failed_step,imitation_last_failure_code,
            imitation_partial_target[0]?L"上次保留的新件当前未激活。":L"没有保留下来可编辑的部分新件。");
    }
}

static ProError imitation_run_mode_impl(ProMdl source,ProPath delivered,int display,int strict,
                                       ImitationOutcome *outcome,PartText *out){
    ProMdl model=NULL;
    ImitationSourceInfo info={0};
    ProWVerstamp before=NULL,after=NULL;
    DeliveryOutput work={0};
    ProMdlName result_name;
    ProPath revision,destination,validated,result_folder=L"";
    ReplayOrder verified_order={0};
    ReplayFailure failure={0};
    ReplayFailure *where=outcome?&outcome->replay:&failure;
    ModelingLesson *lesson=NULL;
    ImitationFlatPlan *inferred=NULL;
    int old_background=model_background_mode,steps=0,external=0,partial=0;
    ProError error=imitation_read_source(source,&info);
    delivered[0]=0;
    if(display){
        imitation_partial_target[0]=0;
        imitation_last_source[0]=0;
        imitation_last_completed=imitation_last_failed_step=0;
        imitation_last_failure_code=0;
        if(source)ProMdlMdlnameGet(source,imitation_last_source);
    }
    if(outcome)outcome->stage=L"读取源件";
    if(error){
        ProMdlType type;
        ReplayFailure failure={0};
        if(source&&!ProMdlTypeGet(source,&type)&&type==PRO_MDL_PART&&
           ProSolidFeatVisit((ProSolid)source,replay_find_failed_feature,NULL,&failure)==PRO_TK_E_FOUND){
            if(outcome){outcome->stage=L"源特征状态预检";outcome->replay=failure;}
            part_text_add(out,L"源特征 %ls（ID %d）%ls，当前不可重建。请修复源件并再生后重试。\n",
                failure.label[0]?failure.label:L"未命名",failure.feature,failure.operation);
            return PRO_TK_BAD_CONTEXT;
        }
        return error;
    }
    const ProError dependencies=replay_source_dependencies(source,&external);
    if(dependencies){
        if(outcome){
            outcome->stage=L"外部参照或族表预检";
            outcome->dependency_block=1;
        }
        imitation_reference_report(source,out);
        replay_preview(source,out);
        if(external)part_text_add(out,L"需让上游装配和参照几何在当前 Creo 会话中可用，再按特征树重建。\n");
        else part_text_add(out,L"如源件为族表母件，请打开要仿照的具体实例后再执行。\n");
        return dependencies;
    }
    ImitationShapeMetrics shape;
    if(outcome)outcome->stage=L"读取源件几何";
    error=imitation_shape_read(source,&shape);
    if(error){
        part_text_add(out,L"源件：");
        imitation_shape_error(&shape,error,out);
        return error;
    }
    ProMdlsubtype subtype;
    error=ProMdlSubtypeGet(source,&subtype);
    if(error)return error;
    if(info.imports){
        ReplayOrder order={0};
        error=ProSolidFeatVisit((ProSolid)source,replay_read_order,NULL,&order);
        ProError cleanup=replay_pattern_source_close(&order.patterns);
        if(!error)error=cleanup;
        if(!error&&(info.imports!=1||order.count!=1))error=PRO_TK_UNSUPPORTED;
        if(!error){
            inferred=calloc(1,sizeof(*inferred));
            error=inferred?imitation_flat_identify(source,&shape,inferred):PRO_TK_OUT_OF_MEMORY;
        }
        if(error){
            part_text_add(out,L"源件含 %d 项导入特征。当前几何推断支持由直线、圆弧和圆组成的单实体等截面板件。\n",info.imports);
            free(inferred);
            return error;
        }
        part_text_add(out,L"从空白模板开始，依据导入几何推断建模步骤。\n");
    }else{
        part_text_add(out,L"从空白模板开始，按原特征树逐步创建。\n");
        lesson=calloc(1,sizeof(*lesson));
    }
#define IMITATE_RUN(call) do{error=(call);if(error){trace(#call,error);part_text_add(out,L"原生步骤 %hs 返回 %d。\n",#call,error);goto done;}}while(0)
    IMITATE_RUN(ProMdlVerstampGet(source,&before));
    if(outcome)outcome->stage=L"准备独立暂存件";
    IMITATE_RUN(fab_scratch(&work));
    IMITATE_RUN(imitation_result_name(source,work.parent,result_name,destination));
    IMITATE_RUN(dv_stage_enter(&work));
    model_background_mode=1;
    if(outcome)outcome->stage=L"逐步重建";
    if(inferred)IMITATE_RUN(imitation_build_flat_plan(inferred,&model,out));
    else{
        IMITATE_RUN(imitation_replay_new_impl(source,subtype,&model,&steps,where,display?&partial:NULL,outcome&&outcome->manual,out));
        if(outcome)outcome->stage=L"保留源材料";
        IMITATE_RUN(imitation_material_replay(source,model,subtype,work.stage,out));
    }
    if(outcome)outcome->stage=L"保存和重开暂存件";
    IMITATE_RUN(ProMdlnameRename(model,result_name));
    if(!inferred)IMITATE_RUN(replay_regenerate_model_relations(model));
    if(!inferred)IMITATE_RUN(imitation_bend_table_replay(source,model,subtype,out));
    IMITATE_RUN(imitation_reopen_candidate(&model,&work,result_name));
    if(outcome)outcome->stage=L"核对依赖、特征树与几何";
    IMITATE_RUN(dv_no_external_dependencies(model));
    IMITATE_RUN(imitation_shape_verify_mode(source,model,&shape,0,out));
    if(!inferred){
        IMITATE_RUN(replay_verify_order_impl(source,model,1,lesson?&verified_order:NULL,out));
        IMITATE_RUN(imitation_sheet_compare(source,model,out));
        IMITATE_RUN(imitation_material_verify(source,model,subtype));
    }
    int edit_verified=0;
    if(strict||external||inferred){
        if(outcome)outcome->stage=L"试改尺寸和恢复";
        IMITATE_RUN(imitation_edit_trial(model,out));
        IMITATE_RUN(imitation_shape_verify_mode(source,model,&shape,0,out));
        edit_verified=1;
        if(outcome)outcome->edit_verified=1;
    }
    if(!inferred)part_text_add(out,L"已完成 %d 个原生建模步骤。\n",steps);
    part_text_add(out,L"暂存的原生零件已从磁盘重新打开并复核。\n");
    if(lesson)method_capture(source,model,subtype,&verified_order,edit_verified,lesson);
    if(outcome)outcome->stage=L"核对源件与交付";
    IMITATE_RUN(ProMdlVerstampGet(source,&after));
    if(!ProVerstampEqual(before,after)){
        part_text_add(out,L"原件会话版本戳发生变化，本次结果暂未发布。\n");
        error=PRO_TK_GENERAL_ERROR;
        goto done;
    }
    IMITATE_RUN(dv_native_revision(work.stage,result_name,revision));
    if(swprintf_s(validated,_countof(validated),L"%ls\\%ls",work.stage,revision)<0){
        error=PRO_TK_BAD_INPUTS;goto done;
    }
    IMITATE_RUN(ProDirectoryChange(work.parent));
    work.in_stage=0;
    if(swprintf_s(result_folder,_countof(result_folder),L"%ls\\im_%08lx_%llx",work.parent,GetCurrentProcessId(),GetTickCount64())<0||
       !CreateDirectoryW(result_folder,NULL)){
        result_folder[0]=0;
        error=PRO_TK_CANT_WRITE;
        goto done;
    }
    if(swprintf_s(destination,_countof(destination),L"%ls\\%ls",result_folder,revision)<0){error=PRO_TK_BAD_INPUTS;goto done;}
    if(!CopyFileW(validated,destination,TRUE)){
        trace("Publish verified staged part",GetLastError());
        error=PRO_TK_CANT_WRITE;goto done;
    }
    wcscpy_s(delivered,PRO_PATH_SIZE,destination);
    part_text_add(out,L"已保存验收通过的独立零件。\n%ls\n",delivered);
done:
    model_background_mode=old_background;
    if(error&&display&&where->step){
        imitation_last_failed_step=where->step;
        imitation_last_failure_code=error;
    }
    if(error&&partial&&model){
        ProMdlName partial_name=L"";
        const ProError native_error=error;
        ProError preserve=part_regenerate(model);
        if(!preserve)preserve=dv_no_external_dependencies(model);
        if(!preserve)preserve=ProMdlVerstampGet(source,&after);
        if(!preserve&&!ProVerstampEqual(before,after))preserve=PRO_TK_INCOMPLETE;
        if(!preserve)preserve=ProMdlMdlnameGet(model,partial_name);
        if(!preserve){
            error=fab_discard(&work,error,out);
            preserve=work.in_stage?PRO_TK_BAD_CONTEXT:dv_display_new(model);
        }
        if(!preserve){
            part_text_add(out,L"已保留并打开新件 %ls.prt，完成 %d 道步骤，未保存。"
                L"第 %d 步 %ls %ls 未完成（Creo %d）。原件保留。\n",
                partial_name,partial,where->step,where->operation?where->operation:L"",where->label,native_error);
            if(outcome)outcome->partial_displayed=1;
            wcscpy_s(imitation_partial_target,_countof(imitation_partial_target),partial_name);
            imitation_last_completed=partial;
            model=NULL;
        }else part_text_add(out,L"前 %d 道暂存步骤未能核验并保留（Creo %d）。\n",partial,preserve);
    }else if(error&&steps&&model&&!delivered[0]){
        part_text_add(out,L"暂存新件将清理，当前没有可继续编辑的仿照结果。\n");
    }
    if(model){
        ProError erase=ProMdlErase(model);
        if(erase)trace("Release imitation result from session",erase);
        else model=NULL;
    }
    if(!delivered[0]&&result_folder[0]&&!RemoveDirectoryW(result_folder))
        part_text_add(out,L"未发布的诊断文件位于 %ls。\n",result_folder);
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    error=fab_discard(&work,error,out);
    if(!error&&delivered[0]){
        if(lesson&&lesson->method.sequence[0]){
            ProError learning_error=method_save(lesson,out);
            trace("Modeling method auto-save",learning_error);
            if(learning_error)
                part_text_add(out,L"零件已验收，建模记忆未写入（%d）。请检查当前实例的数据盘。\n",learning_error);
            if(display)method_last=*lesson;
        }else{
            if(!inferred)part_text_add(out,L"零件已验收，本次没有生成可保存的原生建模步骤。\n");
            if(display)memset(&method_last,0,sizeof(method_last));
        }
    }
    free(lesson);
    free(inferred);
    if(delivered[0]&&display){
        ProError shown=model?PRO_TK_E_IN_USE:ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&model);
        if(!shown)shown=dv_display_new(model);
        if(shown)part_text_add(out,L"文件已保存。请从工作目录打开查看。\n");
    }
    return error;
#undef IMITATE_RUN
}

static ProError imitation_run_mode(ProMdl source,ProPath delivered,int display,int strict,PartText *out){
    return imitation_run_mode_impl(source,delivered,display,strict,NULL,out);
}

static ProError imitation_run_chat(ProMdl source,ProPath delivered,int *partial,PartText *out){
    ImitationOutcome outcome={0};
    ProError error=imitation_run_mode_impl(source,delivered,1,0,&outcome,out);
    *partial=outcome.partial_displayed;
    return error;
}

static ProError imitation_run(ProMdl source,ProPath delivered,int display,PartText *out){
    return imitation_run_mode(source,delivered,display,0,out);
}
