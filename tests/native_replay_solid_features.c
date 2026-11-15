#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError solid_feature_profile(ProMdl model,const wchar_t *name,
                                      const ProVector first,const ProVector second,double local[4])
{
    FdSketch sketch={0};
    ProMatrix frame;
    ProError error=fd_sketch_open(model,name,&sketch,0);
    if(!error)error=ProSectionLocationGet(sketch.section,frame);
    for(int i=0;!error&&i<2;i++){
        local[i]=local[i+2]=0;
        for(int j=0;j<3;j++){
            local[i]+=(first[j]-frame[3][j])*frame[i][j];
            local[i+2]+=(second[j]-frame[3][j])*frame[i][j];
        }
    }
    fd_sketch_free(&sketch);
    return error;
}

static ProError solid_feature_rib(ProMdl model,const wchar_t *name,FILE *log)
{
    wchar_t protocol[512];
    swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_PLANE %ls MID_Y PLANE:TOP 30",name);
    ProError error=expansion_build(model,protocol,log);
    const ProVector pillar_a={2,10,0},pillar_b={12,50,0};
    const ProVector rib_a={12,30,44},rib_b={55,30,20};
    double local[4];
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SKETCH_CREATE %ls PILLAR_FRAME PLANE:FRONT PLANE:RIGHT LINE 0 0 2 0",name);
        error=expansion_build(model,protocol,log);
    }
    if(!error)error=solid_feature_profile(model,L"PILLAR_FRAME",pillar_a,pillar_b,local);
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SKETCH_CREATE %ls PILLAR_SK PLANE:FRONT PLANE:RIGHT RECT %.9g %.9g %.9g %.9g",name,
            fmin(local[0],local[2]),fmin(local[1],local[3]),fabs(local[0]-local[2]),fabs(local[1]-local[3]));
        error=expansion_build(model,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SOLID_EXTRUDE %ls PILLAR PILLAR_SK ADD BLIND 50 NEG",name);
        error=expansion_build(model,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SKETCH_CREATE %ls RIB_FRAME PLANE:MID_Y PLANE:RIGHT LINE 0 0 2 0",name);
        error=expansion_build(model,protocol,log);
    }
    if(!error)error=solid_feature_profile(model,L"RIB_FRAME",rib_a,rib_b,local);
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SKETCH_CREATE %ls RIB_LINE PLANE:MID_Y PLANE:RIGHT LINE %.9g %.9g %.9g %.9g",
            name,local[0],local[1],local[2],local[3]);
        error=expansion_build(model,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SOLID_RIB %ls RIB_REPLAY RIB_LINE 4 NEG SYM",name);
        error=expansion_build(model,protocol,log);
    }
    return error;
}

static ProError solid_shell_local(ProMdl model,int surface)
{
    ProFeature feature;
    ProElement tree=NULL,list=NULL,entry=NULL,*entries=NULL;
    ProFeatureCreateOptions *options=NULL;
    ProErrorlist errors={0};
    FdRef face={0};
    FdTxn refs={0};
    wchar_t token[48];
    swprintf_s(token,_countof(token),L"SURFACE:%d",surface);
    ProError error=ux_named_feature(model,L"SHELL_REPLAY",&feature);
    if(!error)error=fd_ref_resolve(model,token,PRO_SURFACE,&face);
    if(!error)error=ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error){
        error=mc_tree_element(tree,(ProElemId[]){PRO_E_ST_SHELL_LOCL_LIST},1,&list);
        if(error==PRO_TK_E_NOT_FOUND)error=model_element(tree,PRO_E_ST_SHELL_LOCL_LIST,0,0,&list);
    }
    if(!error)error=ProElementAlloc(PRO_E_ST_SHELL_LOCL_CMPD,&entry);
    if(!error)error=fd_ref_element(&refs,entry,PRO_E_ST_SHELL_SPEC_SRF,&face);
    if(!error)error=model_element(entry,PRO_E_ST_SHELL_SPEC_THCK,2,3,NULL);
    if(!error)error=ProArrayAlloc(1,sizeof(*entries),1,(ProArray *)&entries);
    if(!error){
        entries[0]=entry;
        error=ProElementArraySet(list,NULL,entries);
        if(!error)entry=NULL;
    }
    if(!error)error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options);
    if(!error){
        options[0]=PRO_FEAT_CR_NO_OPTS;
        error=ProFeatureWithoptionsRedefine(NULL,&feature,tree,options,
            PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors);
    }
    if(options)ProArrayFree((ProArray *)&options);
    if(entries)ProArrayFree((ProArray *)&entries);
    if(entry)ProElementFree(&entry);
    if(tree)ProFeatureElemtreeFree(&feature,tree);
    fd_cleanup(&refs,error);
    fd_ref_free(&face);
    return error?error:part_regenerate(model);
}

static ProError solid_feature_case(FILE *log,int mode)
{
    ProMdl model=NULL,result=NULL;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&model,name,&geometry);
    wchar_t protocol[512],report[24000]=L"";
    const int rib=mode==1;
    if(!error&&rib)error=solid_feature_rib(model,name,log);
    if(!error&&!rib){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SOLID_SHELL %ls SHELL_REPLAY SURFACE:%d 2",name,geometry.top);
        error=expansion_build(model,protocol,log);
    }
    if(!error&&mode==2)error=solid_shell_local(model,geometry.xfar);
    fwprintf(log,L"SOLID SETUP MODE=%d ERROR=%d\n",mode,error);
    fflush(log);
    PartText out={report,_countof(report),0};
    ProPath delivered=L"";
    if(!error)error=imitation_run_mode(model,delivered,0,1,&out);
    fwprintf(log,L"REPLAY MODE=%d ERROR=%d\n%ls\n",mode,error,report);
    if(!error)error=ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    ProFeature feature;
    if(!error)error=ux_named_feature(result,rib?L"RIB_REPLAY":L"SHELL_REPLAY",&feature);
    ReplayDimensionMatch edit={0};
    edit.value=rib?4:mode==2?3:2;
    edit.type=PRODIMTYPE_LINEAR;
    if(!error)error=ProFeatureDimensionVisit(&feature,replay_match_dimension,NULL,&edit);
    if(!error&&edit.count!=1)error=PRO_TK_INCOMPLETE;
    ProMassProperty before,after,restored;
    if(!error)error=qa_geometry_mass(result,&before);
    if(!error)error=ProDimensionValueSet(&edit.target,edit.value+0.5);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&after);
    if(!error&&after.volume<=before.volume+0.001)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&edit.target,edit.value);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&restored);
    if(!error&&!part_close(before.volume,restored.volume))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"DIMENSION EDIT MODE=%d ERROR=%d MATCHES=%d\n",mode,error,edit.count);
    return error;
}

static int solid_replay_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-solid-features.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError shell=solid_feature_case(log,0);
    const ProError rib=solid_feature_case(log,1);
    const ProError local=solid_feature_case(log,2);
    fwprintf(log,L"SHELL=%d RIB=%d LOCAL=%d PASS=%d\n",shell,rib,local,!shell&&!rib&&!local);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplaySolidFeatures",solid_replay_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplaySolidFeatures\x0060;");
}
