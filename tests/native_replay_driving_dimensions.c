#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static const struct { ProSecdimType kind; double value; } dims[]={
    {PRO_TK_DIM_LINE,14},
    {PRO_TK_DIM_LINES_ANGLE,30},
    {PRO_TK_DIM_RAD,6},
    {PRO_TK_DIM_DIA,18.6},
    {PRO_TK_DIM_PNT_PNT_HORIZ,17.3},
    {PRO_TK_DIM_PNT_PNT_VERT,7.4},
    {PRO_TK_DIM_LINE_LINE,9}
};

static ProError driving_sketch(ProSection section,const ProMatrix frame,double scale,void *context){
    int line[4]={0},arc=0,circle=0,point[2]={0},dimension;
    Pro2dCircledef round={PRO_2D_CIRCLE,{60,47},9.3};
    Pro2dPointdef dot={PRO_2D_POINT,{18,54}};
    ProError error=as_add_line(section,as_point(8,10),as_point(22,10),&line[0]);
    if(!error)error=as_add_line(section,as_point(25,10),as_point(25+sqrt(3.0)*8,18),&line[1]);
    if(!error)error=as_add_line(section,as_point(14,30),as_point(32,30),&line[2]);
    if(!error)error=as_add_line(section,as_point(15,39),as_point(32,39),&line[3]);
    if(!error)error=as_add_arc(section,as_point(59,20),6,0.2,1.4,&arc);
    if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&round,&circle);
    if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&dot,&point[0]);
    dot.pnt[0]=35.3;dot.pnt[1]=61.4;
    if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&dot,&point[1]);
    if(!error)error=fd_section_solve(section,scale);
    int entities[][2]={{line[0],0},{line[0],line[1]},{arc,0},{circle,0},{point[0],point[1]},{point[0],point[1]},{line[2],line[3]}};
    const int references[]={1,2,1,1,2,2,2};
    ProSectionPointType senses[]={PRO_ENT_WHOLE,PRO_ENT_WHOLE};
    for(int i=0;!error&&i<_countof(dims);i++){
        error=fd_dimension_create(section,dims[i].kind,references[i],entities[i],senses,dims[i].value,scale,&dimension);
        trace("Fixture driving kind",dims[i].kind);
        trace("Fixture driving dimension",error);
    }
    return error;
}

static unsigned driving_mask(ProMdl model,FILE *log,double change){
    FdSketch sketch={0};
    ProError error=fd_sketch_open(model,L"DRIVING",&sketch,0);
    unsigned mask=0;
    int count=0,*ids=NULL;
    if(!error)error=ProSecdimIdsGet(sketch.section,&ids,&count);
    for(int i=0;!error&&i<count;i++){
        ProSecdimType type;
        double value=0;
        error=ProSecdimTypeGet(sketch.section,ids[i],&type);
        if(!error)error=ProSecdimValueGet(sketch.section,ids[i],&value);
        for(int j=0;!error&&j<_countof(dims);j++)
            if(type==dims[j].kind&&fabs(value-dims[j].value-change)<1e-4){
                fwprintf(log,L"DIM %d %.10g\n",type,value);
                mask|=1u<<j;
            }
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&sketch);
    return mask;
}

typedef struct { double expected; ProDimension driver; int found; FILE *log; } DrivingEdit;

static ProError driving_pick(ProDimension *dimension,ProError status,ProAppData data){
    DrivingEdit *edit=data;
    if(status)return status;
    double value=0;
    ProBoolean driving=PRO_B_FALSE;
    ProError read=ProDimensionValueGet(dimension,&value);
    ProError status_error=ProDimensionIsDriving(dimension,&driving);
    if(!read&&fabs(value-edit->expected)<1e-4){
        fwprintf(edit->log,L"DRIVER CANDIDATE ID=%d VALUE=%.8g DRIVING=%d STATUS=%d\n",dimension->id,value,driving,status_error);
        if(status_error||!driving)return PRO_TK_NO_ERROR;
        edit->driver=*dimension;
        edit->found++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError driving_edit(ProMdl model,double previous,double next,FILE *log){
    for(int j=0;j<_countof(dims);j++){
        FdSketch sketch={0};
        ProError error=fd_sketch_open(model,L"DRIVING",&sketch,0);
        int count=0,*ids=NULL,matched=-1;
        if(!error)error=ProSecdimIdsGet(sketch.section,&ids,&count);
        for(int i=0;!error&&i<count;i++){
            ProSecdimType type;
            double value=0;
            error=ProSecdimTypeGet(sketch.section,ids[i],&type);
            if(!error)error=ProSecdimValueGet(sketch.section,ids[i],&value);
            if(!error&&type==dims[j].kind&&fabs(value-dims[j].value-previous)<1e-4){
                if(matched>=0)error=PRO_TK_INCOMPLETE;
                else matched=ids[i];
            }
        }
        if(ids)ProArrayFree((ProArray *)&ids);
        DrivingEdit edit={.expected=dims[j].value+previous,.log=log};
        ProFeature feature=sketch.feature;
        fd_sketch_free(&sketch);
        if(!error&&matched<0)error=PRO_TK_E_NOT_FOUND;
        if(!error)error=ProFeatureDimensionVisit(&feature,driving_pick,NULL,&edit);
        fwprintf(log,L"DRIVER KIND=%d SECTION=%d FOUND=%d CREO=%d\n",dims[j].kind,matched,edit.found,error);
        fflush(log);
        if(!error&&edit.found!=1)error=PRO_TK_INCOMPLETE;
        if(!error)error=ProDimensionValueSet(&edit.driver,dims[j].value+next);
        if(!error)error=part_regenerate(model);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}
static int driving_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-driving-dimensions.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture stock;
    ProMatrix frame;
    model_background_mode=1;
    ProError error=fixture(&source,name,&stock);
    if(!error)error=smx_custom_sketch(source,L"DRIVING",L"PLANE:FRONT",L"PLANE:RIGHT",driving_sketch,NULL,frame);
    fwprintf(log,L"SOURCE=%d\n",error);
    unsigned original=error?0:driving_mask(source,log,0);
    ProPath output=L"";
    wchar_t report[16000]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,output,0,0,&out);
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    unsigned rebuilt=error?0:driving_mask(result,log,0);
    if(!error&&(original!=0x7f||rebuilt!=0x7f))error=PRO_TK_INCOMPLETE;
    if(!error)error=driving_edit(result,0,0.75,log);
    unsigned edited=error?0:driving_mask(result,log,0.75);
    if(!error&&edited!=0x7f)error=PRO_TK_INCOMPLETE;
    if(!error)error=driving_edit(result,0.75,0,log);
    unsigned restored=error?0:driving_mask(result,log,0);
    if(!error&&restored!=0x7f)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"%ls\nORIGINAL=%x REBUILT=%x EDITED=%x RESTORED=%x PASS=%d ERROR=%d\n",report,original,rebuilt,edited,restored,!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLDrivingDimensionReplay",driving_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLDrivingDimensionReplay`;");
}