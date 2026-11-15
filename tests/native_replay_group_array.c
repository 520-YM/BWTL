#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static ProError dump_group_feature(ProFeature *feature,ProError status,ProAppData data)
{
    if(status)return status;
    ProFeattype type=0;
    ProGroupStatus group=0;
    ProGrppatternStatus group_pattern=0;
    ProPatternStatus pattern=0;
    ProName name=L"";
    ProGroup owner={0};
    ProFeatureTypeGet(feature,&type);
    ProFeatureGroupStatusGet(feature,&group);
    ProFeatureGrppatternStatusGet(feature,&group_pattern);
    ProFeaturePatternStatusGet(feature,&pattern);
    ProModelitemNameGet(feature,name);
    ProFeatureGroupGet(feature,&owner);
    fwprintf(data,L"TREE %d %ls TYPE=%d GROUP=%d GROUP_PAT=%d PAT=%d OWNER=%d\n",
        feature->id,name,type,group,group_pattern,pattern,owner.id);
    return PRO_TK_NO_ERROR;
}

static ProError group_array_fixture(ProMdl *source,ProMdlName name,int circular,FILE *log)
{
    Fixture geometry;
    ProError error=fixture(source,name,&geometry);
    wchar_t protocol[512];
    const wchar_t *axis_steps[]={
        L"CREO_OP DATUM_PLANE %ls ARRAY_X PLANE:RIGHT 40",
        L"CREO_OP DATUM_PLANE %ls ARRAY_Y PLANE:TOP 30",
        L"CREO_OP DATUM_AXIS %ls ARRAY_AXIS PLANES PLANE:ARRAY_X PLANE:ARRAY_Y"
    };
    for(int i=0;circular&&!error&&i<_countof(axis_steps);i++){
        swprintf_s(protocol,_countof(protocol),axis_steps[i],name);
        error=expansion_build(*source,protocol,log);
    }
    for(int i=0;!error&&i<2;i++){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls SEED_%lc SURFACE:%d SURFACE:%d %d SURFACE:%d %d 6 THROUGH 0",
            name,L'A'+i,geometry.top,geometry.x,15+i*10,geometry.y,12+i*18);
        error=expansion_build(*source,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP REUSE_GROUP %ls PATTERN_GROUP SEED_A SEED_B",name);
        error=expansion_build(*source,protocol,log);
    }
    ProFeature group;
    if(!error)error=ProModelitemByNameInit(*source,PRO_FEATURE,L"PATTERN_GROUP",&group);
    FdRef direction={0};
    FdTxn tx={0};
    if(!error)error=fd_ref_resolve(*source,circular?L"AXIS:ARRAY_AXIS":L"PLANE:RIGHT",circular?PRO_AXIS:PRO_SURFACE,&direction);
    if(!error)error=ProElementAlloc(PRO_E_PATTERN_ROOT,&tx.root);
    if(!error)error=rf_pattern_tree(&tx,&direction,circular,3,circular?90:20,1);
    if(!error)error=ProPatternCreate(&group,PRO_GROUP_PATTERN,tx.root);
    if(!error)error=part_regenerate(*source);
    if(tx.root)ProElementFree(&tx.root);
    for(int i=0;i<tx.ref_count;i++)ProReferenceFree(tx.refs[i]);
    fd_ref_free(&direction);
    fwprintf(log,L"GROUP FIXTURE=%d\n",error);
    if(!error)error=ProSolidFeatVisit((ProSolid)*source,dump_group_feature,NULL,log);
    if(!error){
        ProPattern pattern;
        ProFeature leader,header,*members=NULL;
        ProElement tree=NULL;
        int count=0;
        error=ProFeaturePatternGet(&group,PRO_GROUP_PATTERN,&pattern);
        if(!error)error=ProPatternLeaderGet(&pattern,&leader);
        if(!error)error=ProPatternHeaderGet(&pattern,&header);
        if(!error)error=ProPatternMembersGet(&pattern,&members);
        if(!error)error=ProArraySizeGet((ProArray)members,&count);
        if(!error){
            fwprintf(log,L"LEADER=%d HEADER=%d MEMBERS=%d\n",leader.id,header.id,count);
            for(int i=0;i<count;i++)dump_group_feature(&members[i],0,log);
            error=ProPatternElemtreeCreate(&leader,PRO_GROUP_PATTERN,&tree);
        }
        fwprintf(log,L"GROUP TREE=%d\n",error);
        if(members)ProArrayFree((ProArray *)&members);
        if(tree)ProElementFree(&tree);
    }
    return error;
}

static ProError group_hole_surface(ProGeomitem *item,ProError status,ProAppData data)
{
    ProSurface surface;
    if(status)return status;
    ProError error=ProGeomitemToSurface(item,&surface);
    return error?error:imitation_cylinder_diameter(surface,0,data);
}

