static int replay_quilt_solid_definition(ProElement tree,ProFeattype type,const wchar_t **operation)
{
    ProElement element;
    int option;
    if(type!=PRO_FEAT_PROTRUSION&&type!=PRO_FEAT_CUT)return 0;
    if(!mc_tree_element(tree,(ProElemId[]){PRO_E_STD_USEQLT_QLT},1,&element)){
        *operation=type==PRO_FEAT_CUT?L"曲面加厚去料":L"曲面加厚加料";
        if(!replay_tree_integer(tree,PRO_E_SRF_OFFS_METHOD,&option)&&option!=PRO_OFFS_METH_NORMTOSURF)return 0;
        if(!replay_tree_integer(tree,PRO_E_STD_USEQLT_SIDE,&option)&&option!=PRO_THICKEN_SIDE_ONE&&
           option!=PRO_THICKEN_SIDE_TWO&&option!=PRO_THICKEN_BOTH_SIDES)return 0;
        double thickness=0;
        return !mc_tree_element(tree,(ProElemId[]){PRO_E_THICKNESS},1,&element)&&
            !ProElementDoubleGet(element,NULL,&thickness)&&isfinite(thickness)&&thickness>0;
    }
    *operation=type==PRO_FEAT_CUT?L"曲面实体化去料":L"曲面实体化加料";
    return !mc_tree_element(tree,(ProElemId[]){PRO_E_PATCH_QUILT},1,&element)&&
        (replay_tree_integer(tree,PRO_E_PATCH_TYPE,&option)||option==type);
}

typedef struct {
    ProFeature previous;
    int quilt,found;
} ReplaySymmetricQuilt;

static ProError replay_find_symmetric_quilt(ProFeature *feature,ProError status,ProAppData data)
{
    if(status)return status;
    ReplaySymmetricQuilt *search=data;
    ProFeattype type;
    ProFeatStatus state;
    ProError error=ProFeatureStatusGet(feature,&state);
    if(!error)error=ProFeatureTypeGet(feature,&type);
    if(error||state!=PRO_FEAT_ACTIVE)return error;
    if((type==PRO_FEAT_PROTRUSION||type==PRO_FEAT_CUT)&&search->previous.id){
        ProElement tree=NULL,element;
        ProReference reference=NULL;
        int side,id;
        ProType kind;
        ProMdl owner;
        if(!ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)&&
           !replay_tree_integer(tree,PRO_E_STD_USEQLT_SIDE,&side)&&side==PRO_THICKEN_BOTH_SIDES&&
           !mc_tree_element(tree,(ProElemId[]){PRO_E_STD_USEQLT_QLT},1,&element)&&
           !ProElementReferenceGet(element,NULL,&reference)&&!ProReferenceIdGet(reference,&id)&&
           id==search->quilt&&!ProReferenceTypeGet(reference,&kind)&&kind==PRO_QUILT&&
           !ProReferenceOwnerGet(reference,&owner)&&owner==feature->owner)search->found=1;
        if(reference)ProReferenceFree(reference);
        if(tree)ProFeatureElemtreeFree(feature,tree);
    }
    if(search->found)return PRO_TK_E_FOUND;
    search->previous=*feature;
    return PRO_TK_NO_ERROR;
}

/* Creo 10 can retain a source quilt whose live faces no longer expose area
   after symmetric thickening. Measure that same reference before the feature
   in a temporary history model and still compare the resulting solid normally. */
static ProError replay_symmetric_quilt_area(ProMdl model,int quilt,double *measured)
{
    ReplaySymmetricQuilt search={.quilt=quilt};
    ProError error=ProSolidFeatVisit((ProSolid)model,replay_find_symmetric_quilt,NULL,&search);
    if(error!=PRO_TK_E_FOUND||!search.found)return PRO_TK_E_NOT_FOUND;
    static volatile LONG serial;
    ProMdlName name;
    swprintf_s(name,_countof(name),L"bwtl_area_%08lx_%08lx",GetCurrentProcessId(),
        (unsigned long)InterlockedIncrement(&serial));
    ProMdl history=NULL;
    ProFeature previous;
    ProQuilt original;
    SfArea area={0};
    error=ProMdlnameCopy(model,name,&history);
    if(!error)error=ProFeatureInit((ProSolid)history,search.previous.id,&previous);
    if(!error)error=ProFeatureInsertmodeWithoptionsActivate(&previous,PRO_REGEN_NO_RESOLVE_MODE);
    if(!error)error=ProQuiltInit((ProSolid)history,quilt,&original);
    if(!error)error=ProQuiltSurfaceVisit(original,sf_surface_area,NULL,&area);
    if(!error&&(!area.count||!isfinite(area.area)||area.area<=0))error=PRO_TK_INCOMPLETE;
    if(history){
        ProError cleanup=ProMdlErase(history);
        if(!error)error=cleanup;
    }
    if(!error)*measured=area.area;
    return error;
}

