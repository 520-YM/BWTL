#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize
#include <ProFeatIntr.h>

static ProMdl root;
static ProMdlName asm_name;

typedef struct {ProMdl model;int top,bottom;} ShimFaces;
static ProError shim_fixture_face(ProSurface surface,ProError status,ProAppData context){
    if(status)return status;
    ShimFaces *faces=context;
    ProGeomitem item;
    ProVector point,normal;
    if(ProSurfaceToGeomitem((ProSolid)faces->model,surface,&item)||fd_plane_data(&item,point,normal))return 0;
    if(normal[2]>0.99)faces->top=item.id;
    if(normal[2]<-0.99)faces->bottom=item.id;
    return 0;
}
static int prototype(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl part=NULL;
    ProMdlName part_name=L"FIELD_STOCK";
    ModelPlan stock={0,30,30,8,0,1};
    ProAsmcomp target={0},other={0},distant={0},single={0};
    const char *stage="fieldwork stock";
    FdTxn tx={0};
    PartCommand cmd;
    FdRef sketch={0};
    ProElement section,depth,from,to,parts,row,reference;
    ProSelection selection=NULL;
    ProReference selected=NULL;
    ProFeatintersection *intersections=NULL;
    ProAsmcomppath path;
    ProModelitem item;
    ProError error;
    wchar_t text[PART_TEXT_MAX]=L"";
#define FP(call) do{error=(call);trace(#call,error);if(error)goto done;}while(0)
    FP(quick_seed(part_name,&part));
    FP(model_build_internal(&stock,part_name,&stage,part,NULL,NULL));
    FP(add_component(root,part,-15,&target));
    FP(add_component(root,part,-15,&other));
    FP(add_component(root,part,100,&distant));
    ProMdl unique=NULL;
    ProMdlName unique_name=L"FIELD_UNIQUE";
    FP(quick_seed(unique_name,&unique));
    FP(model_build_internal(&stock,unique_name,&stage,unique,NULL,NULL));
    FP(add_component(root,unique,200,&single));
    wchar_t protocol[1024];
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_FIELD_MARK %ls ID:%d LOCATOR CSYS:PRT_CSYS_DEF 1 2 3 12 2 3",asm_name,single.id);
    FP(command(root,protocol,0,L"复核了两个端点"));
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_FIELD_MARK %ls ID:%d REFUSE_SHARED CSYS:PRT_CSYS_DEF 1 2 3 12 2 3",asm_name,target.id);
    if(!command(root,protocol,1,L"唯一使用")){error=-1;goto done;}
    if(part_command_parse(L"CREO_OP SKETCH_CREATE A FIELD_PROFILE PLANE:ASM_FRONT PLANE:ASM_RIGHT CIRCLE 0 10 3",&cmd)!=1){
        error=PRO_TK_BAD_INPUTS;goto done;
    }
    ProVector center={0,10,0};
    FP(fd_sketch_create_at(&cmd,root,center,text,_countof(text)));
    FP(fd_begin(&tx,root,L"FIELD_CUT",PRO_FEAT_CURVE));
    ProElempath type_path=NULL;
    ProElempathItem item_type={PRO_ELEM_PATH_ITEM_TYPE_ID,{PRO_E_FEATURE_TYPE}};
    ProElement feature_type=NULL;
    FP(ProElempathAlloc(&type_path));
    error=ProElempathDataSet(type_path,&item_type,1);
    if(!error)error=ProElemtreeElementGet(tx.root,type_path,&feature_type);
    ProElempathFree(&type_path);
    if(error)goto done;
    FP(ProElementIntegerSet(feature_type,PRO_FEAT_CUT));
    ProSectionShape shape;
    FP(sd_sketch_ref(root,L"FIELD_PROFILE",&sketch,&shape));
    FP(model_element(tx.root,PRO_E_FEATURE_FORM,1,PRO_EXTRUDE,NULL));
    FP(model_element(tx.root,PRO_E_EXT_SURF_CUT_SOLID_TYPE,1,PRO_EXT_FEAT_TYPE_SOLID,NULL));
    FP(model_element(tx.root,PRO_E_REMOVE_MATERIAL,1,PRO_EXT_MATERIAL_REMOVE,NULL));
    FP(model_element(tx.root,PRO_E_STD_MATRLSIDE,1,PRO_EXT_MATERIAL_SIDE_TWO,NULL));
    FP(model_element(tx.root,PRO_E_FEAT_FORM_IS_THIN,1,PRO_EXT_FEAT_FORM_NO_THIN,NULL));
    FP(model_element(tx.root,PRO_E_STD_DIRECTION,1,PRO_EXT_CR_IN_SIDE_ONE,NULL));
    FP(model_element(tx.root,PRO_E_STD_SECTION,0,0,&section));
    FP(fd_ref_element(&tx,section,PRO_E_SEC_USE_SKETCH,&sketch));
    FP(model_element(tx.root,PRO_E_STD_EXT_DEPTH,0,0,&depth));
    FP(model_element(depth,PRO_E_EXT_DEPTH_FROM,0,0,&from));
    FP(model_element(from,PRO_E_EXT_DEPTH_FROM_TYPE,1,PRO_EXT_DEPTH_FROM_NONE,NULL));
    FP(model_element(depth,PRO_E_EXT_DEPTH_TO,0,0,&to));
    FP(model_element(to,PRO_E_EXT_DEPTH_TO_TYPE,1,PRO_EXT_DEPTH_TO_ALL,NULL));
    FP(fd_create(&tx,0));
    FP(part_regenerate(root));
    ProElement extracted=NULL;
    FP(ProFeatureElemtreeExtract(&tx.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&extracted));
    FP(ProElemtreeWrite(extracted,PRO_ELEMTREE_XML,L"assembly-cut.xml"));
    ProFeatureElemtreeFree(&tx.feature,extracted);
    FP(ProFeatureAsmintersectionsCollect(&tx.feature,&intersections));
    int count=0,level=0,feature=0;
    FP(ProArraySizeGet((ProArray)intersections,&count));
    FP(ProFeatintersectionDataGet(intersections[0],&path,&level,&feature));
    trace("prototype expected path",target.id);
    trace("prototype unselected shared-model path",other.id);
    trace("prototype actual path",path.comp_id_table[0]);
    trace("prototype visibility level",level);
    if(count!=2||level!=0){error=-1;goto done;}
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_FIELD_AUDIT %ls FIELD_CUT",asm_name);
    FP(command(root,protocol,0,L"另有 1 个同模型实例"));
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_FIELD_AUDIT %ls FIELD_PROFILE",asm_name);
    if(!command(root,protocol,1,L"无法确认加工范围")){error=-1;goto done;}
    ShimFaces faces={.model=unique};
    ProSolidBody body;
    FP(qa_single_body(unique,&body));
    FP(ProSolidBodySurfaceVisit(&body,shim_fixture_face,&faces));
    if(!faces.top||!faces.bottom){error=-1;goto done;}
    AxDeviceFrame reference_frame={.axis={{1,0,0},{0,1,0},{0,0,1}}};
    ProOutline bounds;
    FP(ax_device_bounds(unique,&reference_frame,bounds));
    double x=200+0.5*(bounds[0][0]+bounds[1][0]),y=0.5*(bounds[0][1]+bounds[1][1]);
    ProMatrix top_pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{200,0,16,1}};
    ProAsmcomp upper;
    FP(ProAsmcompAssemble((ProSolid)root,unique,top_pose,&upper));
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_FIELD_SHIM %ls FIELD_SHIM ID:%d SURFACE:%d ID:%d SURFACE:%d CSYS:ASM_CSYS %.8g %.8g 12 10",asm_name,single.id,faces.top,upper.id,faces.bottom,x,y);
    FP(command(root,protocol,0,L"实测间隙 8 mm"));
    if(!command(root,protocol,1,NULL)){error=-1;goto done;}
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_FIELD_SHIM %ls OFF_FACE ID:%d SURFACE:%d ID:%d SURFACE:%d CSYS:ASM_CSYS %.8g %.8g 40 10",asm_name,single.id,faces.top,upper.id,faces.bottom,x,y);
    if(!command(root,protocol,1,L"完全容纳")){error=-1;goto done;}
    error=failures?PRO_TK_GENERAL_ERROR:PRO_TK_NO_ERROR;
