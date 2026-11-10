#define BWTL_HARDWARE_FAULT_TEST
#define user_initialize previous_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

typedef struct {AxComponent owner;int shaft,holes[8],n,front,back;double low,high;} HardwareFaces;
static ProError fixture_faces(ProSurface surface,ProError status,ProAppData data){
    HardwareFaces *faces=data;
    if(status)return status;
    ProGeomitem item;
    AxGeometry shape;
    if(ProSurfaceToGeomitem((ProSolid)faces->owner.model,surface,&item)||ax_geometry(&item,&shape))return 0;
    if(shape.type==AX_GEOM_PLANE){
        if(shape.direction[2]>0.999&&fabs(shape.point[2]-faces->high)<0.001)faces->front=item.id;
        if(shape.direction[2]<-0.999&&fabs(shape.point[2]-faces->low)<0.001)faces->back=item.id;
    }else if(shape.type==AX_GEOM_CYLINDER){
        wchar_t token[40];
        int bore;
        ProVector ends[2];
        swprintf_s(token,_countof(token),L"SURFACE:%d",item.id);
        if(ax_hw_cylinder(&faces->owner,token,&shape,ends,&bore))return 0;
        if(!bore)faces->shaft=item.id;
        else if(faces->n<8){
            for(int i=0;i<faces->n;i++){
                ProSurface old;
                ProGeomitem previous;
                AxGeometry axis;
                if(!ProSurfaceInit((ProSolid)faces->owner.model,faces->holes[i],&old)&&
                   !ProSurfaceToGeomitem((ProSolid)faces->owner.model,old,&previous)&&!ax_geometry(&previous,&axis)&&
                   ax_hw_separation(shape.point,axis.point,axis.direction,1)<0.001)return 0;
            }
            faces->holes[faces->n++]=item.id;
        }
    }
    return 0;
}
static ProError find_fixture_faces(ProMdl model,double high,HardwareFaces *faces){
    faces->owner.model=model;faces->high=high;
    return ProSolidSurfaceVisit((ProSolid)model,fixture_faces,NULL,faces);
}
static ProError fixture_hardware(ProMdlName label,double diameter,double t,double bore,ProMdl *model){
    ModelPlan stock={1,diameter,diameter,t,bore,1};
    const char *phase="hardware fixture stock";
    ProError error=quick_seed(label,model);
    return error?error:model_build_internal(&stock,label,&phase,*model,NULL,NULL);
}
static int occurrences(ProMdl assembly,ProMdl model,ProAsmcomp *out,int cap){
    AxScan *scan=NULL;
    if(ax_scan(assembly,1,&scan))return -1;
    int n=0;
    for(int i=0;i<scan->count;i++)if(scan->occurrences[i].depth==1&&scan->occurrences[i].model==model){
        if(n>=cap||ax_component_feature(assembly,scan->occurrences[i].ids[0],out+n)){n=-1;break;}
        n++;
    }
    free(scan);
    return n;
}

