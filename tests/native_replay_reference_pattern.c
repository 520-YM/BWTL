#define user_initialize replay_reference_pattern_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

typedef struct {
    ProDimension dimension[8];
    double before[8],after[8];
    int count,seed,header;
} ReferencePatternDimensions;

static ProError reference_pattern_dimension(ProDimension *dimension,ProError status,ProAppData data){
    if(status)return status;
    ReferencePatternDimensions *dims=data;
    ProDimensiontype type;
    double value;
    ProFeature owner;
    ProError error=ProDimensionTypeGet(dimension,&type);
    if(!error)error=ProDimensionValueGet(dimension,&value);
    if(!error)error=ProDimensionOwnerfeatureGet(dimension,&owner);
    if(error==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
    if(error)return error;
    if(owner.id!=dims->seed&&owner.id!=dims->header)return PRO_TK_NO_ERROR;
    if((type==PRODIMTYPE_DIAMETER&&fabs(value-4)<1e-7)||
       (type==PRODIMTYPE_RADIUS&&fabs(value-2)<1e-7)){
        if(dims->count==_countof(dims->dimension))return PRO_TK_OUT_OF_RANGE;
        const int i=dims->count++;
        dims->dimension[i]=*dimension;
        dims->before[i]=value;
        dims->after[i]=value*1.125;
    }
    return PRO_TK_NO_ERROR;
}

typedef struct {
    double diameter;
    int pattern[5][2],side[2][2];
} ReferencePatternHoles;

static ProError reference_pattern_surface(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    ReferencePatternHoles *holes=data;
    ProSrftype type;
    ProError error=ProSurfaceTypeGet(surface,&type);
    if(error||type!=PRO_SRF_CYL)return error;
    ProGeomitemdata *raw=NULL;
    error=ProSurfaceDataGet(surface,&raw);
    if(error)return error;
    const ProCylinderdata *shape=&raw->data.p_surface_data->srf_shape.cylinder;
    const double x=shape->origin[0],y=shape->origin[1],d=2*fabs(shape->radius);
    int row=fabs(y-145)<0.001?0:fabs(y-445)<0.001?1:-1;
    int column=(int)lround(x/250)+2;
    if(row>=0&&column>=0&&column<5&&fabs(x-(column-2)*250)<0.001){
        if(fabs(d-holes->diameter)>0.001)error=PRO_TK_INCOMPLETE;
        else holes->pattern[column][row]++;
    }else{
        row=fabs(y-195)<0.001?0:fabs(y-395)<0.001?1:-1;
        if(row<0||fabs(fabs(x)-589.5)>0.001||fabs(d-4)>0.001)error=PRO_TK_INCOMPLETE;
        else holes->side[x>0][row]++;
    }
    if(fabs(fabs(shape->e3[2])-1)>1e-7)error=PRO_TK_INCOMPLETE;
    ProGeomitemdataFree(&raw);
    return error;
}

static ProError reference_pattern_holes(ProMdl model,double diameter,FILE *log){
    ReferencePatternHoles holes={0};
    holes.diameter=diameter;
    ProSolidBody body;
    ProError error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodySurfaceVisit(&body,reference_pattern_surface,&holes);
    int patterned=0,side=0;
    for(int i=0;i<5;i++)for(int j=0;j<2;j++)patterned+=holes.pattern[i][j]>0;
    for(int i=0;i<2;i++)for(int j=0;j<2;j++)side+=holes.side[i][j]>0;
    if(!error&&(patterned!=10||side!=4))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"HOLES DIAMETER=%.8g PATTERNED=%d SIDE=%d ERROR=%d\n",diameter,patterned,side,error);
    return error;
}

