#define user_initialize replay_external_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>
#include <float.h>

typedef struct {
    ProDimension dimensions[48];
    int count;
} ExternalWindowDimensions;

static ProError external_window_dimensions(ProDimension *dimension,ProError status,ProAppData data){
    if(status)return status;
    ExternalWindowDimensions *found=data;
    ProDimensiontype type;
    double value;
    if(ProDimensionTypeGet(dimension,&type)||type!=PRODIMTYPE_LINEAR||
       ProDimensionValueGet(dimension,&value)||value<1)return PRO_TK_NO_ERROR;
    if(found->count==_countof(found->dimensions))return PRO_TK_OUT_OF_RANGE;
    found->dimensions[found->count++]=*dimension;
    return PRO_TK_NO_ERROR;
}

static ProError external_window_size(ProFeature *feature,double size[2]){
    FdSketch sketch={0};
    sketch.feature=*feature;
    ProError error=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&sketch.tree);
    if(!error)error=fd_section_from_tree(sketch.tree,&sketch.section);
    if(!error&&!sketch.section)error=PRO_TK_EMPTY;
    int *ids=NULL,count=0,lines=0;
    double min[2]={DBL_MAX,DBL_MAX},max[2]={-DBL_MAX,-DBL_MAX};
    if(!error)error=ProSectionEntityIdsGet(sketch.section,&ids,&count);
    for(int i=0;!error&&i<count;i++){
        ProBoolean construction=PRO_B_FALSE,projection=PRO_B_FALSE;
        error=ProSectionEntityIsConstruction(sketch.section,ids[i],&construction);
        if(!error)error=ProSectionEntityIsProjection(sketch.section,ids[i],&projection);
        if(error||construction||projection)continue;
        Pro2dEntdef *entity=NULL;
        error=ProSectionEntityGet(sketch.section,ids[i],&entity);
        if(error==PRO_TK_INVALID_TYPE){error=PRO_TK_NO_ERROR;continue;}
        if(error)continue;
        if(entity->type!=PRO_2D_LINE){error=PRO_TK_UNSUPPORTED;continue;}
        const Pro2dLinedef *line=(const Pro2dLinedef *)entity;
        for(int j=0;j<2;j++){
            min[j]=fmin(min[j],fmin(line->end1[j],line->end2[j]));
            max[j]=fmax(max[j],fmax(line->end1[j],line->end2[j]));
        }
        lines++;
    }
    if(!error&&lines!=4)error=PRO_TK_INCOMPLETE;
    if(!error)for(int i=0;i<2;i++)size[i]=max[i]-min[i];
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&sketch);
    return error;
}

