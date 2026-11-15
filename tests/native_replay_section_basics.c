#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

typedef struct { Pro2dEntType type; ProBoolean construction; } BasicSketchEntity;
static const BasicSketchEntity basics[]={
    {PRO_2D_LINE,PRO_B_FALSE},{PRO_2D_LINE,PRO_B_TRUE},{PRO_2D_CENTER_LINE,PRO_B_FALSE},
    {PRO_2D_CIRCLE,PRO_B_FALSE},{PRO_2D_ARC,PRO_B_FALSE},{PRO_2D_POINT,PRO_B_FALSE}
};

static ProError basic_sketch_draw(ProSection section,const ProMatrix frame,double scale,void *context){
    int id;
    Pro2dLinedef axis={PRO_2D_CENTER_LINE,{30,9},{48,13}};
    Pro2dCircledef circle={PRO_2D_CIRCLE,{24,30},4};
    Pro2dPointdef point={PRO_2D_POINT,{58,12}};
    ProError error=as_add_line(section,as_point(8,10),as_point(22,15),&id);
    if(!error)error=as_add_line(section,as_point(6,22),as_point(17,25),&id);
    if(!error)error=ProSectionEntityConstructionSet(section,id,PRO_B_TRUE);
    if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&axis,&id);
    if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&circle,&id);
    if(!error)error=as_add_arc(section,as_point(49,39),7,0.2,1.4,&id);
    if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&point,&id);
    return error;
}