static int hardware_run(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl root=NULL,plate=NULL,bolt=NULL,washer=NULL,nut=NULL,pin=NULL,new_bolt=NULL,seed=NULL;
    ProMdlName name=L"",plate_name=L"HW_PLATE";
    const char *stage="hardware setup";
    ProError error;
    ProAsmcomp base,source,bolts[16],duplicate;
    ProMatrix pose;
    double mm;
    int window,n;
    wchar_t b[32],s[32],stack_id[32],one[32],two[32],text[1024],report[PART_TEXT_MAX]=L"";
    PartCommand plate_command;
    PartSnapshot snapshot={0};
#define SETUP(call) do{if((error=(call))){trace("hardware setup " #call,error);check(0,"fixture setup");goto done;}}while(0)
#define DO(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),command(root,text,reject,expected))
    check(part_command_parse(L"CREO_OP QUICK_HINGE HW_PLATE 120 90 10 70 40 14",&plate_command)==1,"four-hole stock protocol");
    SETUP(ProDirectoryCurrentGet(snapshot.directory));
    SETUP(quick_execute(&plate_command,&snapshot,report,_countof(report)));
    SETUP(ProMdlnameInit(plate_name,PRO_MDL_PART,&plate));
    SETUP(fixture_hardware(L"HW_BOLT",10,30,0,&bolt));
    SETUP(fixture_hardware(L"HW_WASHER",24,2,12,&washer));
    SETUP(fixture_hardware(L"HW_NUT",24,6,12,&nut));
    SETUP(fixture_hardware(L"HW_PIN",14.1,8,0,&pin));
    SETUP(fixture_hardware(L"HW_NEW_BOLT",10,35,0,&new_bolt));
    SETUP(fixture_hardware(L"HW_SEED",20,8,0,&seed));
    HardwareFaces p={0},f={0},w={0},u={0},v={0},new_faces={0};
    SETUP(find_fixture_faces(plate,10,&p));
    SETUP(find_fixture_faces(bolt,30,&f));
    SETUP(find_fixture_faces(washer,2,&w));
    SETUP(find_fixture_faces(nut,6,&u));
    SETUP(find_fixture_faces(pin,8,&v));
    SETUP(find_fixture_faces(new_bolt,35,&new_faces));
    check(p.n==4&&p.front&&p.back&&f.shaft&&w.n==1&&w.back&&w.front&&u.n==1&&u.back&&v.shaft,
        "actual four holes and hardware mounting geometry");
    if(p.n!=4||!p.front||!p.back||!f.shaft||!w.n||!u.n||!v.shaft)goto done;
    model_background_mode=0;
    SETUP(model_create_empty(3,name,&stage));
    SETUP(ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&root));
    SETUP(ProMdlWindowGet(root,&window));
    SETUP(ProWindowCurrentSet(window));
    SETUP(qa_mm_per_unit(root,&mm));
    SETUP(add_component(root,plate,0,&base));
    SETUP(add_component(root,seed,1000,&source));
    SETUP(part_regenerate(root));
    ax_path_label(&base.id,1,b,_countof(b));
    ax_path_label(&source.id,1,s,_countof(s));
    ax_batch_fail_after=1;
    DO(1,NULL,L"ASM_ARRAY_LINEAR %ls %ls X 3 45",name,s);
    ax_batch_fail_after=0;
    check(occurrences(root,seed,bolts,16)==1,"failed array discards every staged occurrence");
    DO(0,L"2 个独立实例",L"ASM_ARRAY_LINEAR %ls %ls X 3 45",name,s);
    DO(0,L"3 个独立实例",L"ASM_ARRAY_GRID %ls %ls YZ 2 2 40 35",name,s);
    DO(0,L"3 个独立实例",L"ASM_ARRAY_CIRCULAR %ls %ls Z 0 0 0 4 90",name,s);
    DO(0,L"1 个独立实例",L"ASM_MIRROR_COMPONENT %ls %ls PLANE:ASM_RIGHT HW_NEW_BOLT X",name,s);
    check(occurrences(root,seed,bolts,16)==9,"all pattern members and source exist");
    check(occurrences(root,new_bolt,bolts,16)==1&&!ax_actual_pose(root,&bolts[0],pose)&&
        fabs(pose[3][0]*mm+1000)<0.001&&fabs(pose[0][0]-1)<1e-7,"mirrored counterpart has proper right-handed pose");
    DO(1,L"重合",L"ASM_ARRAY_LINEAR %ls %ls X 3 45",name,s);
    DO(0,L"漏装 4，重复安装 0",L"ASM_FASTENER_AUDIT %ls %ls SURFACE:%d HW_BOLT SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d",
        name,b,p.front,f.shaft,p.holes[0],p.holes[1],p.holes[2],p.holes[3]);
    DO(0,L"2 个独立实例",L"ASM_HOLE_ARRAY %ls %ls SURFACE:%d HW_BOLT SURFACE:%d PLANE:FRONT SURFACE:%d SURFACE:%d",
        name,b,p.front,f.shaft,p.holes[0],p.holes[1]);
    n=occurrences(root,bolt,bolts,16);
    check(n==2,"hole pattern placed only specified holes");
    if(n!=2)goto done;
    ax_path_label(&bolts[0].id,1,one,_countof(one));
    ax_path_label(&bolts[1].id,1,two,_countof(two));
    DO(0,L"板厚 10 mm、垫圈厚 2 mm",L"ASM_FASTENER_STACK %ls %ls SURFACE:%d SURFACE:%d SURFACE:%d HW_BOLT SURFACE:%d PLANE:FRONT HW_WASHER SURFACE:%d SURFACE:%d SURFACE:%d HW_NUT SURFACE:%d SURFACE:%d",
        name,b,p.holes[2],p.front,p.back,f.shaft,w.holes[0],w.back,w.front,u.holes[0],u.back);
    n=occurrences(root,bolt,bolts,16);
    check(n==3&&occurrences(root,washer,&duplicate,1)==1&&occurrences(root,nut,&duplicate,1)==1,"three-part stack exists");
    if(n!=3)goto done;
    ax_path_label(&bolts[2].id,1,stack_id,_countof(stack_id));
    DO(0,L"啮合 6 mm",L"ASM_FASTENER_FIT %ls %ls SURFACE:%d SURFACE:%d SURFACE:%d %ls SURFACE:%d PLANE:FRONT 8 22 6 4",
        name,b,p.holes[2],p.front,p.back,stack_id,f.shaft);
    DO(1,L"超过允许过盈",L"ASM_PRESS_HARDWARE %ls %ls SURFACE:%d HW_PIN SURFACE:%d PLANE:FRONT 0.05 SURFACE:%d",
        name,b,p.front,v.shaft,p.holes[3]);
    DO(0,L"1 个独立实例",L"ASM_PRESS_HARDWARE %ls %ls SURFACE:%d HW_PIN SURFACE:%d PLANE:FRONT 0.2 SURFACE:%d",
        name,b,p.front,v.shaft,p.holes[3]);
    SETUP(ax_actual_pose(root,&bolts[0],pose));
    SETUP(ProAsmcompAssemble((ProAssembly)root,(ProSolid)bolt,pose,&duplicate));
    SETUP(part_regenerate(root));
    DO(0,L"漏装 1，重复安装 1",L"ASM_FASTENER_AUDIT %ls %ls SURFACE:%d HW_BOLT SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d",
        name,b,p.front,f.shaft,p.holes[0],p.holes[1],p.holes[2],p.holes[3]);
    SETUP(model_remove_feature((ProSolid)root,duplicate.id));
    SETUP(part_regenerate(root));
    DO(1,NULL,L"ASM_REPLACE_BATCH %ls HW_BOLT HW_NEW_BOLT SURFACE:%d PLANE:FRONT SURFACE:%d PLANE:FRONT %ls %ls",
        name,f.shaft,new_faces.shaft,one,one);
    DO(1,L"安装接口不一致",L"ASM_REPLACE_BATCH %ls HW_BOLT HW_PIN SURFACE:%d PLANE:FRONT SURFACE:%d PLANE:FRONT %ls %ls",
        name,f.shaft,v.shaft,one,two);
    check(occurrences(root,bolt,bolts,16)==3,"wrong hardware diameter leaves all originals untouched");
    DO(0,L"已批量替换 2 个实例",L"ASM_REPLACE_BATCH %ls HW_BOLT HW_NEW_BOLT SURFACE:%d PLANE:FRONT SURFACE:%d PLANE:FRONT %ls %ls",
        name,f.shaft,new_faces.shaft,one,two);
    check(occurrences(root,bolt,bolts,16)==1&&occurrences(root,new_bolt,bolts,16)==3,"only selected fastener instances replaced");
    DO(0,L"漏装 0，重复安装 0",L"ASM_FASTENER_AUDIT %ls %ls SURFACE:%d HW_NEW_BOLT SURFACE:%d SURFACE:%d SURFACE:%d",
        name,b,p.front,new_faces.shaft,p.holes[0],p.holes[1]);
    check(!part_health(root)&&!ax_recovery_uncertain,"healthy assembly and no uncertain rollback");
done:
    trace("HARDWARE_CASES",cases);
    trace("HARDWARE_FAILURES",failures);
    trace("HARDWARE_DONE",1);
    ProEngineerEnd();
    return 0;
#undef SETUP
#undef DO
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH]=L"";
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLHardwareProbe",hardware_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLHardwareProbe`;");
}
