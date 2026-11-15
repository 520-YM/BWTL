#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES

static ProError references_block_owner(ProMdl model,int id,ProType type,ProFeature *owner,ProModelitem *item){
    ProError error=ProModelitemInit(model,id,type,item);
    if(!error)error=ProGeomitemFeatureGet(item,owner);
    return error;
}

static ProError references_history(FILE *log){
    ProMdl source=NULL,target=NULL;
    ProMdlName name,target_name;
    Fixture src,dst;
    ProFeature old_owner={0},new_owner={0},previous={0};
    ProModelitem old_item,new_item,actual;
    ProReference reference=NULL;
    wchar_t report[1024]=L"";
    PartText out={report,_countof(report),0};
    ProError error=fixture(&source,name,&src);
    if(!error)error=fixture(&target,target_name,&dst);
    if(!error)error=references_block_owner(source,src.vertical[0],PRO_EDGE,&old_owner,&old_item);
    if(!error)error=references_block_owner(target,dst.vertical[0],PRO_EDGE,&new_owner,&new_item);
    if(!error)error=ProReferenceAlloc(&reference);
    if(!error)error=ProReferenceSet(reference,NULL,&old_item);
    if(!error)error=batch_step(source,log,L"CREO_OP DATUM_PLANE %ls MID PLANE:FRONT 10",name);
    if(!error)error=batch_step(source,log,L"CREO_OP SKETCH_CREATE %ls SLOT_SK PLANE:MID PLANE:RIGHT RECT -2 -2 84 64",name);
    if(!error)error=ux_named_feature(source,L"SLOT_SK",&previous);
    if(!error)error=batch_step(source,log,L"CREO_OP SOLID_EXTRUDE %ls SLOT SLOT_SK CUT BLIND 2 NEG",name);
    ImitationReplay replay={.source=source,.target=target,.out=&out,.count=1,.current_feature=999,.last_feature=previous.id};
    replay.mapped[0].source=old_owner.id;
    replay.mapped[0].target=new_owner.id;
    const int mapped=replay.mapped[0].target;
    if(!error){
        ProError live=replay_reference_find(&replay,reference,&actual,0);
        fwprintf(log,L"LIVE REF %d MATCHED %d EXPECTED %d\n",live,live?0:actual.id,new_item.id);
        ProError blocked=replay_reference_recover(&replay,reference,&actual,0,PRO_TK_BAD_CONTEXT);
        if(blocked!=PRO_TK_BAD_CONTEXT||report[0])error=PRO_TK_INCOMPLETE;
    }
    if(!error)error=replay_reference_recover(&replay,reference,&actual,0,PRO_TK_E_NOT_FOUND);
    if(!error&&(actual.id!=new_item.id||replay.count!=1||replay.mapped[0].target!=mapped||
        !wcsstr(report,L"从上一步恢复参照")))error=PRO_TK_INCOMPLETE;
    if(reference)ProReferenceFree(reference);
    if(target)ProMdlErase(target);
    if(source)ProMdlErase(source);
    fwprintf(log,L"%ls\nHISTORY ERROR=%d\n",report,error);
    return error;
}

static ProError references_candidate(FILE *log){
    ProMdl source=NULL,target=NULL;
    ProMdlName a,b;
    Fixture src,dst;
    ProFeature feature;
    ProModelitem item,wrong,actual;
    ProSelection yes=NULL,no=NULL,other_model=NULL;
    ProError error=fixture(&source,a,&src);
    if(!error)error=fixture(&target,b,&dst);
    if(!error)error=references_block_owner(target,dst.top,PRO_SURFACE,&feature,&item);
    if(!error)error=ProSelectionAlloc(NULL,&item,&yes);
    if(!error)error=references_block_owner(target,dst.xfar,PRO_SURFACE,&feature,&wrong);
    if(!error)error=ProSelectionAlloc(NULL,&wrong,&no);
    if(!error)error=references_block_owner(source,src.top,PRO_SURFACE,&feature,&wrong);
    if(!error)error=ProSelectionAlloc(NULL,&wrong,&other_model);
    ReplayGeometry search={.count=2,.ids={dst.top,dst.yfar}};
    if(!error&&replay_validate_pick(target,PRO_SURFACE,&search,yes,&actual))error=PRO_TK_INCOMPLETE;
    if(!error&&actual.id!=item.id)error=PRO_TK_INCOMPLETE;
    if(!error&&replay_validate_pick(target,PRO_SURFACE,&search,no,&actual)!=PRO_TK_INVALID_ITEM)error=PRO_TK_INCOMPLETE;
    if(!error&&replay_validate_pick(target,PRO_SURFACE,&search,other_model,&actual)!=PRO_TK_INVALID_ITEM)error=PRO_TK_INCOMPLETE;
    HMENU menu=native_imitation_menu_create();
    if(!error&&(!menu||GetMenuState(menu,IMITATION_MANUAL,MF_BYCOMMAND)==(UINT)-1))error=PRO_TK_INCOMPLETE;
    if(menu)DestroyMenu(menu);
    if(yes)ProSelectionFree(&yes);
    if(no)ProSelectionFree(&no);
    if(other_model)ProSelectionFree(&other_model);
    if(target)ProMdlErase(target);
    if(source)ProMdlErase(source);
    fwprintf(log,L"MANUAL CANDIDATES ERROR=%d\n",error);
    return error;
}

static int references_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-references.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    ProError candidate=references_candidate(log);
    ProError history=references_history(log);
    fwprintf(log,L"HISTORY=%d MANUAL=%d PASS=%d\n",history,candidate,!history&&!candidate);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayReferences",references_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayReferences\x0060;");
}