static ProError basic_sketch_find(ProSection section,BasicSketchEntity spec,int *id){
    int *ids=NULL,n=0,count=0;
    ProError error=ProSectionEntityIdsGet(section,&ids,&n);
    for(int i=0;!error&&i<n;i++){
        ProBoolean projected,construction;
        Pro2dEntdef *entity=NULL;
        error=ProSectionEntityIsProjection(section,ids[i],&projected);
        if(error||projected)continue;
        error=ProSectionEntityIsConstruction(section,ids[i],&construction);
        if(!error)error=ProSectionEntityGet(section,ids[i],&entity);
        if(!error&&entity->type==spec.type&&construction==spec.construction){
            *id=ids[i];
            count++;
        }
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    return error?error:count==1?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError topology_sketch_draw(ProSection section,const ProMatrix frame,double scale,void *context){
    const int closed=*(const int *)context;
    const double outer[][2]={{5,5},{65,5},{65,45},{5,45},{5,5}};
    const double inner[][2]={{13,14},{13,28},{31,28},{31,14},{13,14}};
    const double open[][2]={{8,11},{22,19},{36,24},{48,15}};
    const double (*profile)[2]=closed?outer:open;
    int id;
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<(closed?4:3);i++)
        error=as_add_line(section,as_p(profile[i]),as_p(profile[i+1]),&id);
    if(closed){
        for(int i=0;!error&&i<4;i++)
            error=as_add_line(section,as_p(inner[i]),as_p(inner[i+1]),&id);
        Pro2dCircledef hole={PRO_2D_CIRCLE,{48,26},5};
        if(!error)error=ProSectionEntityAdd(section,(Pro2dEntdef *)&hole,&id);
    }
    return error;
}

static ProError topology_sketch_verify(ProMdl source,ProMdl result,const wchar_t *name,
                                       ProSectionShape expected,int expected_count){
    FdSketch a={0},b={0};
    int *from=NULL,*to=NULL,n=0,m=0,matched=0;
    ProSectionShape original,rebuilt;
    ProError error=fd_sketch_open(source,name,&a,0);
    if(!error)error=fd_sketch_open(result,name,&b,0);
    if(!error)error=replay_section_frame(a.section,b.section);
    if(!error)error=ProSectionShapeGet(a.section,&original);
    if(!error)error=ProSectionShapeGet(b.section,&rebuilt);
    if(!error&&(original!=expected||rebuilt!=expected))error=PRO_TK_INCOMPLETE;
    if(!error)error=ProSectionEntityIdsGet(a.section,&from,&n);
    if(!error)error=ProSectionEntityIdsGet(b.section,&to,&m);
    for(int i=0;!error&&i<n;i++){
        ProBoolean projected;
        error=ProSectionEntityIsProjection(a.section,from[i],&projected);
        if(error||projected)continue;
        Pro2dEntdef *raw=NULL;
        error=ProSectionEntityGet(a.section,from[i],&raw);
        if(error)continue;
        int matches=0;
        Pro2dLinedef line={0};
        if(raw->type==PRO_2D_LINE)line=*(Pro2dLinedef *)raw;
        for(int j=0;!error&&j<m;j++){
            error=ProSectionEntityIsProjection(b.section,to[j],&projected);
            if(error||projected)continue;
            if(replay_section_entity_compare(a.section,from[i],b.section,to[j]))continue;
            if(line.type==PRO_2D_LINE){
                Pro2dEntdef *other=NULL;
                error=ProSectionEntityGet(b.section,to[j],&other);
                if(error)break;
                const Pro2dLinedef *candidate=(const Pro2dLinedef *)other;
                if(!replay_point2_same(line.end1,candidate->end1)||!replay_point2_same(line.end2,candidate->end2))continue;
            }
            matches++;
        }
        if(!error&&matches!=1)error=PRO_TK_INCOMPLETE;
        if(!error)matched++;
    }
    if(!error&&matched!=expected_count)error=PRO_TK_INCOMPLETE;
    if(from)ProArrayFree((ProArray *)&from);
    if(to)ProArrayFree((ProArray *)&to);
    fd_sketch_free(&a);
    fd_sketch_free(&b);
    return error;
}

static int basic_sketch_trace(FILE *receipt,int feature){
    FILE *journal=runtime_open(L"imitation-native-diagnostics.log",L"rt, ccs=UTF-8");
    if(!journal)return 0;
    wchar_t line[3000],marker[48];
    int begin=0,context=0,end=0,dimensions=0;
    swprintf_s(marker,_countof(marker),L"feature=%d ",feature);
    while(fgetws(line,_countof(line),journal)){
        if(!wcsstr(line,marker))continue;
        if(!wcsncmp(line,L"BEGIN ",6)&&wcsstr(line,L"name=BASIC_INPUT"))begin++;
        if(!wcsncmp(line,L"CONTEXT ",8)&&wcsstr(line,L"parents(0)=")&&wcsstr(line,L"children(")){
            context++;
            const wchar_t *values=wcsstr(line,L"dimensions=");
            dimensions+=values&&wcsstr(values,L":")!=NULL;
        }
        if(!wcsncmp(line,L"END ",4)&&wcsstr(line,L"creo=0"))end++;
    }
    fclose(journal);
    fwprintf(receipt,L"JOURNAL_BEGIN=%d CONTEXT=%d DIMENSIONS=%d END=%d\n",begin,context,dimensions,end);
    return begin==1&&context==1&&dimensions==1&&end==1;
}

static int basic_sketch_case(uiCmdCmdId command,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-section-basics.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture stock;
    ProMatrix frame;
    ProError error=fixture(&source,name,&stock);
    if(!error)error=smx_custom_sketch(source,L"BASIC_INPUT",L"PLANE:FRONT",L"PLANE:RIGHT",basic_sketch_draw,NULL,frame);
    int closed=1,open=0;
    if(!error)error=smx_custom_sketch(source,L"CLOSED_LOOPS",L"PLANE:FRONT",L"PLANE:RIGHT",topology_sketch_draw,&closed,frame);
    if(!error)error=smx_custom_sketch(source,L"OPEN_LOOP",L"PLANE:FRONT",L"PLANE:RIGHT",topology_sketch_draw,&open,frame);
    fwprintf(log,L"SOURCE=%d\n",error);
    ProPath delivered=L"";
    wchar_t text[18000]=L"";
    PartText out={text,_countof(text),0};
    if(!error)error=imitation_run_mode(source,delivered,0,0,&out);
    fwprintf(log,L"REPLAY=%d\n%ls\n",error,text);
    if(!error)error=ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    FdSketch original={0},rebuilt={0};
    if(!error)error=fd_sketch_open(source,L"BASIC_INPUT",&original,0);
    if(!error)error=fd_sketch_open(result,L"BASIC_INPUT",&rebuilt,0);
    if(!error)error=replay_section_frame(original.section,rebuilt.section);
    for(int i=0;!error&&i<_countof(basics);i++){
        int a,b;
        error=basic_sketch_find(original.section,basics[i],&a);
        if(!error)error=basic_sketch_find(rebuilt.section,basics[i],&b);
        if(!error)error=replay_section_entity_compare(original.section,a,rebuilt.section,b);
        fwprintf(log,L"READBACK TYPE=%d CONSTRUCTION=%d CREO=%d\n",basics[i].type,basics[i].construction,error);
    }
    int journal=!error&&basic_sketch_trace(log,original.feature.id);
    fd_sketch_free(&original);
    fd_sketch_free(&rebuilt);
    ProError loops=error?error:topology_sketch_verify(source,result,L"CLOSED_LOOPS",PRO_SECSHAPE_MULTI_CLOSED_LOOPS,9);
    ProError path=error?error:topology_sketch_verify(source,result,L"OPEN_LOOP",PRO_SECSHAPE_1_OPEN_LOOP,3);
    fwprintf(log,L"BASICS=%d JOURNAL=%d CLOSED_LOOPS=%d OPEN_LOOP=%d PASS=%d\n",
        !error,journal,loops,path,!error&&journal&&!loops&&!path);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,error_buffer);
    uiCmdCmdId command;
    if(!error)error=ProCmdActionAdd("BWTLBasicReplay",basic_sketch_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLBasicReplay`;");
}
