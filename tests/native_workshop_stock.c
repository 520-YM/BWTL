#define user_initialize previous_sheetmetal_initialize
#include "native_sheetmetal.c"
#undef user_initialize
#undef CASE

static ProMdl stock_model;
static ProMdlName stock_name;

static int stock_case(int reject,const wchar_t *expected,const wchar_t *format,...){
    wchar_t protocol[1800],reply[PART_TEXT_MAX]=L"";
    PartCommand cmd;
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    if(!snapshot)return 0;
    va_list args;
    va_start(args,format);
    int size=vswprintf_s(protocol,_countof(protocol),format,args);
    va_end(args);
    ProError error=part_snapshot_take_mode(stock_model,snapshot,0);
    if(!error&&(size<0||part_command_parse(protocol,&cmd)!=1))error=PRO_TK_BAD_INPUTS;
    if(!error)error=part_execute(&cmd,snapshot,reply,_countof(reply));
    int ok=(reject?error!=0:error==0)&&(!expected||wcsstr(reply,expected));
    FILE *file=_wfopen(L"workshop-stock.log",L"a, ccs=UTF-8");
    if(file){fwprintf(file,L"%ls\nRESULT=%d PASS=%d\n%ls\n",protocol,error,ok,reply);fclose(file);}
    part_snapshot_clear(snapshot);
    free(snapshot);
    cases++;
    if(!ok)failures++;
    return ok;
}

static int stock_file(const wchar_t *path,const wchar_t *data){
    FILE *file=_wfopen(path,L"wt, ccs=UTF-8");
    if(!file)return 0;
    int ok=fputws(data,file)>=0;
    return fclose(file)==0&&ok;
}

static int stock_contents(const wchar_t *path,const wchar_t *needle){
    FILE *file=_wfopen(path,L"rt, ccs=UTF-8");
    if(!file)return 0;
    wchar_t line[512];
    int found=0;
    while(fgetws(line,_countof(line),file))if(wcsstr(line,needle)){found=1;break;}
    fclose(file);
    return found;
}

static int stock_top(ProMdl model){
    SmxGeometry *geometry=calloc(1,sizeof(*geometry));
    if(!geometry)return 0;
    int id=0;
    if(!smx_geometry(model,geometry)){
        for(int i=0;i<geometry->count;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type==PRO_SRF_PLANE&&face->axis[2]>0.99999){id=face->id;break;}
        }
    }
    free(geometry);
    return id;
}

