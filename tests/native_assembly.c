#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>
#include <ProMaterial.h>

static void check(int pass,const char *label){
    cases++;
    if(!pass)failures++;
    trace(label,pass?0:-1);
}

static ProError command(ProMdl model,const wchar_t *protocol,int reject,const wchar_t *expected){
    PartCommand parsed;
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    wchar_t report[PART_TEXT_MAX]=L"";
    ProError error=snapshot?part_snapshot_take_mode(model,snapshot,0):PRO_TK_OUT_OF_MEMORY;
    if(!error){
        if(part_command_parse(protocol,&parsed)!=1)error=PRO_TK_BAD_INPUTS;
        else error=part_execute(&parsed,snapshot,report,_countof(report));
    }
    int pass=(reject?error!=0:error==0)&&(!expected||wcsstr(report,expected));
    check(pass,"assembly command");
    FILE *log=_wfopen(L"assembly-cases.log",L"a, ccs=UTF-8");
    if(log){
        fwprintf(log,L"%ls\nRESULT=%d PASS=%d EXPECTED=%ls\n%ls\n",protocol,error,pass,expected?expected:L"",report);
        fclose(log);
    }
    if(snapshot){part_snapshot_clear(snapshot);free(snapshot);}
    return error;
}

static ProError csys_name(ProFeature *feature,ProError status,ProAppData data){
    ProFeattype type;
    if(status)return status;
    if(!ProFeatureTypeGet(feature,&type)&&type==PRO_FEAT_CSYS)
        ProModelitemNameGet(feature,data);
    return PRO_TK_NO_ERROR;
}

static int labels(ProMdl assembly,wchar_t first[32],wchar_t second[32]){
    AxScan *scan=NULL;
    if(ax_scan(assembly,1,&scan))return 0;
    int pass=scan->count==2&&ax_path_label(scan->occurrences[0].ids,1,first,32)&&
        ax_path_label(scan->occurrences[1].ids,1,second,32)&&wcscmp(first,second);
    free(scan);
    return pass;
}

static ProError add_component(ProMdl assembly,ProMdl child,double x,ProAsmcomp *component){
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{x,0,0,1}};
    return ProAsmcompAssemble((ProSolid)assembly,child,pose,component);
}

static ProError material_assign(ProMdl part){
    ProMaterial material={L"plastic_pvc",(ProSolid)part};
    ProError error=ProMaterialfileRead((ProSolid)part,material.matl_name);
    trace("read fixture material",error);
    if(!error)error=ProMaterialCurrentSet(&material);
    trace("assign fixture material",error);
    return error;
}

static ProMdl part=NULL,other=NULL,assembly=NULL;
static ProMdlName name,source_name,other_name;
static ProWVerstamp stamp=NULL;

static int begin(uiCmdCmdId id,uiCmdValue *value,void *data){
    Fixture f;
    const char *stage="assembly fixture";
    wchar_t text[512];
    ProError error;
    int window;
    model_background_mode=0;
    error=fixture(&part,source_name,&f);
    if(!error)error=fixture(&part,source_name,&f);
    if(!error)error=fixture(&other,other_name,&f);
    if(!error)error=material_assign(part);
    if(!error)error=material_assign(other);
    if(!error){
        swprintf_s(text,_countof(text),L"CREO_OP DV_ATTR %ls 1 %ls PART_CODE AX_TEST",other_name,source_name);
        error=run(other,text,0);
    }
    if(!error)error=ProMdlVerstampGet(part,&stamp);
    if(!error)error=model_create_empty(3,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&assembly);
    if(!error)error=ProMdlWindowGet(assembly,&window);
    if(!error)error=ProWindowCurrentSet(window);
    if(!error)error=ProMacroLoad(L"~ Command `BWTLAssemblyRun`;");
    if(error){
        check(0,stage);
        trace("fixture error",error);
        trace("ASSEMBLY_FAILURES",failures);
        ProEngineerEnd();
    }
    return 0;
}

