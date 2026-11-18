#define user_initialize bwtl_bso_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>
typedef struct {AxComponent owner;int shaft,holes[8],n,front,back;double low,high;} HardwareFaces;
static ProError fixture_faces(ProSurface surface,ProError status,ProAppData data){
    HardwareFaces *faces=data;
    if(status)return status;
    ProGeomitem item;
    AxGeometry shape;
    if(ProSurfaceToGeomitem((ProSolid)faces->owner.model,surface,&item)||ax_geometry(&item,&shape))return 0;
    if(shape.type==AX_GEOM_PLANE){
        if(shape.direction[2]>0.999&&fabs(shape.point[2]-faces->high)<0.001)faces->front=item.id;
        if(shape.direction[2]<-0.999&&fabs(shape.point[2]-faces->low)<0.001)faces->back=item.id;
    }else if(shape.type==AX_GEOM_CYLINDER){
        wchar_t token[40];
        int bore;
        ProVector ends[2];
        swprintf_s(token,_countof(token),L"SURFACE:%d",item.id);
        if(ax_hw_cylinder(&faces->owner,token,&shape,ends,&bore))return 0;
        if(!bore)faces->shaft=item.id;
        else if(faces->n<8){
            for(int i=0;i<faces->n;i++){
                ProSurface old;
                ProGeomitem previous;
                AxGeometry axis;
                if(!ProSurfaceInit((ProSolid)faces->owner.model,faces->holes[i],&old)&&
                   !ProSurfaceToGeomitem((ProSolid)faces->owner.model,old,&previous)&&!ax_geometry(&previous,&axis)&&
                   ax_hw_separation(shape.point,axis.point,axis.direction,1)<0.001)return 0;
            }
            faces->holes[faces->n++]=item.id;
        }
    }
    return 0;
}
static ProError find_fixture_faces(ProMdl model,double high,HardwareFaces *faces){
    faces->owner.model=model;faces->high=high;
    return ProSolidSurfaceVisit((ProSolid)model,fixture_faces,NULL,faces);
}
static int occurrences(ProMdl assembly,ProMdl model,ProAsmcomp *out,int cap){
    AxScan *scan=NULL;
    if(ax_scan(assembly,1,&scan))return -1;
    int n=0;
    for(int i=0;i<scan->count;i++)if(scan->occurrences[i].depth==1&&scan->occurrences[i].model==model){
        if(n>=cap||ax_component_feature(assembly,scan->occurrences[i].ids[0],out+n)){n=-1;break;}
        n++;
    }
    free(scan);
    return n;
}

static ProError add_component(ProMdl assembly,ProMdl child,double x,ProAsmcomp *component){
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{x,0,0,1}};
    return ProAsmcompAssemble((ProSolid)assembly,child,pose,component);
}

static FILE *bso_report;

static ProError bso_select_face(ProAsmcomppath *path,ProMdl owner,int id){
    ProModelitem item;
    ProSelection selection=NULL;
    ProError error=ProModelitemInit(owner,id,PRO_SURFACE,&item);
    if(!error)error=ProSelectionAlloc(path,&item,&selection);
    if(!error)error=ProSelbufferSelectionAdd(selection);
    if(selection)ProSelectionFree(&selection);
    return error;
}

static int bso_check(const char *name,ProError error){
    fprintf(bso_report,"%s=%d\n",name,error==0);
    if(error)fprintf(bso_report,"CREO_STATUS=%d\n",error);
    fflush(bso_report);
    return error!=0;
}

