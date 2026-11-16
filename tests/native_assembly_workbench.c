#define BWTL_ASSEMBLY_WORKBENCH_TEST
#define BWTL_REUSE_ENCLOSURE_FIXTURE
#include "native_assembly_enclosure.c"
#undef user_initialize

typedef struct {ProSurface face;int ids[4];double x,y,z;} AwFixtureEdges;
static ProError aw_fixture_edge(ProEdge edge,ProError error,ProAppData data){
    if(error)return error;
    AwFixtureEdges *list=data;
    ProVector a,b;
    int id;
    if((error=ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL))||
       (error=ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL))||(error=ProEdgeIdGet(edge,&id)))return error;
    const double t=0.001;
    if(fabs(a[2]-list->z)>t||fabs(b[2]-list->z)>t)return PRO_TK_NO_ERROR;
    if(fabs(a[1])<t&&fabs(b[1])<t)list->ids[0]=id;
    if(fabs(a[0]-list->x)<t&&fabs(b[0]-list->x)<t)list->ids[1]=id;
    if(fabs(a[1]-list->y)<t&&fabs(b[1]-list->y)<t)list->ids[2]=id;
    if(fabs(a[0])<t&&fabs(b[0])<t)list->ids[3]=id;
    return PRO_TK_NO_ERROR;
}
static ProError aw_fixture_contour(ProContour contour,ProError error,ProAppData data){
    if(error)return error;
    AwFixtureEdges *list=data;
    ProContourTraversal how;
    error=ProContourTraversalGet(contour,&how);
    if(error||how!=PRO_CONTOUR_TRAV_EXTERNAL)return error;
    return ProContourEdgeVisit(list->face,contour,aw_fixture_edge,NULL,data);
}

static ProError aw_fixture_tag(ProMdl assembly,const wchar_t *id,const wchar_t *name,const wchar_t *value){
    AxComponent found={0};
    ProError error=ax_component_resolve(assembly,id,&found);
    if(error)return error;
    ProParamvalue parameter={0};
    ProParameter output;
    parameter.type=PRO_PARAM_STRING;
    wcscpy_s(parameter.value.s_val,_countof(parameter.value.s_val),value);
    return ProParameterWithUnitsCreate(&found.feature,(wchar_t *)name,&parameter,NULL,&output);
}
typedef struct {ProName symbol;} AwFixtureDimension;
static ProError aw_fixture_dimension(ProDimension *dimension,ProError status,ProAppData context){
    if(status)return status;
    AwFixtureDimension *entry=context;
    if(entry->symbol[0])return PRO_TK_NO_ERROR;
    ProDimensiontype type;
    ProBoolean driven;
    if(!ProDimensionTypeGet(dimension,&type)&&type==PRODIMTYPE_LINEAR&&
       !ProDimensionIsReldriven(dimension,&driven)&&!driven)
        return ProDimensionSymbolGet(dimension,entry->symbol);
    return PRO_TK_NO_ERROR;
}

static ProError aw_fixture_inner_line(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    AwFixtureEdges *pick=data;
    ProEnttype kind;
    ProError error=ProEdgeTypeGet(edge,&kind);
    if(error||kind!=PRO_ENT_LINE)return error;
    ProVector a,b;
    if((error=ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL))||
       (error=ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL)))return error;
    if(fabs(a[1]-60)<.001&&fabs(b[1]-60)<.001&&fabs(a[0]-b[0])>55)
        return ProEdgeIdGet(edge,&pick->ids[0]);
    return PRO_TK_NO_ERROR;
}

static ProError aw_fixture_inner_loop(ProContour contour,ProError status,ProAppData data){
    if(status)return status;
    AwFixtureEdges *pick=data;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(contour,&traversal);
    return error||traversal!=PRO_CONTOUR_TRAV_INTERNAL?error:
        ProContourEdgeVisit(pick->face,contour,aw_fixture_inner_line,NULL,data);
}