done:
    if(tx.errors.error_list)for(int i=0;i<tx.errors.error_number&&i<16;i++){
        trace("prototype error element",tx.errors.error_list[i].err_item_id);
        trace("prototype error code",tx.errors.error_list[i].error);
    }
    if(intersections)ProFeatintersectionProarrayFree(intersections);
    fd_ref_free(&sketch);
    fd_cleanup(&tx,error);
    if(selected)ProReferenceFree(selected);
    if(selection)ProSelectionFree(&selection);
    trace("FIELD_PROTOTYPE_RESULT",error);
    ProEngineerEnd();
    return 0;
#undef FP
}
static int prepare(uiCmdCmdId id,uiCmdValue *value,void *data){
    const char *stage="fieldwork assembly";
    model_background_mode=0;
    ProError e=model_create_empty(3,asm_name,&stage);
    if(!e)e=ProMdlnameInit(asm_name,PRO_MDL_ASSEMBLY,&root);
    if(!e)e=ProMacroLoad(L"~ Command `BWTLFieldPrototype`;");
    if(e){trace("fieldwork prepare",e);ProEngineerEnd();}
    return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId id;
    ProError e=ProCmdActionAdd("BWTLFieldPrepare",prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!e)e=ProCmdActionAdd("BWTLFieldPrototype",prototype,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return e?e:ProMacroLoad(L"~ Command `BWTLFieldPrepare`;");
}