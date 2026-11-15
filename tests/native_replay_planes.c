#define user_initialize expansion_pattern_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>


static ProError expansion_build(ProMdl model,const wchar_t *protocol,FILE *log){
    PartCommand command={0};
    wchar_t message[PART_TEXT_MAX]=L"";
    ProError error=part_command_parse(protocol,&command)==1?
        foundation_execute(&command,model,message,_countof(message)):PRO_TK_BAD_INPUTS;
    fwprintf(log,L"%ls → Creo %d\n%ls\n",protocol,error,message);
    fflush(log);
    return error;
}


typedef struct {int id;ProVector xyz;} Contact;
static ProError pick_cylinder(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    Contact *contact=data;
    ProSrftype kind;
    ProError error=ProSurfaceTypeGet(surface,&kind);
    if(error||kind!=PRO_SRF_CYL)return error;
    ProGeomitemdata *raw=NULL;
    error=ProSurfaceDataGet(surface,&raw);
    if(!error){
        ProSurfacedata *geom=raw->data.p_surface_data;
        ProUvParam uv={(geom->uv_min[0]+geom->uv_max[0])*0.5,(geom->uv_min[1]+geom->uv_max[1])*0.5};
        error=ProSurfaceXyzdataEval(surface,uv,contact->xyz,NULL,NULL,NULL);
        if(!error)error=ProSurfaceIdGet(surface,&contact->id);
    }
    if(raw)ProGeomitemdataFree(&raw);
    return error?error:PRO_TK_E_FOUND;
}

typedef struct {double old_value;ProDimension dim;int count;} PlaneTrialDimension;
static ProError plane_trial_dimension(ProDimension *dim,ProError status,ProAppData data){
    if(status)return status;
    PlaneTrialDimension *trial=data;
    double value;
    ProError error=ProDimensionValueGet(dim,&value);
    if(!error&&fabs(value-trial->old_value)<1e-7){trial->dim=*dim;trial->count++;}
    return error;
}
static ProError plane_trial_offset(ProMdl model,double value){
    ProFeature feature;
    PlaneTrialDimension trial={32};
    ProError error=ux_named_feature(model,L"END_X",&feature);
    if(!error)error=ProFeatureDimensionVisit(&feature,plane_trial_dimension,NULL,&trial);
    if(!error&&trial.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&trial.dim,value);
    if(!error)error=part_regenerate(model);
    return error;
}
static ProError plane_trial_read(ProMdl model,const wchar_t *name,ProVector point,ProVector normal){
    ProFeature feature;
    ProGeomitem plane;
    ProError error=ux_named_feature(model,name,&feature);
    if(!error)error=fd_feature_geom(&feature,PRO_SURFACE,&plane);
    if(!error)error=fd_plane_data(&plane,point,normal);
    return error;
}
static ProError plane_trial_compare(ProMdl source,ProMdl result,const wchar_t *name){
    ProFeature a,b;
    ProError error=ux_named_feature(source,name,&a);
    if(!error)error=ux_named_feature(result,name,&b);
    if(!error)error=replay_verify_plane(&a,&b);
    return error;
}
static ProError plane_trial_link(ProMdl model,const wchar_t *child,const wchar_t *parent){
    ProFeature a,b;
    ProError error=ux_named_feature(model,child,&a);
    if(!error)error=ux_named_feature(model,parent,&b);
    int *ids=NULL,count=0,found=0;
    if(!error)error=ProFeatureParentsGet(&a,&ids,&count);
    for(int i=0;!error&&i<count;i++)found+=ids[i]==b.id;
    if(ids)ProArrayFree((ProArray *)&ids);
    return error?error:found==1?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}
