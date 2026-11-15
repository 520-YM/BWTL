#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError trial_material_read(ProMdl model,wchar_t *name,ProMaterial *material)
{
    ProPath directory,original,working;
    ProError error=ProDirectoryCurrentGet(directory);
    if(!error){
        swprintf_s(original,_countof(original),L"C:\\Program Files\\PTC\\Creo 10.0.0.0\\Common Files\\creo_standards\\material_database\\%ls.mtl",name);
        swprintf_s(working,_countof(working),L"%ls\\%ls.mtl",directory,name);
        if(!CopyFileW(original,working,FALSE))error=PRO_TK_CANT_WRITE;
    }
    if(!error)error=ProMaterialfileRead((ProSolid)model,name);
    if(!error){
        material->part=(ProSolid)model;
        wcscpy_s(material->matl_name,_countof(material->matl_name),name);
    }
    return error;
}

static ProError trial_material_units(ProMdl model)
{
    ProUnitsystem current;
    ProUnititem length,mass;
    ProUnitsystem *systems=NULL;
    int count=0,chosen=-1;
    ProError error=ProMdlPrincipalunitsystemGet(model,&current);
    if(!error)error=ProUnitsystemUnitGet(&current,PRO_UNITTYPE_LENGTH,&length);
    if(!error)error=ProUnitsystemUnitGet(&current,PRO_UNITTYPE_MASS,&mass);
    if(!error)error=ProMdlUnitsystemsCollect(model,&systems);
    if(!error)error=ProArraySizeGet((ProArray)systems,&count);
    for(int i=0;!error&&i<count&&chosen<0;i++){
        ProUnititem other_length,other_mass;
        ProUnitConversion linear,weight;
        if(!ProUnitsystemUnitGet(&systems[i],PRO_UNITTYPE_LENGTH,&other_length)&&
           !ProUnitsystemUnitGet(&systems[i],PRO_UNITTYPE_MASS,&other_mass)&&
           !ProUnitConversionCalculate(&length,&other_length,&linear)&&
           !ProUnitConversionCalculate(&mass,&other_mass,&weight)&&
           fabs(linear.scale-1)<1e-10&&fabs(weight.scale-1)>1e-5)chosen=i;
    }
    if(!error&&chosen<0)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=ProMdlPrincipalunitsystemSet(model,&systems[chosen],PRO_UNITCONVERT_SAME_SIZE,PRO_B_FALSE,PRO_VALUE_UNUSED);
    if(systems)ProArrayFree((ProArray *)&systems);
    return error;
}

static ProError material_trial(FILE *log,int distinct)
{
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&source,name,&geometry);
    if(!error&&distinct)error=trial_material_units(source);
    ProMaterial part={0},body={0};
    if(!error)error=trial_material_read(source,L"metal_steel_low_carbon",&part);
    if(!error)error=ProMaterialCurrentSet(&part);
    if(!error&&distinct){
        ProSolidBody single;
        error=qa_single_body(source,&single);
        if(!error)error=trial_material_read(source,L"metal_aluminium_6061",&body);
        if(!error)error=ProSolidBodyMaterialSet(&single,&body);
    }
    if(!error)error=part_regenerate(source);
    fwprintf(log,L"MATERIAL SETUP DISTINCT=%d ERROR=%d\n",distinct,error);
    ProPath delivered=L"";
    wchar_t report[20000]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,delivered,0,1,&out);
    fwprintf(log,L"MATERIAL REBUILD DISTINCT=%d ERROR=%d\n%ls\n",distinct,error,report);
    if(!error)error=ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    ProMaterial got_part={0},got_body={0};
    if(!error)error=imitation_material_read(result,&got_part,&got_body);
    if(!error&&(_wcsicmp(got_part.matl_name,part.matl_name)||
       _wcsicmp(got_body.matl_name,distinct?body.matl_name:part.matl_name)))error=PRO_TK_INCOMPLETE;
    if(!error)error=imitation_material_mass(source,result);
    if(!error&&distinct){
        ProUnitsystem from,to;
        ProUnititem a,b;
        ProUnitConversion conversion={0};
        error=ProMdlPrincipalunitsystemGet(source,&from);
        if(!error)error=ProMdlPrincipalunitsystemGet(result,&to);
        if(!error)error=ProUnitsystemUnitGet(&from,PRO_UNITTYPE_MASS,&a);
        if(!error)error=ProUnitsystemUnitGet(&to,PRO_UNITTYPE_MASS,&b);
        if(!error)error=ProUnitConversionCalculate(&a,&b,&conversion);
        if(!error&&fabs(conversion.scale-1)<1e-5)error=PRO_TK_INCOMPLETE;
        fwprintf(log,L"MASS UNIT CONVERSION %.10g ERROR=%d\n",conversion.scale,error);
    }
    fwprintf(log,L"MATERIAL READBACK DISTINCT=%d ERROR=%d\n",distinct,error);
    return error;
}

static int material_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-material.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError same=material_trial(log,0);
    const ProError distinct=material_trial(log,1);
    fwprintf(log,L"SAME=%d DISTINCT=%d PASS=%d\n",same,distinct,!same&&!distinct);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayMaterial",material_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayMaterial\x0060;");
}
