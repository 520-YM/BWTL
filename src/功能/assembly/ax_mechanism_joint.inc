/* Creo 10.0.0.0 can terminate on repeated native Mechanism edits.
   Keep BWTL motion links in occurrence parameters and apply verified placements. */
typedef struct {
    double low,high,value;
    int bounded;
} AxJointAxis;

typedef struct {
    AxComponent base;
    ProMdl part;
    ProSelection base_ref[3],part_ref[3];
    AxJointAxis axes[2];
    int kind;
    ProMatrix pose;
} AxJoint;

enum {AX_MOTION_ANGLE,AX_MOTION_TRAVEL};
static wchar_t *const ax_joint_param[]={L"BWTL_LINK_V1",L"BWTL_LINK_BASE",L"BWTL_LINK_PART",
    L"BWTL_LINK_ANGLE",L"BWTL_LINK_TRAVEL"};

static int ax_joint_has_axis(int kind,int axis){
    return kind==AX_JOINT_CYLINDER||kind==(axis==AX_MOTION_ANGLE?AX_JOINT_PIN:AX_JOINT_SLIDER);
}

static void ax_joint_free(AxJoint *joint){
    for(int i=0;i<3;i++){
        if(joint->base_ref[i])ProSelectionFree(&joint->base_ref[i]);
        if(joint->part_ref[i])ProSelectionFree(&joint->part_ref[i]);
    }
}

static ProError ax_joint_parameter(ProAsmcomp *feature,int index,ProParameter *parameter,ProParamvalue *value){
    ProError error=ProParameterInit(feature,ax_joint_param[index],parameter);
    if(!error)error=ProParameterValueWithUnitsGet(parameter,value,NULL);
    return error?error:value->type==PRO_PARAM_STRING?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_joint_set_parameter(ProAsmcomp *feature,int index,ProParamvalue *wanted,int create){
    ProParameter parameter;
    ProParamvalue actual={0};
    ProError error=create?ProParameterWithUnitsCreate(feature,ax_joint_param[index],wanted,NULL,&parameter):
        ax_joint_parameter(feature,index,&parameter,&actual);
    if(!error&&!create)error=ProParameterValueWithUnitsSet(&parameter,wanted,NULL);
    if(!error)error=ax_joint_parameter(feature,index,&parameter,&actual);
    return error?error:wcscmp(actual.value.s_val,wanted->value.s_val)?PRO_TK_GENERAL_ERROR:PRO_TK_NO_ERROR;
}

static int ax_joint_axis_value(const AxJointAxis *axis,ProParamvalue *value){
    value->type=PRO_PARAM_STRING;
    return swprintf_s(value->value.s_val,PRO_LINE_SIZE,L"%d %.17g %.17g %.17g",
        axis->bounded,axis->low,axis->high,axis->value)>0;
}

static ProError ax_joint_axis_get(ProAsmcomp *feature,int axis,AxJointAxis *result){
    ProParameter parameter;
    ProParamvalue value={0};
    ProError error=ax_joint_parameter(feature,axis+3,&parameter,&value);
    if(error)return error;
    wchar_t trailing[2];
    int count=swscanf_s(value.value.s_val,L"%d %lg %lg %lg %1ls",&result->bounded,&result->low,
        &result->high,&result->value,trailing,(unsigned)_countof(trailing));
    double limit=axis==AX_MOTION_ANGLE?179.999:10000;
    if(count!=4||(result->bounded!=0&&result->bounded!=1)||!isfinite(result->low)||
       !isfinite(result->high)||!isfinite(result->value)||result->low < -limit||result->high > limit||
       (result->bounded?(result->low>=result->high||result->value<result->low||result->value>result->high):
        (result->low!=0||result->high!=0||result->value!=0)))return PRO_TK_BAD_CONTEXT;
    return PRO_TK_NO_ERROR;
}

static ProError ax_joint_reference_get(ProMdl model,ProAsmcomppath *path,int id,ProType type,ProSelection *selection){
    ProModelitem item;
    ProFeature owner;
    ProFeatStatus status;
    ProError error=ProModelitemInit(model,id,type,&item);
    if(!error)error=ProGeomitemFeatureGet(&item,&owner);
    if(!error)error=ProFeatureStatusGet(&owner,&status);
    if(error||status!=PRO_FEAT_ACTIVE)return error?error:PRO_TK_BAD_CONTEXT;
    return ProSelectionAlloc(path,&item,selection);
}