typedef struct {ProMdl model;ProFeature *members;int count;unsigned char found[64];} UxPatternFaces;
static ProError ux_pattern_face(ProSurface surface,ProError status,ProAppData data){UxPatternFaces *s=data;ProGeomitem item;ProFeature owner;ProError e;if(status)return status;if((e=ProSurfaceToGeomitem((ProSolid)s->model,surface,&item))||(e=ProGeomitemFeatureGet(&item,&owner)))return e;for(int i=0;i<s->count;i++)if(owner.id==s->members[i].id)s->found[i]=1;return 0;}
static ProError ux_pattern(FdTxn *tx,const PartCommand *cmd,int kind){
 ProFeature seed,header,leader,*members=NULL;ProPattern pattern;ProElement branch,dir,compound,first;FdRef a={0},b={0};ProError e;int count=0,n1=0,n2=0,created=0;double increment=0;ProVector p,normal,other;
#define UP(call) do{if((e=(call))){trace("reuse pattern " #call,e);goto done;}}while(0)
 UP(ux_plain_seed(tx->model,cmd->arg[3],&seed));UP(ProElementFree(&tx->root));UP(ProElementAlloc(PRO_E_PATTERN_ROOT,&tx->root));
 UP(model_element(tx->root,PRO_E_GENPAT_TYPE,1,kind==UX_GRID?PRO_GENPAT_DIR_DRIVEN:kind==UX_CURVE?PRO_GENPAT_CRV_DRIVEN:PRO_GENPAT_POINT_DRIVEN,NULL));UP(model_element(tx->root,PRO_E_GENPAT_REGEN_METHOD,1,PRO_PAT_GENERAL,NULL));
 if(kind==UX_GRID){
  UP(fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&a));UP(fd_ref_resolve(tx->model,cmd->arg[8],PRO_SURFACE,&b));UP(fd_plane_data(&a.item,p,normal));UP(fd_plane_data(&b.item,p,other));if(fabs(ad_dot(normal,other))>1e-6){e=PRO_TK_BAD_INPUTS;goto done;}
  UP(model_element(tx->root,PRO_E_GENPAT_DIR,0,0,&branch));part_integer(cmd->arg[5],&n1);part_integer(cmd->arg[9],&n2);count=n1*n2;
  for(int i=0;i<2;i++){
   UP(model_element(branch,i?PRO_E_GENPAT_DIR2:PRO_E_GENPAT_DIR1,0,0,&dir));UP(model_element(dir,PRO_E_DIRECTION_COMPOUND,0,0,&compound));UP(fd_ref_element(tx,compound,PRO_E_DIRECTION_REFERENCE,i?&b:&a));UP(model_element(compound,PRO_E_DIRECTION_FLIP,1,PRO_DIRECTION_FLIP_ALONG,NULL));
   UP(model_element(branch,i?PRO_E_DIR_PAT_DIR2_OPT:PRO_E_DIR_PAT_DIR1_OPT,1,-1,NULL));UP(model_element(branch,i?PRO_E_DIR_PAT_DIR2_FLIP:PRO_E_DIR_PAT_DIR1_FLIP,1,!wcscmp(cmd->arg[i?11:7],L"NEG"),NULL));UP(model_element(branch,i?PRO_E_GENPAT_DIR2_INC:PRO_E_GENPAT_DIR1_INC,2,as_arg(cmd,i?10:6)*tx->scale,NULL));
   UP(model_element(branch,i?PRO_E_GENPAT_DIM_SECOND_DIR:PRO_E_GENPAT_DIM_FIRST_DIR,0,0,&first));UP(model_element(branch,i?PRO_E_GENPAT_DIM_SECOND_DIR_NUM_INST:PRO_E_GENPAT_DIM_FIRST_DIR_NUM_INST,1,i?n2:n1,NULL));
  }
 }else if(kind==UX_CURVE){
  UP(sd_profile_curve(tx->model,cmd->arg[4]+6,&a));UP(model_element(tx->root,PRO_E_GENPAT_CURVE,0,0,&branch));
  int numbered=!wcscmp(cmd->arg[5],L"COUNT");if(numbered)part_integer(cmd->arg[6],&count);else increment=as_arg(cmd,6)*tx->scale;
  UP(model_element(branch,PRO_E_GENPAT_CRV_PLC_TYPE,1,numbered?1:0,NULL));UP(model_element(branch,PRO_E_GENPAT_CRV_NUMBER,1,numbered?count:0,NULL));UP(model_element(branch,PRO_E_GENPAT_CRV_SPACING,2,increment,NULL));UP(model_element(branch,PRO_E_GENPAT_CRV_FLIP,1,!wcscmp(cmd->arg[7],L"NEG"),NULL));
 }else{
  ProFeature sketch;UP(ux_named_feature(tx->model,cmd->arg[4]+7,&sketch));a.item=sketch;a.owner_feature=sketch.id;UP(ProSelectionAlloc(NULL,(ProModelitem *)&a.item,&a.selection));
  UP(model_element(tx->root,PRO_E_GENPAT_POINT,0,0,&branch));UP(model_element(branch,PRO_E_GENPAT_POINT_REF_TYPE,1,PRO_GENPAT_REF_SKETCH,NULL));UP(model_element(tx->root,PRO_E_GENPAT_OFFSET_FROM_SKET,1,0,NULL));
 }
 if(kind!=UX_GRID){
  /* The installed TestPatternCreate SDK sample binds external sketches through
     STD_SECTION/SEC_USE_SKETCH; CRV_PNT_REF alone leaves an incomplete sketch. */
  ProElement section;UP(model_element(tx->root,PRO_E_STD_SECTION,0,0,&section));UP(model_element(section,PRO_E_STD_SEC_METHOD,1,0,NULL));UP(fd_ref_element(tx,section,PRO_E_SEC_USE_SKETCH,&a));UP(model_element(tx->root,PRO_E_GENPAT_CRV_OR_TYPE,1,1,NULL));
 }
 {ProFeature reference;int seed_number,reference_number;UP(ProFeatureNumberGet(&seed,&seed_number));for(int i=0;i<(kind==UX_GRID?2:1);i++){UP(ProFeatureInit((ProSolid)tx->model,i?b.owner_feature:a.owner_feature,&reference));UP(ProFeatureNumberGet(&reference,&reference_number));if(reference_number>=seed_number){tx->stage="pattern reference must precede seed in regeneration order";e=PRO_TK_BAD_CONTEXT;goto done;}}}
 e=ProPatternCreate(&seed,PRO_FEAT_PATTERN,tx->root);if(!ProFeaturePatternGet(&seed,PRO_FEAT_PATTERN,&pattern))created=1;if(e){trace("reuse ProPatternCreate",e);goto done;}if(!created){e=PRO_TK_GENERAL_ERROR;goto done;}
 UP(ProPatternHeaderGet(&pattern,&header));UP(ProModelitemNameSet((ProModelitem *)&header,tx->name));
