#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#include <ProMirror.h>

static ProError mirror_trial_create(ProMdl model,const wchar_t *label,const FdRef *input)
{
    FdTxn tx={0};
    FdRef plane={0};
    ProError error=fd_begin(&tx,model,label,PRO_FEAT_SRF_MDL);
    if(!error)error=fd_ref_resolve(model,L"PLANE:MIRROR_PLANE",PRO_SURFACE,&plane);
    if(!error)error=model_element(tx.root,PRO_E_SRF_TRANS_TYPE,1,PRO_SURF_TRANS_TYPE_MIRROR,NULL);
    if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_MIRROR_REF_ITEMS,input);
    if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_MIRROR_REF_PLANE,&plane);
    if(!error)error=model_element(tx.root,PRO_E_COPY_NO_COPY,1,PRO_MIRROR_KEEP_ORIGINAL,NULL);
    if(!error)error=fd_create(&tx,0);
    for(int i=0;tx.errors.error_list&&i<tx.errors.error_number;i++){
        trace("mirror feature element",tx.errors.error_list[i].err_item_id);
        trace("mirror feature element error",tx.errors.error_list[i].error);
    }
    if(!error)error=part_regenerate(model);
    fd_ref_free(&plane);
    fd_cleanup(&tx,error);
    return error;
}

typedef struct {ProDimension dimension;int count;} MirrorPlaneOffset;

static ProError mirror_plane_offset(ProDimension *dimension,ProError status,ProAppData data)
{
    if(status)return status;
    MirrorPlaneOffset *offset=data;
    ProDimensiontype type;
    double value=0;
    ProError error=ProDimensionTypeGet(dimension,&type);
    if(!error)error=ProDimensionValueGet(dimension,&value);
    if(!error&&type==PRODIMTYPE_LINEAR&&fabs(value-105)<1e-7){
        offset->dimension=*dimension;
        offset->count++;
    }
    return error;
}

static ProError mirror_quilt_max(ProSurface surface,ProError status,ProAppData data)
{
    if(status)return status;
    ProVector direction={1,0,0},low,high;
    ProError error=ProSurfaceExtremesEval(surface,direction,low,high);
    if(!error&&high[0]>*(double *)data)*(double *)data=high[0];
    return error;
}

static ProError mirror_edge_x(ProMdl model,int body,double *x)
{
    if(body){
        ProOutline outline;
        ProError error=imitation_material_outline(model,outline);
        if(!error)*x=outline[1][0];
        return error;
    }
    FdRef ref={0};
    ProQuilt quilt;
    ProError error=sf_ref(model,L"QUILT:QUILT_MIRROR",&ref);
    if(!error)error=ProGeomitemToQuilt(&ref.item,&quilt);
    *x=-1e10;
    if(!error)error=ProQuiltSurfaceVisit(quilt,mirror_quilt_max,NULL,x);
    fd_ref_free(&ref);
    return error;
}

static ProError mirror_check_edit(const wchar_t *output,int body,FILE *log)
{
    ProMdl result=NULL;
    ProFeature plane,mirror;
    MirrorPlaneOffset offset={0};
    ProFeattype type;
    double before=0,after=0,restored=0;
    ProError error=ProMdlFiletypeLoad((wchar_t *)output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=ux_named_feature(result,L"MIRROR_PLANE",&plane);
    if(!error)error=ux_named_feature(result,body?L"BODY_MIRROR":L"QUILT_MIRROR",&mirror);
    if(!error)error=ProFeatureTypeGet(&mirror,&type);
    if(!error&&type!=PRO_FEAT_SRF_MDL)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProFeatureDimensionVisit(&plane,mirror_plane_offset,NULL,&offset);
    if(!error&&offset.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=mirror_edge_x(result,body,&before);
    if(!error)error=ProDimensionValueSet(&offset.dimension,107);
    if(!error)error=part_regenerate(result);
    if(!error)error=mirror_edge_x(result,body,&after);
    if(!error&&fabs(after-before-4)>0.002)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&offset.dimension,105);
    if(!error)error=part_regenerate(result);
    if(!error)error=mirror_edge_x(result,body,&restored);
    if(!error&&fabs(restored-before)>0.002)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"MIRROR PLANE EDIT BODY=%d ERROR=%d X=%.6f/%.6f/%.6f\n",body,error,before,after,restored);
    if(result)ProMdlErase(result);
    return error;
}

static ProError mirror_trial(FILE *log,int body)
{
    ProMdl source=NULL;
    ProMdlName name;
    Fixture stock;
    wchar_t command[1024];
    ProError error=fixture(&source,name,&stock);
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP DATUM_PLANE %ls MIRROR_PLANE PLANE:RIGHT 105",name);
        error=expansion_build(source,command,log);
    }
    if(!error&&!body){
        swprintf_s(command,_countof(command),L"CREO_OP SKETCH_CREATE %ls Q_SKETCH PLANE:FRONT PLANE:RIGHT RECT 10 20 15 25",name);
        error=expansion_build(source,command,log);
        if(!error){
            swprintf_s(command,_countof(command),L"CREO_OP SURFACE_EXTRUDE %ls Q_SEED Q_SKETCH 25 NEG",name);
            error=expansion_build(source,command,log);
        }
    }
    FdRef input={0};
    if(!error)error=body?qa_single_body(source,&input.item):sf_ref(source,L"QUILT:Q_SEED",&input);
    if(!error&&body)error=ProSelectionAlloc(NULL,&input.item,&input.selection);
    if(!error)error=mirror_trial_create(source,body?L"BODY_MIRROR":L"QUILT_MIRROR",&input);
    fd_ref_free(&input);
    fwprintf(log,L"MIRROR FIXTURE BODY=%d ERROR=%d\n",body,error);
    if(!error){
        int bodies=0;
        ProMassProperty mass={0};
        sd_body_count(source,&bodies);
        qa_geometry_mass(source,&mass);
        fwprintf(log,L"MIRROR SOURCE BODY=%d BODIES=%d VOLUME=%.9f\n",body,bodies,mass.volume);
        if(bodies!=(body?2:1))error=PRO_TK_INCOMPLETE;
    }
    fflush(log);
    if(!error)error=ProMdlSave(source);
    if(!error){
        ProPath output=L"";
        wchar_t report[16000]=L"";
        PartText out={report,_countof(report),0};
        replay_preview(source,&out);
        error=imitation_run_mode(source,output,0,1,&out);
        fwprintf(log,L"MIRROR REPLAY BODY=%d ERROR=%d\n%ls\n",body,error,report);
        if(!error)error=mirror_check_edit(output,body,log);
    }
    return error;
}

static int mirror_trial_case(uiCmdCmdId command,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-quilt-body-mirror.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 1;}
    model_background_mode=1;
    ProError quilt=mirror_trial(log,0);
    ProError body=mirror_trial(log,1);
    fwprintf(log,L"QUILT=%d BODY=%d PASS=%d\n",quilt,body,!quilt&&!body);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80])
{
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLQuiltBodyMirror",mirror_trial_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLQuiltBodyMirror\x0060;");
}
