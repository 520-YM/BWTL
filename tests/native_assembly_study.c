
#define user_initialize study_app_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *study_log;
static int study_failures,study_checks;
static void study_check(const wchar_t *name,ProError error){
    fwprintf(study_log,L"%ls %ls (%d)\n",error?L"FAIL":L"PASS",name,error);
    fflush(study_log);
    study_checks++;
    study_failures+=error!=0;
}

static ProError study_block(const wchar_t *name,double length,double x,ProMdl *part){
    model_background_mode=1;
    ProMdlName target;
    wcscpy_s(target,_countof(target),name);
    ProError error=quick_seed(target,part);
    const char *stage="";
    const ModelPlan block={.a=length,.b=25,.height=4,.ready=1};
    if(!error)error=model_build_internal(&block,target,&stage,*part,NULL,NULL);
    if(!error)error=ax_layout_csys(*part,L"STUDY_PORT",L"CSYS:PRT_CSYS_DEF",x,0,0);
    if(!error)error=ProMdlSave(*part);
    return error;
}
static ProError study_assembly(const wchar_t *name,double mount,ProMdl *target){
    ProError error=ax_new_subassembly(name,target);
    if(!error)error=ax_layout_csys(*target,L"STUDY_MOUNT",L"CSYS:ASM_CSYS",mount,0,0);
    return error;
}
static ProError study_seed_constraint(ProMdl assembly,ProMdl part,ProAsmcomp *placed){
    AxGeometry ga,gb;
    AxComponent base={0},child={0};
    base.model=assembly;
    child.model=part;
    ProAsmcompconstraint *rows=NULL;
    ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProError error=ax_root_path(assembly,&base.path);
    if(!error)error=ProArrayAlloc(0,sizeof(*rows),1,(ProArray *)&rows);
    if(!error)error=ax_add_relation(&rows,&base,&child,L"CSYS:STUDY_MOUNT",L"CSYS:STUDY_PORT",PRO_ASM_CSYS,0,&ga,&gb);
    if(error==PRO_TK_INVALID_TYPE){
        ProSelection a=NULL,b=NULL;
        ProAsmcompconstraint row=NULL;
        FdRef fa={0},fb={0};
        error=fd_ref_resolve(assembly,L"CSYS:STUDY_MOUNT",PRO_CSYS,&fa);
        if(!error)error=fd_ref_resolve(part,L"CSYS:STUDY_PORT",PRO_CSYS,&fb);
        if(!error)error=ProSelectionAlloc(&base.path,&fa.item,&a);
        if(!error)error=ProSelectionAlloc(NULL,&fb.item,&b);
        if(!error)error=ProAsmcompconstraintAlloc(&row);
        if(!error)error=ProAsmcompconstraintTypeSet(row,PRO_ASM_CSYS);
        if(!error)error=ProAsmcompconstraintAsmreferenceSet(row,a,PRO_DATUM_SIDE_NONE);
        if(!error)error=ProAsmcompconstraintCompreferenceSet(row,b,PRO_DATUM_SIDE_NONE);
        if(!error)error=ProArrayObjectAdd((ProArray *)&rows,PRO_VALUE_UNUSED,1,&row);
        if(error&&row)ProAsmcompconstraintFree(row);
        if(a)ProSelectionFree(&a);
        if(b)ProSelectionFree(&b);
        fd_ref_free(&fa);
        fd_ref_free(&fb);
    }
    if(!error)error=ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)part,identity,placed);
    if(!error)error=ProAsmcompConstraintsSet(NULL,placed,rows);
    if(!error)error=part_regenerate(assembly);
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return error;
}


static ProError study_company_bracket(PartText *out){
    ProMdl created=NULL,plate=NULL;
    ProError error=ax_new_subassembly(L"STUDY_COMPANY_GUIDE",&created);
    if(!error)error=ProMdlnameRetrieve(L"M26M4181031",PRO_MDL_PART,&plate);
    ProAsmcomp support={0};
    ProAsmcompconstraint *rows=NULL;
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    if(!error)error=ProArrayAlloc(0,sizeof(*rows),1,(ProArray *)&rows);
    if(!error)error=ax_add_relation(&rows,NULL,NULL,NULL,NULL,PRO_ASM_FIX,0,NULL,NULL);
    if(!error)error=ProAsmcompAssemble((ProAssembly)created,(ProSolid)plate,pose,&support);
    if(!error)error=ProAsmcompConstraintsSet(NULL,&support,rows);
    if(!error)error=part_regenerate(created);
    if(rows)ProAsmcompconstraintArrayFree(rows);
    if(error)return error;
    wchar_t support_id[64];
    ax_path_label(&support.id,1,support_id,_countof(support_id));
    if(!error)error=ax_study_apply(created,L"M26G2000000",L"ID:40/4995/10",L"S-M4-1ZI_8",support_id,1,out);
    if(!error)error=ax_study_audit(created,NULL,NULL,0,out);
    return error;
}


