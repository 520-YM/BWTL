#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static ProError constraint_pair(ProSection section,ProConstraintType type,
                               int a,ProSectionPointType pa,int b,ProSectionPointType pb){
    ProSelection pick[2]={0};
    const int n=b<0?1:2;
    ProError error=ProSectionEntityGetSelected(section,a,pa,NULL,PRO_VALUE_UNUSED,&pick[0]);
    if(!error&&n==2)error=ProSectionEntityGetSelected(section,b,pb,NULL,PRO_VALUE_UNUSED,&pick[1]);
    int id;
    if(!error)error=ProSectionConstraintCreate(section,pick,n,type,&id);
    for(int i=0;i<n;i++)if(pick[i])ProSelectionFree(&pick[i]);
    trace("Fixture constraint type",type);
    trace("Fixture constraint code",error);
    return error==PRO_TK_E_FOUND?PRO_TK_NO_ERROR:error;
}

static ProError constraint_sketch(ProSection section,const ProMatrix frame,double scale,void *context){
    const double segments[][4]={{5,5,18,5},{18,5,18,14},{2,27,14,34},{19,28,31,35},
        {7,49,15,55},{31,49,25,57},{42,9,62,9}};
    int line[_countof(segments)],circles[3];
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<_countof(segments);i++)
        error=as_add_line(section,as_p(segments[i]),as_p(segments[i]+2),&line[i]);
    Pro2dCircledef circle={PRO_2D_CIRCLE,{52,14},5};
    if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&circle,&circles[0]);
    circle.center[0]=56;circle.center[1]=36;circle.radius=3;
    if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&circle,&circles[1]);
    circle.radius=7;
    if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&circle,&circles[2]);
    if(!error)error=ProSectionIntentManagerModeSet(section,PRO_B_TRUE);
    if(!error)error=constraint_pair(section,PRO_CONSTRAINT_HORIZONTAL_ENT,line[0],PRO_ENT_WHOLE,-1,0);
    if(!error)error=constraint_pair(section,PRO_CONSTRAINT_VERTICAL_ENT,line[1],PRO_ENT_WHOLE,-1,0);
    if(!error)error=constraint_pair(section,PRO_CONSTRAINT_SAME_POINT,line[0],PRO_ENT_END,line[1],PRO_ENT_START);
    if(!error)error=constraint_pair(section,PRO_CONSTRAINT_PARALLEL_ENTS,line[2],PRO_ENT_WHOLE,line[3],PRO_ENT_WHOLE);
    if(!error)error=constraint_pair(section,PRO_CONSTRAINT_ORTHOG_ENTS,line[4],PRO_ENT_WHOLE,line[5],PRO_ENT_WHOLE);
    if(!error)error=constraint_pair(section,PRO_CONSTRAINT_TANGENT_ENTS,line[6],PRO_ENT_WHOLE,circles[0],PRO_ENT_WHOLE);
    if(!error)error=constraint_pair(section,PRO_CONSTRAINT_SAME_POINT,circles[1],PRO_ENT_CENTER,circles[2],PRO_ENT_CENTER);
    return error;
}

static unsigned int constraint_types(ProSection section){
    unsigned int mask=0;
    ProIntlist ids=NULL;
    int count=0;
    if(ProSectionConstraintsIdsGet(section,&ids,&count))return 0;
    for(int i=0;i<count;i++){
        ProConstraintType type;
        ProConstraintStatus status;
        int n=0,*refs=NULL;
        ProSectionPointType *senses=NULL;
        if(!ProSectionConstraintsGet(section,ids[i],&type,&status,&n,&refs,&senses)&&status==PRO_TK_CONSTRAINT_ENABLED){
            if(type>=0&&type<=7)mask|=1u<<type;
            if(type==PRO_CONSTRAINT_SAME_POINT&&n==2&&senses[0]==PRO_ENT_CENTER&&senses[1]==PRO_ENT_CENTER)
                mask|=1u<<8;
        }
        if(refs)ProArrayFree((ProArray *)&refs);
        if(senses)ProArrayFree((ProArray *)&senses);
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    return mask;
}

static int constraints_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-sketch-constraints.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture stock;
    ProMatrix frame;
    model_background_mode=1;
    ProError error=fixture(&source,name,&stock);
    if(!error)error=smx_custom_sketch(source,L"CONSTRAINED",L"PLANE:FRONT",L"PLANE:RIGHT",constraint_sketch,NULL,frame);
    fwprintf(log,L"SOURCE=%d\n",error);
    ProPath output=L"";
    wchar_t report[18000]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,output,0,0,&out);
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    FdSketch a={0},b={0};
    if(!error)error=fd_sketch_open(source,L"CONSTRAINED",&a,0);
    if(!error)error=fd_sketch_open(result,L"CONSTRAINED",&b,0);
    unsigned int original=0,rebuilt=0;
    if(!error){
        original=constraint_types(a.section);
        rebuilt=constraint_types(b.section);
        const unsigned int expected=0x1b7;
        if((original&expected)!=expected||(rebuilt&expected)!=expected)error=PRO_TK_INCOMPLETE;
    }
    fd_sketch_free(&a);
    fd_sketch_free(&b);
    fwprintf(log,L"%ls\nORIGINAL=%x REBUILT=%x PASS=%d ERROR=%d\n",report,original,rebuilt,!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLConstraintReplay",constraints_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLConstraintReplay`;");
}