static ProError plane_trial_reference_report(ProMdl source,ProMdl target,FILE *log){
    ProFeature feature;
    ProGeomitem item;
    ProCurve curve;
    ProReference original=NULL,mapped=NULL;
    ProError error=ux_named_feature(source,L"LINE_40",&feature);
    if(!error)error=ad_single_curve(&feature,&item,&curve);
    if(!error)error=ProReferenceAlloc(&original);
    if(!error)error=ProReferenceSet(original,NULL,&item);
    ImitationReplay replay={0};
    replay.source=source;
    replay.target=target;
    if(!error&&replay_map_reference(&replay,original,&mapped)!=PRO_TK_E_NOT_FOUND)
        error=PRO_TK_INCOMPLETE;
    if(!error&&(!replay.reference_error.owner[0]||replay.reference_error.type!=PRO_CURVE||
       replay.reference_error.item!=item.id||replay.reference_error.error!=PRO_TK_E_NOT_FOUND))
        error=PRO_TK_INCOMPLETE;
    wchar_t report[512]=L"";
    PartText out={report,_countof(report),0};
    replay_report_reference_failure(&replay.reference_error,&out);
    if(!error&&(!wcsstr(report,L"ID ")||!wcsstr(report,L"Creo -4")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"MISSING REFERENCE REPORT=%d %ls",error,report);
    if(original)ProReferenceFree(original);
    if(mapped)ProReferenceFree(mapped);
    return error;
}
static ProError planes_case(FILE *log,int curves){
    ProMdl source=NULL,result=NULL;
    ModelPlan plan={1,40,40,20,0,1};
    ProMdlName name;
    ProPath output=L"";
    const char *stage=NULL;
    wchar_t protocol[512],text[16000]=L"";
    PartText out={text,_countof(text),0};
    ProError error=model_create(&plan,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    Contact contact={0};
    if(!error){
        error=ProSolidSurfaceVisit((ProSolid)source,pick_cylinder,NULL,&contact);
        if(error==PRO_TK_E_FOUND)error=PRO_TK_NO_ERROR;
        if(!error&&!contact.id)error=PRO_TK_E_NOT_FOUND;
    }
    const wchar_t *steps[]={
        L"DATUM_POINT %ls P_ZERO CSYS:PRT_CSYS_DEF 0 0 0",
        L"DATUM_POINT %ls P_X CSYS:PRT_CSYS_DEF 20 0 0",
        L"DATUM_POINT %ls P_YZ CSYS:PRT_CSYS_DEF 0 20 8",
        L"DATUM_PLANE_3POINT %ls THREE POINT:P_ZERO POINT:P_X POINT:P_YZ",
        L"DATUM_AXIS %ls TILT_AXIS PLANES PLANE:RIGHT PLANE:TOP",
        L"DATUM_PLANE_ANGLE %ls ANGLED PLANE:RIGHT AXIS:TILT_AXIS 35",
        L"DATUM_PLANE_ANGLE %ls ANGLED_BACK PLANE:RIGHT AXIS:TILT_AXIS -25",
        L"DATUM_PLANE %ls END_X PLANE:RIGHT 32",
        L"DATUM_PLANE_MID %ls MID_X PLANE:RIGHT PLANE:END_X"
    };
    const wchar_t *curve_steps[]={
        L"SKETCH_CREATE %ls LINE_40 PLANE:TOP PLANE:RIGHT LINE 0 0 40 0",
        L"DATUM_POINT_CURVE %ls PERCENT_25 CURVE:LINE_40 PERCENT 25",
        L"DATUM_POINT_CURVE %ls LENGTH_12 CURVE:LINE_40 LENGTH 12",
        L"DATUM_POINT_CURVE %ls AT_START CURVE:LINE_40 START 0",
        L"DATUM_POINT_CURVE %ls AT_END CURVE:LINE_40 END 0",
        L"DATUM_PLANE_NORMAL %ls LINE_NORMAL CURVE:LINE_40 POINT:LENGTH_12",
        L"SKETCH_CREATE %ls ARC_ROUTE PLANE:TOP PLANE:RIGHT ARC 0 0 20 0 90",
        L"DATUM_POINT_CURVE %ls ARC_POINT CURVE:ARC_ROUTE PERCENT 40",
        L"DATUM_PLANE_NORMAL %ls ARC_NORMAL CURVE:ARC_ROUTE POINT:ARC_POINT",
        L"DATUM_PLANE %ls MID_Z PLANE:FRONT 10"
    };
    for(int i=0;!error&&i<_countof(steps);i++){
        wcscpy_s(protocol,_countof(protocol),L"CREO_OP ");
        swprintf_s(protocol+8,_countof(protocol)-8,steps[i],name);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_POINT %ls P_TOUCH CSYS:PRT_CSYS_DEF %.9f %.9f %.9f",
            name,contact.xyz[0],contact.xyz[1],contact.xyz[2]);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_PLANE_TANGENT %ls TANGENT SURFACE:%d POINT:P_TOUCH",name,contact.id);
        error=expansion_build(source,protocol,log);
    }
    for(int i=0;!error&&curves&&i<_countof(curve_steps);i++){
        wcscpy_s(protocol,_countof(protocol),L"CREO_OP ");
        swprintf_s(protocol+8,_countof(protocol)-8,curve_steps[i],name);
        error=expansion_build(source,protocol,log);
    }
    if(!error&&curves){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_CURVE_INTERSECT %ls INTERSECTION SURFACE:%d PLANE:MID_Z",name,contact.id);
        error=expansion_build(source,protocol,log);
    }
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    if(!error&&(!wcsstr(text,L"新特征 MID_X ID ")||!wcsstr(text,L"Creo 0")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"REPLAY CURVES=%d ERROR=%d\n%ls\n",curves,error,text);
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    const wchar_t *planes[]={L"THREE",L"ANGLED",L"ANGLED_BACK",L"END_X",L"MID_X",L"TANGENT",L"LINE_NORMAL",L"ARC_NORMAL"};
    for(int i=0;!error&&i<(curves?_countof(planes):6);i++)error=plane_trial_compare(source,result,planes[i]);
    if(!error&&curves)error=plane_trial_link(result,L"LINE_NORMAL",L"LENGTH_12");
    if(!error&&curves)error=plane_trial_link(result,L"ARC_POINT",L"ARC_ROUTE");
    if(!error&&curves)error=plane_trial_reference_report(source,result,log);
    if(!error)error=plane_trial_link(result,L"MID_X",L"END_X");
    if(!error)error=plane_trial_link(result,L"THREE",L"P_YZ");
    if(!error)error=plane_trial_link(result,L"TANGENT",L"P_TOUCH");
    ProVector point,normal;
    if(!error)error=plane_trial_offset(result,36);
    if(!error)error=plane_trial_read(result,L"MID_X",point,normal);
    if(!error&&fabs(point[0]-18)>0.0002)error=PRO_TK_INCOMPLETE;
    if(!error){
        ProFeature end;
        PlaneTrialDimension trial={36};
        error=ux_named_feature(result,L"END_X",&end);
        if(!error)error=ProFeatureDimensionVisit(&end,plane_trial_dimension,NULL,&trial);
        if(!error&&trial.count!=1)error=PRO_TK_INCOMPLETE;
        if(!error)error=ProDimensionValueSet(&trial.dim,32);
        if(!error)error=part_regenerate(result);
        if(!error)error=plane_trial_compare(source,result,L"MID_X");
    }
    fwprintf(log,L"OFFSET EDIT 32 TO 36 AND RESTORE=%d\n",error);
    return error;
}
static int planes_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-planes.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    ProError planes=planes_case(log,0),curves=planes_case(log,1);
    fwprintf(log,L"PLANES=%d CURVES=%d PASS=%d\n",planes,curves,!planes&&!curves);
    fclose(log);
    ProEngineerEnd();
    return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayPlanes",planes_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayPlanes\x0060;");
}
