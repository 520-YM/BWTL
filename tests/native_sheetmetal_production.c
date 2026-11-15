
#define BWTL_ENABLE_SMX_CANDIDATE
#define user_initialize previous_sheetmetal_initialize
#include "native_sheetmetal.c"
#undef user_initialize
#undef CASE

static ProMdl smx_model;
static ProMdlName smx_name;

static ProError smx_test_call(const wchar_t *format,...){
    wchar_t protocol[1800],receipt[PART_TEXT_MAX]=L"";
    PartCommand cmd;
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    if(!snapshot)return PRO_TK_OUT_OF_MEMORY;
    va_list args;
    va_start(args,format);
    int size=vswprintf_s(protocol,_countof(protocol),format,args);
    va_end(args);
    ProError error=part_snapshot_take_mode(smx_model,snapshot,0);
    if(!error&&(size<0||part_command_parse(protocol,&cmd)!=1))error=PRO_TK_BAD_INPUTS;
    if(!error)error=part_execute(&cmd,snapshot,receipt,_countof(receipt));
    FILE *file=_wfopen(L"production-cases.log",L"a, ccs=UTF-8");
    if(file){fwprintf(file,L"%ls\nRESULT=%d\n%ls\n",protocol,error,receipt);fclose(file);}
    part_snapshot_clear(snapshot);
    free(snapshot);
    return error;
}

static int smx_test_file(const wchar_t *name,const wchar_t *content){
    FILE *file=_wfopen(name,L"wt, ccs=UTF-8");
    if(!file)return 0;
    int ok=fputws(content,file)>=0;
    return fclose(file)==0&&ok;
}

static void smx_expect(int condition,const char *label){
    cases++;
    if(!condition)failures++;
    trace(label,condition?0:-1);
}