static int run_assembly(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl sub=NULL;
    ProMdlName sub_name;
    ProName assembly_csys=L"",part_csys=L"";
    ProWVerstamp after=NULL,asm_stamp=NULL,asm_after=NULL;
    ProPath directory,after_dir;
    ProAsmcomp inner,outer;
    const char *stage="nested fixture";
    wchar_t text[512],first[32],second[32],nested[64];
    ProError error;
#define CALL(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),command(assembly,text,reject,expected))
    CALL(0,L"组件实例 0",L"ASM_TREE %ls",name);
    CALL(1,NULL,L"ASM_TREE %ls",source_name);
    CALL(1,NULL,L"ASM_MOVE %ls ID:999 0 0 0 1 0 0",name);
    CALL(0,L"0, 0, 0",L"ASM_INSERT %ls %ls 0 0 0",name,source_name);
    CALL(0,L"90, 0, 0",L"ASM_INSERT %ls %ls 90 0 0",name,source_name);
    int labeled=labels(assembly,first,second);
    check(labeled,"duplicate model has two distinct native instance paths");
    if(!labeled)goto done;
    CALL(0,L"活动零件 2",L"ASM_TREE %ls",name);
    CALL(1,NULL,L"ASM_CLEARANCE %ls %ls %ls 1",name,first,first);
    CALL(0,L"最小间隙 10 mm",L"ASM_CLEARANCE %ls %ls %ls 9",name,first,second);
    CALL(0,L"低于要求",L"ASM_CLEARANCE %ls %ls %ls 11",name,first,second);
    CALL(0,L"未检出正体积干涉",L"ASM_INTERFERENCE %ls %ls %ls",name,first,second);
    CALL(0,L"80, 0, 0",L"ASM_MOVE %ls %ls 90 0 0 80 0 0",name,second);
    CALL(0,L"最小间隙 0 mm",L"ASM_CLEARANCE %ls %ls %ls 0",name,first,second);
    CALL(0,L"未检出正体积干涉",L"ASM_INTERFERENCE %ls %ls %ls",name,first,second);
    CALL(1,NULL,L"ASM_MOVE %ls %ls 89 0 0 70 0 0",name,second);
    CALL(0,L"70, 0, 0",L"ASM_MOVE %ls %ls 80 0 0 70 0 0",name,second);
    CALL(0,L"12000 mm³",L"ASM_INTERFERENCE %ls %ls %ls",name,first,second);
    CALL(0,L"正体积干涉 1 对",L"ASM_INTERFERENCE %ls ALL",name);
    CALL(0,L"存在干涉",L"ASM_CLEARANCE %ls %ls %ls 0",name,first,second);
    CALL(0,NULL,L"ASM_STATE %ls %ls SUPPRESS",name,second);
    CALL(0,L"普通抑制",L"ASM_TREE %ls",name);
    CALL(1,NULL,L"ASM_INTERFERENCE %ls %ls %ls",name,first,second);
    CALL(0,L"1 个活动",L"ASM_BOM %ls SUPPRESSED_BOM",name);
    CALL(0,NULL,L"ASM_STATE %ls %ls RESUME",name,second);
    CALL(0,L"原点 (70, 0, 0)",L"ASM_TREE %ls",name);
    ProSolidFeatVisit((ProSolid)assembly,csys_name,NULL,assembly_csys);
    ProSolidFeatVisit((ProSolid)part,csys_name,NULL,part_csys);
    check(assembly_csys[0]&&part_csys[0],"both datum coordinate systems exist");
    if(!assembly_csys[0]||!part_csys[0])goto done;
    CALL(1,NULL,L"ASM_CSYS %ls %ls CSYS:MISSING CSYS:%ls",name,second,part_csys);
    CALL(0,L"坐标系",L"ASM_CSYS %ls %ls CSYS:%ls CSYS:%ls",name,second,assembly_csys,part_csys);
    CALL(1,NULL,L"ASM_MOVE %ls %ls 0 0 0 1 0 0",name,second);
    CALL(0,L"96000 mm³",L"ASM_INTERFERENCE %ls %ls %ls",name,first,second);

    model_background_mode=1;
    error=model_create_empty(3,sub_name,&stage);
    model_background_mode=0;
    if(!error)error=ProMdlnameInit(sub_name,PRO_MDL_ASSEMBLY,&sub);
    if(!error)error=add_component(sub,other,0,&inner);
    if(!error)error=part_regenerate((ProSolid)sub);
    if(!error)error=add_component(assembly,sub,200,&outer);
    if(!error)error=part_regenerate((ProSolid)assembly);
    check(!error,"create native nested fixture");
    if(error){trace("nested fixture error",error);goto done;}
    swprintf_s(nested,_countof(nested),L"ID:%d/%d",outer.id,inner.id);
    CALL(0,nested,L"ASM_TREE %ls",name);
    CALL(0,L"最小间隙 120 mm",L"ASM_CLEARANCE %ls %ls %ls 119",name,first,nested);
    CALL(1,NULL,L"ASM_STATE %ls %ls SUPPRESS",name,nested);
    CALL(0,L"3 对组合，正体积干涉 1 对",L"ASM_INTERFERENCE %ls ALL",name);
    ProMdlVerstampGet(assembly,&asm_stamp);
    ProDirectoryCurrentGet(directory);
    CALL(0,L"288000 mm³",L"ASM_MASS %ls",name);
    CALL(0,L"Creo 装配质量 0.1512 kg。重心 (106.6666667, 30, 10) mm",L"ASM_MASS %ls",name);
    CALL(0,NULL,L"ASM_BOM %ls FULL_BOM",name);
    CALL(1,NULL,L"ASM_BOM %ls FULL_BOM",name);
    CALL(0,NULL,L"ASM_STEP %ls ASM_STEP_OUT",name);
    CALL(1,NULL,L"ASM_STEP %ls ASM_STEP_OUT",name);
    check(!ProMdlVerstampGet(assembly,&asm_after)&&asm_stamp&&ProVerstampEqual(asm_stamp,asm_after),
        "inspection and export preserve assembly stamp");
    check(!ProDirectoryCurrentGet(after_dir)&&!wcscmp(directory,after_dir),"export preserves working directory");
    check(!ProMdlVerstampGet(part,&after)&&stamp&&ProVerstampEqual(stamp,after),"placement preserves source part stamp");
    ProMdl disk_part=NULL;
    ProMdlName disk_name;
    Fixture disk_fixture;
    model_background_mode=1;
    error=fixture(&disk_part,disk_name,&disk_fixture);
    model_background_mode=0;
    if(!error)error=ProMdlSave(disk_part);
    if(!error)error=ProMdlErase(disk_part);
    int disk_only=!error&&ProMdlnameInit(disk_name,PRO_MDL_PART,&disk_part)==PRO_TK_E_NOT_FOUND;
    check(disk_only,"local fixture exists on disk and is not loaded");
    if(disk_only)CALL(0,L"350, 0, 0",L"ASM_INSERT %ls %ls 350 0 0",name,disk_name);
    CALL(1,NULL,L"ASM_INSERT %ls NO_SUCH_PART 0 0 0",name);
#undef CALL
done:
    if(stamp)ProVerstampFree(&stamp);
    if(after)ProVerstampFree(&after);
    if(asm_stamp)ProVerstampFree(&asm_stamp);
    if(asm_after)ProVerstampFree(&asm_after);
    trace("ASSEMBLY_CASES",cases);
    trace("ASSEMBLY_FAILURES",failures);
    trace("ASSEMBLY_DONE",1);
    ProEngineerEnd();
    return 0;
}

