#define user_initialize replay_datums_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProError replay_datum_command(ProMdl model,const wchar_t *line,FILE *log){
    PartCommand command={0};
    wchar_t text[4000]=L"";
    ProError error=part_command_parse(line,&command)==1?
        foundation_execute(&command,model,text,_countof(text)):PRO_TK_BAD_INPUTS;
    fwprintf(log,L"%ls = %d\n%ls\n",line,error,text);
    return error;
}

static ProError replay_datum_link(ProFeature *child,ProFeature *parent){
    int *ids=NULL,count=0,found=0;
    ProError error=ProFeatureParentsGet(child,&ids,&count);
    for(int i=0;!error&&i<count;i++)if(ids[i]==parent->id)found++;
    if(ids)ProArrayFree((ProArray *)&ids);
    return error?error:found==1?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError replay_datums_compare(ProMdl source,ProMdl target,FILE *log){
    const wchar_t *names[]={L"SHIFTED",L"P_ONE",L"P_TWO",L"THROUGH_P"};
    ProFeature src[4],dst[4];
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<_countof(names);i++){
        error=ux_named_feature(source,names[i],&src[i]);
        if(!error)error=ux_named_feature(target,names[i],&dst[i]);
    }
    if(!error)error=replay_verify_csys(&src[0],&dst[0]);
    for(int i=1;!error&&i<=2;i++){
        error=replay_verify_points(&src[i],&dst[i]);
        if(!error)error=replay_datum_link(&dst[i],&dst[0]);
        if(!error)error=replay_datum_link(&dst[3],&dst[i]);
    }
    FdRef axes[2]={0};
    ProVector origins[2],directions[2];
    if(!error)error=fd_feature_geom(&src[3],PRO_AXIS,&axes[0].item);
    if(!error)error=fd_feature_geom(&dst[3],PRO_AXIS,&axes[1].item);
    for(int i=0;!error&&i<2;i++)error=ad_line(&axes[i],origins[i],directions[i]);
    if(!error&&(!ad_near(origins[0],origins[1],0.0002)||!ad_near(directions[0],directions[1],1e-7)))
        error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"DATUM LINKS AND DIRECTIONS=%d\n",error);
    return error;
}


typedef struct {ProDimension value;int count;} ReplayAngle;

static ProError replay_datum_angle(ProDimension *dimension,ProError status,ProAppData data){
    if(status)return status;
    ReplayAngle *angle=data;
    ProDimensiontype type;
    double value;
    ProError error=ProDimensionTypeGet(dimension,&type);
    if(!error)error=ProDimensionValueGet(dimension,&value);
    if(!error&&type==PRODIMTYPE_ANGLE&&fabs(value-30)<1e-7){
        angle->value=*dimension;
        angle->count++;
    }
    return error;
}

static ProError replay_datum_edit(ProMdl source,ProMdl target,FILE *log){
    ProFeature frame,axis,p2;
    ReplayAngle angle={0};
    ProError error=ux_named_feature(target,L"SHIFTED",&frame);
    if(!error)error=ux_named_feature(target,L"THROUGH_P",&axis);
    if(!error)error=ux_named_feature(target,L"P_TWO",&p2);
    if(!error)error=ProFeatureDimensionVisit(&frame,replay_datum_angle,NULL,&angle);
    if(!error&&angle.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&angle.value,45);
    const int changed=!error;
    if(!error)error=part_regenerate(target);
    FdRef a={0},p={0};
    ProVector origin,direction,point;
    if(!error)error=fd_feature_geom(&axis,PRO_AXIS,&a.item);
    if(!error)error=fd_feature_geom(&p2,PRO_POINT,&p.item);
    if(!error)error=ad_line(&a,origin,direction);
    if(!error)error=ad_point(&p,point);
    const double unit=sqrt(0.5);
    const ProVector expected={10+20*unit,20+20*unit,30},expected_direction={unit,unit,0};
    if(!error&&(!ad_near(point,expected,0.0002)||fabs(ad_dot(direction,expected_direction))<0.999999))
        error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"DATUM ANGLE EDIT 30 TO 45=%d\n",error);
    if(changed){
        ProError restore=ProDimensionValueSet(&angle.value,30);
        if(!restore)restore=part_regenerate(target);
        if(!restore)restore=replay_datums_compare(source,target,log);
        if(!error)error=restore;
    }
    return error;
}

static int replay_datums_case(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-datums.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ModelPlan plan={0,100,80,20,0,1};
    ProMdlName name;
    ProMdl source=NULL,target=NULL;
    const char *stage=NULL;
    ProPath output=L"";
    wchar_t protocol[512],text[16000]=L"";
    PartText out={text,_countof(text),0};
    model_background_mode=1;
    ProError error=model_create(&plan,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    const wchar_t *steps[]={
        L"DATUM_CSYS %ls SHIFTED CSYS:PRT_CSYS_DEF 10 20 30 0 0 30",
        L"DATUM_POINT %ls P_ONE CSYS:SHIFTED 0 0 0",
        L"DATUM_POINT %ls P_TWO CSYS:SHIFTED 20 0 0",
        L"DATUM_AXIS_2POINT %ls THROUGH_P POINT:P_ONE POINT:P_TWO"
    };
    for(int i=0;!error&&i<_countof(steps);i++){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP ");
        swprintf_s(protocol+8,_countof(protocol)-8,steps[i],name);
        error=replay_datum_command(source,protocol,log);
    }
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"REPLAY=%d PATH=%ls\n%ls\n",error,output,text);
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&target);
    if(!error)error=replay_datums_compare(source,target,log);
    if(!error)error=replay_datum_edit(source,target,log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=replay_datums_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayDatums",replay_datums_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayDatums\x0060;");
}

