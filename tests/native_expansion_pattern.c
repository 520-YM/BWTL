#define user_initialize expansion_pattern_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>


typedef struct {int top,bottom,x,y,xfar,yfar,vertical[4],nv;} Fixture;
static ProError pick(ProGeomitem *item,ProError status,ProAppData data){
    Fixture *f=data;ProVector p,n,a,b,v;ProEdge edge;ProError e;if(status)return status;
    if(item->type==PRO_SURFACE){
        if(fd_plane_data(item,p,n))return 0;
        ProFeature owner;ProFeattype ft=PRO_FEAT_DATUM;ProGeomitemFeatureGet(item,&owner);ProFeatureTypeGet(&owner,&ft);
        if(ft!=PRO_FEAT_DATUM&&fabs(n[2])>0.99){if(fabs(p[2]-20)<1e-6)f->top=item->id;if(fabs(p[2])<1e-6)f->bottom=item->id;}
        if(!f->x&&n[0]>0.99&&fabs(p[0])<1e-6)f->x=item->id;
        if(!f->y&&n[1]>0.99&&fabs(p[1])<1e-6)f->y=item->id;
        if(n[0]>0.99&&fabs(p[0]-80)<1e-6)f->xfar=item->id;
        if(n[1]>0.99&&fabs(p[1]-60)<1e-6)f->yfar=item->id;
    }else{
        if((e=ProGeomitemToEdge(item,&edge))||(e=ProEdgeXyzdataEval(edge,0,p,a,b,v))||(e=ProEdgeXyzdataEval(edge,1,n,a,b,v)))return e;
        if(fabs(p[0]-n[0])<1e-6&&fabs(p[1]-n[1])<1e-6&&fabs(fabs(p[2]-n[2])-20)<1e-6&&f->nv<4)f->vertical[f->nv++]=item->id;
    }return 0;
}
static ProError pick_feature(ProFeature *f,ProError status,ProAppData data){ProError e;if(status)return status;e=ProFeatureGeomitemVisit(f,PRO_SURFACE,pick,NULL,data);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;e=ProFeatureGeomitemVisit(f,PRO_EDGE,pick,NULL,data);return e==PRO_TK_E_NOT_FOUND?0:e;}
typedef struct {ProMdl model;Fixture *f;} PickFinal;
static ProError pick_final_surface(ProSurface surface,ProError status,ProAppData data){PickFinal *p=data;ProGeomitem item;ProError e;if(status)return status;if((e=ProSurfaceToGeomitem((ProSolid)p->model,surface,&item)))return e;return pick(&item,0,p->f);}
static ProError fixture(ProMdl *model,ProMdlName name,Fixture *f){
    ModelPlan p={0,80,60,20,0,1};const char *stage="new fixture";ProError e;memset(f,0,sizeof(*f));
    if((e=model_create(&p,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,model)))return e;
    if((e=part_regenerate(*model)))return e;
    e=ProSolidFeatVisit((ProSolid)*model,pick_feature,NULL,f);if(e)return e;
    {PickFinal final={*model,f};f->top=0;f->bottom=0;if((e=ProSolidSurfaceVisit((ProSolid)*model,pick_final_surface,NULL,&final)))return e;}
    return f->top&&f->bottom&&f->x&&f->y&&f->nv==4?0:PRO_TK_E_NOT_FOUND;
}
static ProError expansion_build(ProMdl model,const wchar_t *protocol,FILE *log){
    PartCommand command={0};
    wchar_t message[PART_TEXT_MAX]=L"";
    ProError error=part_command_parse(protocol,&command)==1?
        foundation_execute(&command,model,message,_countof(message)):PRO_TK_BAD_INPUTS;
    fwprintf(log,L"%ls → Creo %d\n%ls\n",protocol,error,message);
    fflush(log);
    return error;
}

typedef struct {ProDimension dimension;int count;} ExpansionDiameter;