static ProError study_company_pattern(PartText *out){
    ProMdl created=NULL,fan=NULL;
    ProError error=ax_new_subassembly(L"STUDY_COMPANY_FAN",&created);
    if(!error)error=ProMdlnameRetrieve(L"FAN-120X38",PRO_MDL_PART,&fan);
    AxStudyRule support={0};
    if(!error)error=ax_study_read(L"M26G2000000",L"ID:41/583/252",&support);
    ProAsmcomp fixed={0};
    ProAsmcompconstraint *rows=NULL;
    if(!error)error=ProArrayAlloc(0,sizeof(*rows),1,(ProArray *)&rows);
    if(!error)error=ax_add_relation(&rows,NULL,NULL,NULL,NULL,PRO_ASM_FIX,0,NULL,NULL);
    if(!error)error=ProAsmcompAssemble((ProAssembly)created,(ProSolid)fan,support.pose,&fixed);
    if(!error)error=ProAsmcompConstraintsSet(NULL,&fixed,rows);
    if(!error)error=part_regenerate(created);
    if(rows)ProAsmcompconstraintArrayFree(rows);
    if(error)return error;
    const int positions[]={258,266,267,268,269};
    for(int i=0;!error&&i<_countof(positions);i++){
        wchar_t key[40];
        swprintf_s(key,_countof(key),L"ID:41/583/%d",positions[i]);
        error=ax_study_apply(created,L"M26G2000000",key,L"120-FANGHUWANG",NULL,0,out);
    }
    if(!error)error=ax_study_apply(created,L"M26G2000000",L"ID:41/583/268",L"120-FANGHUWANG",NULL,0,out);
    if(!error)error=ax_study_audit(created,NULL,NULL,0,out);
    AxScan *scan=NULL;
    if(!error)error=ax_scan(created,0,&scan);
    if(!error&&scan->count!=6)error=PRO_TK_INCOMPLETE;
    if(!error){
        for(int i=1;i<=5;i++){
            ProAsmcomp item;
            ProMatrix actual;
            error=ax_component_feature(created,scan->occurrences[i].ids[0],&item);
            if(!error)error=ax_actual_pose(created,&item,actual);
            if(!error&&fabs(actual[3][0]-(270-(i-1)*135))>0.005)error=PRO_TK_INCOMPLETE;
            if(error)break;
        }
    }
    free(scan);
    if(!error)error=ProMdlSave(created);
    return error;
}