static int smx_test_run(uiCmdCmdId command,uiCmdValue *value,void *context){
    ProError error=0;
    ShFixture f;
    ProPath origin;
    double mm,thickness;
    wchar_t template_row[128];
    swprintf_s(template_row,_countof(template_row),L"DOOR %ls T1\n",smx_name);
#define MUST(call) do{error=(call);trace(#call,error);if(error)goto done;}while(0)
#define CASE(reject,label,format,...) do{ProError result=smx_test_call(L"CREO_OP " format,__VA_ARGS__);smx_expect((reject)?result!=0:result==0,label);}while(0)
    smx_expect(smx_test_file(L"BWTL_SMT_TEMPLATES.tsv",template_row),"template index fixture");
    smx_expect(smx_test_file(L"BWTL_SMT_MATERIALS.tsv",L"SECC 2 1.99 ZINC 2.5 1.5 SECC2 M1\nSECC 2.5 2.51 ZINC 2 1.5 SECC2 M1\nSPCC 2 2.01 PAINT 3 2 SPCC2 M2\n"),"material index fixture");
    smx_expect(smx_test_file(L"BWTL_SMT_BENDS.tsv",L"SECC 2 2 90 AIR 0.4 B1\n"),"bend index fixture");
    smx_expect(smx_test_file(L"BWTL_SMT_STANDARD.tsv",L"R1 M1 B1 T1\n"),"standard index fixture");
    CASE(1,"unknown template refused",L"SMX_TEMPLATE %ls MISSING",L"bad_template");
    CASE(0,"company template created",L"SMX_TEMPLATE %ls DOOR",L"company_door");
    MUST(ProMdlnameInit(L"company_door",PRO_MDL_PART,&smx_model));
    MUST(ProMdlMdlnameGet(smx_model,smx_name));
    MUST(ProMdlOriginGet(smx_model,origin));
    CASE(0,"native sketch",L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",smx_name);
    CASE(0,"native sheet wall",L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",smx_name);
    sh_test_refs(smx_model,&f);
    CASE(0,"material lookup",L"SMX_MATERIAL %ls SECC 2",smx_name);
    CASE(0,"material substitution compares actual rows",L"SMX_SUBSTITUTE %ls SECC SPCC 2",smx_name);
    CASE(1,"material size not in library",L"SMX_MATERIAL %ls SECC 3",smx_name);
    CASE(0,"metadata initializes in one operation",L"SMX_METADATA %ls F001 D001 A SECC ZINC 2 LASER",smx_name);
    CASE(1,"metadata conflicts do not overwrite",L"SMX_METADATA %ls F002 D001 A SECC ZINC 2 LASER",smx_name);
    CASE(0,"coordinate frames read back",L"SMX_FRAMES %ls CSYS:PRT_CSYS_DEF CSYS:PRT_CSYS_DEF",smx_name);
    CASE(0,"paired planar thickness",L"SMX_GAUGE %ls",smx_name);
    CASE(0,"stock classification",L"SMX_ATTACH %ls",smx_name);
    CASE(0,"body inventory",L"SMX_BODIES %ls",smx_name);
    CASE(0,"grain metadata",L"SMX_GRAIN %ls SURFACE:%d EDGE:%d COATED FILM",smx_name,f.face,f.edge);
    CASE(0,"bend-rule table lookup",L"SMX_RULE %ls SECC 2 2 90 AIR",smx_name);
    CASE(1,"unmatched rule refused",L"SMX_RULE %ls SECC 2 2 85 AIR",smx_name);
    CASE(0,"standards revision check",L"SMX_REVISION %ls R1",smx_name);
    CASE(1,"standard migration guards missing bend proof",L"SMX_REVISION %ls R1 APPLY NONE",smx_name);
    CASE(0,"copy thickness preview",L"SMX_THICK_PREVIEW %ls 2 2.5 SURFACE:%d",smx_name,f.face);
    MUST(fab_thickness(smx_model,&mm,&thickness));
    smx_expect(fabs(thickness-2)<0.0001,"preview keeps source thickness");
    CASE(1,"bad hold dimension refused",L"SMX_THICK_APPLY %ls 2 2.5 X 75 0.01 SURFACE:%d",smx_name,f.face);
    CASE(0,"actual thickness apply and replay",L"SMX_THICK_APPLY %ls 2 2.5 X 80 0.01 SURFACE:%d",smx_name,f.face);
    MUST(fab_thickness(smx_model,&mm,&thickness));
    smx_expect(fabs(thickness-2.5)<0.0001,"applied model thickness is 2.5 mm");
    CASE(0,"compare bend directions in disposable copies",L"SMX_DIRECTIONS %ls EDGE:%d 20 90 2 SURFACE:%d",smx_name,f.edge,f.face);
    CASE(0,"target dimension flange",L"SMX_OUTER %ls SIZED EDGE:%d Z 30 90 2 POS 0.005",smx_name,f.edge);
    sh_test_refs(smx_model,&f);
    CASE(0,"bend cylinders pair",L"SMX_PAIR %ls",smx_name);
    CASE(0,"bend classification",L"SMX_CONFIDENCE %ls",smx_name);
    CASE(0,"bend dimensions",L"SMX_DIMENSION %ls SURFACE:%d",smx_name,f.bend);
    CASE(0,"grain dependent bend radius on actual bend",L"SMX_RADIUS %ls SECC GRAIN 5 0",smx_name);
    CASE(0,"allowance evidence",L"SMX_ALLOWANCE_SOURCE %ls",smx_name);
    CASE(0,"coupon known values",L"SMX_COUPON %ls 2 2 90 96.3982297 100",smx_name);
    CASE(0,"verify laser contours of bent plate",L"SMX_LASER %ls SURFACE:%d 0.2 0.2 15",smx_name,f.face);
    CASE(0,"read flattened contour and bends",L"SMX_CROSS_BEND %ls SURFACE:%d",smx_name,f.face);
    {
        ModelPlan plate={0,80,60,2,0,1};
        const char *stage="production sheet openings";
        MUST(model_create(&plate,smx_name,&stage));
        MUST(ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model));
        SmxGeometry *geometry=calloc(1,sizeof(*geometry));
        if(!geometry){error=PRO_TK_OUT_OF_MEMORY;goto done;}
        error=smx_geometry(smx_model,geometry);
        int top=0;
        for(int i=0;!error&&i<geometry->count;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type==PRO_SRF_PLANE&&face->axis[2]>0.99)top=face->id;
        }
        free(geometry);
        if(error||!top){error=error?error:PRO_TK_E_NOT_FOUND;goto done;}
        CASE(0,"target datum set before sheet conversion",L"DATUM_PLANE %ls TARGET PLANE:FRONT 15",smx_name);
        CASE(0,"opening coordinates at plate face",L"DATUM_CSYS %ls PANEL_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",smx_name);
        CASE(0,"rebuild flat source as sheet metal copy",L"SMX_REBUILD %ls RECOVERED SURFACE:%d 2",smx_name,top);
        CASE(0,"compare registered geometry to prior model",L"SMX_GEOMETRY_DIFF %ls COMPANY_DOOR 0.01",smx_name);
        CASE(0,"opening fixture converted",L"SMT_CONVERT %ls TO_SMT SURFACE:%d 2",smx_name,top);
        geometry=calloc(1,sizeof(*geometry));
        if(!geometry){error=PRO_TK_OUT_OF_MEMORY;goto done;}
        error=smx_geometry(smx_model,geometry);
        top=0;
        for(int i=0;!error&&i<geometry->count;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type==PRO_SRF_PLANE&&face->axis[2]>0.99)top=face->id;
        }
        free(geometry);
        if(error||!top){error=error?error:PRO_TK_E_NOT_FOUND;goto done;}
        CASE(0,"circular opening group",L"SMX_GRID %ls CIRCLES SURFACE:%d CSYS:PANEL_AT PLANE:RIGHT 15 15 2 1 10 0 ROUND 4 0 0 1",smx_name,top);
        CASE(1,"unregistered mount template rejected",L"SMX_MOUNT %ls BAD SURFACE:%d CSYS:PANEL_AT PLANE:RIGHT NOPE 32 32 1",smx_name,top);
        smx_expect(smx_test_file(L"BWTL_SMT_MOUNTS.tsv",L"FAN M1 ROUND 0 0 3 0 0\nFAN M1 ROUND 12 0 3 0 0\n"),"approved hole fixture");
        CASE(0,"apply approved mount template",L"SMX_MOUNT %ls FAN SURFACE:%d CSYS:PANEL_AT PLANE:RIGHT FAN 30 32 1",smx_name,top);
        CASE(0,"clip grid near plate edge",L"SMX_CLIP_GRID %ls CLIPPED SURFACE:%d CSYS:PANEL_AT PLANE:RIGHT 65 45 2 1 15 0 SLOT 6 3 0 1",smx_name,top);
        CASE(0,"rounded connector interface",L"SMX_INTERFACE %ls PORT SURFACE:%d CSYS:PANEL_AT PLANE:RIGHT ROUNDRECT 55 15 12 8 2 1",smx_name,top);
        geometry=calloc(1,sizeof(*geometry));
        if(!geometry){error=PRO_TK_OUT_OF_MEMORY;goto done;}
        error=smx_geometry(smx_model,geometry);
        int holes[2]={0},n=0;
        for(int i=0;!error&&i<geometry->count&&n<2;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type==PRO_SRF_CYL&&fabs(face->radius-2)<0.0001&&fabs(face->point[1]-15)<0.0001){
                if(fabs(face->point[0]-15)<0.0001&&!holes[0]){holes[0]=face->id;n++;}
                if(fabs(face->point[0]-25)<0.0001&&!holes[1]){holes[1]=face->id;n++;}
            }
        }
        free(geometry);
        smx_expect(n==2,"find actual cylinders");
        if(n==2){
            CASE(0,"measure normal hole axis",L"SMX_NORMAL %ls SURFACE:%d SURFACE:%d 0.1",smx_name,top,holes[0]);
            CASE(0,"report bend tangency without a nearby bend",L"SMX_BEND_CLEARANCE %ls SURFACE:%d SURFACE:%d 1",smx_name,top,holes[0]);
            CASE(0,"read web between existing holes",L"SMX_WEB_CLEARANCE %ls SURFACE:%d SURFACE:%d 2 SURFACE:%d",smx_name,top,holes[0],holes[1]);
            CASE(1,"wrong existing opening count rejected",L"SMX_EDIT_CIRCLES %ls CIRCLES_SK CIRCLES 4 5 3",smx_name);
            CASE(0,"edit two existing native holes",L"SMX_EDIT_CIRCLES %ls CIRCLES_SK CIRCLES 4 5 2",smx_name);
        }
        CASE(0,"laser risk checks on mixed line arc and circle contours",L"SMX_LASER %ls SURFACE:%d 3 3 16",smx_name,top);
        CASE(0,"export folded or flat geometry for reproduction",L"SMT_DXF %ls REBUILD_FLAT SURFACE:%d",smx_name,top);
        const double axis_x[]={1,0,0};
        CabinetEdgeSearch plate_edge={0};
        plate_edge.model=smx_model;plate_edge.face_id=top;plate_edge.parallel=axis_x;
        MUST(cm_edges_on_face(&plate_edge));
        smx_expect(plate_edge.matches>0,"locate actual flange edge on converted sheet");
        if(plate_edge.matches){
            CASE(0,"flange reaches existing datum plane",L"SMX_TO_PLANE %ls TO_TARGET EDGE:%d PLANE:TARGET 0 90 2 NEG 0.01",
                smx_name,plate_edge.edge.id);
            ProFeature target_wall;
            CabinetFace target_face;
            if(!cm_feature_find(smx_model,L"TO_TARGET",&target_wall)&&!cm_wall_face(&target_wall,&target_face)){
                plate_edge.matches=0;
                MUST(cm_edges_on_face(&plate_edge));
                if(plate_edge.matches){
                    wchar_t ref[40],edge_token[40];
                    swprintf_s(ref,_countof(ref),L"SURFACE:%d",target_face.id);
                    swprintf_s(edge_token,_countof(edge_token),L"EDGE:%d",plate_edge.edge.id);
                    SmxFlangeSpec probe={SMX_INNER,0,0,90,2,0.01,edge_token,L"NEG",ref,L"INNER_PROBE"};
                    ProMdl temporary=NULL;
                    FdTxn probe_tx={0};
                    CabinetEdge read_edge;
                    ProFeature wall;
                    double expected=0;
                    ProError result=smx_copy_scratch(smx_model,&temporary);
                    if(!result)result=smx_flange_add(temporary,&probe,6,&probe_tx,&read_edge,&wall);
                    if(!result)result=smx_flange_measure(temporary,&probe,&read_edge,&wall,1,&expected);
                    fd_cleanup(&probe_tx,0);
                    if(temporary)ProMdlErase(temporary);
                    smx_expect(!result&&expected>0,"measure real clearance before creating matching wall");
                    if(!result)CASE(0,"create wall for measured inside clearance",L"SMX_INNER %ls INSIDE %ls %ls %.8g 90 2 NEG 0.01",
                        smx_name,edge_token,ref,expected);
                }
            }
            if(holes[0]){
                geometry=calloc(1,sizeof(*geometry));
                if(!geometry){error=PRO_TK_OUT_OF_MEMORY;goto done;}
                error=smx_geometry(smx_model,geometry);
                int bore=0;
                for(int i=0;!error&&i<geometry->count;i++){
                    const SmxFace *face=&geometry->faces[i];
                    if(face->type==PRO_SRF_CYL&&fabs(face->radius-2.5)<0.0001&&fabs(face->point[0]-15)<0.0001&&
                       fabs(face->point[1]-15)<0.0001)bore=face->id;
                }
                free(geometry);
                if(bore)CASE(0,"hole to real bend tangent",L"SMX_BEND_CLEARANCE %ls SURFACE:%d SURFACE:%d 1",smx_name,top,bore);
            }
        }
        ProMdlName slot_stock;
        wcscpy_s(slot_stock,_countof(slot_stock),smx_name);
        int slot_top=top;
        MUST(model_create_empty(2,smx_name,&stage));
        MUST(ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model));
        CASE(0,"reconstruct closed DXF geometry",L"SMX_DXF_REBUILD %ls REBUILT REBUILD_FLAT PLANE:FRONT PLANE:RIGHT 2 POS",smx_name);
        MUST(model_create_empty(2,smx_name,&stage));
        MUST(ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model));
        CASE(0,"tab stock sketch",L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",smx_name);
        CASE(0,"tab stock wall",L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",smx_name);
        sh_test_refs(smx_model,&f);
        CASE(0,"paired tab and mating slot on separate copies",L"SMX_TAB_PAIR %ls TAB_PRT %ls SLOT_PRT EDGE:%d SURFACE:%d CSYS:PANEL_AT PLANE:RIGHT 20 10 14 2 POS 45 45 0.2 1",smx_name,slot_stock,f.edge,slot_top);
    }
    {
        const char *stage="production split body fixture";
        MUST(model_create_empty(2,smx_name,&stage));
        MUST(ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model));
        CASE(0,"first sheet body stock",L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",smx_name);
        CASE(0,"first sheet body",L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",smx_name);
        CASE(0,"second sheet body stock",L"SMT_SKETCH %ls SECOND_SK PLANE:FRONT PLANE:RIGHT RECT 100 0 30 40",smx_name);
        FdTxn native={0};
        PartCommand second={0};
        wcscpy_s(second.arg[3],PART_TOKEN_MAX,L"SKETCH:SECOND_SK");
        wcscpy_s(second.arg[4],PART_TOKEN_MAX,L"2");
        wcscpy_s(second.arg[5],PART_TOKEN_MAX,L"POS");
        error=sh_begin(&native,smx_model,L"SECOND_BODY",PRO_FEAT_DATUM_SURF);
        if(!error)error=sh_first(&native,&second);
        fd_cleanup(&native,error);
        smx_expect(!error,"create true second sheet body");
        if(!error){
            ProSolidBody *bodies=NULL;
            int n=0,first=0;
            MUST(ProSolidBodiesCollect((ProSolid)smx_model,&bodies));
            error=ProArraySizeGet((ProArray)bodies,&n);
            if(!error&&n>0)first=bodies[0].id;
            ProArrayFree((ProArray *)&bodies);
            trace("native sheet bodies returned by Creo",n);
            CASE(0,"inspect actual returned bodies",L"SMX_BODIES %ls",smx_name);
            if(n==2){
                CASE(0,"separate one active sheet body",L"SMX_SPLIT %ls BODY:%d SPLIT_OUT",smx_name,first);
            }else{
                CASE(1,"single body partition rejected without mutation",L"SMX_SPLIT %ls BODY:%d SPLIT_OUT",smx_name,first);
                smx_expect(0,"full production gate requires native multiple sheet bodies");
            }
        }
    }
    {
        const char *stage="production hem fixtures";
        MUST(model_create_empty(2,smx_name,&stage));
        MUST(ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model));
        CASE(0,"hinge stock sketch",L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",smx_name);
        CASE(0,"hinge stock wall",L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",smx_name);
        sh_test_refs(smx_model,&f);
        CASE(0,"native hinge curl and actual pin clearances",L"SMX_PIN_BARREL %ls BARREL EDGE:%d 10 4 POS 7 82 0.4 1 0.02",smx_name,f.edge);
        MUST(model_create_empty(2,smx_name,&stage));
        MUST(ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model));
        CASE(0,"hem stock sketch",L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",smx_name);
        CASE(0,"hem stock wall",L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",smx_name);
        sh_test_refs(smx_model,&f);
        CASE(0,"near contact return geometry and overall stack",L"SMX_CLOSED_HEM %ls CLOSED EDGE:%d 20 0.05 POS 0.12 4.10 0.02",smx_name,f.edge);
    }
    {
        const char *stage="production bend and seam fixtures";
        MUST(model_create_empty(2,smx_name,&stage));
        MUST(ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model));
        CASE(0,"section stock sketch",L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",smx_name);
        CASE(0,"section stock wall",L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",smx_name);
        sh_test_refs(smx_model,&f);
        CASE(0,"chained flange with separate native bends",L"SMX_PROFILE %ls PROFILE EDGE:%d 18 90 2 POS 12 90 2 NEG",smx_name,f.edge);
        MUST(model_create_empty(2,smx_name,&stage));
        MUST(ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model));
        CASE(0,"offset bend stock sketch",L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",smx_name);
        CASE(0,"offset bend stock wall",L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",smx_name);
        sh_test_refs(smx_model,&f);
        CASE(1,"flat plate free edge cannot be used as a closed seam",L"SMX_RIP %ls SURFACE:%d 0.25 EDGE:%d",smx_name,f.face,f.edge);
        CASE(0,"offset bend measured from actual sheet edge",L"SMX_BEND_OFFSET %ls OFFSET EDGE:%d 18 PLANE:RIGHT 90 2 POS FIX1",smx_name,f.edge);
        MUST(model_create_empty(2,smx_name,&stage));
        MUST(ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model));
        CASE(0,"four walls in corner fixture",L"CASE_TRAY %ls TRAY PLANE:FRONT PLANE:RIGHT 90 70 2 20 2 5 POS",smx_name);
        ProFeature walls[2];
        CabinetFace faces[2];
        MUST(cm_feature_find(smx_model,L"TRAY_W1",&walls[0]));
        MUST(cm_feature_find(smx_model,L"TRAY_W2",&walls[1]));
        MUST(cm_wall_face(&walls[0],&faces[0]));
        MUST(cm_wall_face(&walls[1],&faces[1]));
        CASE(0,"minimum clearance on two original wall faces",L"SMX_FLANGE_CLEARANCE %ls SURFACE:%d SURFACE:%d 2",smx_name,faces[0].id,faces[1].id);
        const double along_x[]={1,0,0},along_y[]={0,1,0};
        CabinetEdgeSearch tips[2]={{0},{0}};
        for(int i=0;i<2;i++){
            tips[i].model=smx_model;tips[i].face_id=faces[i].id;tips[i].parallel=i?along_y:along_x;
            MUST(cm_edges_on_face(&tips[i]));
        }
        if(!tips[0].matches||!tips[1].matches){
            tips[0].matches=0;tips[1].matches=0;tips[0].parallel=along_y;tips[1].parallel=along_x;
            MUST(cm_edges_on_face(&tips[0]));MUST(cm_edges_on_face(&tips[1]));
        }
        smx_expect(tips[0].matches&&tips[1].matches,"both wall tips located");
        if(tips[0].matches&&tips[1].matches){
            ProMdl original=smx_model;
            ProMdlName saved;
            wcscpy_s(saved,_countof(saved),smx_name);
            for(int variant=0;variant<2;variant++){
                ProMdl copy=NULL;
                MUST(smx_copy_scratch(original,&copy));
                MUST(dv_display_new(copy));
                smx_model=copy;
                MUST(ProMdlMdlnameGet(smx_model,smx_name));
                if(variant==0){
                    CASE(0,"asymmetrical miter using two different trims",L"SMX_MITER_OFFSET %ls MITER EDGE:%d EDGE:%d 3 1 2 1.5 2 1",
                        smx_name,tips[0].edge.id,tips[1].edge.id);
                }else{
                    CASE(1,"weld clearance outside tolerance rolls back both ends",L"SMX_WELD_SEAM %ls WELD EDGE:%d EDGE:%d 2.6 0.6 2.6 0.6 2 2 0.1 0.1 0.3 0.02",
                        smx_name,tips[0].edge.id,tips[1].edge.id);
                    CASE(0,"weld allowances and measured seam",L"SMX_WELD_SEAM %ls WELD EDGE:%d EDGE:%d 2.6 0.6 2.6 0.6 2 5.6568542 0.1 0.1 0.3 0.02",
                        smx_name,tips[0].edge.id,tips[1].edge.id);
                }
                smx_model=original;
                wcscpy_s(smx_name,_countof(smx_name),saved);
                MUST(dv_display_new(original));
                MUST(ProMdlErase(copy));
            }
            CASE(0,"miter two wall ends and measure new seam",L"SMX_MITER %ls MITER EDGE:%d EDGE:%d 3 1 2 1",smx_name,tips[0].edge.id,tips[1].edge.id);
        }
        sh_test_refs(smx_model,&f);
        SmxGeometry *geometry=calloc(1,sizeof(*geometry));
        if(!geometry){error=PRO_TK_OUT_OF_MEMORY;goto done;}
        error=smx_geometry(smx_model,geometry);
        int bend[2]={0};
        for(int i=0;!error&&i<geometry->count;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type!=PRO_SRF_CYL)continue;
            ProSurface surface;
            ProGeomitem item;
            ProFeature feature;
            ProSmtSurfType role;
            if(ProSurfaceInit((ProSolid)smx_model,face->id,&surface)||
               ProSmtSurfaceTypeGet((ProPart)smx_model,surface,&role)||role!=PRO_SMT_SURF_FACE||
               ProSurfaceToGeomitem((ProSolid)smx_model,surface,&item)||ProGeomitemFeatureGet(&item,&feature))continue;
            for(int j=0;j<2;j++)if(feature.id==walls[j].id)bend[j]=face->id;
        }
        free(geometry);
        smx_expect(bend[0]&&bend[1],"find both corner bend references");
        if(bend[0]&&bend[1]){
            CASE(0,"compare all three native reliefs on copies",L"SMX_RELIEF_COMPARE %ls CHECK_RELIEF SURFACE:%d SURFACE:%d SURFACE:%d 22 12",smx_name,f.face,bend[0],bend[1]);
            CASE(0,"apply native corner relief",L"SMX_CORNER_RELIEF %ls CORNER SURFACE:%d SURFACE:%d SURFACE:%d RECT 22 12",smx_name,f.face,bend[0],bend[1]);
        }
    }
done:
    trace("SMX_CASES",cases);
    trace("SMX_FAILURES",failures);
    trace("SMX_DONE",error);
    ProEngineerEnd();
    return 0;
#undef CASE
#undef MUST
}

static int smx_test_prepare(uiCmdCmdId id,uiCmdValue *value,void *context){
    const char *stage="production sheetmetal fixture";
    model_background_mode=0;
    ProError error=model_create_empty(2,smx_name,&stage);
    if(!error)error=ProMdlnameInit(smx_name,PRO_MDL_PART,&smx_model);
    if(!error)error=ProMacroLoad(L"~ Command \140BWTLSmxRun\140;");
    if(error){trace("SMX_DONE",error);ProEngineerEnd();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLSmxPrepare",smx_test_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLSmxRun",smx_test_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \140BWTLSmxPrepare\140;");
}