static int bso_flow(uiCmdCmdId id,uiCmdValue *value,void *data){
    int verify=bso_report!=NULL;
    if(!verify)bso_report=fopen("bso-workflow.log","w");
    if(!bso_report){ProEngineerEnd();return 0;}
    ProError error=0;
    static ProMdl root,panel,saved_part;
    ProMdl source=NULL;
    static ProAsmcomp base;
    ProAsmcomp bolts[4];
    ProMdlName assembly_name=L"",plate_name=L"BSO_FIXTURE_PANEL";
    ProAsmcomppath place;
    ProIdTable panel_path;
    PartSnapshot snapshot={0};
    PartCommand plate;
    static HardwareFaces faces;
    wchar_t report[PART_TEXT_MAX]=L"",origin[MAX_PATH]=L"",saved[MAX_PATH]=L"";
    const char *stage="new isolated test assembly";
    PartText out={report,_countof(report),0};
    int window;
    static double mm;
#define STEP(label,expr) do{error=(expr);if(bso_check(label,error))goto done;}while(0)
    if(verify)goto select_holes;
    STEP("NATURAL_REQUESTS",fastener_bso_request(L"我要bsoM3X6")!=BSO_SAVE||
        fastener_bso_request(L"装配 BSO M3×6 到选中的孔位")!=BSO_PLACE||
        fastener_bso_request(L"查找 BSO_3_5M3X6")!=BSO_FIND||
        fastener_bso_request(L"装配 BSO M3×60")||
        fastener_bso_request(L"不要装配 BSO M3×6")?PRO_TK_BAD_INPUTS:0);
    wcscpy_s(smart_latest_request,_countof(smart_latest_request),L"读取公司库");
    wchar_t sandbox_data[MAX_PATH];
    wcscpy_s(sandbox_data,_countof(sandbox_data),runtime_data);
    if(!GetEnvironmentVariableW(L"BWTL_TEST_COMPANY_DATA",runtime_data,_countof(runtime_data))){
        error=PRO_TK_BAD_INPUTS;
        goto done;
    }
    error=company_list(L"公司库",L"",0,&out);
    memset(smart_latest_request,0,sizeof(smart_latest_request));
    if(!error)error=fastener_bso_read(&source,origin,&out);
    wcscpy_s(runtime_data,_countof(runtime_data),sandbox_data);
    if(bso_check("LIBRARY_ALIAS_AND_EXACT_INSTANCE",error))goto done;
    STEP("PROJECT_COPY",fastener_bso_copy(source,&saved_part,saved,&out));
    STEP("FILE_EXISTS",GetFileAttributesW(saved)==INVALID_FILE_ATTRIBUTES?PRO_TK_CANT_OPEN:0);
    ProMdl again=NULL;
    STEP("REUSE_SAME_PROJECT_FILE",fastener_bso_copy(source,&again,saved,&out));
    STEP("SAME_PROJECT_MODEL",saved_part!=again?PRO_TK_BAD_CONTEXT:0);
    STEP("PANEL_PROTOCOL",part_command_parse(L"CREO_OP QUICK_HINGE BSO_FIXTURE_PANEL 120 90 1.5 70 40 5.4",&plate)!=1?PRO_TK_BAD_INPUTS:0);
    STEP("PROJECT_DIRECTORY",ProDirectoryCurrentGet(snapshot.directory));
    STEP("CREATE_PANEL",quick_execute(&plate,&snapshot,report,_countof(report)));
    STEP("READ_PANEL",ProMdlnameInit(plate_name,PRO_MDL_PART,&panel));
    STEP("FIND_HOLE_WALLS",find_fixture_faces(panel,1.5,&faces));
    STEP("FOUR_HOLES_AND_FACE",faces.n!=4||!faces.front?PRO_TK_BAD_CONTEXT:0);
    model_background_mode=0;
    STEP("CREATE_ASSEMBLY",model_create_empty(3,assembly_name,&stage));
    STEP("READ_ASSEMBLY",ProMdlnameInit(assembly_name,PRO_MDL_ASSEMBLY,&root));
    STEP("READ_ASSEMBLY_WINDOW",ProMdlWindowGet(root,&window));
    STEP("SET_CURRENT_WINDOW",ProWindowCurrentSet(window));
    STEP("READ_MM",qa_mm_per_unit(root,&mm));
    STEP("ADD_PANEL",add_component(root,panel,0,&base));
    STEP("REGENERATE",part_regenerate(root));
    STEP("WAIT_FOR_ACTIVE_WINDOW",ProMacroLoad(L"~ Command `BWTLBSOFlow`;"));
    return 0;

select_holes:
    panel_path[0]=base.id;
    STEP("ASSEMBLY_PATH",ProAsmcomppathInit((ProSolid)root,panel_path,1,&place));
    STEP("CLEAR_SELECTION",ProSelbufferClear());
    STEP("PRESSING_FACE",bso_select_face(&place,panel,faces.front));
    for(int i=0;i<4;i++)STEP("SELECT_HOLE",bso_select_face(&place,panel,faces.holes[i]));

    AxHardwareSite sites[12];
    int count=0;
    STEP("VALIDATE_SELECTED_HOLES",fastener_bso_selection(root,mm,sites,&count,&out));
    STEP("FOUR_ACTUAL_SITES",count!=4?PRO_TK_BAD_CONTEXT:0);
    STEP("BATCH_PLACEMENT",fastener_bso_place(root,saved_part,sites,count,mm,&out));
    STEP("FOUR_PLACED_COMPONENTS",occurrences(root,saved_part,bolts,4)!=4?PRO_TK_BAD_CONTEXT:0);
    STEP("DUPLICATE_REFUSED",fastener_bso_place(root,saved_part,sites,count,mm,&out)!=PRO_TK_E_FOUND?PRO_TK_BAD_CONTEXT:0);
    STEP("FOUR_COMPONENTS_PRESERVED",occurrences(root,saved_part,bolts,4)!=4?PRO_TK_BAD_CONTEXT:0);
done:
    if(error){
        FILE *detail=_wfopen(L"bso-workflow-detail.txt",L"wt, ccs=UTF-8");
        if(detail){fputws(report,detail);fclose(detail);}
    }
    fprintf(bso_report,"BSO_WORKFLOW_PASS=%d\n",error==0);
    fclose(bso_report);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=bwtl_bso_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLBSOFlow",bso_flow,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLBSOFlow\x0060;");
}