static int study_main(uiCmdCmdId cmd,uiCmdValue *val,void *app){
    study_log=_wfopen(L"study.log",L"wt, ccs=UTF-8");
    if(!study_log){ProEngineerEnd();return 0;}
    wchar_t mode[32]=L"",directory[MAX_PATH]=L"",report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    GetEnvironmentVariableW(L"BWTL_STUDY_MODE",mode,_countof(mode));
    GetEnvironmentVariableW(L"BWTL_STUDY_WORK",directory,_countof(directory));
    ProError error=ProDirectoryChange(directory);
    study_check(L"E-drive workspace",error);
    if(!error&&!wcscmp(mode,L"company")){
        ProPath input;
        ProMdl source=NULL;
        swprintf_s(input,_countof(input),L"%ls\\m26g2000000.asm.123",directory);
        error=ProMdlFiletypeLoad(input,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,&source);
        if(!error)error=ax_learning_save(source,&out);
        study_check(L"55-inch native capture",error);
        wchar_t map[MAX_PATH],pdf[MAX_PATH];
        if(!error&&(!GetEnvironmentVariableW(L"BWTL_STUDY_LAYOUT",map,_countof(map))||
                   !GetEnvironmentVariableW(L"BWTL_STUDY_PDF",pdf,_countof(pdf))))error=PRO_TK_E_NOT_FOUND;
        if(!error)error=ax_study_import_layout(L"M26G2000000",pdf,map,&out);
        study_check(L"PDF digest and table cross-check",error);
        const wchar_t *items[]={L"06",L"07",L"12-上",L"12-下",L"16",L"28"};
        for(int i=0;!error&&i<_countof(items);i++)study_check(items[i],ax_study_card(L"M26G2000000",items[i],&out));
    }else if(!error&&!wcscmp(mode,L"company-variant")){
        study_check(L"company guide bracket with different existing fastener",study_company_bracket(&out));
    }else if(!error&&!wcscmp(mode,L"company-pattern")){
        study_check(L"company fan guards occupy five original stations and repeated replay is idempotent",study_company_pattern(&out));
    }else if(!error&&!wcscmp(mode,L"company-reopen")){
        ProMdl target=NULL;
        int loaded=0;
        error=ax_load_subassembly(L"STUDY_COMPANY_GUIDE",&target,&loaded);
        if(!error)error=ax_study_mark(target,L"M26G2000000",L"ID:40/4995/10",&out);
        study_check(L"company bracket replacement case reopened",error);
    }else if(!error&&!wcscmp(mode,L"capture")){
        ProMdl target=NULL,part=NULL;
        ProAsmcomp placed={0};
        error=study_block(L"STUDY_OLD_PART",35,4,&part);
        if(!error)error=study_assembly(L"STUDY_OLD_ASM",25,&target);
        if(!error)error=ax_study_demo_start(target,&out);
        if(!error)error=study_seed_constraint(target,part,&placed);
        study_check(L"source original placement",error);
        if(!error)error=ax_study_demo_end(target,&out);
        study_check(L"demonstration and native memory save",error);
        if(!error){
            wchar_t id[45],folder[MAX_PATH],key[MAX_PATH];
            ax_path_label(&placed.id,1,id,_countof(id));
            fwprintf(study_log,L"SOURCE_ID=%ls EVENTS=%d\n",id,ax_study_demo.count);
            study_check(L"read persisted native rule immediately",ax_study_card(L"STUDY_OLD_ASM",id,&out));
            if(ax_learning_directory(folder,0)&&swprintf_s(key,MAX_PATH,L"%ls\\study-fixture-id.txt",folder)>=0){
                FILE *f=_wfopen(key,L"wt, ccs=UTF-8");
                if(f){fwprintf(f,L"%ls\n",id);fclose(f);}
            }
        }
    }else if(!error){
        wchar_t folder[MAX_PATH],key[MAX_PATH],id[45]=L"";
        if(ax_learning_directory(folder,0)&&swprintf_s(key,MAX_PATH,L"%ls\\study-fixture-id.txt",folder)>=0){
            FILE *f=_wfopen(key,L"rt, ccs=UTF-8");
            if(f){fgetws(id,_countof(id),f);fclose(f);id[wcscspn(id,L"\r\n")]=0;}
        }
        ProMdl target=NULL,part=NULL;
        if(!id[0])error=PRO_TK_E_NOT_FOUND;
        if(!error&&!wcscmp(mode,L"replay")){
            error=study_block(L"STUDY_NEW_PART",64,9,&part);
            if(!error)error=study_assembly(L"STUDY_NEW_ASM",50,&target);
            if(!error)error=ax_study_apply(target,L"STUDY_OLD_ASM",id,L"自动",NULL,1,&out);
            study_check(L"restart, identify changed part, establish constraints and save",error);
            if(!error){
                AxScan *tree=NULL;
                error=ax_scan(target,0,&tree);
                if(!error&&tree->count!=1)error=PRO_TK_INCOMPLETE;
                if(!error){
                    AxStudyReceipt actual={0};
                    error=ax_study_inspect(target,tree->occurrences[0].ids[0],&actual);
                    if(!error&&fabs(actual.pose[3][0]-41)>0.001)error=PRO_TK_INCOMPLETE;
                    fwprintf(study_log,L"POSE=%.12g\n",actual.pose[3][0]);
                }
                free(tree);
                study_check(L"new mount 50 minus new component datum 9 gives x=41 mm",error);
                study_check(L"native positioning audit",ax_study_audit(target,NULL,NULL,0,&out));
            }
        }else if(!error){
            int loaded=0;
            error=ax_load_subassembly(L"STUDY_NEW_ASM",&target,&loaded);
            if(!error)error=ax_study_mark(target,L"STUDY_OLD_ASM",id,&out);
            study_check(L"independent reopen recognizes saved changed component",error);
            if(!error)study_check(L"reopened assembly audit",ax_study_audit(target,NULL,NULL,0,&out));
        }
        if(!id[0])study_check(L"source memory available after restart",error);
    }
    fwprintf(study_log,L"%ls\nSTUDY_DONE=%d CHECKS=%d\n",report,study_failures,study_checks);
    fclose(study_log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=study_app_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLAssemblyStudyFixture",study_main,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLAssemblyStudyFixture\x0060;");
}
