#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES

typedef struct {ProSurface surface;int id;} WallCircle;

static ProError wall_circle_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    WallCircle *circle=data;
    ProEnttype type;
    if(!ProEdgeTypeGet(edge,&type)&&(type==PRO_ENT_CIRCLE||type==PRO_ENT_ARC))
        return ProEdgeIdGet(edge,&circle->id);
    return PRO_TK_NO_ERROR;
}

static ProError wall_circle_contour(ProContour contour,ProError status,ProAppData data){
    WallCircle *circle=data;
    return status?status:ProContourEdgeVisit(circle->surface,contour,wall_circle_edge,NULL,circle);
}

static ProError walls_collar(ProMdl source,const wchar_t *name,FILE *log){
    ProError error=batch_step(source,log,L"CREO_OP SMT_SKETCH %ls ROUND_SK PLANE:FRONT PLANE:RIGHT CIRCLE 40 30 12",name);
    if(!error)error=batch_step(source,log,L"CREO_OP SMT_CUT %ls ROUND_HOLE SKETCH:ROUND_SK NEG",name);
    BatchFace face={source,0};
    WallCircle circle={0};
    if(!error)error=ProSolidSurfaceVisit((ProSolid)source,batch_face,NULL,&face);
    if(!error)error=ProSurfaceInit((ProSolid)source,face.face,&circle.surface);
    if(!error)error=ProSurfaceContourVisit(circle.surface,wall_circle_contour,NULL,&circle);
    if(!error&&!circle.id)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=batch_step(source,log,L"CREO_OP CASE_COLLAR %ls COLLAR EDGE:%d 7 2 POS",name,circle.id);
    return error;
}

static ProError walls_adjacent(ProMdl source,const wchar_t *name,int gap,FILE *log){
    BatchFace face={source,0};
    ProSurface surface;
    CabinetBlank blank={0};
    blank.model=source;
    ProError error=ProSolidSurfaceVisit((ProSolid)source,batch_face,NULL,&face);
    blank.face=face.face;
    if(!error)error=ProSurfaceInit((ProSolid)source,face.face,&surface);
    if(!error)error=ProSurfaceContourVisit(surface,cm_blank_contour,NULL,&blank);
    if(!error&&blank.count!=4)error=PRO_TK_E_NOT_FOUND;
    if(error)return error;
    CabinetEdge *base=&blank.edges[0],tip;
    int adjacent=-1;
    for(int i=1;i<blank.count;i++){
        CabinetEdge *edge=&blank.edges[i];
        if(fabs(ad_dot(edge->axis,base->axis))<1e-8&&
            (cm_distance(edge->a,base->a)<.001||cm_distance(edge->b,base->a)<.001))adjacent=i;
    }
    if(adjacent<0)return PRO_TK_E_NOT_FOUND;
    FdTxn txn={0};
    txn.model=source;
    txn.scale=1;
    ProFeature primary,neighbor;
    CabinetFace wall;
    error=cm_attach(&txn,L"PRIMARY",base,8,base->length,20,90,2,L"POS",&primary);
    fwprintf(log,L"ATTACH EDGE %d OFFSET 8 ERROR=%d\n",base->id,error);
    if(!error)error=batch_step(source,log,L"CREO_OP SMT_FLANGE %ls NEIGHBOR EDGE:%d 23 90 2 POS",name,blank.edges[adjacent].id);
    if(!error&&gap)error=cm_wall_tip(source,&primary,base,&tip);
    if(!error&&gap)error=cm_feature_find(source,L"NEIGHBOR",&neighbor);
    if(!error&&gap)error=cm_wall_face(&neighbor,&wall);
    if(!error&&gap){
        CabinetFace primary_face;
        error=cm_wall_face(&primary,&primary_face);
        if(!error)fwprintf(log,L"PRIMARY FACE=%d POSITION=(%.5g,%.5g,%.5g) NORMAL=(%.5g,%.5g,%.5g)\n",primary_face.id,
            primary_face.point[0],primary_face.point[1],primary_face.point[2],
            primary_face.normal[0],primary_face.normal[1],primary_face.normal[2]);
    }
    if(!error&&gap==1)error=batch_step(source,log,L"CREO_OP CASE_GAP %ls PRIMARY EDGE:%d SURFACE:%d 12 1",name,tip.id,wall.id);
    if(!error&&gap==2)error=batch_step(source,log,L"CREO_OP CASE_END %ls PRIMARY EDGE:%d START 5 1",name,tip.id);
    return error;
}

static ProError wall_edit(ProMdl model,const wchar_t *name,double height,double next,FILE *log){
    FdTxn tx={0};
    ProMassProperty a,b,c;
    ProError error=sh_begin(&tx,model,L"WALL_EDIT_TEST",PRO_FEAT_WALL);
    if(!error)error=cm_feature_find(model,name,&tx.feature);
    if(!error)error=qa_geometry_mass(model,&a);
    if(!error)error=sh_profile_resize(&tx,next,0);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&b);
    if(!error&&part_close(a.volume,b.volume))error=PRO_TK_INCOMPLETE;
    if(!error)error=sh_profile_resize(&tx,height,0);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&c);
    if(!error&&!part_close(a.volume,c.volume))error=PRO_TK_INCOMPLETE;
    fd_cleanup(&tx,0);
    fwprintf(log,L"EDIT %ls %.6g -> %.6g -> %.6g ERROR=%d\n",name,height,next,height,error);
    return error;
}

static int walls_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-walls.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    int pass=0;
    for(int i=0;i<4;i++){
        ProMdl source=NULL,result=NULL;
        ProMdlName name;
        ProError error=batch_sheet(&source,name,log);
        if(!error)error=i==0?walls_collar(source,name,log):walls_adjacent(source,name,i>1?i-1:0,log);
        if(!error)error=batch_reopen(source,&result,log);
        if(!error&&i==0)error=wall_edit(result,L"COLLAR",7,8,log);

        if(!error&&i==2)error=batch_edit(result,L"PRIMARY_GAP_SK",PRODIMTYPE_LINEAR,4.05,4.55,log);
        if(!error&&i==3)error=batch_edit(result,L"PRIMARY_END_SK",PRODIMTYPE_LINEAR,5.05,5.55,log);
        if(result)ProMdlErase(result);
        if(source)ProMdlErase(source);
        fwprintf(log,L"WALLS CASE %d ERROR=%d\n",i,error);
        pass+=!error;
    }
    fwprintf(log,L"WALLS CASES=%d PASS=%d\n",pass,pass==4);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayWalls",walls_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLReplayWalls`;");
}

