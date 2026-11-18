
#define user_initialize flow_toolkit_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>
static int flow_fail,flow_tests;
static FILE *flow_receipt;
static void verify(int n,ProError error,const wchar_t *detail){
    fwprintf(flow_receipt,L"%03d %ls %ls error=%d\n",n,error?L"FAIL":L"PASS",detail,error);
    fflush(flow_receipt);
    flow_fail+=error!=0;flow_tests++;
}

static ProError flow_box(const wchar_t *name,ProMdl *part){
    ProMdlName label;
    wcscpy_s(label,_countof(label),name);
    const ModelPlan box={.a=20,.b=18,.height=3,.ready=1};
    const char *stage="";
    ProError error=quick_seed(label,part);
    if(!error)error=model_build_internal(&box,label,&stage,*part,NULL,NULL);
    return error;
}

static ProError flow_add_csys(ProMdl assembly,ProMdl part,const wchar_t *place,ProAsmcomp *item){
    ProAsmcomppath path;
    FdRef a={0},b={0};
    ProSelection parent=NULL,child=NULL;
    ProAsmcompconstraint *rows=NULL,row=NULL;
    ProMatrix initial={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProError error=ax_root_path(assembly,&path);
    if(!error)error=fd_ref_resolve(assembly,place,PRO_CSYS,&a);
    if(!error){ProMdlType type;error=ProMdlTypeGet(part,&type);if(!error)error=fd_ref_resolve(part,type==PRO_MDL_ASSEMBLY?L"CSYS:ASM_CSYS":L"CSYS:PRT_CSYS_DEF",PRO_CSYS,&b);}
    if(!error)error=ProSelectionAlloc(&path,&a.item,&parent);
    if(!error)error=ProSelectionAlloc(NULL,&b.item,&child);
    if(!error)error=ProArrayAlloc(0,sizeof(*rows),1,(ProArray *)&rows);
    if(!error)error=ProAsmcompconstraintAlloc(&row);
    if(!error)error=ProAsmcompconstraintTypeSet(row,PRO_ASM_CSYS);
    if(!error)error=ProAsmcompconstraintAsmreferenceSet(row,parent,PRO_DATUM_SIDE_NONE);
    if(!error)error=ProAsmcompconstraintCompreferenceSet(row,child,PRO_DATUM_SIDE_NONE);
    if(!error)error=ProArrayObjectAdd((ProArray *)&rows,PRO_VALUE_UNUSED,1,&row);
    if(!error)row=NULL;
    if(!error)error=ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)part,initial,item);
    if(!error)error=ProAsmcompConstraintsSet(NULL,item,rows);
    if(!error)error=part_regenerate(assembly);
    if(rows)ProAsmcompconstraintArrayFree(rows);
    if(row)ProAsmcompconstraintFree(row);
    if(parent)ProSelectionFree(&parent);
    if(child)ProSelectionFree(&child);
    fd_ref_free(&a);fd_ref_free(&b);
    return error;
}

static ProError flow_mount_points(const wchar_t *name,ProMdl *model){
    ProError error=ax_new_subassembly(name,model);
    if(!error)error=ax_layout_csys(*model,L"LEFT_MOUNT",L"CSYS:ASM_CSYS",0,0,0);
    if(!error)error=ax_layout_csys(*model,L"RIGHT_MOUNT",L"CSYS:ASM_CSYS",25,0,0);
    return error;
}

static ProError flow_attach(ProMdl parent,ProMdl part,ProAsmcomp *left,ProAsmcomp *right,ProAsmcomp *placed){
    AxComponent a={0},b={0},c={0};
    c.model=part;
    AxGeometry ga,gb;
    wchar_t p1[48],p2[48];
    ax_path_label(&left->id,1,p1,_countof(p1));
    ax_path_label(&right->id,1,p2,_countof(p2));
    ProError error=ax_component_resolve(parent,p1,&a);
    if(!error)error=ax_component_resolve(parent,p2,&b);
    ProAsmcompconstraint *rows=NULL;
    if(!error)error=ProArrayAlloc(0,sizeof(*rows),3,(ProArray *)&rows);
    if(!error)error=ax_add_relation(&rows,&a,&c,L"PLANE:RIGHT",L"PLANE:RIGHT",PRO_ASM_ALIGN,0,&ga,&gb);
    if(!error)error=ax_add_relation(&rows,&b,&c,L"PLANE:TOP",L"PLANE:TOP",PRO_ASM_ALIGN,0,&ga,&gb);
    if(!error)error=ax_add_relation(&rows,&b,&c,L"PLANE:FRONT",L"PLANE:FRONT",PRO_ASM_ALIGN,0,&ga,&gb);
    ProMatrix seed={{1,0,0,0},{0,1,0,0},{0,0,1,0},{2,0,0,1}};
    if(!error)error=ProAsmcompAssemble((ProAssembly)parent,(ProSolid)part,seed,placed);
    if(!error)error=ProAsmcompConstraintsSet(NULL,placed,rows);
    if(!error)error=part_regenerate(parent);
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return error;
}


typedef struct {
    AxComponent owner;
    int front,holes[8],count,shaft;
    double height;
} FlowFixtureFaces;

