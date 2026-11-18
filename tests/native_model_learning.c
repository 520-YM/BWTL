#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static ProError method_test_build(ProMdl source,const wchar_t *name,const Fixture *geometry,
                                  int count,int pitch,int diameter,FILE *log){
    wchar_t protocol[512];
    swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d 25 SURFACE:%d 20 %d THROUGH 0",
        name,geometry->top,geometry->x,geometry->y,diameter);
    ProError error=expansion_build(source,protocol,log);
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP FEATURE_PATTERN_LINEAR %ls DIR_PATTERN SEED PLANE:RIGHT %d %d POS",
            name,count,pitch);
        error=expansion_build(source,protocol,log);
    }
    return error;
}

static int test_prompt(const wchar_t *query,wchar_t kind,const wchar_t *expected,const wchar_t *excluded){
    FILE *prompt=runtime_open(L"method-prompt-fixture.txt",L"w+t, ccs=UTF-8");
    if(!prompt)return 0;
    int cases=method_prompt_write(prompt,query,kind);
    if(fflush(prompt)||fseek(prompt,0,SEEK_SET)){fclose(prompt);return 0;}
    wchar_t text[6000]=L"";
    for(int i=0;i<_countof(text)-1;i++){
        wint_t c=fgetwc(prompt);
        if(c==WEOF)break;
        text[i]=(wchar_t)c;
    }
    fclose(prompt);
    return expected?cases>0&&wcsstr(text,expected)&&(!excluded||!wcsstr(text,excluded)):cases==0;
}

static ProError method_verify_adapted(ProMdl model,FILE *log){
    ProFeature seed;
    ProError error=ux_named_feature(model,L"SEED",&seed);
    ProPattern pattern;
    ProFeature *members=NULL;
    int count=0;
    if(!error)error=ProFeaturePatternGet(&seed,PRO_FEAT_PATTERN,&pattern);
    if(!error)error=ProPatternMembersGet(&pattern,&members);
    if(!error)error=ProArraySizeGet((ProArray)members,&count);
    if(members)ProArrayFree((ProArray *)&members);
    if(!error&&count!=4)error=PRO_TK_INCOMPLETE;
    ReplayDimensionMatch match={0};
    match.type=PRODIMTYPE_DIAMETER;
    match.value=8;
    if(!error)error=ProFeatureDimensionVisit(&seed,replay_match_dimension,NULL,&match);
    if(!error&&match.count!=1)error=PRO_TK_INCOMPLETE;
    ProMassProperty before,after;
    if(!error)error=qa_geometry_mass(model,&before);
    const double expected=80*60*20-4*20*AS_PI*16;
    if(!error&&fabs(before.volume-expected)>0.0002)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&match.target,8.5);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&after);
    if(!error&&fabs(before.volume-after.volume-4*20*AS_PI*(4.25*4.25-16))>0.0002)
        error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&match.target,8);
    if(!error)error=part_regenerate(model);
    fwprintf(log,L"ADAPTED_PATTERN_MEMBERS=%d DIAMETER_EDIT=%d\n",count,error);
    return error;
}
static int learning_test(uiCmdCmdId command,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"model-learning.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    HMENU menu=native_imitation_menu_create();
    wchar_t label[64]=L"";
    HMENU learn=menu?GetSubMenu(menu,6):NULL;
    int menu_ok=learn&&GetMenuItemID(learn,0)==IMITATION_LEARN&&
        GetMenuStringW(learn,IMITATION_LEARN,label,_countof(label),MF_BYCOMMAND)>0&&
        !wcscmp(label,L"保存最近的建模方法");
    if(menu)DestroyMenu(menu);
    int empty_ok=method_save_last(&out)==PRO_TK_BAD_CONTEXT;
    report[0]=0;
    ProMdl source=NULL,result=NULL;
    ProMdlName name,second;
    Fixture original;
    ProPath delivered=L"";
    model_background_mode=1;
    ProError error=fixture(&source,name,&original);
    if(!error)error=method_test_build(source,name,&original,3,15,6,log);
    if(!error)error=imitation_run_mode(source,delivered,1,0,&out);
    fwprintf(log,L"REPLAY=%d\n%ls\n",error,report);fflush(log);
    int capture=!error&&wcsstr(method_last.method.sequence,L"HP")&&
        wcsstr(method_last.evidence,L"阵列种子")&&wcsstr(method_last.evidence,L"父特征")&&
        wcsstr(method_last.evidence,L"驱动");
    report[0]=0;
    if(!error){
        imitation_pending=IMITATION_LEARN;
        imitation_ui_command(0,NULL,NULL);
    }
    wchar_t learned[MAX_PATH]=L"";
    wcscpy_s(learned,_countof(learned),method_last.saved);
    ModelingMethod loaded={0};
    int save_ok=learned[0]&&method_header_read(learned,&loaded)&&
        !wcscmp(loaded.sequence,method_last.method.sequence);
    int repeat_ok=save_ok&&!method_save_last(&out)&&!wcscmp(learned,method_last.saved);
    memset(&method_last,0,sizeof(method_last));
    int retrieved=test_prompt(L"按你学过的方法画个实体零件，做一排安装孔",L'S',
        L"孔 → 阵列",name);
    int unrelated=test_prompt(L"你好，天气如何",0,NULL,NULL)&&
        test_prompt(L"画钣金折边，做一排孔",L'M',NULL,NULL);

    /* Apply the retrieved seed-and-pattern method to a new model using new inputs. */
    Fixture changed;
    if(!error)error=fixture(&result,second,&changed);
    if(!error)error=method_test_build(result,second,&changed,4,10,8,log);
    if(!error)error=ProMdlSave(result);
    ProPath next=L"";
    if(!error)error=ProMdlOriginGet(result,next);
    if(!error)error=ProMdlErase(result);
    if(!error)error=ProMdlFiletypeLoad(next,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=method_verify_adapted(result,log);

    fwprintf(log,L"MENU=%d EMPTY=%d CAPTURE=%d SAVE=%d REPEAT=%d RETRIEVE=%d FILTER=%d NEW_MODEL=%d PASS=%d\n",
        menu_ok,empty_ok,capture,save_ok,repeat_ok,retrieved,unrelated,!error,
        menu_ok&&empty_ok&&capture&&save_ok&&repeat_ok&&retrieved&&unrelated&&!error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLModelLearningTest",learning_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLModelLearningTest`;");
}