static ProError stock_flange_hole(ProMdl model,const CabinetEdge *edge,int *bore){
    FdTxn view={0};
    PanelFrame frame={0};
    double mm,t;
    ProError error=fab_thickness(model,&mm,&t);
    if(error)return error;
    view.model=model;view.scale=1/mm;
    frame.face_id=edge->face;
    ProCsysdata *csys=&frame.placement.csys;
    cm_point(edge->a,edge->axis,edge->length/2,csys->origin);
    cm_point(csys->origin,edge->inward,8/mm,csys->origin);
    memcpy(csys->x_vector,edge->axis,sizeof(ProVector));
    memcpy(csys->y_vector,edge->inward,sizeof(ProVector));
    memcpy(csys->z_vector,edge->normal,sizeof(ProVector));
    for(int i=0;i<3;i++)frame.inward[i]=-edge->normal[i];
    PartCommand cmd={0};
    swprintf_s(cmd.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",edge->face);
    wcscpy_s(cmd.arg[5],PART_TOKEN_MAX,L"PLANE:RIGHT");
    PanelProfile opening={PN_CIRCLE,0,0,6,6};
    ProMatrix sketch_frame;
    error=pn_profile_probes(&view,&frame,&opening,t,0);
    if(!error)error=pn_sketch(model,&cmd,&frame,L"MOVING_HOLE_SK",&opening,1,1,sketch_frame);
    if(!error)error=pn_cut(&view,&frame,L"MOVING_HOLE",L"MOVING_HOLE_SK",sketch_frame,t,1);
    if(error)return error;
    SmxGeometry *geometry=calloc(1,sizeof(*geometry));
    if(!geometry)return PRO_TK_OUT_OF_MEMORY;
    error=smx_geometry(model,geometry);
    *bore=0;
    if(!error){
        for(int i=0;i<geometry->count;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type==PRO_SRF_CYL&&fabs(face->radius-3)<0.0001){*bore=face->id;break;}
        }
    }
    free(geometry);
    return error?error:*bore?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static int stock_test_run(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProError error=0;
    ShFixture refs;
    const char *stage="stock sheet fixture";
#define MUST(call) do{error=(call);if(error){failures++;trace(#call,error);goto done;}}while(0)
#define CHECK(condition) do{cases++;if(!(condition)){failures++;trace(#condition,-1);}}while(0)
#define CASE(reject,expected,format,...) stock_case(reject,expected,L"CREO_OP " format,__VA_ARGS__)
    CHECK(CASE(0,NULL,L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 120 100",stock_name));
    CHECK(CASE(0,NULL,L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",stock_name));
    sh_test_refs(stock_model,&refs);
    CHECK(CASE(0,NULL,L"SMT_DXF %ls REMNANT SURFACE:%d",stock_name,refs.face));
    MUST(model_create_empty(2,stock_name,&stage));
    MUST(ProMdlnameInit(stock_name,PRO_MDL_PART,&stock_model));
    CHECK(CASE(0,NULL,L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",stock_name));
    CHECK(CASE(0,NULL,L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",stock_name));
    sh_test_refs(stock_model,&refs);
    CHECK(CASE(0,NULL,L"SMT_DXF %ls RECTANGLE SURFACE:%d",stock_name,refs.face));
    CHECK(CASE(0,L"净料 4800",L"SMY_STOCK_MEASURE %ls SURFACE:%d 7850",stock_name,refs.face));
    ProMdl prior_version=NULL;
    MUST(smx_model_copy(stock_model,L"VERSION_PRIOR",&prior_version));
    CHECK(CASE(0,NULL,L"SMT_SKETCH %ls HOLE_SK PLANE:FRONT PLANE:RIGHT CIRCLE 40 30 5",stock_name));
    CHECK(CASE(0,NULL,L"SMT_CUT %ls OPENING SKETCH:HOLE_SK NEG",stock_name));
    sh_test_refs(stock_model,&refs);
    CHECK(CASE(0,L"净料 4721.4602",L"SMY_STOCK_MEASURE %ls SURFACE:%d 7850",stock_name,refs.face));
    CHECK(CASE(0,NULL,L"SMT_DXF %ls HOLED SURFACE:%d",stock_name,refs.face));
    CASE(0,L"展开差异 2 个",L"SMY_VERSION_COMPARE %ls VERSION_PRIOR SURFACE:%d SURFACE:%d 0.001 SECC SECC",stock_name,refs.face,refs.face);
    MUST(ProMdlErase(prior_version));
    {
        ProMdl original=stock_model,left=NULL,right=NULL;
        ProMdlName original_name,left_name,right_name;
        wcscpy_s(original_name,_countof(original_name),stock_name);
        MUST(smx_copy_scratch(stock_model,&left));
        MUST(dv_display_new(left));
        stock_model=left;
        MUST(ProMdlMdlnameGet(left,left_name));
        wcscpy_s(stock_name,_countof(stock_name),left_name);
        CASE(0,NULL,L"SMT_SKETCH %ls LEFT_SK PLANE:FRONT PLANE:RIGHT CIRCLE 20 18 3",left_name);
        CASE(0,NULL,L"SMT_CUT %ls LEFT_CUT SKETCH:LEFT_SK NEG",left_name);
        MUST(smx_copy_scratch(original,&right));
        MUST(dv_display_new(right));
        stock_model=right;
        MUST(ProMdlMdlnameGet(right,right_name));
        CASE(0,NULL,L"SMT_SKETCH %ls RIGHT_SK PLANE:FRONT PLANE:RIGHT CIRCLE 60 18 3",right_name);
        CASE(0,NULL,L"SMT_CUT %ls RIGHT_CUT SKETCH:RIGHT_SK NEG",right_name);
        wcscpy_s(stock_name,_countof(stock_name),right_name);
        CASE(0,L"展开互为镜像",L"SMY_MIRROR_PAIR %ls %ls SURFACE:%d SURFACE:%d X 40 0.001",right_name,left_name,refs.face,refs.face);
        CASE(0,L"不是指定基准",L"SMY_MIRROR_PAIR %ls %ls SURFACE:%d SURFACE:%d X 41 0.001",right_name,left_name,refs.face,refs.face);
        MUST(dv_display_new(original));
        MUST(ProMdlErase(left));
        MUST(ProMdlErase(right));
        stock_model=original;
        wcscpy_s(stock_name,_countof(stock_name),original_name);
    }
    CHECK(stock_file(L"JOB.tsv",L"LEFT RECTANGLE SECC 2 FREE UP 3 L TURN\nRIGHT RECTANGLE SECC 2 FREE UP 2 R TURN\nLONG HOLED SECC 2 X UP 1 N TURN\nOVERSIZE REMNANT SPCC 2 X NONE 1 N LOCK\n"));
    CHECK(stock_file(L"REMNANTS.tsv",L"RM1 REMNANT SECC 2 FREE UP\nRM2 RECTANGLE SECC 2 X DOWN\n"));
    CASE(0,L"组 3",L"SMY_STOCK_GROUP %ls JOB",stock_name);
    CASE(0,L"无可用方向",L"SMY_STOCK_FIT %ls JOB 175 85 5 6 3",stock_name);
    CASE(0,L"正排 2 件/张",L"SMY_STOCK_GRID %ls JOB 175 85 5 6 3",stock_name);
    CASE(0,L"订单 7 件，已排 6 件，未排 1 件",L"SMY_STOCK_PACK %ls JOB 175 85 5 6 3 MIXED",stock_name);
    CASE(0,L"LONG × 1，N 件，纹向 X，覆膜 UP，允许 0°",L"SMY_STOCK_HAND %ls JOB",stock_name);
    CASE(0,L"最小实测间距 3 mm",L"SMY_STOCK_AUDIT %ls JOB 175 85 5 6 3",stock_name);
    CASE(0,L"等长矩形直边候选",L"SMY_STOCK_COMMON %ls JOB 0.18",stock_name);
    CASE(0,L"RM2 未匹配",L"SMY_STOCK_REMNANT %ls JOB REMNANTS 2",stock_name);
    CASE(0,L"订单 7 件，已排 6 件，未排 1 件",L"SMY_STOCK_TOTAL %ls JOB 175 85 5 6 3 SUMMARY",stock_name);
    CASE(1,NULL,L"SMY_STOCK_PACK %ls JOB 175 85 5 6 3 MIXED",stock_name);
    CASE(1,NULL,L"SMY_STOCK_FIT %ls JOB 5 10 6 1 0",stock_name);
    CASE(1,NULL,L"SMY_STOCK_GROUP %ls MISSING",stock_name);
    CHECK(stock_contents(L"SUMMARY\\order-reconciliation.csv",L"\"OVERSIZE\",1,0,1"));
    CHECK(stock_contents(L"MIXED\\placements.csv",L"\"RIGHT\",R,"));
    CHECK(stock_contents(L"SUMMARY\\stock-summary.csv",L"SECC"));
    {
        FabDxf *dirty=calloc(1,sizeof(*dirty));
        if(!dirty){error=PRO_TK_OUT_OF_MEMORY;goto done;}
        error=fab_dxf_read(L"RECTANGLE.dxf",dirty);
        if(!error){
            dirty->entities[0].a[0]+=0.003;
            dirty->entities[dirty->count++]=dirty->entities[1];
            dirty->entities[dirty->count++]=(FabEntity){1,1,0,{20,20},{20,20}};
            error=smy_flat_write(L"DIRTY.dxf",dirty);
        }
        free(dirty);
        CHECK(!error);
        CASE(0,L"非相邻交点 0",L"SMY_FLAT_TOPOLOGY %ls HOLED",stock_name);
        CASE(1,L"重复 1，短到零 1",L"SMY_FLAT_TOPOLOGY %ls DIRTY",stock_name);
        CASE(0,L"合拢 1 对微小断点",L"SMY_FLAT_CLEAN %ls DIRTY CLEANED 0.01",stock_name);
        CHECK(dv_file_nonempty(L"CLEANED\\DIRTY.dxf"));
        CASE(0,L"变换结果已逐一回读",L"SMY_FLAT_DATUM %ls REBASED 5 5 90 RECTANGLE HOLED",stock_name);
        CHECK(dv_file_nonempty(L"REBASED\\HOLED.dxf"));
        SmxGeometry *geometry=calloc(1,sizeof(*geometry));
        if(!geometry){error=PRO_TK_OUT_OF_MEMORY;goto done;}
        int bore=0;
        if(!smx_geometry(stock_model,geometry)){
            for(int i=0;i<geometry->count;i++){
                const SmxFace *face=&geometry->faces[i];
                if(face->type==PRO_SRF_CYL&&fabs(face->radius-5)<0.0001){bore=face->id;break;}
            }
        }
        free(geometry);
        CHECK(bore>0);
        CHECK(stock_file(L"BWTL_SMT_FASTENERS.tsv",L"NUT SECC PRESS 9.95 10.05 1.5 2.5 4 REV1\nRIVNUT SECC RIVET 9.9 10.1 1 3 4 REV2\nSTUDX SECC STUD 7.95 8.05 1.5 2.5 4 REV3\n"));
        CASE(0,L"孔径 符合，夹持厚度 符合",L"SMY_PRESS_CHECK %ls NUT SECC SURFACE:%d SURFACE:%d",stock_name,refs.face,bore);
        CASE(0,L"孔径 符合，夹持厚度 符合",L"SMY_RIVET_GRIP %ls RIVNUT SECC SURFACE:%d SURFACE:%d",stock_name,refs.face,bore);
        CASE(1,L"没有匹配",L"SMY_PRESS_CHECK %ls NUT SPCC SURFACE:%d SURFACE:%d",stock_name,refs.face,bore);
        CASE(0,L"均未发现侵入",L"SMY_PRESS_ACCESS %ls SURFACE:%d SURFACE:%d 18 20 20 15 1",stock_name,refs.face,bore);
        CASE(0,L"均未发现侵入",L"SMY_DRIVER_ACCESS %ls SURFACE:%d SURFACE:%d 18 20 8 0.25",stock_name,refs.face,bore);
        CASE(0,L"超过孔径",L"SMY_DRIVER_ACCESS %ls SURFACE:%d SURFACE:%d 18 20 11 0.25",stock_name,refs.face,bore);
        CASE(0,L"塞规 取样通过",L"SMY_GAUGE_ACCESS %ls SURFACE:%d SURFACE:%d 15 6 9.8 0.05",stock_name,refs.face,bore);
        CASE(0,L"短于实测板厚",L"SMY_GAUGE_ACCESS %ls SURFACE:%d SURFACE:%d 15 1 9.8 0.05",stock_name,refs.face,bore);
        {
            ProMdl first=stock_model;
            ProName first_name,second;
            wcscpy_s(first_name,_countof(first_name),stock_name);
            ModelPlan plate={0,80,60,2,0,1};
            MUST(model_create(&plate,second,&stage));
            MUST(ProMdlnameInit(second,PRO_MDL_PART,&stock_model));
            int top=stock_top(stock_model);
            CASE(0,NULL,L"DATUM_CSYS %ls JOIN_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",second);
            CASE(0,NULL,L"SMT_CONVERT %ls JOIN_SMT SURFACE:%d 2",second,top);
            top=stock_top(stock_model);
            CASE(0,NULL,L"SMY_PRESS_STUD %ls JOIN_HOLE SURFACE:%d CSYS:JOIN_AT PLANE:RIGHT STUDX SECC 40 30 8 1",second,top);
            SmxGeometry *join=calloc(1,sizeof(*join));
            if(!join){error=PRO_TK_OUT_OF_MEMORY;goto done;}
            int join_bore=0;
            if(!smx_geometry(stock_model,join)){
                for(int i=0;i<join->count;i++){
                    const SmxFace *entry=&join->faces[i];
                    if(entry->type==PRO_SRF_CYL&&fabs(entry->radius-4)<0.0001){join_bore=entry->id;break;}
                }
            }
            free(join);
            CHECK(join_bore>0);
            MUST(dv_display_new(first));
            stock_model=first;
            wcscpy_s(stock_name,_countof(stock_name),first_name);
            if(join_bore){
                CASE(0,L"孔系几何 符合",L"SMY_RIVET_ALIGN %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d CSYS:JOIN_AT SURFACE:%d SURFACE:%d 0.01 7.9 10.1 16 0.05",stock_name,second,refs.face,bore,top,join_bore);
                CASE(0,L"孔系几何 超限",L"SMY_RIVET_ALIGN %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d CSYS:JOIN_AT SURFACE:%d SURFACE:%d 0.01 9.9 10.1 16 0.05",stock_name,second,refs.face,bore,top,join_bore);
            }
        }
        CASE(0,L"输出 1 段，焊缝总长 70 mm",L"SMY_WELD_SCHEDULE %ls WELD_FULL EDGE:%d CONT 5 5 0 0",stock_name,refs.edge);
        CASE(0,L"输出 4 段，焊缝总长 40 mm",L"SMY_WELD_SCHEDULE %ls WELD_SHORT EDGE:%d STITCH 5 5 10 20",stock_name,refs.edge);
        CASE(1,NULL,L"SMY_WELD_SCHEDULE %ls BAD_WELD EDGE:%d STITCH 5 5 10 8",stock_name,refs.edge);
        wchar_t join_job[400];
        swprintf_s(join_job,_countof(join_job),L"PRESS NUT SECC SURFACE:%d SURFACE:%d 0 0 0 0\nWELD TIG SECC EDGE:%d STITCH 5 5 10 20\n",
            refs.face,bore,refs.edge);
        CHECK(stock_file(L"JOIN_JOB.tsv",join_job));
        CASE(0,L"已核对 2 道",L"SMY_JOIN_CARD %ls JOIN_CARD JOIN_JOB",stock_name);
        CHECK(stock_contents(L"JOIN_CARD\\fastening-weld-card.csv",L"焊缝总长_mm,40"));
        swprintf_s(join_job,_countof(join_job),L"STUD STUDX SECC SURFACE:%d SURFACE:%d 0 0 0 0\n",refs.face,bore);
        CHECK(stock_file(L"JOIN_BAD.tsv",join_job));
        CASE(1,L"实际孔径",L"SMY_JOIN_CARD %ls JOIN_BAD JOIN_BAD",stock_name);
        CHECK(GetFileAttributesW(L"JOIN_BAD")==INVALID_FILE_ATTRIBUTES);
        CASE(0,L"几何、位置及方向一致",L"SMY_FLAT_MATCH %ls HOLED SURFACE:%d 0.001",stock_name,refs.face);
        CASE(1,L"未匹配",L"SMY_FLAT_MATCH %ls RECTANGLE SURFACE:%d 0.001",stock_name,refs.face);
        CHECK(stock_file(L"LOCK.tsv",L"P01 A SECC 2 3 RECTANGLE\nP02 B SECC 2 2 HOLED\n"));
        CASE(0,L"已交付 2 份",L"SMY_FLAT_LOCK %ls LOCK LOCKED",stock_name);
        CHECK(stock_contents(L"LOCKED\\dxf-version-index.csv",L"SHA256"));
        CHECK(dv_file_nonempty(L"LOCKED\\P02_B_SECC_T2_Q2.dxf"));
        CHECK(stock_file(L"LOCK_DUP.tsv",L"P01 A SECC 2 3 RECTANGLE\nP01 A SECC 2 3 HOLED\n"));
        CASE(1,L"整批未交付",L"SMY_FLAT_LOCK %ls LOCK_DUP FAILED_LOCK",stock_name);
        CHECK(GetFileAttributesW(L"FAILED_LOCK")==INVALID_FILE_ATTRIBUTES);
        CASE(0,NULL,L"SMT_FLANGE %ls FLANGE EDGE:%d 22 90 2 POS",stock_name,refs.edge);
        ProFeature wall;
        CabinetFace face;
        MUST(cm_feature_find(stock_model,L"FLANGE",&wall));
        MUST(cm_wall_face(&wall,&face));
        CASE(0,L"已保存选定固定面",L"SMY_FLAT_COMPARE %ls FLAT_OPTIONS SURFACE:%d SURFACE:%d",stock_name,refs.face,face.id);
        CHECK(stock_contents(L"FLAT_OPTIONS\\fixed-face-options.csv",L"备选"));
        CASE(0,L"折后 XYZ 和展开 XY",L"SMY_FLAT_HOLE_MAP %ls FLAT_HOLE SURFACE:%d SURFACE:%d SURFACE:%d",stock_name,refs.face,refs.face,bore);
        CHECK(stock_contents(L"FLAT_HOLE\\hole-folded-flat.csv",L"展开X_mm"));
        CASE(1,NULL,L"SMY_FLAT_HOLE_MAP %ls FLAT_WRONG SURFACE:%d SURFACE:%d SURFACE:%d",stock_name,refs.face,refs.face,refs.face);
        CASE(0,L"1 道折弯中线和 2 条切线",L"SMY_FLAT_BEND_LAYERS %ls BEND_MARKS SURFACE:%d",stock_name,refs.face);
        CHECK(dv_file_nonempty(L"BEND_MARKS\\bend-layers.dxf"));
        FabBends first_bend={0};
        MUST(smy_bends_load(stock_model,&first_bend));
        CHECK(first_bend.count==1);
        const double xaxis[]={1,0,0};
        CabinetEdgeSearch flange_edge={0};
        flange_edge.model=stock_model;flange_edge.face_id=face.id;flange_edge.parallel=xaxis;
        MUST(cm_edges_on_face(&flange_edge));
        CHECK(flange_edge.matches>0);
        if(flange_edge.matches&&first_bend.count==1){
            int bend=first_bend.rows[0].surface,e=flange_edge.edge.id;
            CASE(0,L"27 处板厚取样通过",L"SMY_GAUGE_FACE %ls SURFACE:%d EDGE:%d 20 2 3",stock_name,bend,e);
            CASE(0,L"小于挡料夹口",L"SMY_GAUGE_FACE %ls SURFACE:%d EDGE:%d 100 2 3",stock_name,bend,e);
            ProMdl original=stock_model,try_map=NULL;
            ProMdlName original_name;
            wcscpy_s(original_name,_countof(original_name),stock_name);
            MUST(smx_copy_scratch(stock_model,&try_map));
            MUST(dv_display_new(try_map));
            stock_model=try_map;
            MUST(ProMdlMdlnameGet(try_map,stock_name));
            int moving_bore=0;
            ProError fixture=stock_flange_hole(stock_model,&flange_edge.edge,&moving_bore);
            trace("workshop moving flange hole fixture",fixture);
            CHECK(!fixture&&moving_bore>0);
            if(!fixture)CASE(0,L"折后 XYZ 和展开 XY",L"SMY_FLAT_HOLE_MAP %ls MOVING_HOLE_MAP SURFACE:%d SURFACE:%d SURFACE:%d",
                stock_name,refs.face,flange_edge.edge.face,moving_bore);
            MUST(dv_display_new(original));
            MUST(ProMdlErase(try_map));
            stock_model=original;
            wcscpy_s(stock_name,_countof(stock_name),original_name);
        }
        CabinetEdgeSearch free_edge={0};
        free_edge.model=stock_model;free_edge.face_id=refs.face;free_edge.parallel=xaxis;
        MUST(cm_edges_on_face(&free_edge));
        CHECK(free_edge.matches>0);
        if(free_edge.matches)CASE(0,NULL,L"SMT_FLANGE %ls FLANGE_2 EDGE:%d 16 90 2 NEG",stock_name,free_edge.edge.id);
        CASE(0,L"正向 1、反向 1",L"SMY_FLAT_BEND_LAYERS %ls OPPOSITE_BENDS SURFACE:%d",stock_name,refs.face);
        FabBends bends={0};
        MUST(smy_bends_load(stock_model,&bends));
        CHECK(bends.count==2);
        if(bends.count==2){
            int a=bends.rows[0].surface,b=bends.rows[1].surface;
            wchar_t data[256];
            swprintf_s(data,_countof(data),L"SURFACE:%d DIE1 BACK1 DIM_A\nSURFACE:%d DIE2 BACK2 DIM_B\nBEFORE SURFACE:%d SURFACE:%d\n",
                a,b,b,a);
            CHECK(stock_file(L"BEND_JOB.tsv",data));
            CHECK(stock_file(L"BWTL_SMT_DFM.tsv",L"SECC 2 90 DIE1 8 12 25\n"));
            CASE(0,L"最长自由挡料直边",L"SMY_DFM_FLANGE %ls SECC DIE1",stock_name);
            CASE(0,L"原生折弯长 80 mm",L"SMY_DFM_SHORT %ls SECC DIE1",stock_name);
            CASE(0,L"bend-identities.csv",L"SMY_BEND_NUMBER %ls BEND_IDS",stock_name);
            CASE(0,L"bend-dependencies.csv",L"SMY_BEND_ORDER %ls BEND_ORDER BEND_JOB",stock_name);
            CASE(0,L"换模 1 次",L"SMY_BEND_CANDIDATES %ls BEND_JOB",stock_name);
            CHECK(stock_file(L"BWTL_SMT_PRESS_FORCE.tsv",L"SECC DIE1 MACHINE1 2 90 100 25 160 500 REV1\nSECC DIE1 MACHINE2 2 90 100 7 160 500 REV1\n"));
            CASE(0,L"估算 8 kN",L"SMY_BEND_FORCE %ls SURFACE:%d SECC DIE1",stock_name,a);
            CHECK(stock_file(L"BWTL_SMT_BEND_TOOLS.tsv",L"P1 DIE1 MACHINE1 16 1 1.5 2.5 80 100 100 REV1\nP2 DIE1 MACHINE1 16 1 1.5 2.5 80 100 30 REV1\n"));
            CASE(0,L"库参数初筛 符合",L"SMY_BEND_TOOL %ls SURFACE:%d P1 DIE1 MACHINE1",stock_name,a);
            CASE(0,L"库参数初筛 超限",L"SMY_BEND_TOOL %ls SURFACE:%d P2 DIE1 MACHINE1",stock_name,a);
            CASE(1,L"未登记",L"SMY_BEND_TOOL %ls SURFACE:%d P3 DIE1 MACHINE1",stock_name,a);
            CASE(0,L"角差 -1.5°",L"SMY_BEND_TRIAL %ls BEND_TRIAL SURFACE:%d SECC DIE1 LOT_A 91.5",stock_name,a);
            CASE(0,L"2 处空档",L"SMY_BEND_SEGMENTS %ls SURFACE:%d 0 20 22 40 40 65",stock_name,a);
            CASE(0,L"bend-operation-card.csv",L"SMY_BEND_CARD %ls BEND_CARD BEND_JOB TOOL",stock_name);
            CHECK(stock_contents(L"BEND_CARD\\bend-operation-card.csv",L"\"DIE2\",\"BACK2\",\"DIM_B\""));
            swprintf_s(data,_countof(data),L"THICKNESS T CALIPER 2 0.05 0.05\nBEND_A BEND:%d ANGLE_GAUGE 90 1 1\n",a);
            CHECK(stock_file(L"FIRST_RULES.tsv",data));
            CASE(0,L"2 项首件检验记录",L"SMY_FIRST_CARD %ls FIRST_ARTICLE FIRST_RULES",stock_name);
            CHECK(stock_contents(L"FIRST_ARTICLE\\first-article.csv",L"\"THICKNESS\",\"T\",\"CALIPER\""));
            CHECK(CopyFileW(L"BEND_CARD\\bend-operation-card.csv",L"BENDS_INPUT.csv",TRUE));
            CHECK(CopyFileW(L"FIRST_ARTICLE\\first-article.csv",L"FIRST_INPUT.csv",TRUE));
            CHECK(stock_file(L"FAST_INPUT.csv",L"item,quantity\nNUT,2\n"));
            FILE *pdf=fopen("DRAW_INPUT.pdf","wb");
            CHECK(pdf!=NULL);
            if(pdf){fputs("%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n2 0 obj<</Type/Pages/Count 0/Kids[]>>endobj\n%%EOF\n",pdf);fclose(pdf);}
            CASE(1,L"缺少",L"SMY_DELIVERY_BUNDLE %ls MISSING_BUNDLE A SURFACE:%d NO_PDF BENDS_INPUT FIRST_INPUT FAST_INPUT",
                stock_name,refs.face);
            CASE(0,L"已生成交付包",L"SMY_DELIVERY_BUNDLE %ls DELIVERY A SURFACE:%d DRAW_INPUT BENDS_INPUT FIRST_INPUT FAST_INPUT",
                stock_name,refs.face);
            CASE(0,L"七份交付文件的散列",L"SMY_DELIVERY_CHECK %ls DELIVERY A SURFACE:%d",stock_name,refs.face);
            CASE(1,L"版本或原生模型状态已变化",L"SMY_DELIVERY_CHECK %ls DELIVERY B SURFACE:%d",stock_name,refs.face);
            CHECK(stock_file(L"DELIVERY\\fasteners.csv",L"modified\n"));
            CASE(1,L"fasteners.csv 缺失、被修改",L"SMY_DELIVERY_CHECK %ls DELIVERY A SURFACE:%d",stock_name,refs.face);
            swprintf_s(data,_countof(data),L"SURFACE:%d DIE1 BACK1 DIM_A\nSURFACE:%d DIE2 BACK2 DIM_B\nBEFORE SURFACE:%d SURFACE:%d\nBEFORE SURFACE:%d SURFACE:%d\n",
                a,b,b,a,a,b);
            CHECK(stock_file(L"BEND_CYCLE.tsv",data));
            CASE(1,L"先后要求互相矛盾",L"SMY_BEND_CANDIDATES %ls BEND_CYCLE",stock_name);
        }
    }
    {
        ModelPlan plate={0,80,60,2,0,1};
        MUST(model_create(&plate,stock_name,&stage));
        MUST(ProMdlnameInit(stock_name,PRO_MDL_PART,&stock_model));
        int top=stock_top(stock_model);
        CHECK(top>0);
        CASE(0,NULL,L"DATUM_CSYS %ls FORM_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",stock_name);
        CASE(0,NULL,L"SMT_CONVERT %ls TO_SMT SURFACE:%d 2",stock_name,top);
        top=stock_top(stock_model);
        CHECK(top>0);
        CHECK(stock_file(L"BWTL_SMT_FORM_TOOLS.tsv",L"RECESS SECC DIE_30 1.5 2.5 IN 8 REV1\nBOSS SECC DIE_40 1.5 2.5 OUT 5 REV2\n"));
        CASE(0,L"模具 DIE_30，版本 REV1",L"SMY_FORM_LIBRARY %ls RECESS SECC IN 4",stock_name);
        CASE(1,L"没有符合",L"SMY_FORM_LIBRARY %ls RECESS SECC OUT 4",stock_name);
        CASE(0,L"成形几何有裕量，模具包络不足",L"SMY_FORM_CLEARANCE %ls SURFACE:%d CSYS:FORM_AT PLANE:RIGHT 10 30 8 6 20 16 2",stock_name,top);
        CASE(1,L"保持原状",L"SMY_FORM_BOSS %ls OUTSIDE SURFACE:%d CSYS:FORM_AT PLANE:RIGHT ROUND 79 58 12 0 4 1",stock_name,top);
        CASE(0,L"实测偏移 +4 mm",L"SMY_FORM_BOSS %ls BOSS SURFACE:%d CSYS:FORM_AT PLANE:RIGHT ROUND 23 30 18 0 4 1",stock_name,top);
        CASE(0,L"已在副本原生展平 BOSS",L"SMY_FORM_FLAT %ls BOSS_FLAT SURFACE:%d BOSS FLAT",stock_name,top);
        CHECK(dv_file_nonempty(L"BOSS_FLAT\\formed-flat.dxf"));
        CASE(0,L"启用成形切口投影",L"SMY_FORM_FLAT %ls BOSS_CUTS SURFACE:%d BOSS CUTS",stock_name,top);
        CASE(1,L"未能展平",L"SMY_FORM_FLAT %ls BAD_FORM SURFACE:%d NONFORM FLAT",stock_name,top);
        CASE(0,L"实测偏移 -4 mm",L"SMY_FORM_RECESS %ls RECESS SURFACE:%d CSYS:FORM_AT PLANE:RIGHT RECT 56 30 18 12 4 1",stock_name,top);
        CASE(1,L"未能展平",L"SMY_FORM_FLAT %ls BOTH_FORMS SURFACE:%d BOSS FLAT",stock_name,top);
        CASE(0,L"已排布 4 个点焊位置",L"SMY_SPOT_WELDS %ls SPOTS SURFACE:%d CSYS:FORM_AT PLANE:RIGHT 12 12 45 0 15 4 2",stock_name,top);
        CHECK(stock_contents(L"SPOTS\\spot-weld-layout.csv",L"电极直径_mm"));
        CASE(1,L"未交付整组",L"SMY_SPOT_WELDS %ls BAD_SPOTS SURFACE:%d CSYS:FORM_AT PLANE:RIGHT 2 12 45 0 15 4 2",stock_name,top);
        CASE(1,L"整组未切",L"SMY_PLUG_SLOT %ls BAD_WELD_CUT SURFACE:%d CSYS:FORM_AT PLANE:RIGHT 18 48 6 20 48 10 4 2",stock_name,top);
        CASE(0,L"已创建塞焊孔",L"SMY_PLUG_SLOT %ls WELD_CUT SURFACE:%d CSYS:FORM_AT PLANE:RIGHT 18 48 6 56 48 10 4 2",stock_name,top);
        MUST(model_create(&plate,stock_name,&stage));
        MUST(ProMdlnameInit(stock_name,PRO_MDL_PART,&stock_model));
        top=stock_top(stock_model);
        CASE(0,NULL,L"DATUM_CSYS %ls DISPLAY_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",stock_name);
        CASE(0,NULL,L"SMT_CONVERT %ls DISPLAY_SMT SURFACE:%d 2",stock_name,top);
        top=stock_top(stock_model);
        CASE(1,L"遮边小于",L"SMY_CASE_DISPLAY %ls BAD_SCREEN SURFACE:%d CSYS:DISPLAY_AT PLANE:RIGHT 40 30 30 20 35 25 1 2 2",stock_name,top);
        CASE(0,L"最差遮边 X/Y 4 / 4 mm",L"SMY_CASE_DISPLAY %ls SCREEN SURFACE:%d CSYS:DISPLAY_AT PLANE:RIGHT 40 30 30 20 40 30 1 2 2",stock_name,top);
        CHECK(stock_file(L"BWTL_SMT_CABLE_TRIM.tsv",L"R8 SECC 10 14 8 1.5 2.5 REV1\n"));
        CASE(1,L"净径超出规则",L"SMY_CABLE_ENTRY %ls BAD_CABLE SURFACE:%d CSYS:DISPLAY_AT PLANE:RIGHT R8 SECC 12 14 8 1 2",stock_name,top);
        CASE(1,L"未取得完整承托区",L"SMY_CABLE_ENTRY %ls EDGE_CABLE SURFACE:%d CSYS:DISPLAY_AT PLANE:RIGHT R8 SECC 5 14 6 0.5 2",stock_name,top);
        CASE(0,L"已开 Ø10 mm 贯穿孔",L"SMY_CABLE_ENTRY %ls CABLE SURFACE:%d CSYS:DISPLAY_AT PLANE:RIGHT R8 SECC 12 14 6 0.5 2",stock_name,top);
    }
    {
        MUST(model_create_empty(2,stock_name,&stage));
        MUST(ProMdlnameInit(stock_name,PRO_MDL_PART,&stock_model));
        CHECK(stock_file(L"MODULES.tsv",L"SCREEN 0 0 0 70 45 20 2 2 3\nPOWER 2 2 20 22 22 35 1 1 2\n"));
        CASE(0,L"原生壁面净空间",L"SMY_CASE_SPACE %ls CASE_SPACE PLANE:FRONT PLANE:RIGHT 2 2 5 POS MODULES 6 6 3",stock_name);
        ProPath template_path;
        DWORD chars=GetEnvironmentVariableW(L"BWTL_SMY_DRAWING_FIXTURE",template_path,_countof(template_path));
        if(chars&&chars<_countof(template_path)){
            CHECK(CopyFileW(template_path,L"TEST_A3.drw",TRUE));
            CASE(1,NULL,L"SMY_DRAW_TEMPLATE %ls NO_DRAW MISSING P01 A SECC",stock_name);
            CASE(0,L"实际关联当前钣金件的视图",L"SMY_DRAW_TEMPLATE %ls SHEET_DRAW TEST_A3 P01 A SECC",stock_name);
        }
    }
done:
    trace("STOCK_CASES",cases);
    trace("STOCK_FAILURES",failures);
    trace("STOCK_DONE",error);
    ProEngineerEnd();
    return 0;
#undef CHECK
#undef CASE
#undef MUST
}

static int stock_test_prepare(uiCmdCmdId id,uiCmdValue *value,void *context){
    const char *stage="stock prepare";
    model_background_mode=0;
    ProError error=model_create_empty(2,stock_name,&stage);
    if(!error)error=ProMdlnameInit(stock_name,PRO_MDL_PART,&stock_model);
    if(!error)error=ProMacroLoad(L"~ Command \140BWTLStockRun\140;");
    if(error){trace("STOCK_PREPARE",error);ProEngineerEnd();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLStockPrepare",stock_test_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLStockRun",stock_test_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \140BWTLStockPrepare\140;");
}