static int aw_fixture_run(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl root=NULL,part=NULL,frame=NULL,glass=NULL,nested=NULL,short_fastener=NULL,big=NULL,prior=NULL;
    ProMdlName name=L"",sub=L"";
    ProWVerstamp original=NULL,new_stamp=NULL;
    ProPath directory;
    const char *stage="";
    const wchar_t *source=L"AWB_PLATE";
    wchar_t line[1600],report[PART_TEXT_MAX],a[40],b[40],s[40],g[40],h[40],roof[40],ft_a[40],ft_b[40],nested_id[60];
    DevFaces plate={0},windows={0},glass_faces={0},fastener1={0};
    int window;
    PartCommand parsed;
    PartSnapshot snapshot={0};
    ProAsmcomp sub_instance,sub_part;
    ProError error=PRO_TK_NO_ERROR;
#define STEP(call) do{if((error=(call))){trace("workbench fixture " #call,error);check(0,"workbench fixture");goto done;}}while(0)
#define CASE(reject,expected,protocol,...) do{swprintf_s(line,_countof(line),L"CREO_OP " protocol,name,__VA_ARGS__);command(root,line,reject,expected);}while(0)
    STEP(env_stock(L"AWB_PLATE",0,100,80,10,1,&part));
    STEP(env_stock(L"AWB_GLASS",0,50,30,3,0,&glass));
    STEP(env_faces(part,10,&plate));
    STEP(env_datum(part,L"CREO_OP DATUM_AXIS AWB_PLATE TRACK PLANES PLANE:TOP PLANE:RIGHT"));
    STEP(env_datum(part,L"CREO_OP DATUM_CSYS AWB_PLATE ALT CSYS:PRT_CSYS_DEF 2 0 0 0 0 0"));
    STEP(env_faces(glass,3,&glass_faces));
    AwFixtureEdges edges={0};
    edges.x=50;edges.y=30;edges.z=3;
    STEP(ProSurfaceInit((ProSolid)glass,glass_faces.top_id,&edges.face));
    STEP(ProSurfaceContourVisit(edges.face,aw_fixture_contour,NULL,&edges));
    check(edges.ids[0]&&edges.ids[1]&&edges.ids[2]&&edges.ids[3],"four native seam edges");
    STEP(env_datum(part,L"CREO_OP DATUM_CSYS AWB_PLATE K2 CSYS:PRT_CSYS_DEF 100 0 0 0 0 0"));
    STEP(env_datum(part,L"CREO_OP DATUM_CSYS AWB_PLATE K3 CSYS:PRT_CSYS_DEF 100 80 0 0 0 0"));
    STEP(env_datum(part,L"CREO_OP DATUM_CSYS AWB_PLATE K4 CSYS:PRT_CSYS_DEF 0 80 0 0 0 0"));
    STEP(env_datum(part,L"CREO_OP DATUM_CURVE AWB_PLATE WIRE1 CSYS:PRT_CSYS_DEF 0 0 30 20 0 30"));
    STEP(env_datum(part,L"CREO_OP DATUM_CURVE AWB_PLATE WIRE2 CSYS:PRT_CSYS_DEF 20 0 30 20 30 30"));
    STEP(env_datum(part,L"CREO_OP DATUM_CURVE AWB_PLATE DISCONNECTED CSYS:PRT_CSYS_DEF 80 0 30 80 20 30"));
    STEP(env_datum(part,L"CREO_OP DATUM_CURVE_SPLINE AWB_PLATE WIRE_SPLINE CSYS:PRT_CSYS_DEF 20 0 35 25 5 36 30 12 39"));
    STEP(env_datum(part,L"CREO_OP DATUM_CSYS AWB_PLATE RW_START CSYS:PRT_CSYS_DEF -5 15 15 0 0 0"));
    STEP(env_datum(part,L"CREO_OP DATUM_CSYS AWB_PLATE RW_END CSYS:PRT_CSYS_DEF 110 15 15 0 0 0"));
    if(!plate.top_id||!plate.bottom_id||!plate.side||!plate.hole[0]){error=PRO_TK_E_NOT_FOUND;goto done;}
    if(part_command_parse(L"CREO_OP DV_ATTR AWB_PLATE 0 PART_CODE AWB-01 REVISION B",&parsed)!=1){error=PRO_TK_BAD_INPUTS;goto done;}
    STEP(dv_execute(part,&parsed,report,_countof(report)));
    if(part_command_parse(L"CREO_OP DV_ATTR AWB_PLATE 0 BWTL_SLOT ST1 BWTL_KIND END_PLATE BWTL_STATION BAY_A BWTL_BUILD_STATUS TEMP BWTL_BUILD_NOTE TRIAL",&parsed)!=1){error=PRO_TK_BAD_INPUTS;goto done;}
    STEP(dv_execute(part,&parsed,report,_countof(report)));
    if(part_command_parse(L"CREO_OP DV_ATTR AWB_PLATE 0 BWTL_TORQUE 2 BWTL_LOCKING PATCH BWTL_TORQUE_MARK YES",&parsed)!=1){error=PRO_TK_BAD_INPUTS;goto done;}
    STEP(dv_execute(part,&parsed,report,_countof(report)));
    if(part_command_parse(L"CREO_OP DV_ATTR AWB_GLASS 0 PART_CODE GLASS-01 BWTL_KIND MARKER",&parsed)!=1){error=PRO_TK_BAD_INPUTS;goto done;}
    STEP(dv_execute(glass,&parsed,report,_countof(report)));
    if(part_command_parse(L"CREO_OP QUICK_SCREEN AWB_FRAME 100 80 10 60 40 80 60 6",&parsed)!=1){error=PRO_TK_BAD_INPUTS;goto done;}
    STEP(ProDirectoryCurrentGet(snapshot.directory));
    STEP(quick_execute(&parsed,&snapshot,report,_countof(report)));
    STEP(ProMdlnameInit(L"AWB_FRAME",PRO_MDL_PART,&frame));
    STEP(env_faces(frame,10,&windows));
    if(!windows.window[0]||!windows.window[1]||!windows.window[2]||!windows.window[3]){error=PRO_TK_E_NOT_FOUND;goto done;}
    STEP(env_stock(L"AWB_FST_SMALL",1,6,6,12,0,&short_fastener));
    STEP(env_stock(L"AWB_FST_BIG",1,6,6,15,0,&big));
    STEP(env_faces(short_fastener,12,&fastener1));
    STEP(env_datum(short_fastener,L"CREO_OP DATUM_CSYS AWB_FST_SMALL PAD_SUPPORT CSYS:PRT_CSYS_DEF 0 6 0 0 0 0"));
    if(!fastener1.shaft||!fastener1.bottom_id){error=PRO_TK_E_NOT_FOUND;goto done;}
    if(part_command_parse(L"CREO_OP DV_ATTR AWB_FST_SMALL 0 BWTL_KIND FASTENER SPEC M6 BWTL_LENGTH 12",&parsed)!=1){error=PRO_TK_BAD_INPUTS;goto done;}
    STEP(dv_execute(short_fastener,&parsed,report,_countof(report)));
    if(part_command_parse(L"CREO_OP DV_ATTR AWB_FST_BIG 0 BWTL_KIND FASTENER SPEC M6 BWTL_LENGTH 15",&parsed)!=1){error=PRO_TK_BAD_INPUTS;goto done;}
    STEP(dv_execute(big,&parsed,report,_countof(report)));
    if(!CopyFileW(L"C:\\Program Files\\PTC\\Creo 10.0.0.0\\Common Files\\creo_standards\\material_database\\plastic_pvc.mtl",
        L"plastic_pvc.mtl",FALSE)){error=PRO_TK_CANT_OPEN;goto done;}
    STEP(material_assign(part));
    STEP(material_assign(frame));
    STEP(material_assign(glass));
    STEP(material_assign(short_fastener));
    STEP(material_assign(big));
    model_background_mode=1;
    STEP(model_create_empty(3,sub,&stage));
    STEP(ProMdlnameInit(sub,PRO_MDL_ASSEMBLY,&nested));
    STEP(add_component(nested,part,1,&sub_part));
    STEP(part_regenerate(nested));
    model_background_mode=0;
    STEP(model_create_empty(3,name,&stage));
    STEP(ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&root));
    STEP(ProMdlWindowGet(root,&window));
    STEP(ProWindowCurrentSet(window));
    STEP(env_add(root,part,0,0,0,a));
    STEP(env_add(root,part,102,0,1,b));
    STEP(env_add(root,frame,300,0,0,s));
    STEP(env_add(root,glass,325,25,0,g));
    STEP(env_add(root,glass,30,25,11,h));
    STEP(env_add(root,glass,30,25,18,roof));
    STEP(env_add(root,short_fastener,12,12,10,ft_a));
    STEP(env_add(root,big,85,15,10,ft_b));
    STEP(add_component(root,nested,600,&sub_instance));
    STEP(part_regenerate(root));
    swprintf_s(nested_id,_countof(nested_id),L"ID:%d/%d",sub_instance.id,sub_part.id);
    STEP(aw_fixture_tag(root,roof,L"BWTL_KIND",L"HARNESS"));
    swprintf_s(line,_countof(line),L"%ls@CSYS:K2",a);
    STEP(aw_fixture_tag(root,roof,L"BWTL_WIRE_FROM",line));
    swprintf_s(line,_countof(line),L"%ls@CSYS:K3",b);
    STEP(aw_fixture_tag(root,roof,L"BWTL_WIRE_TO",line));
    STEP(ProMdlVerstampGet(root,&original));
    STEP(ProDirectoryCurrentGet(directory));

    CASE(0,b,L"ASM_W_FIND %ls %ls +X 2 0.01",source);
    CASE(0,nested_id,L"ASM_W_FIND %ls %ls +X 3 0.01",source);
    CASE(1,L"不能核实",L"ASM_W_FIND %ls %ls +X 4 0.01",source);
    CASE(0,L"对平面",L"ASM_W_PAIRS %ls %ls %ls 2.1 0.01",a,b);
    CASE(0,L"方向相反",L"ASM_W_ORIENTATION %ls %ls CSYS:PRT_CSYS_DEF -Z +Z 5",a);
    CASE(0,L"已核对 0 条约束",L"ASM_W_CONFLICT %ls %ls",a);
    CASE(0,L"沿三轴平移",L"ASM_W_FREEDOM %ls %ls",a);
    CASE(0,L"第 2 层",L"ASM_W_OCCURRENCE %ls %ls",nested_id);
    CASE(1,NULL,L"ASM_W_OCCURRENCE %ls %ls",L"ID:99999/99999");
    CASE(0,L"最短合格长度 15 mm",L"ASM_W_SCREW_LENGTH %ls %ls SURFACE:%d %ls SURFACE:%d 4 8 M5X12 12 M5X18 18 M5X15 15",a,plate.top_id,a,plate.bottom_id);
    CASE(0,L"螺钉将顶底",L"ASM_W_BLIND_BOTTOM %ls %ls SURFACE:%d %ls SURFACE:%d 20 7 8 4 1",a,plate.top_id,a,plate.bottom_id);
    CASE(0,L"高出 1 mm",L"ASM_W_COUNTERSINK %ls %ls SURFACE:%d %ls SURFACE:%d 90 90 0.5 1",a,plate.top_id,b,plate.top_id);
    CASE(0,L"孔径范围符合",L"ASM_W_PRESS_FIT %ls %ls SURFACE:%d %ls SURFACE:%d SURFACE:%d 9 11 7 9 2",a,plate.top_id,a,plate.bottom_id,plate.hole[0]);
    CASE(0,L"工艺条件不符合",L"ASM_W_PRESS_FIT %ls %ls SURFACE:%d %ls SURFACE:%d SURFACE:%d 9 11 7 9 2 10 3",a,plate.top_id,a,plate.bottom_id,plate.hole[0]);
    CASE(0,L"工艺条件符合",L"ASM_W_PRESS_FIT %ls %ls SURFACE:%d %ls SURFACE:%d SURFACE:%d 9 11 7 9 4 10 3",a,plate.top_id,a,plate.bottom_id,plate.hole[0]);
    CASE(0,L"所给",L"ASM_W_SCREW_LENGTH %ls %ls SURFACE:%d %ls SURFACE:%d 6 9 M5X10 10",a,plate.top_id,a,plate.bottom_id);
    CASE(0,L"不符合",L"ASM_W_RIVET_GRIP %ls %ls SURFACE:%d %ls SURFACE:%d 8 12 3 4",a,plate.top_id,a,plate.bottom_id);
    CASE(0,L"共 0 侧不足",L"ASM_W_THERMAL_GAP %ls %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d 9 23 60 4 50 40",g,s,windows.window[0],windows.window[1],windows.window[2],windows.window[3]);
    CASE(0,L"最小 1 mm",L"ASM_W_DOOR_GAP %ls 0.8 1.2 0.1 %ls SURFACE:%d %ls SURFACE:%d",a,plate.top_id,h,glass_faces.bottom_id);
    CASE(0,L"风向不符合 1 个",L"ASM_W_FANS %ls +Z 1 %ls CSYS:PRT_CSYS_DEF +Z %ls CSYS:PRT_CSYS_DEF -Z",a,b);
    CASE(0,L"排水口高于底板低点",L"ASM_W_DRAIN %ls %ls SURFACE:%d %ls SURFACE:%d CSYS:ASM_CSYS 0.5",a,plate.bottom_id,b,plate.side);
    CASE(0,L"版本不符 3",L"ASM_W_REVISIONS %ls %ls",L"AWB-01 A");
    CASE(0,L"超出图纸公差",L"ASM_W_ASBUILT %ls %ls SURFACE:%d %ls SURFACE:%d 11 10 0.5 0.5",a,plate.top_id,a,plate.bottom_id);
    CASE(0,L"方正度符合",L"ASM_W_SQUARE %ls 0.01 0.1 %ls CSYS:PRT_CSYS_DEF %ls CSYS:K2 %ls CSYS:K3 %ls CSYS:K4",a,a,a,a);
    CASE(0,L"合成 0 mm",L"ASM_W_TWIST %ls %ls CSYS:PRT_CSYS_DEF %ls CSYS:PRT_CSYS_DEF 102 0 1 0.01 0.1",a,b);
    CASE(0,L"应伸出 +3 mm",L"ASM_W_LEVEL %ls CSYS:ASM_CSYS %ls SURFACE:%d -3 5 8 6",a,plate.bottom_id);
    CASE(0,L"旋转 +90°，平移 (10, 20) mm",L"ASM_W_ANCHORS %ls CSYS:ASM_CSYS 0.01 %ls CSYS:PRT_CSYS_DEF 10 20 %ls CSYS:K2 10 120 %ls CSYS:K3 -70 120",a,a,a);
    CASE(0,L"下料长度 62 mm",L"ASM_W_WIRE_LENGTH %ls 3 4 5 0.01 %ls CURVE:WIRE1 %ls CURVE:WIRE2",a,a);
    CASE(1,L"与上一段端点不连续",L"ASM_W_WIRE_LENGTH %ls 0 0 0 0.01 %ls CURVE:WIRE1 %ls CURVE:DISCONNECTED",a,a);
    CASE(0,L"段内没有弯曲",L"ASM_W_WIRE_BEND %ls 8 %ls CURVE:WIRE1",a);
    CASE(1,L"弯曲半径未核实",L"ASM_W_WIRE_BEND %ls 8 %ls CURVE:WIRE_SPLINE",a);
    CASE(0,L"面积或线径不符合",L"ASM_W_CHANNEL_FILL %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d 5 40 20 4",s,windows.window[0],windows.window[1],windows.window[2],windows.window[3]);
    CASE(0,L"遮挡 1 边",L"ASM_W_DISPLAY_VIEW %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d %ls SURFACE:%d 35 0",s,windows.window[0],windows.window[1],windows.window[2],windows.window[3],g,glass_faces.bottom_id);
    CASE(0,L"4 边需核对",L"ASM_W_BYPASS %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d %ls SURFACE:%d 4",s,windows.window[0],windows.window[1],windows.window[2],windows.window[3],g,glass_faces.bottom_id);
    CASE(0,L"净截面 900 mm²",L"ASM_W_AIRFLOW %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d 1 1000 %ls",s,windows.window[0],windows.window[1],windows.window[2],windows.window[3],g);
    CASE(0,L"最小空气间隙 2 mm",L"ASM_W_VOLTAGE_ZONE %ls %ls %ls 3",a,b);
    CASE(0,L"4 个方向不足",L"ASM_W_RAIN_COVER %ls CSYS:ASM_CSYS %ls SURFACE:%d %ls SURFACE:%d 1 1 1 1",roof,glass_faces.bottom_id,h,glass_faces.top_id);
    CASE(0,L"超限接头 0 个",L"ASM_W_SEAL_LOOP %ls %ls SURFACE:%d %ls SURFACE:%d 5 15 25 0.01 EDGE:%d EDGE:%d EDGE:%d EDGE:%d",h,glass_faces.top_id,roof,glass_faces.bottom_id,edges.ids[0],edges.ids[1],edges.ids[2],edges.ids[3]);

    CASE(0,L"冲突 1 对",L"ASM_W_SLOT_RULE %ls%ls",L"");
    CASE(0,L"规则不符 1 条",L"ASM_W_REQUIRED %ls ROOT AWB-01 GLASS-01 2%ls",L"");
    CASE(0,L"规则不符 1 条",L"ASM_W_TERMINALS %ls ROOT END_PLATE 3 MARKER 3%ls",L"");
    CASE(0,L"工艺标注不符 1 个",L"ASM_W_TORQUE %ls %ls 3 0.1 PATCH YES %ls 2 0.1 PATCH YES",a,b);
    CASE(0,L"数量差 1 项",L"ASM_W_PRODUCTION_BOM %ls AWB-01 3 GLASS-01 2%ls",L"");
    CASE(0,L"临时/代用/待采购 3 个",L"ASM_W_TEMPORARY %ls%ls",L"");
    CASE(0,L"工位匹配 3 个",L"ASM_W_ISSUE_LIST %ls BAY_A SHOP_PICK_LIST%ls",L"");
    check(dv_file_nonempty(L"SHOP_PICK_LIST\\workstation-picking.csv"),"manufacturing pick list on E workspace");
    CASE(0,L"有 1 个共享",L"ASM_W_SHARED_MODELS %ls %ls %ls",a,sub);
    CASE(0,L"预计可再放 1 个",L"ASM_W_RAIL_CAPACITY %ls CSYS:ASM_CSYS 0 210 0 50 2 8 5 10 %ls %ls",a,b);
    CASE(0,L"判断为反装",L"ASM_W_FASTENER_SIDE %ls %ls SURFACE:%d SURFACE:%d %ls SURFACE:%d SURFACE:%d 0.01 0.01",a,plate.hole[0],plate.top_id,ft_a,fastener1.shaft,fastener1.bottom_id);
    CASE(0,L"发现 1 对邻近混长",L"ASM_W_MIXED_SCREWS %ls ROOT 100%ls",L"");
    CASE(0,L"已排 2 个，未排 0 个",L"ASM_W_ELECTRIC_LAYOUT %ls CSYS:ASM_CSYS 0 0 220 200 5 %ls CSYS:PRT_CSYS_DEF +Y %ls CSYS:PRT_CSYS_DEF +X",a,b);
    CASE(0,L"Creo 直接组件依赖",L"ASM_W_INSTALL_ORDER %ls 2 %ls %ls %ls %ls",a,b,b,a);
    CASE(1,L"形成循环",L"ASM_W_INSTALL_ORDER %ls 2 %ls %ls %ls %ls %ls %ls",a,b,b,a,a,b);
    CASE(0,L"2 段，1 段不足",L"ASM_W_TRANSPORT %ls %ls CSYS:ASM_CSYS 2 1 1 1 STRAIGHT_X 107 87 17 TURN 100 100 17",a);
    CASE(0,L"紧固工具空间核对 3/3",L"ASM_W_TOOL_ACCESS %ls 1 STRAIGHT %ls %ls AXIS:TRACK SLIDE 0 3 2 ANGLED %ls %ls AXIS:TRACK SLIDE 0 3 2 SWING %ls %ls AXIS:TRACK ROTATE 0 3 2",a,s,a,s,a,s);
    CASE(1,NULL,L"ASM_W_TOOL_ACCESS %ls 1 STRAIGHT %ls %ls AXIS:TRACK SLIDE 0 3 2",a,s);
    CASE(0,L"关门末段核对 1/1",L"ASM_W_DOOR_FINAL %ls 1 CLOSE %ls %ls AXIS:TRACK ROTATE 5 0 3",a,s);
    CASE(0,L"长度符合",L"ASM_W_TERMINAL_ACCESS %ls 1 6 10 4 WIRE %ls %ls AXIS:TRACK SLIDE 0 3 2 TOOL %ls %ls AXIS:TRACK SLIDE 0 2 2",a,s,a,s);
    CASE(0,L"连接器插拔核对 3/3",L"ASM_W_PLUG_PATH %ls 1 PLUG %ls %ls AXIS:TRACK SLIDE 0 9 2 RELEASE %ls %ls AXIS:TRACK ROTATE 0 3 2 CABLE %ls %ls AXIS:TRACK SLIDE 0 5 2",a,s,a,s,a,s);
    CASE(0,L"导轨模块拆卸核对 1/1",L"ASM_W_RAIL_SERVICE %ls 1 SLIDE %ls %ls AXIS:TRACK SLIDE 0 5 2",a,s);
    CASE(0,L"小耗材取出核对 2/2",L"ASM_W_SMALL_SERVICE %ls 1 GRIP %ls %ls AXIS:TRACK SLIDE 0 6 2 REMOVE %ls %ls AXIS:TRACK SLIDE 0 4 2",a,s,a,s);
    CASE(0,L"滤网更换核对 2/2",L"ASM_W_FILTER_SERVICE %ls 1 DRAW %ls %ls AXIS:TRACK SLIDE 0 9 2 GRIP %ls %ls AXIS:TRACK SLIDE 0 7 2",a,s,a,s);
    CASE(0,L"部件装入轨迹核对 3/3",L"ASM_W_INSTALL_PATH %ls 1 ENTRY %ls %ls AXIS:TRACK SLIDE 0 3 2 TURN %ls %ls AXIS:TRACK ROTATE 0 8 3 SEAT %ls %ls AXIS:TRACK SLIDE 0 5 2",a,s,a,s,a,s);
    CASE(0,L"按给定垫片取 +1 mm",L"ASM_W_DOOR_SHIM %ls 2 0.5 4 %ls SURFACE:%d %ls SURFACE:%d",a,plate.top_id,h,glass_faces.bottom_id);
    CASE(0,L"有拉紧风险",L"ASM_W_DOOR_WIRE %ls ROOT CSYS:ASM_CSYS %ls CSYS:K3 AXIS:TRACK 0 90 120 10 9",b);
    CASE(0,L"余量符合",L"ASM_W_EDGE_WIRE %ls %ls CURVE:WIRE1 %ls EDGE:%d 4 0.5",a,h,edges.ids[0]);
    CASE(0,L"外皮间距 -4 mm，夹角 90°",L"ASM_W_WIRE_CROSS %ls %ls CURVE:WIRE1 %ls CURVE:WIRE2 4 4 1 5 60",a,a);
    CASE(0,L"缺连接 1、表外连接 0",L"ASM_W_CONNECTIONS %ls %ls@CSYS:K2 %ls@CSYS:K3 %ls@CSYS:K4 ROOT@CSYS:ASM_CSYS",a,b,b);
    CASE(0,L"影响装配 0 对",L"ASM_W_WELD_CONFLICT %ls %ls %ls 0",a,b);
    CASE(0,L"焊接及打磨工具核对 2/2",L"ASM_W_WELD_TOOL %ls 1 WELD %ls %ls AXIS:TRACK SLIDE 0 2 2 GRIND %ls %ls AXIS:TRACK SLIDE 0 2 2",a,s,a,s);
    CASE(0,L"已在活动模型高亮",L"ASM_W_HIGHLIGHT %ls %ls SURFACE:%d",a,plate.top_id);
    CASE(0,L"已清除装配检查高亮",L"ASM_W_HIGHLIGHT %ls %ls",L"CLEAR");
    CASE(0,L"核对 1 个安装面与 1 个指定孔；0 项不符",L"ASM_W_INTERFACE %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d 10 0.1 0.1 SURFACE:%d 15 15 0.1 0.1",a,plate.top_id,plate.hole[0]);
    AwFixtureDimension dim={0};
    STEP(ProSolidDimensionVisit((ProSolid)part,PRO_B_FALSE,aw_fixture_dimension,NULL,&dim));
    if(dim.symbol[0])CASE(0,L"只预览可读取",L"ASM_W_CHANGE_PREVIEW %ls %ls %ls 25",a,dim.symbol);
    else check(0,"read a real linear driver for preview");
    CASE(0,L"已输出 2 个实际接口",L"ASM_W_INTERFACE_DRAW %ls INTERFACES CSYS:ASM_CSYS INLET %ls CSYS:K2 ANCHOR %ls CSYS:K4",a,b);
    check(dv_file_nonempty(L"INTERFACES\\assembly-interfaces.svg")&&dv_file_nonempty(L"INTERFACES\\assembly-interfaces.csv"),"native interface SVG and CSV");
    FILE *orders=_wfopen(L"order_rows.csv",L"wt, ccs=UTF-8");
    if(!orders){error=PRO_TK_CANT_WRITE;goto done;}
    fwprintf(orders,L"ORDER,ASSEMBLY,MAX_X,MAX_Y,MAX_Z,MIN_GAP,COMPONENT_A,COMPONENT_B\n");
    fwprintf(orders,L"OK1,%ls,2000,1000,1000,1,%ls,%ls\n",name,a,b);
    fwprintf(orders,L"BAD1,%ls,2000,1000,1000,3,%ls,%ls\n",name,a,b);
    fclose(orders);
    CASE(0,L"通过 1 行，有超差 1 行",L"ASM_W_ORDER_AUDIT %ls order_rows.csv%ls",L"");
    CASE(0,L"释放后路径命中组件",L"ASM_W_DROP_FASTENER %ls %ls CSYS:PRT_CSYS_DEF -Z 30 %ls",h,a);
    CASE(0,L"标牌视线受遮挡",L"ASM_W_LABEL_VIEW %ls %ls CSYS:PRT_CSYS_DEF -Z 30 %ls",h,a);
    CASE(0,L"垂直滴水路径命中组件",L"ASM_W_WATER_PATH %ls %ls CSYS:PRT_CSYS_DEF -Z 30 %ls",h,a);
    CASE(0,L"释放后路径命中组件",L"ASM_W_RELEASE_DROP %ls %ls CSYS:PRT_CSYS_DEF -Z 30 %ls",h,a);
    AwFixtureEdges opening_edge={0};
    STEP(ProSurfaceInit((ProSolid)frame,windows.top_id,&opening_edge.face));
    STEP(ProSurfaceContourVisit(opening_edge.face,aw_fixture_inner_loop,NULL,&opening_edge));
    check(opening_edge.ids[0]>0,"actual top-face inner profile edge found");
    CASE(0,L"0 角不足",L"ASM_W_GLASS_CORNER %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d EDGE:%d %ls SURFACE:%d EDGE:%d 0.5 4",s,windows.top_id,opening_edge.ids[0],g,glass_faces.top_id,edges.ids[0]);
    CASE(0,L"胶槽截面 1：净宽 60 mm，净深 3 mm",L"ASM_W_GLUE_SPACE %ls 30 2 10 %ls SURFACE:%d %ls SURFACE:%d %ls SURFACE:%d %ls SURFACE:%d",s,windows.window[0],s,windows.window[1],g,glass_faces.bottom_id,g,glass_faces.top_id);
    CASE(0,L"留空不足",L"ASM_W_GLASS_PRESSURE %ls %ls 3 %ls",a,b);
    CASE(0,L"侵入散热禁放包络",L"ASM_W_HEAT_ZONE %ls %ls SURFACE:%d 5 1 %ls",a,plate.top_id,h);
    CASE(0,L"有直接回流风险",L"ASM_W_AIR_PAIR %ls %ls CSYS:PRT_CSYS_DEF +X %ls CSYS:PRT_CSYS_DEF -X 110 10",a,b);
    CASE(0,L"几何适配",L"ASM_W_GROUND_BOND %ls %ls SURFACE:%d SURFACE:%d %ls SURFACE:%d SURFACE:%d 0.1",ft_a,fastener1.shaft,fastener1.bottom_id,a,plate.hole[0],plate.top_id);
    CASE(0,L"户外附件安装和检修核对 2/2",L"ASM_W_OUTDOOR_ACCESS %ls 1 INSTALL %ls %ls AXIS:TRACK SLIDE 0 5 2 REMOVE %ls %ls AXIS:TRACK SLIDE 0 5 2",a,s,a,s);
    CASE(0,L"截面几何可通过",L"ASM_W_CONNECTOR_OPENING %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d %ls X 25 1",s,windows.window[0],windows.window[1],windows.window[2],windows.window[3],g);
    CASE(0,L"需要先安装，然后封板",L"ASM_W_CLOSE_BLOCKS %ls 3 OPEN %ls %ls AXIS:TRACK SLIDE 0 1 2 CLOSED %ls %ls AXIS:TRACK SLIDE 0 1 2",a,s,a,b);
    CASE(0,L"当前姿态几何通过",L"ASM_W_POSTURE %ls CSYS:ASM_CSYS X 90 5 -2000 -2000 2000 -2000 2000 2000 -2000 2000%ls",L"");
    CASE(0,L"工装和临时支撑核对 3/3",L"ASM_W_TEMP_SUPPORT %ls 1 SUPPORT %ls %ls AXIS:TRACK SLIDE 0 2 2 CLAMP %ls %ls AXIS:TRACK SLIDE 0 2 2 HAND %ls %ls AXIS:TRACK SLIDE 0 2 2",a,s,a,s,a,s);
    CASE(0,b,L"ASM_W_REMOVE_MIN %ls 3 BLOCK %ls %ls AXIS:TRACK SLIDE 0 1 2 BLOCK %ls %ls AXIS:TRACK SLIDE 0 1 2",a,b,a,s);
    CASE(0,L"当前回装方向符合",L"ASM_W_REASSEMBLE %ls %ls CSYS:PRT_CSYS_DEF ROOT CSYS:ASM_CSYS 0.01 0.01",a);
    CASE(0,L"生成 2 个站位复核点",L"ASM_W_WORKPOINTS %ls %ls %ls",a,b);
    CASE(0,L"已生成 1 步拆装 SVG",L"ASM_W_REMOVAL_DRAW %ls DISASSEMBLY CSYS:ASM_CSYS %ls +X %ls %ls@CSYS:K2",a,ft_a,b);
    check(dv_file_nonempty(L"DISASSEMBLY\\service-removal.svg"),"service removal SVG from actual occurrences");
    CASE(0,L"脚轮回转与刹车核对 2/2",L"ASM_W_CASTER_BRAKE %ls 1 SWIVEL %ls %ls AXIS:TRACK ROTATE 0 360 13 BRAKE %ls %ls AXIS:TRACK SLIDE 0 4 2",a,s,a,s);
    CASE(0,L"0 向不足",L"ASM_W_ROOM_CLEARANCE %ls %ls CSYS:ASM_CSYS -5 110 -5 90 -5 20 1 1 1 1 1 1",a);
    CASE(0,L"跨距 80 mm",L"ASM_W_HANDLING %ls CSYS:ASM_CSYS %ls CSYS:K3 %ls CSYS:K2 %ls CSYS:K3 2000 79 81 0",a,a,a);
    CASE(0,L"涉及 1 个订单",L"ASM_W_ORDER_IMPACT %ls %ls %ls",a,sub);
    CASE(0,L"OUTDOOR 检查 7 项",L"ASM_W_PRE_RELEASE %ls OUTDOOR CSYS:ASM_CSYS 2000 2000 2000%ls",L"");
    STEP(ProMdlVerstampGet(root,&new_stamp));
    check(ProVerstampEqual(original,new_stamp),"workbench checks preserved root model");
    STEP(ProMdlnameCopy(root,L"AWB_PRIOR",&prior));
    CASE(0,L"有 0 项站位不符",L"ASM_W_DOOR_HAND %ls AWB_PRIOR CSYS:ASM_CSYS LEFT 150 10 HINGE %ls CSYS:PRT_CSYS_DEF L LATCH %ls CSYS:PRT_CSYS_DEF R LIMIT %ls CSYS:PRT_CSYS_DEF L WIRE %ls CSYS:PRT_CSYS_DEF L",a,s,b,h);
    CASE(0,L"已记录当前装配",L"ASM_W_CHECKPOINT %ls %ls",L"CAPTURE");
    aw_test_failed_candidate=1;
    CASE(0,L"使用第 2 组参照完成",L"ASM_W_ALIGN %ls 5 1 %ls CSYS:ASM_CSYS CSYS:PRT_CSYS_DEF CSYS:ASM_CSYS CSYS:ALT",a);
    CASE(0,L"已保存并回读",L"ASM_W_METHOD_RECORD %ls %ls ANGLE_BRACKET",a);
    CASE(0,L"已有相同",L"ASM_W_METHOD_RECORD %ls %ls ANGLE_BRACKET",a);
    CASE(0,L"当前可读的成功方案",L"ASM_W_METHOD_RECALL %ls %ls ANGLE_BRACKET",b);
    check(dv_file_nonempty(L"assembly-methods.txt"),"learned assembly case stored on E workspace");
    check(!aw_test_failed_candidate&&!ax_recovery_uncertain,"first candidate failed after native write and recovery verified");
    CASE(1,L"未完成 1 处",L"ASM_W_ALIGN %ls 300 10 %ls CSYS:MISSING CSYS:PRT_CSYS_DEF CSYS:ALSO_MISSING CSYS:ALT",b);
    CASE(0,L"新完成 1 处，复核已完成 1 处，未完成 0 处",L"ASM_W_ALIGN_BATCH %ls 300 10 %ls CSYS:ASM_CSYS CSYS:PRT_CSYS_DEF CSYS:ASM_CSYS CSYS:ALT %ls CSYS:MISSING CSYS:PRT_CSYS_DEF CSYS:ASM_CSYS CSYS:ALT",a,b);
    CASE(0,L"新完成 0 处，复核已完成 2 处",L"ASM_W_ALIGN_BATCH %ls 300 10 %ls CSYS:ASM_CSYS CSYS:PRT_CSYS_DEF CSYS:ASM_CSYS CSYS:ALT %ls CSYS:MISSING CSYS:PRT_CSYS_DEF CSYS:ASM_CSYS CSYS:ALT",a,b);
    CASE(0,L"当前成果留在 Creo 会话，未保存",L"ASM_W_RESULT %ls%ls",L"");
    CASE(0,L"位姿或状态变化 2",L"ASM_W_CHECKPOINT %ls %ls",L"COMPARE");
    CASE(0,L"移位 2",L"ASM_W_REWORK %ls AWB_PRIOR 0.01 0.1 REWORK_OUT%ls",L"");
    check(dv_file_nonempty(L"REWORK_OUT\\assembly-rework.csv"),"rework list exported for matching occurrence paths");
    CASE(0,L"个站位",L"ASM_W_BEAM_LAYOUT %ls CSYS:ASM_CSYS 0 140 10 60 PREVIEW %ls",ft_a);
    CASE(0,L"已生成并再生核对",L"ASM_W_ROUTE_WRITE %ls CABLE_ROUTE CSYS:ASM_CSYS 2 1 %ls CSYS:RW_START %ls CSYS:RW_END -20 0 12 130 40 28 %ls",a,a,ft_a);
    CASE(0,L"布置 2 个现有线夹",L"ASM_W_CLIP_WRITE %ls AWB_FST_SMALL CSYS:PRT_CSYS_DEF %ls CURVE:WIRE1 CSYS:ASM_CSYS 4 8 2 2 NONE",a);
    CASE(0,L"调整 1 个现有位置，新增 2",L"ASM_W_BEAM_LAYOUT %ls CSYS:ASM_CSYS 0 140 10 60 APPLY %ls",ft_a);
    CASE(0,L"调整 1 个现有位置，新增 2",L"ASM_W_TRAY_LAYOUT %ls CSYS:ASM_CSYS 0 80 15 3 APPLY %ls",ft_b);
    CASE(0,L"2 个防窜块",L"ASM_W_GLASS_BLOCKS %ls %ls CSYS:PRT_CSYS_DEF AWB_FST_SMALL CSYS:PAD_SUPPORT 25 0.3 3 SIDES",g);
    ProPath after_dir;
    check(!ProDirectoryCurrentGet(after_dir)&&!_wcsicmp(directory,after_dir),"workbench checks preserved workspace");
done:
    if(original)ProVerstampFree(&original);
    if(new_stamp)ProVerstampFree(&new_stamp);
    trace("WORKBENCH_CASES",cases);
    trace("WORKBENCH_FAILURES",failures);
    trace("WORKBENCH_DONE",error);
    ProEngineerEnd();
    return 0;
#undef STEP
#undef CASE
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    trace("workbench_legacy_loaded",0);
    ProError error=PRO_TK_NO_ERROR;
    uiCmdCmdId cmd;
    if(!error)error=ProCmdActionAdd("BWTLWorkbenchVerify",aw_fixture_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(error)return error;
    return ProMacroLoad(L"~ Command \x0060" L"BWTLWorkbenchVerify\x0060;");
}