static ProError group_array_diameters(ProFeature *instances,int count,int edited)
{
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<count;i++){
        ProFeature *members=NULL;
        int size=0,holes=0;
        error=ProGroupFeaturesCollect((ProGroup *)&instances[i],&members);
        if(!error)error=ProArraySizeGet((ProArray)members,&size);
        for(int j=0;!error&&j<size;j++){
            if(members[j].id==instances[i].id)continue;
            ImitationDiameterRead geometry={1,6,6.5,0,0};
            error=ProFeatureGeomitemVisit(&members[j],PRO_SURFACE,group_hole_surface,NULL,&geometry);
            if(!error&&((edited&&!holes)?
                (geometry.old_count!=0||geometry.new_count<1):
                (geometry.old_count<1||geometry.new_count!=0)))error=PRO_TK_INCOMPLETE;
            holes++;
        }
        if(members)ProArrayFree((ProArray *)&members);
    }
    return error;
}

static ProError group_array_verify(const wchar_t *output,FILE *log)
{
    ProMdl result=NULL;
    ProFeature group,*instances=NULL;
    ProPattern pattern;
    int count=0;
    ProError error=ProMdlFiletypeLoad((wchar_t *)output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=ProModelitemByNameInit(result,PRO_FEATURE,L"PATTERN_GROUP",&group);
    if(!error)error=ProFeaturePatternGet(&group,PRO_GROUP_PATTERN,&pattern);
    if(!error)error=ProPatternMembersGet(&pattern,&instances);
    if(!error)error=ProArraySizeGet((ProArray)instances,&count);
    if(!error&&count!=3)error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<count;i++){
        ProFeature *members=NULL;
        int size=0,holes=0;
        error=ProGroupFeaturesCollect((ProGroup *)&instances[i],&members);
        if(!error)error=ProArraySizeGet((ProArray)members,&size);
        for(int j=0;!error&&j<size;j++){
            if(members[j].id==instances[i].id)continue;
            ProFeattype type;
            error=ProFeatureTypeGet(&members[j],&type);
            if(!error&&type!=PRO_FEAT_HOLE)error=PRO_TK_INCOMPLETE;
            if(!error&&i==0){
                ProName name=L"";
                error=ProModelitemNameGet(&members[j],name);
                if(!error&&wcscmp(name,holes==0?L"SEED_A":L"SEED_B"))error=PRO_TK_INCOMPLETE;
            }
            holes++;
        }
        if(!error&&holes!=2)error=PRO_TK_INCOMPLETE;
        if(members)ProArrayFree((ProArray *)&members);
    }
    fwprintf(log,L"GROUP MEMBERS AND ORDER=%d\n",error);
    ProFeature seed;
    ExpansionDiameter diameter={0};
    ProMassProperty before={0},edited={0},restored={0};
    if(!error)error=ProModelitemByNameInit(result,PRO_FEATURE,L"SEED_A",&seed);
    if(!error)error=ProFeatureDimensionVisit(&seed,expansion_diameter,NULL,&diameter);
    if(!error&&diameter.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=qa_geometry_mass(result,&before);
    if(!error)error=ProDimensionValueSet(&diameter.dimension,6.5);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&edited);
    const double expected=60*AS_PI*(3.25*3.25-3*3);
    const double volume_tolerance=0.01;
    if(!error)error=group_array_diameters(instances,count,1);
    fwprintf(log,L"GROUP VOLUME BEFORE=%.9f AFTER=%.9f EXPECTED_CHANGE=%.9f DIMENSIONS=%d ERROR=%d\n",
        before.volume,edited.volume,expected,diameter.count,error);
    if(!error&&fabs(before.volume-edited.volume-expected)>volume_tolerance)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&diameter.dimension,6);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&restored);
    if(!error)error=group_array_diameters(instances,count,0);
    if(!error&&fabs(restored.volume-before.volume)>volume_tolerance)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"GROUP SEED EDIT AND RESTORE=%d\n",error);
    if(instances)ProArrayFree((ProArray *)&instances);
    if(result)ProMdlErase(result);
    return error;
}

static ProError group_array_replay(int circular,FILE *log)
{
    ProMdl source=NULL;
    ProMdlName name;
    ProPath output=L"";
    wchar_t report[20000]=L"";
    PartText out={report,_countof(report),0};
    ProError error=group_array_fixture(&source,name,circular,log);
    if(!error)error=ProMdlSave(source);
    if(!error)replay_preview(source,&out);
    if(!error&&!wcsstr(report,L"待接入 0。"))error=PRO_TK_INCOMPLETE;
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"GROUP REPLAY=%d\n%ls\n",error,report);
    if(!error)error=group_array_verify(output,log);
    return error;
}

static int group_array_case(void)
{
    FILE *log=NULL;
    _wfopen_s(&log,L"replay-group-array.log",L"w, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 1;}
    model_background_mode=1;
    ProError linear=group_array_replay(0,log);
    ProError circular=group_array_replay(1,log);
    fwprintf(log,L"PASS=%d LINEAR=%d CIRCULAR=%d\n",!linear&&!circular,linear,circular);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80])
{
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLGroupArray",group_array_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLGroupArray\x0060;");
}