static ProError external_window_trial(ProMdl source,const wchar_t *path,FILE *log,PartText *out){
    ProMdl result=NULL;
    ProError error=ProMdlFiletypeLoad((wchar_t *)path,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    ReplayOrder order={0};
    order.generated_datums=1;
    if(!error)error=ProSolidFeatVisit((ProSolid)result,replay_read_order,NULL,&order);
    ProError close=replay_pattern_source_close(&order.patterns);
    if(!error)error=close;
    if(!error&&order.count!=11)error=PRO_TK_INCOMPLETE;
    ProFeature window;
    if(!error)error=ProFeatureInit((ProSolid)result,order.step[10].id,&window);
    double before[2],changed[2],restored[2];
    ProMassProperty old_mass,new_mass;
    ExternalWindowDimensions dims={0};
    if(!error)error=external_window_size(&window,before);
    if(!error)error=qa_geometry_mass(result,&old_mass);
    if(!error)error=ProFeatureDimensionVisit(&window,external_window_dimensions,NULL,&dims);
    int verified=0;
    for(int i=0;!error&&!verified&&i<dims.count;i++){
        ProDimension *dimension=&dims.dimensions[i];
        double old;
        if(ProDimensionValueGet(dimension,&old)||ProDimensionValueSet(dimension,old+0.5))continue;
        ProError trial=part_regenerate(result);
        if(!trial)trial=external_window_size(&window,changed);
        if(!trial)trial=qa_geometry_mass(result,&new_mass);
        verified=!trial&&(fabs(before[0]-changed[0])>0.01||fabs(before[1]-changed[1])>0.01)&&
            fabs(new_mass.volume-old_mass.volume)>0.01;
        error=ProDimensionValueSet(dimension,old);
        if(!error)error=part_regenerate(result);
        if(!error)error=external_window_size(&window,restored);
        if(!error&&(fabs(before[0]-restored[0])>0.001||fabs(before[1]-restored[1])>0.001))
            error=PRO_TK_INCOMPLETE;
        if(verified)fwprintf(log,L"WINDOW %.8g x %.8g -> %.8g x %.8g -> %.8g x %.8g mm\n",
            before[0],before[1],changed[0],changed[1],restored[0],restored[1]);
    }
    if(!error&&!verified)error=PRO_TK_INCOMPLETE;
    ImitationShapeMetrics original;
    if(!error)error=imitation_shape_read(source,&original);
    if(!error)error=imitation_shape_verify_mode(source,result,&original,0,out);
    if(result){
        ProError cleanup=ProMdlErase(result);
        if(!error)error=cleanup;
    }
    return error;
}


static ProError external_standalone(ProPath path,FILE *log){
    ProMdl model=NULL,upstream=NULL;
    ProError error=ProMdlnameInit(L"M26Z0141001-V01",PRO_MDL_ASSEMBLY,&upstream)==PRO_TK_E_NOT_FOUND?
        PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
    if(!error)error=ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&model);
    if(!error)error=dv_no_external_dependencies(model);
    if(!error)error=part_regenerate(model);
    if(!error&&ProMdlnameInit(L"M26Z0141001-V01",PRO_MDL_ASSEMBLY,&upstream)!=PRO_TK_E_NOT_FOUND)
        error=PRO_TK_BAD_CONTEXT;
    ReplayOrder order={0};
    order.generated_datums=1;
    if(!error)error=ProSolidFeatVisit((ProSolid)model,replay_read_order,NULL,&order);
    ProError cleanup=replay_pattern_source_close(&order.patterns);
    if(!error)error=cleanup;
    if(!error&&(order.count!=11||!order.step[0].conversion||order.step[10].type!=PRO_FEAT_CUT_SMT))
        error=PRO_TK_INCOMPLETE;
    ProFeature window;
    double size[2]={0};
    if(!error)error=ProFeatureInit((ProSolid)model,order.step[10].id,&window);
    if(!error)error=external_window_size(&window,size);
    double mm=0,thickness=0;
    if(!error)error=fab_thickness(model,&mm,&thickness);
    if(!error&&(fabs(thickness-1)>0.001||fabs(size[0]*mm-466)>0.001||fabs(size[1]*mm-86)>0.001))
        error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"STANDALONE=%d STEPS=%d THICKNESS=%.8g WINDOW=%.8g x %.8g\n",
        error,order.count,thickness,size[0]*mm,size[1]*mm);
    return error;
}


static int external_case(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"external-acceptance.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProPath part,assembly,output=L"";
    if(GetEnvironmentVariableW(L"BWTL_EXTERNAL_RESULT",part,_countof(part))){
        external_standalone(part,log);
        fclose(log);
        ProEngineerEnd();
        return 0;
    }
    ProMdl source=NULL,context=NULL;
    wchar_t text[20000]=L"";
    PartText out={text,_countof(text),0};
    ProError error=GetEnvironmentVariableW(L"BWTL_EXTERNAL_PART",part,_countof(part))&&
        GetEnvironmentVariableW(L"BWTL_EXTERNAL_ASSEMBLY",assembly,_countof(assembly))?
        ProMdlFiletypeLoad(part,PRO_MDLFILE_PART,PRO_B_FALSE,&source):PRO_TK_BAD_INPUTS;
    if(!error){
        int count=0;
        ProError preflight=replay_source_dependencies(source,&count);
        fwprintf(log,L"FROZEN=%d\n",preflight);
        if(preflight!=PRO_TK_BAD_CONTEXT)error=PRO_TK_INCOMPLETE;
    }
    if(!error)error=ProMdlFiletypeLoad(assembly,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,&context);
    int count=0;
    if(!error)error=replay_source_dependencies(source,&count);
    fwprintf(log,L"ACTIVE=%d COUNT=%d\n",error,count);
    fflush(log);
    if(!error&&count!=12)error=PRO_TK_INCOMPLETE;
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"REPLAY=%d\n%ls\n",error,text);
    if(!error){
        text[0]=0;
        out=(PartText){text,_countof(text),0};
        error=external_window_trial(source,output,log,&out);
        fwprintf(log,L"WINDOW_EDIT=%d\n%ls\n",error,text);
    }
    fwprintf(log,L"PASS=%d\n",!error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=replay_external_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLExternalCase",external_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLExternalCase\x0060;");
}
