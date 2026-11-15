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

typedef struct {
    const double (*centers)[2];
    int count,found[5];
    double radius;
} ProfilePatternHoles;

static ProError profile_pattern_surface(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    ProfilePatternHoles *holes=data;
    ProSrftype type;
    ProError error=ProSurfaceTypeGet(surface,&type);
    if(error||type!=PRO_SRF_CYL)return error;
    ProGeomitemdata *raw=NULL;
    error=ProSurfaceDataGet(surface,&raw);
    if(error)return error;
    const ProCylinderdata *cylinder=&raw->data.p_surface_data->srf_shape.cylinder;
    int match=-1;
    for(int i=0;i<holes->count;i++)if(fabs(holes->centers[i][0]-cylinder->origin[0])<0.001&&
       fabs(holes->centers[i][1]-cylinder->origin[1])<0.001)match=i;
    if(match<0||fabs(fabs(cylinder->e3[2])-1)>1e-7||
       fabs(fabs(cylinder->radius)-holes->radius)>0.001)error=PRO_TK_INCOMPLETE;
    else holes->found[match]=1;
    ProGeomitemdataFree(&raw);
    return error;
}

static ProError profile_pattern_holes(ProMdl model,int kind,double radius,FILE *log){
    const double points[4][5][2]={
        {{10,10},{35,20},{60,40}},
        {{10,10},{30,10},{50,10},{70,10}},
        {{10,10},{25,10},{40,10},{55,10},{70,10}},
        {{60,30},{40,50},{20,30}}
    };
    const int counts[]={3,4,5,3};
    ProfilePatternHoles holes={0};
    holes.centers=points[kind];
    holes.count=counts[kind];
    holes.radius=radius;
    ProSolidBody body;
    ProError error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodySurfaceVisit(&body,profile_pattern_surface,&holes);
    int found=0;
    for(int i=0;i<holes.count;i++)found+=holes.found[i];
    if(!error&&found!=holes.count)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"HOLES KIND=%d DIAMETER=%.8g COUNT=%d/%d ERROR=%d\n",
        kind,2*radius,found,holes.count,error);
    return error;
}

static ProError expansion_check_holes(ProPath output,int kind,FILE *log){
    ProMdl result=NULL;
    ProFeature seed;
    ExpansionDiameter found={0};
    ProError error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=ProModelitemByNameInit(result,PRO_FEATURE,L"SEED",&seed);
    if(!error)error=ProFeatureDimensionVisit(&seed,expansion_diameter,NULL,&found);
    if(!error&&found.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=profile_pattern_holes(result,kind,3,log);
    if(!error)error=ProDimensionValueSet(&found.dimension,6.5);
    int changed=!error;
    if(!error)error=part_regenerate(result);
    if(!error)error=profile_pattern_holes(result,kind,3.25,log);
    if(changed){
        ProError restore=ProDimensionValueSet(&found.dimension,6);
        if(!restore)restore=part_regenerate(result);
        if(!restore)restore=profile_pattern_holes(result,kind,3,log);
        if(!error)error=restore;
    }
    return error;
}

static ProError expansion_profile_pattern(int kind,FILE *log){
    ProMdl source=NULL;
    ProMdlName name;
    Fixture geometry;
    wchar_t protocol[512],text[16000]=L"";
    PartText out={text,_countof(text),0};
    ProPath output=L"";
    const int expected=kind==0?3:kind==1?4:kind==2?5:3;
    ProError error=fixture(&source,name,&geometry);
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_PLANE %ls PATH_PLANE PLANE:FRONT 20",name);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        const wchar_t *profile=kind==0?
            L"CREO_OP REUSE_POINT_SKETCH %ls LOCATIONS PLANE:PATH_PLANE PLANE:RIGHT 10 10 35 20 60 40":
            kind==3?L"CREO_OP SKETCH_CREATE %ls LOCATIONS PLANE:PATH_PLANE PLANE:RIGHT ARC 40 30 20 0 180":
            L"CREO_OP SKETCH_CREATE %ls LOCATIONS PLANE:PATH_PLANE PLANE:RIGHT LINE 10 10 70 10";
        swprintf_s(protocol,_countof(protocol),profile,name);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d %d SURFACE:%d %d 6 THROUGH 0",
            name,geometry.top,geometry.x,kind==3?60:10,geometry.y,kind==3?30:10);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        const wchar_t *pattern=kind==0?
            L"CREO_OP REUSE_POINTS %ls PROFILE_PATTERN SEED SKETCH:LOCATIONS":
            kind==1?L"CREO_OP REUSE_CURVE %ls PROFILE_PATTERN SEED CURVE:LOCATIONS COUNT 4 POS":
            kind==2?L"CREO_OP REUSE_CURVE %ls PROFILE_PATTERN SEED CURVE:LOCATIONS SPACING 15 POS":
            L"CREO_OP REUSE_CURVE %ls PROFILE_PATTERN SEED CURVE:LOCATIONS COUNT 3 POS";
        swprintf_s(protocol,_countof(protocol),pattern,name);
        error=expansion_build(source,protocol,log);
    }
    if(!error)error=ProMdlSave(source);
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"REPLAY %d=%d\n%ls\n",kind,error,text);
    if(!error)error=expansion_check_holes(output,kind,log);
    fwprintf(log,L"CASE %d=%d\n",kind,error);
    fflush(log);
    return error;
}

static int expansion_profiles(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"expansion-profile-pattern.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    int passed=0;
    for(int i=0;i<4;i++)passed+=expansion_profile_pattern(i,log)==PRO_TK_NO_ERROR;
    fwprintf(log,L"PASS=%d CASES=%d\n",passed==4,passed);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLProfilePatternCase",expansion_profiles,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLProfilePatternCase\x0060;");
}

