#define user_initialize previous_sheetmetal_initialize
#include "native_sheetmetal.c"
#undef user_initialize
#undef CASE

static ProMdl continuation_plate;
static ProMdlName continuation_name;

static int continuation_case(int reject,const wchar_t *expected,const wchar_t *format,...){
    wchar_t protocol[1800],reply[PART_TEXT_MAX]=L"";
    PartCommand cmd;
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    if(!snapshot){failures++;return 0;}
    va_list args;
    va_start(args,format);
    int size=vswprintf_s(protocol,_countof(protocol),format,args);
    va_end(args);
    ProError error=part_snapshot_take_mode(continuation_plate,snapshot,0);
    if(!error&&(size<0||part_command_parse(protocol,&cmd)!=1))error=PRO_TK_BAD_INPUTS;
    if(!error)error=part_execute(&cmd,snapshot,reply,_countof(reply));
    int ok=(reject?error!=0:error==0)&&(!expected||wcsstr(reply,expected));
    FILE *file=_wfopen(L"workshop-continuation.log",L"a, ccs=UTF-8");
    if(file){fwprintf(file,L"%ls\nRESULT=%d PASS=%d\n%ls\n",protocol,error,ok,reply);fclose(file);}
    part_snapshot_clear(snapshot);
    free(snapshot);
    cases++;
    if(!ok)failures++;
    return ok;
}

static int continuation_face(ProMdl model,int sign){
    SmxGeometry *geometry=calloc(1,sizeof(*geometry));
    if(!geometry)return 0;
    int id=0;
    double largest=0;
    if(!smx_geometry(model,geometry)){
        for(int i=0;i<geometry->count;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type==PRO_SRF_PLANE&&sign*face->axis[2]>0.99999&&face->area>largest){
                id=face->id;
                largest=face->area;
            }
        }
    }
    free(geometry);
    return id;
}

static int continuation_bore(ProMdl model,double radius){
    SmxGeometry *geometry=calloc(1,sizeof(*geometry));
    if(!geometry)return 0;
    int id=0;
    if(!smx_geometry(model,geometry)){
        for(int i=0;i<geometry->count;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type==PRO_SRF_CYL&&fabs(face->radius-radius)<0.0001){id=face->id;break;}
        }
    }
    free(geometry);
    return id;
}

static int continuation_mount_bores(ProMdl model,int *first,int *second){
    SmxGeometry *geometry=calloc(1,sizeof(*geometry));
    if(!geometry)return 0;
    *first=0;
    *second=0;
    if(!smx_geometry(model,geometry)){
        for(int i=0;i<geometry->count;i++){
            const SmxFace *face=&geometry->faces[i];
            if(face->type!=PRO_SRF_CYL||fabs(face->radius-2)>0.0001)continue;
            if(fabs(face->point[0]-23)<0.0001&&fabs(face->point[1]-13)<0.0001)*first=face->id;
            if(fabs(face->point[0]-57)<0.0001&&fabs(face->point[1]-47)<0.0001)*second=face->id;
        }
    }
    free(geometry);
    return *first&&*second;
}