static ProError reference_pattern_trial(ProMdl model,FILE *log){
    ReplayOrder order={0};
    order.generated_datums=1;
    order.read_candidate=1;
    ProError error=dv_no_external_dependencies(model);
    if(!error)error=part_regenerate(model);
    if(!error)error=ProSolidFeatVisit((ProSolid)model,replay_read_order,NULL,&order);
    ProError cleanup=replay_pattern_source_close(&order.patterns);
    if(!error)error=cleanup;
    if(!error&&(order.count!=3||order.step[0].type!=PRO_FEAT_DATUM_SURF||
       order.step[1].type!=PRO_FEAT_CUT_SMT||order.step[1].pattern_type!=PRO_GENPAT_REF_DRIVEN||
       order.step[1].members!=5||order.step[2].type!=PRO_FEAT_CUT_SMT))error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<order.count;i++)if(order.step[i].sections!=1)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"TREE STEPS=%d KIND=%d INSTANCES=%d ERROR=%d\n",
        order.count,order.step[1].pattern_type,order.step[1].members,error);
    if(!error)error=reference_pattern_holes(model,4,log);
    ProFeature seed;
    ReferencePatternDimensions dims={0};
    if(!error)error=ProFeatureInit((ProSolid)model,order.step[1].id,&seed);
    ProFeature header;
    ProPattern pattern;
    if(!error)error=ProFeaturePatternGet(&seed,PRO_FEAT_PATTERN,&pattern);
    if(!error)error=ProPatternHeaderGet(&pattern,&header);
    if(!error){
        dims.seed=seed.id;
        dims.header=header.id;
        error=ProSolidDimensionVisit((ProSolid)model,PRO_B_FALSE,reference_pattern_dimension,NULL,&dims);
    }
    if(!error&&dims.count!=1&&dims.count!=2)error=PRO_TK_INCOMPLETE;
    int changed=0;
    for(int i=0;!error&&i<dims.count;i++){
        error=ProDimensionValueSet(&dims.dimension[i],dims.after[i]);
        if(!error)changed++;
    }
    fwprintf(log,L"SEED DRIVERS=%d CHANGED=%d ERROR=%d\n",dims.count,changed,error);
    if(!error)error=part_regenerate(model);
    if(!error)error=reference_pattern_holes(model,4.5,log);
    ProError restored=PRO_TK_NO_ERROR;
    for(int i=0;i<changed;i++){
        ProError restore=ProDimensionValueSet(&dims.dimension[i],dims.before[i]);
        if(!restored)restored=restore;
    }
    if(!restored&&changed)restored=part_regenerate(model);
    if(!restored&&changed)restored=reference_pattern_holes(model,4,log);
    if(!error)error=restored;
    return error;
}

static int reference_pattern_case(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"reference-pattern-acceptance.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProPath part,assembly,output=L"";
    ProMdl source=NULL,context=NULL,rebuilt=NULL;
    wchar_t text[16000]=L"";
    PartText out={text,_countof(text),0};
    ProError error;
    if(GetEnvironmentVariableW(L"BWTL_REFERENCE_PATTERN_RESULT",output,_countof(output))){
        error=ProMdlnameInit(L"M26Z0100000-V01",PRO_MDL_ASSEMBLY,&context)==PRO_TK_E_NOT_FOUND?
            PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
    }else{
        error=GetEnvironmentVariableW(L"BWTL_EXPANSION_INPUT",part,_countof(part))&&
            GetEnvironmentVariableW(L"BWTL_EXPANSION_ASSEMBLY",assembly,_countof(assembly))?
            ProMdlFiletypeLoad(part,PRO_MDLFILE_PART,PRO_B_FALSE,&source):PRO_TK_BAD_INPUTS;
        int count=0;
        if(!error&&replay_source_dependencies(source,&count)!=PRO_TK_BAD_CONTEXT)error=PRO_TK_INCOMPLETE;
        if(!error)error=ProMdlFiletypeLoad(assembly,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,&context);
        if(!error)error=replay_source_dependencies(source,&count);
        if(!error&&count!=32)error=PRO_TK_INCOMPLETE;
        fwprintf(log,L"ACTIVE REFERENCES=%d ERROR=%d\n",count,error);
        fflush(log);
        if(!error)error=imitation_run_mode(source,output,0,1,&out);
        fwprintf(log,L"REPLAY=%d PATH=%ls\n%ls\n",error,output,text);
        fflush(log);
    }
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&rebuilt);
    if(!error)error=reference_pattern_trial(rebuilt,log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=replay_reference_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReferencePatternCase",reference_pattern_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReferencePatternCase\x0060;");
}