static int reload(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl current=NULL,imported=NULL;
    ProPath path;
    ProMassProperty mass;
    AxScan *scan=NULL;
    double scale=0;
    ProMdlCurrentGet(&current);
    check(!current,"STEP import starts in clean native session");
    ProError error=dv_work_file(L"source.stp",path);
    if(!error)error=ProIntfimportModelWithOptionsMdlnameCreate(path,NULL,PRO_INTF_IMPORT_STEP,
        PRO_MDL_ASSEMBLY,PRO_IMPORTREP_MASTER,L"ASSEMBLY_REOPENED",NULL,NULL,&imported);
    trace("STEP reload error",error);
    check(!error&&imported,"STEP native assembly reopens");
    error=imported?ax_scan(imported,0,&scan):PRO_TK_BAD_CONTEXT;
    trace("reopened tree scan",error);
    if(scan){
        trace("reopened tree count",scan->count);
        trace("reopened tree parts",scan->parts);
        trace("reopened tree unavailable",scan->unavailable);
    }
    int near_origin=0,translated=0,deepest=0,shallowest=AX_PATH_DEPTH;
    if(!error&&scan&&!qa_mm_per_unit(imported,&scale))for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(!item->model||item->type!=PRO_MDL_PART)continue;
        ProIdTable ids={0};
        ProAsmcomppath placement;
        ProMatrix pose;
        memcpy(ids,item->ids,item->depth*sizeof(int));
        if(ProAsmcomppathInit((ProSolid)imported,ids,item->depth,&placement)||
           ProAsmcomppathTrfGet(&placement,PRO_B_TRUE,pose))continue;
        if(item->depth>deepest)deepest=item->depth;
        if(item->depth<shallowest)shallowest=item->depth;
        if(!part_close(pose[3][1]*scale,0)||!part_close(pose[3][2]*scale,0))continue;
        if(part_close(pose[3][0]*scale,0))near_origin++;
        if(part_close(pose[3][0]*scale,200))translated++;
    }
    check(!error&&scan&&!scan->unavailable&&scan->parts==3&&deepest>shallowest,
        "STEP native structure retains repeated parts and nested occurrence");
    check(near_origin==2&&translated==1,"STEP native instance positions are preserved");
    check(imported&&!qa_mm_per_unit(imported,&scale)&&!qa_geometry_mass(imported,&mass)&&
        part_close(mass.volume*scale*scale*scale,288000),"STEP native volume preserved");
    free(scan);
    trace("ASSEMBLY_RELOAD_CASES",cases);
    trace("ASSEMBLY_RELOAD_FAILURES",failures);
    trace("ASSEMBLY_RELOAD_DONE",1);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))
        return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))
        return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    trace("legacy_bridge_loaded",0);
    uiCmdCmdId id;
    int blank=GetEnvironmentVariableA("BWTL_DELIVERY_BLANK",marker,sizeof(marker))==3&&!strcmp(marker,"yes");
    ProError error=ProCmdActionAdd("BWTLAssemblyRun",run_assembly,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLAssemblyProbe",blank?reload:begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLAssemblyProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