static int continuation_run(uiCmdCmdId id,uiCmdValue *value,void *context){
    const char *stage="workshop continuation";
    ProError error=0;
    ProMdl other;
    ProMdlName other_name;
    ProMdlName warmup;
    ModelPlan plate={0,80,60,2,0,1};
#define MUST(call) do{error=(call);if(error){failures++;trace(#call,error);goto done;}}while(0)
#define CASE(reject,expected,format,...) continuation_case(reject,expected,L"CREO_OP " format,__VA_ARGS__)
    MUST(model_create_empty(2,warmup,&stage));
    MUST(model_create(&plate,other_name,&stage));
    MUST(ProMdlnameInit(other_name,PRO_MDL_PART,&other));
    continuation_plate=other;
    CASE(0,NULL,L"DATUM_CSYS %ls CASE_ZERO CSYS:PRT_CSYS_DEF 0 0 0 0 0 0",other_name);
    CASE(0,NULL,L"DATUM_CSYS %ls CASE_SEAL CSYS:PRT_CSYS_DEF 0 0 -5 0 0 0",other_name);
    MUST(model_create(&plate,continuation_name,&stage));
    MUST(ProMdlnameInit(continuation_name,PRO_MDL_PART,&continuation_plate));
    int top=continuation_face(continuation_plate,1),other_top=continuation_face(other,1),other_bottom=continuation_face(other,-1);
    if(!top||!other_top||!other_bottom){failures++;error=PRO_TK_BAD_CONTEXT;goto done;}
    CASE(0,NULL,L"DATUM_CSYS %ls DOOR_ZERO CSYS:PRT_CSYS_DEF 0 0 0 0 0 0",continuation_name);
    CASE(0,NULL,L"SMT_CONVERT %ls WALL SURFACE:%d 2",continuation_name,top);
    top=continuation_face(continuation_plate,1);
    CASE(0,L"偏差 0 mm",L"SMY_CASE_FLUSH %ls %ls CSYS:DOOR_ZERO SURFACE:%d CSYS:CASE_ZERO SURFACE:%d 40 30 0 0.1",continuation_name,other_name,top,other_top);
    CASE(0,L"超限",L"SMY_CASE_FLUSH %ls %ls CSYS:DOOR_ZERO SURFACE:%d CSYS:CASE_SEAL SURFACE:%d 40 30 0 0.1",continuation_name,other_name,top,other_top);
    CASE(1,L"有效板面",L"SMY_CASE_FLUSH %ls %ls CSYS:DOOR_ZERO SURFACE:%d CSYS:CASE_ZERO SURFACE:%d 400 30 0 0.1",continuation_name,other_name,top,other_top);
    CASE(0,L"压缩率 25%",L"SMY_CASE_SEAL %ls %ls CSYS:DOOR_ZERO SURFACE:%d CSYS:CASE_SEAL SURFACE:%d 40 30 4 20 30 20 10",continuation_name,other_name,top,other_bottom);
    CASE(0,L"超限",L"SMY_CASE_SEAL %ls %ls CSYS:DOOR_ZERO SURFACE:%d CSYS:CASE_SEAL SURFACE:%d 40 30 4 10 20 20 10",continuation_name,other_name,top,other_bottom);
    CASE(1,L"密封区域",L"SMY_CASE_SEAL %ls %ls CSYS:DOOR_ZERO SURFACE:%d CSYS:CASE_SEAL SURFACE:%d 40 30 4 20 30 200 10",continuation_name,other_name,top,other_bottom);
    CASE(0,L"退出行程取样未见侵入",L"SMY_TOOL_WITHDRAWAL %ls CSYS:DOOR_ZERO 40 30 5 20 10 8 2 0.1 RECT",continuation_name);
    CASE(0,L"退出途中已确认干涉",L"SMY_TOOL_WITHDRAWAL %ls CSYS:DOOR_ZERO 40 30 5 -5 10 10 2 0.1 ROUND",continuation_name);
    CASE(0,L"退出终点已干涉",L"SMY_TOOL_WITHDRAWAL %ls CSYS:DOOR_ZERO 40 30 5 1 10 8 2 0.1 RECT",continuation_name);
    {
        FILE *rules=_wfopen(L"BWTL_SMT_VENT_INTERFACES.tsv",L"wt, ccs=UTF-8");
        if(!rules){error=PRO_TK_GENERAL_ERROR;failures++;goto done;}
        fputws(L"FAN F20 20 0 34 34 4 40 40 1.5 2.5 A\nFILTER T24 24 20 36 34 4 42 40 1.5 2.5 B\nHOOD H24 24 14 34 28 4 40 34 1.5 2.5 C\nHOOD TOO_THICK 24 14 34 28 4 40 34 3 4 D\n",rules);
        if(fclose(rules)){error=PRO_TK_GENERAL_ERROR;failures++;goto done;}
        const wchar_t *kinds[]={L"FAN",L"FILTER",L"HOOD"};
        const wchar_t *items[]={L"F20",L"T24",L"H24"};
        for(int i=0;i<3;i++){
            MUST(model_create(&plate,continuation_name,&stage));
            MUST(ProMdlnameInit(continuation_name,PRO_MDL_PART,&continuation_plate));
            top=continuation_face(continuation_plate,1);
            CASE(0,NULL,L"DATUM_CSYS %ls VENT_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",continuation_name);
            CASE(0,NULL,L"SMT_CONVERT %ls WALL SURFACE:%d 2",continuation_name,top);
            top=continuation_face(continuation_plate,1);
            if(i==0){
                CASE(1,L"未登记",L"SMY_VENT_INTERFACE %ls MISSING SURFACE:%d CSYS:VENT_AT PLANE:RIGHT FAN UNKNOWN 40 30 1",continuation_name,top);
                CASE(1,L"接口外框未通过",L"SMY_VENT_INTERFACE %ls ON_EDGE SURFACE:%d CSYS:VENT_AT PLANE:RIGHT FAN F20 5 30 1",continuation_name,top);
                CASE(1,L"超出接口允许",L"SMY_VENT_INTERFACE %ls WRONG_GAUGE SURFACE:%d CSYS:VENT_AT PLANE:RIGHT HOOD TOO_THICK 40 30 1",continuation_name,top);
            }
            CASE(0,L"已创建 5 个贯穿轮廓",L"SMY_VENT_INTERFACE %ls VENTS SURFACE:%d CSYS:VENT_AT PLANE:RIGHT %ls %ls 40 30 1",
                continuation_name,top,kinds[i],items[i]);
            if(i==0){
                top=continuation_face(continuation_plate,1);
                int bore=continuation_bore(continuation_plate,2);
                if(!top||!bore){failures++;error=PRO_TK_E_NOT_FOUND;goto done;}
                CASE(0,L"符合",L"SMY_FINISH_PROTECTION %ls SURFACE:%d CSYS:VENT_AT 8 8 4 4 2 1 2",continuation_name,top);
                CASE(0,L"超限",L"SMY_FINISH_PROTECTION %ls SURFACE:%d CSYS:VENT_AT 40 30 6 6 2 1 2",continuation_name,top);
                CASE(0,L"超限",L"SMY_FINISH_PROTECTION %ls SURFACE:%d CSYS:VENT_AT 8 8 4 4 2 1 0.5",continuation_name,top);
                CASE(0,L"符合",L"SMY_COATING_POINT %ls MASK SURFACE:%d SURFACE:%d 4.4 0.2 0.6 2",continuation_name,top,bore);
                CASE(0,L"符合",L"SMY_COATING_POINT %ls HANG SURFACE:%d SURFACE:%d 3.6 0.2 0.6 2",continuation_name,top,bore);
                CASE(0,L"超限",L"SMY_COATING_POINT %ls HANG SURFACE:%d SURFACE:%d 4.2 0.2 0.6 2",continuation_name,top,bore);
                FILE *mount=_wfopen(L"BWTL_SMT_MOUNT_CHECKS.tsv",L"wt, ccs=UTF-8");
                if(!mount){error=PRO_TK_GENERAL_ERROR;failures++;goto done;}
                fputws(L"WALL A 23 13 3.9 4.1 0.1 2 V1\nWALL A 57 47 3.9 4.1 0.1 2 V1\n"
                    L"HANG A 23 13 3.9 4.1 0.1 2 V2\nHANG A 57 47 3.9 4.1 0.1 2 V2\n"
                    L"SUPPORT A 23 13 3.9 4.1 0.1 2 V3\nSUPPORT A 57 47 3.9 4.1 0.1 2 V3\n",mount);
                if(fclose(mount)){error=PRO_TK_GENERAL_ERROR;failures++;goto done;}
                int first,second;
                if(!continuation_mount_bores(continuation_plate,&first,&second)){
                    failures++;error=PRO_TK_E_NOT_FOUND;goto done;
                }
                CASE(0,L"共 2 孔，超限 0",L"SMY_MOUNT_INSPECTION %ls WALL A SURFACE:%d CSYS:VENT_AT SURFACE:%d SURFACE:%d",continuation_name,top,first,second);
                CASE(0,L"共 2 孔，超限 0",L"SMY_MOUNT_INSPECTION %ls HANG A SURFACE:%d CSYS:VENT_AT SURFACE:%d SURFACE:%d",continuation_name,top,first,second);
                CASE(0,L"共 2 孔，超限 0",L"SMY_MOUNT_INSPECTION %ls SUPPORT A SURFACE:%d CSYS:VENT_AT SURFACE:%d SURFACE:%d",continuation_name,top,first,second);
                CASE(0,L"共 2 孔，超限 2",L"SMY_MOUNT_INSPECTION %ls WALL A SURFACE:%d CSYS:VENT_AT SURFACE:%d SURFACE:%d",continuation_name,top,second,first);
                CASE(1,NULL,L"SMY_MOUNT_INSPECTION %ls WALL A SURFACE:%d CSYS:VENT_AT SURFACE:%d SURFACE:%d",continuation_name,top,first,first);
                CASE(1,L"模板须登记",L"SMY_MOUNT_INSPECTION %ls WALL MISSING SURFACE:%d CSYS:VENT_AT SURFACE:%d SURFACE:%d",continuation_name,top,first,second);
            }
        }
    }
    {
        ShFixture refs;
        MUST(model_create_empty(2,continuation_name,&stage));
        MUST(ProMdlnameInit(continuation_name,PRO_MDL_PART,&continuation_plate));
        CASE(0,NULL,L"SMT_SKETCH %ls BLANK PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",continuation_name);
        CASE(0,NULL,L"SMT_FIRST_WALL %ls WALL SKETCH:BLANK 2 POS",continuation_name);
        CASE(0,NULL,L"SMT_SKETCH %ls BEND_LINE PLANE:FRONT PLANE:RIGHT LINE 0 40 80 40",continuation_name);
        CASE(0,NULL,L"SMT_BEND %ls BEND CURVE:BEND_LINE 90 2 POS FIX1",continuation_name);
        sh_test_refs(continuation_plate,&refs);
        CASE(0,NULL,L"SMT_SKETCH %ls BEAD_PROFILE SURFACE:%d PLANE:RIGHT RECT 20 -20 12 8",continuation_name,refs.face);
        CASE(0,NULL,L"SMT_FORM %ls BEAD SKETCH:BEAD_PROFILE 4 1 POS",continuation_name);
        FabBends bends={0};
        MUST(smy_bends_load(continuation_plate,&bends));
        ProFeature bend;
        MUST(cm_feature_find(continuation_plate,L"BEND",&bend));
        refs.bend=0;
        for(int i=0;i<bends.count;i++)if(bends.rows[i].id==bend.id)refs.bend=bends.rows[i].surface;
        if(!refs.bend){failures++;error=PRO_TK_BAD_CONTEXT;goto done;}
        CASE(0,L"符合",L"SMY_FORM_INTERACTION %ls BEAD SURFACE:%d 1",continuation_name,refs.bend);
        CASE(0,L"超限",L"SMY_FORM_INTERACTION %ls BEAD SURFACE:%d 100",continuation_name,refs.bend);
        CASE(1,L"未识别",L"SMY_FORM_INTERACTION %ls BEAD SURFACE:%d 5",continuation_name,refs.face);
    }
    {
        MUST(model_create_empty(2,continuation_name,&stage));
        MUST(ProMdlnameInit(continuation_name,PRO_MDL_PART,&continuation_plate));
        CASE(0,NULL,L"SMT_SKETCH %ls BLANK PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",continuation_name);
        CASE(0,NULL,L"SMT_FIRST_WALL %ls WALL SKETCH:BLANK 2 POS",continuation_name);
        CASE(0,NULL,L"SMT_SKETCH %ls BEND_LINE PLANE:FRONT PLANE:RIGHT LINE 0 40 80 40",continuation_name);
        CASE(0,NULL,L"SMT_BEND %ls BEND CURVE:BEND_LINE 90 2 POS FIX1",continuation_name);
        ProPath directory,leaf;
        MUST(ProDirectoryCurrentGet(directory));
        MUST(dv_export_file(continuation_plate,directory,PRO_INTF_EXPORT_STEP,leaf));
        CASE(0,L"导入独立零件",L"DV_IMPORT STEP_FOLDED %ls",leaf);
        MUST(ProMdlnameInit(L"STEP_FOLDED",PRO_MDL_PART,&continuation_plate));
        int top=continuation_face(continuation_plate,1);
        if(!top){failures++;error=PRO_TK_E_NOT_FOUND;goto done;}
        CASE(1,NULL,L"SMX_REBUILD STEP_FOLDED BAD_GAUGE SURFACE:%d 4",top);
        CASE(0,L"1 条折弯线",L"SMX_REBUILD STEP_FOLDED REBUILT SURFACE:%d 2",top);
        CASE(1,NULL,L"SMX_REBUILD STEP_FOLDED REBUILT SURFACE:%d 2",top);
    }
done:
    trace("CONTINUATION_CASES",cases);
    trace("CONTINUATION_FAILURES",failures);
    trace("CONTINUATION_DONE",error);
    ProEngineerEnd();
    return 0;
#undef CASE
#undef MUST
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLWorkshopContinuation",continuation_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \140BWTLWorkshopContinuation\140;");
}