static ProError flow_fixture_surface(ProSurface surface,ProError status,ProAppData app){
    FlowFixtureFaces *f=app;
    if(status)return status;
    ProGeomitem item;
    AxGeometry shape;
    if(ProSurfaceToGeomitem((ProSolid)f->owner.model,surface,&item)||ax_geometry(&item,&shape))return PRO_TK_NO_ERROR;
    if(shape.type==AX_GEOM_PLANE&&shape.direction[2]>0.999&&fabs(shape.point[2]-f->height)<0.001)f->front=item.id;
    if(shape.type!=AX_GEOM_CYLINDER)return PRO_TK_NO_ERROR;
    wchar_t token[40];
    swprintf_s(token,_countof(token),L"SURFACE:%d",item.id);
    int bore;
    ProVector ends[2];
    if(ax_hw_cylinder(&f->owner,token,&shape,ends,&bore))return PRO_TK_NO_ERROR;
    if(!bore){f->shaft=item.id;return PRO_TK_NO_ERROR;}
    for(int i=0;i<f->count;i++){
        ProSurface previous;
        ProGeomitem other;
        AxGeometry axis;
        if(!ProSurfaceInit((ProSolid)f->owner.model,f->holes[i],&previous)&&
            !ProSurfaceToGeomitem((ProSolid)f->owner.model,previous,&other)&&!ax_geometry(&other,&axis)&&
            ax_hw_separation(shape.point,axis.point,axis.direction,1)<0.001)return PRO_TK_NO_ERROR;
    }
    if(f->count<_countof(f->holes))f->holes[f->count++]=item.id;
    return PRO_TK_NO_ERROR;
}