#ifdef BWTL_REUSE_DIAGNOSTIC_TEST
 {ProElement native=NULL;ProPath path;swprintf_s(path,PRO_PATH_SIZE,L"%ls_request.xml",tx->name);ProElemtreeWrite(tx->root,PRO_ELEMTREE_XML,path);ProError extraction=ProPatternElemtreeCreate(&seed,PRO_FEAT_PATTERN,&native);trace("native pattern extraction",extraction);if(!extraction){swprintf_s(path,PRO_PATH_SIZE,L"%ls_native.xml",tx->name);ProElemtreeWrite(native,PRO_ELEMTREE_XML,path);ProElementFree(&native);}}
#endif
 UP(part_regenerate(tx->model));UP(ProPatternLeaderGet(&pattern,&leader));UP(ProPatternMembersGet(&pattern,&members));int actual;UP(ProArraySizeGet((ProArray)members,&actual));
 if(leader.id!=seed.id||actual<2||actual>64||(count&&count!=actual)){trace("reuse pattern actual count",actual);e=PRO_TK_GENERAL_ERROR;goto done;}
 UxPatternFaces faces={0};faces.model=tx->model;faces.members=members;faces.count=actual;UP(ProSolidSurfaceVisit((ProSolid)tx->model,ux_pattern_face,NULL,&faces));
 for(int i=0;i<actual;i++){ProFeatStatus status;UP(ProFeatureStatusGet(&members[i],&status));if(status!=PRO_FEAT_ACTIVE||!faces.found[i]){e=PRO_TK_BAD_CONTEXT;goto done;}}
 tx->feature=header;
done:if(e&&created&&(ProPatternDelete(&pattern)||part_regenerate(tx->model)))tx->rollback_ok=0;if(members)ProArrayFree((ProArray *)&members);fd_ref_free(&a);fd_ref_free(&b);return e;
#undef UP
}