static ProError expansion_diameter(ProDimension *dimension,ProError status,ProAppData data){
    if(status)return status;
    ExpansionDiameter *found=data;
    ProDimensiontype type;
    double value;
    if(!ProDimensionTypeGet(dimension,&type)&&type==PRODIMTYPE_DIAMETER&&
       !ProDimensionValueGet(dimension,&value)&&fabs(value-6)<1e-7){
        found->dimension=*dimension;
        found->count++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError expansion_check_holes(const wchar_t *output,int count,double depth,FILE *log){
    ProMdl result=NULL;
    ProFeature seed;
    ExpansionDiameter found={0};
    ProMassProperty before,edited,restored;
    ProError error=ProMdlFiletypeLoad((wchar_t *)output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=ProModelitemByNameInit(result,PRO_FEATURE,L"SEED",&seed);
    if(!error)error=ProFeatureDimensionVisit(&seed,expansion_diameter,NULL,&found);
    if(!error&&found.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=qa_geometry_mass(result,&before);
    if(!error)error=ProDimensionValueSet(&found.dimension,6.5);
    int changed=!error;
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&edited);
    const double expected=count*depth*AS_PI*(3.25*3.25-3*3);
    if(!error&&fabs(before.volume-edited.volume-expected)>1e-4)error=PRO_TK_INCOMPLETE;
    if(changed){
        ProError restore=ProDimensionValueSet(&found.dimension,6);
        if(!restore)restore=part_regenerate(result);
        if(!restore)restore=qa_geometry_mass(result,&restored);
        if(!restore&&fabs(restored.volume-before.volume)>1e-4)restore=PRO_TK_INCOMPLETE;
        if(!error)error=restore;
    }
    if(result){
        ProError cleanup=ProMdlErase(result);
        if(!error)error=cleanup;
    }
    fwprintf(log,L"PATTERN_DIAMETER_EDIT_%d=%d\n",count,error);
    return error;
}


static ProError expansion_circular(FILE *log){
    ProMdl source=NULL;
    ProMdlName name;
    Fixture geometry;
    ProPath output=L"";
    wchar_t protocol[512],text[10000]=L"";
    PartText out={text,_countof(text),0};
    ProError error=fixture(&source,name,&geometry);
    const wchar_t *steps[]={
        L"CREO_OP DATUM_PLANE %ls MID_X PLANE:RIGHT 40",
        L"CREO_OP DATUM_PLANE %ls MID_Y PLANE:TOP 30",
        L"CREO_OP DATUM_AXIS %ls CENTER_AXIS PLANES PLANE:MID_X PLANE:MID_Y"
    };
    for(int i=0;!error&&i<_countof(steps);i++){
        swprintf_s(protocol,_countof(protocol),steps[i],name);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d 55 SURFACE:%d 30 6 BLIND 8",
            name,geometry.top,geometry.x,geometry.y);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP FEATURE_PATTERN_CIRCULAR %ls CIRCULAR SEED AXIS:CENTER_AXIS 4 90 POS",name);
        error=expansion_build(source,protocol,log);
    }
    if(!error)error=ProMdlSave(source);
    if(!error){
        ProWVerstamp before=NULL,after=NULL;
        ProBoolean dirty_before=0,dirty_after=0;
        ProMdlVerstampGet(source,&before);
        ProMdlModificationVerify(source,&dirty_before);
        replay_preview(source,&out);
        ProMdlVerstampGet(source,&after);
        ProMdlModificationVerify(source,&dirty_after);
        const int equal=before&&after&&ProVerstampEqual(before,after);
        fwprintf(log,L"PATTERN_READ_VERSION_EQUAL=%d DIRTY=%d/%d\n",equal,dirty_before,dirty_after);
        if(!equal||dirty_before||dirty_after)error=PRO_TK_INCOMPLETE;
        if(before)ProVerstampFree(&before);
        if(after)ProVerstampFree(&after);
    }
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"CIRCULAR_REPLAY=%d\n%ls\n",error,text);
    if(!error)error=expansion_check_holes(output,4,8,log);
    return error;
}


static int expansion_pattern_case(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"expansion-pattern.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProMdl source=NULL;
    ProMdlName name;
    Fixture geometry;
    ProPath output=L"";
    wchar_t protocol[512],text[12000]=L"";
    PartText out={text,_countof(text),0};
    model_background_mode=1;
    ProError error=fixture(&source,name,&geometry);
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d 20 SURFACE:%d 30 6 THROUGH 0",
            name,geometry.top,geometry.x,geometry.y);
        if(!error)error=expansion_build(source,protocol,log);
        swprintf_s(protocol,_countof(protocol),L"CREO_OP FEATURE_PATTERN_LINEAR %ls LINEAR SEED PLANE:RIGHT 3 15 POS",name);
        if(!error)error=expansion_build(source,protocol,log);

    }
    fwprintf(log,L"FIXTURE=%d\n",error);
    if(!error){
        replay_preview(source,&out);
        error=imitation_run_mode(source,output,0,1,&out);
    }
    fwprintf(log,L"REPLAY=%d\n%ls\n",error,text);
    if(!error)error=expansion_check_holes(output,3,20,log);
    ProError circular=expansion_circular(log);
    fwprintf(log,L"PASS=%d\n",!error&&!circular);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

#ifndef BWTL_REUSE_EXPANSION_PATTERN
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLExpansionPattern",expansion_pattern_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLExpansionPattern\x0060;");
}
#endif