static ProError flow_fixture_plate(const wchar_t *name,int pitch,ProMdl *model,FlowFixtureFaces *faces){
    PartSnapshot snapshot={0};
    PartCommand cmd;
    wchar_t line[200],receipt[1000];
    swprintf_s(line,_countof(line),L"CREO_OP QUICK_HINGE %ls 150 110 10 %d 40 14",name,pitch);
    if(part_command_parse(line,&cmd)!=1)return PRO_TK_BAD_INPUTS;
    ProError error=ProDirectoryCurrentGet(snapshot.directory);
    if(!error)error=quick_execute(&cmd,&snapshot,receipt,_countof(receipt));
    ProMdlName label;
    wcscpy_s(label,_countof(label),name);
    if(!error)error=ProMdlnameInit(label,PRO_MDL_PART,model);
    if(error)return error;
    faces->owner.model=*model;
    faces->height=10;
    error=ProSolidSurfaceVisit((ProSolid)*model,flow_fixture_surface,NULL,faces);
    return error?error:faces->count==4&&faces->front?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static void flow_fixture_holes(const FlowFixtureFaces *f,wchar_t holes[140],wchar_t face[30]){
    swprintf_s(holes,140,L"SURFACE:%d,SURFACE:%d,SURFACE:%d,SURFACE:%d",f->holes[0],f->holes[1],f->holes[2],f->holes[3]);
    swprintf_s(face,30,L"SURFACE:%d",f->front);
}



typedef struct {double want;ProName label;} MatrixDimension;
static ProError flow_variant_dimension(ProDimension *dim,ProError status,ProAppData data){
    MatrixDimension *chosen=data;
    double reading;
    ProDimensiontype type;
    if(!status&&!*chosen->label&&!ProDimensionTypeGet(dim,&type)&&type==PRODIMTYPE_LINEAR&&
        !ProDimensionValueGet(dim,&reading)&&fabs(reading-chosen->want)<0.001)
        ProDimensionSymbolGet(dim,chosen->label);
    return PRO_TK_NO_ERROR;
}


static ProError flow_prepare_intake_drawing(ProMdl source,PartText *report){
    ProError error=PRO_TK_NO_ERROR;
    PartText out=*report;
    ProMdlName name=L"STD_FIXTURE_DRAWING",template=L"a4_drawing";
    ProMdlnameShortdata model={0};
    if(!error)error=ProMdlMdlnameGet(source,model.name);
    wcscpy_s(model.type,_countof(model.type),L"prt");
    ProDrawing drawing=NULL;
    ProDwgcreateErrs info=NULL;
    if(!error)error=ProDrawingFromTemplateCreate(name,template,&model,0,&drawing,&info);
    if(info){
        int reports=0;
        ProDwgcreateErrsCountGet(info,&reports);
        part_text_add(&out,L"工程图创建源代码 %d，Creo 报告 %d 条，返回图纸 %d。\n",error,reports,drawing!=NULL);
        ProDwgcreateErrsFree(&info);
    }
    if(error==PRO_TK_DWGCREATE_ERRORS&&drawing)error=PRO_TK_NO_ERROR;
    if(error==PRO_TK_DWGCREATE_ERRORS){
        ProMdlName another=L"a3-part-h";
        info=NULL;
        error=ProDrawingFromTemplateCreate(name,another,&model,0,&drawing,&info);
        if(info)ProDwgcreateErrsFree(&info);
        if(error==PRO_TK_DWGCREATE_ERRORS&&drawing)error=PRO_TK_NO_ERROR;
    }
    if(!error){
        ProError attached=ProDrawingSolidAdd(drawing,(ProSolid)source);
        if(attached!=PRO_TK_E_IN_USE)error=attached;
    }
    if(!error)error=ProMdlDisplay((ProMdl)drawing);
    if(!error){
        ProPoint3d on_sheet={160,115,0};
        ProMatrix front={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
        ProView view;
        error=ProDrawingGeneralviewCreate(drawing,(ProSolid)source,1,PRO_B_FALSE,on_sheet,1,front,&view);
    }
    if(!error)error=ProMdlSave((ProMdl)drawing);
    return error;
}

static void flow_intake_verify(ProMdl source,PartText *report){
    wchar_t base[MAX_PATH],pattern[MAX_PATH],matrix[MAX_PATH],details[2600]=L"";
    PartText out={details,_countof(details),0};
    if(!storage_method_directory(base,L"标准件规格试改",0)||
        swprintf_s(pattern,_countof(pattern),L"%ls\\probe-*",base)<0){verify(59,PRO_TK_CANT_OPEN,L"已验收规格矩阵");return;}
    WIN32_FIND_DATAW found;
    HANDLE items=FindFirstFileW(pattern,&found);
    if(items==INVALID_HANDLE_VALUE){verify(59,PRO_TK_E_NOT_FOUND,L"已验收规格矩阵");return;}
    FindClose(items);
    if(swprintf_s(matrix,_countof(matrix),L"%ls\\%ls\\重开验收.tsv",base,found.cFileName)<0){
        verify(59,PRO_TK_OUT_OF_RANGE,L"规格证据路径");return;
    }
    FILE *scope=_wfopen(L"flow-intake-scope.tsv",L"wt, ccs=UTF-8");
    ProError error=scope?PRO_TK_NO_ERROR:PRO_TK_CANT_WRITE;
    if(scope){
        fwprintf(scope,L"四孔支撑板\t板厚\t8\t12\t坐标系和四孔\t超范围需重新验证\t%ls\n",matrix);
        if(fclose(scope))error=PRO_TK_CANT_WRITE;
    }
    if(!error)error=std_scope_add(L"四孔支撑板",L"装配",L"BW_FLOW_SOURCE",L"flow-intake-scope.tsv",&out);
    ProMdlName name=L"STD_FIXTURE_DRAWING";
    ProMdl drawing=NULL;
    if(!error)error=ProMdlnameInit(name,PRO_MDL_DRAWING,&drawing);
    ProPath dwg_path;
    if(!error)error=ProMdlOriginGet((ProMdl)drawing,dwg_path);
    if(!error&&std_intake(L"STD-001",L"A",L"四孔支撑板",L"wrong-matrix.tsv",dwg_path,&out)!=PRO_TK_INCOMPLETE)
        error=PRO_TK_INCOMPLETE;
    if(!error)error=std_intake(L"STD-001",L"A",L"四孔支撑板",matrix,dwg_path,&out);
    if(!error&&(!wcsstr(details,L"已通过入库检查")||
        std_intake(L"STD-001",L"A",L"四孔支撑板",matrix,dwg_path,&out)!=PRO_TK_E_FOUND))error=PRO_TK_INCOMPLETE;
    verify(59,error,L"检测缺失验收回执，实际关联 Creo 工程图后登记，拒绝重复入库");
    part_text_add(report,L"%ls",details);
}

static void flow_variant_test(ProMdl source,PartText *out){
    MatrixDimension pick={.want=10};
    ProError error=ProSolidDimensionVisit((ProSolid)source,PRO_B_FALSE,flow_variant_dimension,NULL,&pick);
    if(!error&&!*pick.label)error=PRO_TK_E_NOT_FOUND;
    if(!error){
        FILE *file=_wfopen(L"flow-matrix.tsv",L"wt, ccs=UTF-8");
        if(!file)error=PRO_TK_CANT_WRITE;
        else{
            fwprintf(file,L"薄板\t%ls=8\n厚板\t%ls=12\n",pick.label,pick.label);
            if(fclose(file))error=PRO_TK_CANT_WRITE;
        }
    }
    wchar_t report[2800]=L"";
    PartText details={report,_countof(report),0};
    if(!error)error=std_variant_matrix(source,L"STD-001",L"A",L"flow-matrix.tsv",&details);
    if(!error&&(!wcsstr(report,L"规格通过 2/2")||!wcsstr(report,L"接口孔数 4")))error=PRO_TK_INCOMPLETE;
    verify(54,error,L"独立保存两种厚度，实际重开、再编辑、恢复并测量各四个原生孔");
    if(!error)flow_intake_verify(source,&details);
    part_text_add(out,L"%ls",report);
}
static void flow_standard_parts(ProMdl old,ProMdl revised,ProMdl changed,
    const wchar_t *old_face,const wchar_t *old_holes,const wchar_t *same_face,const wchar_t *same_holes,
    const wchar_t *new_face,const wchar_t *new_holes,PartText *report){
    wchar_t output[4500]=L"";
    PartText result={output,_countof(output),0};
    const wchar_t *frame=L"CSYS:PRT_CSYS_DEF";
    ProError error=ProMdlSave(old);
    if(!error)error=ProMdlSave(revised);
    if(!error)error=ProMdlSave(changed);
    if(!error)error=flow_prepare_intake_drawing(old,&result);
    if(!error)error=ProMdlDisplay(old);
    if(!error)error=ProMdlSave(old);
    if(!error)error=ProMdlSave(revised);
    if(!error)error=ProMdlSave(changed);
    if(!error)error=std_record(old,L"STD-001",L"A",old_face,frame,old_holes,&result);
    if(!error)error=std_record(revised,L"STD-002",L"A",same_face,frame,same_holes,&result);
    if(!error)error=std_record(changed,L"STD-003",L"B",new_face,frame,new_holes,&result);
    if(error)part_text_add(report,L"标准件登记准备：%ls",output);
    verify(52,error,L"三套来自 Creo 原生定位面、坐标系和孔位的标准接口已写入 E 盘");
    StdRecord entry={0};
    if(!error)error=std_lookup(L"STD-001",L"A",&entry);
    if(!error)error=std_search(NULL,NULL,0,0.02,&entry,&result);
    if(!error&&!wcsstr(output,L"检查 3 份接口，匹配 1 份"))error=PRO_TK_INCOMPLETE;
    verify(52,error,L"根据完整定位指纹查到同孔组替代件并排除改距件");
    output[0]=0;
    const double roomy[]={160,120,11},tight_limits[]={140,120,11};
    if(!error)error=std_search(L"+Z",roomy,70,0.02,NULL,&result);
    if(!error&&(!wcsstr(output,L"匹配 2 份")||wcsstr(output,L"STD-003")))error=PRO_TK_INCOMPLETE;
    if(!error&&std_search(L"+Z",tight_limits,70,0.02,NULL,&result)!=PRO_TK_E_NOT_FOUND)error=PRO_TK_INCOMPLETE;
    if(!error&&std_search(L"-Z",roomy,70,0.02,NULL,&result)!=PRO_TK_E_NOT_FOUND)error=PRO_TK_INCOMPLETE;
    verify(51,error,L"按孔距、配合方向、安装空间同时匹配，方向或尺寸不合则排除");
    if(!error)error=std_alias_add(L"STD-001",L"A",L"SUP-1123",&result);
    if(!error)error=std_alias_query(L"SUP-1123",&result);
    if(!error&&std_alias_add(L"STD-003",L"B",L"SUP-1123",&result)!=PRO_TK_E_FOUND)error=PRO_TK_INCOMPLETE;
    if(!error)error=std_lookup(L"SUP-1123",L"A",&entry);
    if(!error&&_wcsicmp(entry.key,L"STD-001"))error=PRO_TK_INCOMPLETE;
    verify(53,error,L"供应商料号可以检索，冲突写入被拒绝并保留已有归属");
    if(!error)error=std_compare(L"SUP-1123",L"A",L"STD-003",L"B",0.02,&result);
    if(!error&&!wcsstr(output,L"含超差值"))error=PRO_TK_INCOMPLETE;
    verify(57,error,L"替代件差异给出孔位超差、孔径、材料、包络和面角");
    ProPath origin;
    if(!error)error=ProMdlOriginGet(old,origin);
    if(!error)error=std_source_add(L"STD-001",L"A",L"原生建模",origin,L"独立Creo验收样件",&result);
    if(!error)error=std_source_query(L"SUP-1123",L"A",&result);
    if(!error&&(!wcsstr(output,L"独立Creo验收样件")||!wcsstr(output,L"当前可读取")))error=PRO_TK_INCOMPLETE;
    verify(60,error,L"标准件源文件已验证实际存在，别名可读回来源和原因");
    if(!error)flow_variant_test(old,report);
    part_text_add(report,L"%ls",output);
}


static void flow_where_check(ProMdl root,PartText *out){
    wchar_t detail[1800]=L"";
    PartText result={detail,_countof(detail),0};
    ProPath origin;
    ProError error=ProMdlSave(root);
    if(!error)error=ProMdlOriginGet(root,origin);
    if(!error){
        FILE *file=_wfopen(L"flow-where.tsv",L"wt, ccs=UTF-8");
        if(!file)error=PRO_TK_CANT_WRITE;
        else{
            fwprintf(file,L"55寸立式\t演示订单\t%ls\n",origin);
            if(fclose(file))error=PRO_TK_CANT_WRITE;
        }
    }
    if(!error)error=std_where_used(L"STD-001",L"A",L"flow-where.tsv",&result);
    if(!error&&(!wcsstr(detail,L"找到 1 个引用候选")||!wcsstr(detail,L"父 ROOT")))error=PRO_TK_INCOMPLETE;
    verify(58,error,L"扫实际 Creo 装配且识别出旧标准件实例、父装配和订单");
    part_text_add(out,L"%ls",detail);
}
static void flow_groups(PartText *out){
    ProMdl root=NULL,old=NULL,revised=NULL,changed=NULL,hardware=NULL;
    FlowFixtureFaces a={0},b={0},c={0},t={0};
    ProAsmcomp old_item={0},new_item={0},changed_item={0};
    ProError error=flow_fixture_plate(L"FLOW_HOLE_OLD",70,&old,&a);
    if(!error)error=flow_fixture_plate(L"FLOW_HOLE_SAME",70,&revised,&b);
    if(!error)error=flow_fixture_plate(L"FLOW_HOLE_CHANGED",85,&changed,&c);
    if(!error){
        const ModelPlan spec={.cylinder=1,.a=10,.b=10,.height=25,.ready=1};
        const char *phase="";
        error=quick_seed(L"FLOW_HOLE_BOLT",&hardware);
        if(!error)error=model_build_internal(&spec,L"FLOW_HOLE_BOLT",&phase,hardware,NULL,NULL);
        t.owner.model=hardware;
    }
    if(!error)error=ProSolidSurfaceVisit((ProSolid)hardware,flow_fixture_surface,NULL,&t);
    if(!error&&!t.shaft)error=PRO_TK_INCOMPLETE;
    if(!error)error=ax_new_subassembly(L"FLOW_HOLE_CASE",&root);
    if(!error)error=ax_layout_csys(root,L"HOLE_OLD",L"CSYS:ASM_CSYS",0,0,0);
    if(!error)error=ax_layout_csys(root,L"HOLE_SAME",L"CSYS:ASM_CSYS",250,0,0);
    if(!error)error=ax_layout_csys(root,L"HOLE_NEW",L"CSYS:ASM_CSYS",500,0,0);
    if(!error)error=flow_add_csys(root,old,L"CSYS:HOLE_OLD",&old_item);
    if(!error)error=flow_add_csys(root,revised,L"CSYS:HOLE_SAME",&new_item);
    if(!error)error=flow_add_csys(root,changed,L"CSYS:HOLE_NEW",&changed_item);
    if(error){verify(45,error,L"真实孔组样件准备失败");return;}
    wchar_t x[140],y[140],z[140],f[30],g[30],h[30],u[32],v[32],w[32],shaft[32];
    flow_fixture_holes(&a,x,f);
    flow_fixture_holes(&b,y,g);
    flow_fixture_holes(&c,z,h);
    flow_standard_parts(old,revised,changed,f,x,g,y,h,z,out);
    ax_path_label(&old_item.id,1,u,_countof(u));
    ax_path_label(&new_item.id,1,v,_countof(v));
    ax_path_label(&changed_item.id,1,w,_countof(w));
    swprintf_s(shaft,_countof(shaft),L"SURFACE:%d",t.shaft);
    error=ax_group_compare(root,u,f,x,v,g,y,0.01,-1,out);
    verify(45,error==PRO_TK_E_FOUND?0:PRO_TK_INCOMPLETE,L"实测对称孔组必须有首孔约定");
    error=ax_group_compare(root,u,f,x,v,g,y,0.01,0,out);
    verify(45,error,L"首孔约束后，独立新件对应四孔");
    error=ax_group_compare(root,u,f,x,w,h,z,0.01,0,out);
    verify(45,error==PRO_TK_E_NOT_FOUND?0:PRO_TK_INCOMPLETE,L"拒绝孔间距从 70 改到 85 的不合配组");
    wchar_t first[40];
    swprintf_s(first,_countof(first),L"SURFACE:%d",c.holes[0]);
    error=ax_group_stations(root,w,h,L"FLOW_HOLE_BOLT",shaft,L"PLANE:FRONT",first,1,out);
    verify(46,error,L"在新规格孔组先装一个实际紧固件");
    if(!error)error=ax_group_stations(root,w,h,L"FLOW_HOLE_BOLT",shaft,L"PLANE:FRONT",z,0,out);
    if(!error&&!wcsstr(out->text,L"4 个实际孔位，空位 3，重复 0"))error=PRO_TK_INCOMPLETE;
    verify(46,error,L"读出未占用的新规格三站");
    if(!error)error=ax_group_stations(root,w,h,L"FLOW_HOLE_BOLT",shaft,L"PLANE:FRONT",z,1,out);
    if(!error)error=ax_group_stations(root,w,h,L"FLOW_HOLE_BOLT",shaft,L"PLANE:FRONT",z,1,out);
    if(!error&&!wcsstr(out->text,L"4 个实际孔位，空位 0，重复 0"))error=PRO_TK_INCOMPLETE;
    verify(46,error,L"只补三处空位并重新测量，重复发指令不重复安装");
    if(!error)flow_where_check(root,out);
}


static ProError flow_resume_fixture(ProMdl left,ProMdl right,const wchar_t *source,int a,int b,PartText *out){
    ProMdl target=NULL;
    ProError error=ProMdlSave(left);
    if(!error)error=ProMdlSave(right);
    if(!error)error=flow_mount_points(L"BW_FLOW_CONTINUE",&target);
    if(!error)error=ProMdlSave(target);
    if(error)return error;
    ProPath cwd;
    wchar_t plan[MAX_PATH];
    if((error=ProDirectoryCurrentGet(cwd)))return error;
    if(swprintf_s(plan,_countof(plan),L"%ls\\flow-resume-plan.tsv",cwd)<0)return PRO_TK_CANT_WRITE;
    FILE *file=_wfopen(plan,L"wt, ccs=UTF-8");
    if(!file)return PRO_TK_CANT_WRITE;
    fwprintf(file,L"ID:%d\tBW_FLOW_LEFT\t-\nID:%d\tBW_FLOW_RIGHT\t-\n",a,b);
    if(fclose(file))return PRO_TK_CANT_WRITE;
    error=ax_resume_begin(target,L"resume-fixture",source,plan,out);
    if(!error)error=ax_resume_run(target,L"resume-fixture",1,out);
    if(!error&&!wcsstr(out->text,L"已保存 1/2 步"))error=PRO_TK_INCOMPLETE;
    return error;
}

static ProError flow_resume_reopen(const wchar_t *cwd,PartText *out){
    ProMdl target=NULL;
    ProError error=ProDirectoryChange((wchar_t *)cwd);
    ProMdlName name=L"BW_FLOW_CONTINUE";
    if(!error)error=ProMdlnameRetrieve(name,PRO_MDLFILE_ASSEMBLY,&target);
    if(!error)error=ax_resume_run(target,L"resume-fixture",0,out);
    if(!error&&!wcsstr(out->text,L"已保存 1/2 步"))error=PRO_TK_INCOMPLETE;
    if(!error)error=ax_resume_run(target,L"resume-fixture",1,out);
    if(!error)error=ax_resume_run(target,L"resume-fixture",1,out);
    if(!error&&!wcsstr(out->text,L"已保存 2/2 步"))error=PRO_TK_INCOMPLETE;
    AxScan *tree=NULL;
    if(!error)error=ax_scan(target,0,&tree);
    if(!error&&tree->count!=2)error=PRO_TK_INCOMPLETE;
    free(tree);
    return error;
}


static void flow_rule_fixtures(PartText *out){
    wchar_t details[3500]=L"";
    PartText result={details,_countof(details),0};
    FILE *proof=_wfopen(L"flow-method-proof.txt",L"wt, ccs=UTF-8");
    if(!proof){verify(55,PRO_TK_CANT_WRITE,L"方法范围验收证据");return;}
    fputws(L"独立 Creo 原生装配已核对两支撑和三面定位。\n",proof);
    fclose(proof);
    FILE *table=_wfopen(L"flow-method-scope.tsv",L"wt, ccs=UTF-8");
    if(!table){verify(55,PRO_TK_CANT_WRITE,L"方法范围验收表");return;}
    fwprintf(table,L"双支撑样机\t孔距\t25\t85\t安装基准和原生三面\t超范围需验收\tflow-method-proof.txt\n");
    fclose(table);
    ProError error=std_scope_add(L"双支撑定位",L"装配",L"BW_FLOW_SOURCE",L"flow-method-scope.tsv",&result);
    if(!error)error=std_scope_query(L"双支撑定位",L"孔距",40,&result);
    if(!error&&!wcsstr(details,L"超范围需验收"))error=PRO_TK_INCOMPLETE;
    verify(55,error,L"同一实际学习案例保存证据、参照及允许的孔距范围");
    if(!error&&std_scope_query(L"双支撑定位",L"孔距",100,&result)!=PRO_TK_E_NOT_FOUND)error=PRO_TK_INCOMPLETE;
    if(!error&&std_scope_add(L"未知案例",L"装配",L"BW_FLOW_UNKNOWN",L"flow-method-scope.tsv",&result)!=PRO_TK_E_NOT_FOUND)
        error=PRO_TK_INCOMPLETE;
    verify(55,error,L"超出尺寸范围和缺少学习案例时无匹配");
    FILE *rules=_wfopen(L"flow-process-rules.tsv",L"wt, ccs=UTF-8");
    if(!rules){verify(56,PRO_TK_CANT_WRITE,L"工艺规则样件");return;}
    fwprintf(rules,L"*\t*\t板厚\tBASE\t2026-01-01\t-\t1\t通用板厚\n"
        L"Cab55\tSUP\t板厚\tA\t2026-01-01\t2026-04-30\t2\t旧版\n"
        L"Cab55\tSUP\t板厚\tB\t2026-05-01\t-\t2.5\t供应商五月生效版本\n");
    fclose(rules);
    details[0]=0;
    error=std_rule_import(L"flow-process-rules.tsv",&result);
    if(!error)error=std_rule_query(L"Cab55",L"SUP",L"板厚",L"2026-04-30",&result);
    if(!error)error=std_rule_query(L"Cab55",L"SUP",L"板厚",L"2026-05-01",&result);
    if(!error)error=std_rule_query(L"Cab55",L"ALT",L"板厚",L"2026-05-01",&result);
    if(!error&&(!wcsstr(details,L"修订 A")||!wcsstr(details,L"修订 B")||!wcsstr(details,L"修订 BASE")||
       std_rule_query(L"Cab55",L"SUP",L"板厚",L"2026-02-30",&result)!=PRO_TK_BAD_INPUTS))error=PRO_TK_INCOMPLETE;
    verify(56,error,L"按切换日期、机型和供应商切换真实装配工艺规则");
    rules=_wfopen(L"flow-process-overlap.tsv",L"wt, ccs=UTF-8");
    if(!rules)error=PRO_TK_CANT_WRITE;
    else{
        fwprintf(rules,L"Cab55\tSUP\t板厚\tA\t2026-01-01\t2026-05-01\t2\t旧版\n"
          L"Cab55\tSUP\t板厚\tB\t2026-05-01\t-\t2.5\t重叠一天\n");
        fclose(rules);
    }
    if(!error&&std_rule_import(L"flow-process-overlap.tsv",&result)!=PRO_TK_E_FOUND)error=PRO_TK_INCOMPLETE;
    if(!error)error=std_rule_query(L"Cab55",L"SUP",L"板厚",L"2026-05-01",&result);
    verify(56,error,L"日期重叠拒绝导入，先前生效的规则仍可查询");
    part_text_add(out,L"%ls",details);
}

static int exercise(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    flow_receipt=_wfopen(L"flow-acceptance.log",L"wt, ccs=UTF-8");
    if(!flow_receipt){ProEngineerEnd();return 0;}
    static wchar_t report[PART_TEXT_MAX];
    PartText out={report,_countof(report),0};
    wchar_t reopen[MAX_PATH];
    DWORD supplied=GetEnvironmentVariableW(L"BWTL_RESUME_WORKSPACE",reopen,_countof(reopen));
    if(supplied){
        ProError resume=supplied<_countof(reopen)?flow_resume_reopen(reopen,&out):PRO_TK_BAD_CONTEXT;
        verify(48,resume,L"全新 Creo 进程从 1/2 保存点恢复，只装剩余一件，再执行不会重复");
        fwprintf(flow_receipt,L"\nPID=%lu\n%ls\nRESUME_FINAL=%d CHECKS=%d\n",GetCurrentProcessId(),report,flow_fail,flow_tests);
        fclose(flow_receipt);
        ProEngineerEnd();
        return 0;
    }
    const wchar_t *source_key=L"BW_FLOW_SOURCE";
    ProMdl source=NULL,left=NULL,right=NULL,mount=NULL,derived=NULL;
    ProAsmcomp a={0},b={0},c={0};
    ProError error=flow_box(L"BW_FLOW_LEFT",&left);
    if(!error)error=flow_box(L"BW_FLOW_RIGHT",&right);
    if(!error)error=flow_box(L"BW_FLOW_MOUNT",&mount);
    if(!error)error=flow_mount_points(source_key,&source);
    if(!error)error=flow_add_csys(source,left,L"CSYS:LEFT_MOUNT",&a);
    if(!error)error=flow_add_csys(source,right,L"CSYS:RIGHT_MOUNT",&b);
    if(!error)error=flow_attach(source,mount,&a,&b,&c);
    if(!error)error=ax_learning_save(source,&out);
    verify(42,error,L"独立学习样件包含两个支撑与一个三面定位模块");
    wchar_t id[AX_STUDY_PATH],role[AX_STUDY_PATH],casefile[MAX_PATH];
    if(!error){
        ax_path_label(&c.id,1,id,_countof(id));
        FILE *layout=ax_study_layout_file(source_key,casefile)?_wfopen(casefile,L"wt, ccs=UTF-8"):NULL;
        if(!layout)error=PRO_TK_CANT_WRITE;
        else{
            fwprintf(layout,L"BWTL-ASSEMBLY-LAYOUT-1 %ls\n12-上\t导向轮支架\tBW_FLOW_MOUNT\t%ls\n",source_key,id);
            if(fclose(layout))error=PRO_TK_CANT_WRITE;
        }
    }
    if(!error){
        wchar_t text[MAX_PATH];
        if(!ax_flow_sentence(L"把上方导向轮支架装上。",text)||!ax_flow_key(source_key,text,role,&out)||wcscmp(role,id))
            error=PRO_TK_INCOMPLETE;
    }
    verify(41,error,L"口语站位和图纸编号关联成功");
    if(!error){
        const wchar_t *names[]={L"上方导向轮支架"};
        error=ax_flow_plan(source_key,names,1,&out);
        if(!error&&(!wcsstr(report,L"执行顺序共 3 步")||!wcsstr(report,L"3. 父装配 ROOT")))error=PRO_TK_INCOMPLETE;
    }
    verify(42,error,L"按实际约束依赖生成支撑后模块的计划");
    if(!error)error=flow_mount_points(L"BW_FLOW_DERIVED",&derived);
    if(!error)error=ax_flow_install(derived,source_key,L"上方导向轮支架",NULL,1,&out);
    AxScan *tree=NULL;
    if(!error)error=ax_scan(derived,0,&tree);
    if(!error&&tree->count!=3)error=PRO_TK_INCOMPLETE;
    free(tree);
    if(!error)error=ax_study_audit(derived,NULL,NULL,0,&out);
    verify(43,error,L"从空装配自动补两个支撑再安装模块");
    if(!error){
        ProMdl separate=NULL,newa=NULL,newb=NULL;
        ProAsmcomp aa={0},bb={0};
        wchar_t mapping[260];
        error=flow_box(L"BW_FLOW_NEW_LEFT",&newa);
        if(!error)error=flow_box(L"BW_FLOW_NEW_RIGHT",&newb);
        if(!error)error=flow_mount_points(L"BW_FLOW_MAPPED",&separate);
        if(!error)error=flow_add_csys(separate,newa,L"CSYS:LEFT_MOUNT",&aa);
        if(!error)error=flow_add_csys(separate,newb,L"CSYS:RIGHT_MOUNT",&bb);
        swprintf_s(mapping,_countof(mapping),L"ID:%d=ID:%d,ID:%d=ID:%d",a.id,aa.id,b.id,bb.id);
        if(!error)error=ax_study_apply(separate,source_key,id,L"BW_FLOW_MOUNT",mapping,0,&out);
        if(!error)error=ax_study_audit(separate,NULL,NULL,0,&out);
    }
    verify(44,error,L"同一模块的两个支撑分别重绑定到不同型号");
    if(!error)error=ax_flow_direction_save(source,source_key,L"朝外",L"CSYS:LEFT_MOUNT",L"-Z",&out);
    if(!error)error=ax_flow_direction_get(derived,source_key,L"朝外",L"ROOT",&out);
    if(!error&&(!wcsstr(report,L"LEFT_MOUNT 的 -Z")||!wcsstr(report,L", -1)")))error=PRO_TK_INCOMPLETE;
    verify(49,error,L"把站位方向写入案例文件并在独立派生装配中读回实际方向");
    if(!error)error=ax_flow_revision_note(source,source_key,L"测试双支撑规则",&out);
    if(!error)error=ax_flow_revision_note(source,source_key,L"留存相同定位规则",&out);
    if(!error)error=ax_flow_history(source_key,&out);
    if(!error&&(!wcsstr(report,L"原因 测试双支撑规则")||!wcsstr(report,L"学习验收：3/3")||
        !wcsstr(report,L"与上一份规则内容一致")||!wcsstr(report,L"显示 2 条")))error=PRO_TK_INCOMPLETE;
    verify(50,error,L"保存两次装配学习修订、验收计数和可对比的规则快照");
    if(!error){
        ProMdl wrapper=NULL;
        ProAsmcomp nested={0};
        ProMdlType type;
        wchar_t path[AX_STUDY_PATH],section[AX_STUDY_PATH],expected[260];
        error=flow_mount_points(L"BW_FLOW_SYSTEM",&wrapper);
        if(!error)error=ProMdlTypeGet(source,&type);
        if(!error&&type!=PRO_MDL_ASSEMBLY)error=PRO_TK_BAD_CONTEXT;
        if(!error)error=flow_add_csys(wrapper,source,L"CSYS:LEFT_MOUNT",&nested);
        if(!error)error=ax_learning_save(wrapper,&out);
        int ids[]={nested.id,c.id};
        ax_path_label(ids,2,path,_countof(path));
        ax_path_label(ids,1,section,_countof(section));
        const wchar_t *labels[]={path};
        if(!error)error=ax_flow_plan(L"BW_FLOW_SYSTEM",labels,1,&out);
        swprintf_s(expected,_countof(expected),L"3. 父装配 %ls，%ls BW_FLOW_MOUNT",section,path);
        if(!error&&!wcsstr(report,expected))error=PRO_TK_INCOMPLETE;
    }
    verify(42,error,L"在独立顶层系统实际学习子装配，并在正确父层列出三个步骤");
    if(!error){
        ProMdl stage=NULL;
        ProAsmcomp sa={0},sb={0};
        wchar_t first[260],second[260];
        error=flow_mount_points(L"BW_FLOW_BRANCHES",&stage);
        if(!error)error=flow_add_csys(stage,left,L"CSYS:LEFT_MOUNT",&sa);
        if(!error)error=flow_add_csys(stage,right,L"CSYS:RIGHT_MOUNT",&sb);
        if(!error)error=ProMdlSave(left);
        if(!error)error=ProMdlSave(right);
        if(!error)error=ProMdlSave(stage);
        swprintf_s(first,_countof(first),L"ID:%d=ID:%d,ID:%d=ID:%d",a.id,sa.id,b.id,sb.id);
        swprintf_s(second,_countof(second),L"ID:%d=ID:%d,ID:%d=ID:999999",a.id,sa.id,b.id);
        wchar_t details[2500]=L"";
        PartText result={details,_countof(details),0};
        if(!error)error=ax_flow_compare_branches(stage,source_key,id,L"BW_FLOW_MOUNT",first,second,&result);
        AxScan *check=NULL;
        if(!error)error=ax_scan(stage,0,&check);
        if(!error&&(check->count!=2||!wcsstr(details,L"候选 1 通过")||!wcsstr(details,L"候选 2 未通过")||
            !wcsstr(details,L"通过 1/2")))error=PRO_TK_INCOMPLETE;
        free(check);
        part_text_add(&out,L"%ls",details);
    }
    verify(47,error,L"独立试件实际比较两套支撑，定位未通过的参照，原装配仍只有两个支撑");
    if(!error)error=flow_resume_fixture(left,right,source_key,a.id,b.id,&out);
    verify(48,error,L"实机保存首个组件及可持久读取的剩余步骤");
    if(!error)flow_groups(&out);
    if(!error)flow_rule_fixtures(&out);
    fwprintf(flow_receipt,L"\nPID=%lu\n%ls\nFLOW_FINAL=%d CHECKS=%d\n",GetCurrentProcessId(),report,flow_fail,flow_tests);
    fclose(flow_receipt);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=flow_toolkit_init(argc,argv,version,build,buffer);
    uiCmdCmdId action;
    if(!error)error=ProCmdActionAdd("BWTLFlowAcceptance",exercise,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&action);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLFlowAcceptance\x0060;");
}
