#include "imitation_replay_refs.inc"
#include "imitation_replay_state.inc"
#include "imitation_replay_ref_pattern.inc"
#include "imitation_replay_sketch.inc"
#include "imitation_replay_surface_collection.inc"
#include "imitation_replay_chamfer.inc"

/* Each call builds a new element tree. Section objects and model
   references are allocated in the target's own modeling context. */
static ProError replay_element(ImitationReplay *replay,ProElement old,ProElement *result){
    ProElemId id;
    ProBoolean compound=0,array=0,multi=0;
    ProError error=ProElementIdGet(old,&id);
    if(!error)error=ProElementIsCompound(old,NULL,&compound);
    if(!error)error=ProElementIsArray(old,NULL,&array);
    if(!error)error=ProElementIsMultival(old,NULL,&multi);
    if(error)return error;
    if(id==PRO_E_SKETCHER||id==PRO_E_RNDCH_AMBIGUITY||id==PRO_E_STD_SEC_SELECT||id==PRO_E_HOLE_PARAMETERS||id==PRO_E_HOLE_NOTE||
       id==PRO_E_DTMPLN_FIT||id==PRO_E_GENPAT_RELATION_EDIT)return PRO_TK_NO_ERROR;
    if(id==PRO_E_GENPAT_DIR_VAR_TYPE){
        int variation;
        error=ProElementIntegerGet(old,NULL,&variation);
        if(error)return error;
        if(variation!=PRO_PAT_VALUE_DRIVEN)return PRO_TK_UNSUPPORTED;
    }
    ProElement fresh=NULL;
    if((error=ProElementAlloc(id,&fresh)))return error;
    if(compound||array){
        ProElement *children=NULL,*new_array=NULL;
        int count=0;
        error=ProArrayAlloc(0,sizeof(ProElement),1,(ProArray *)&children);
        if(!error)error=array?ProElementArrayGet(old,NULL,&children):ProElementChildrenGet(old,NULL,&children);
        if(error==PRO_TK_EMPTY||error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
        if(!error&&children)error=ProArraySizeGet((ProArray)children,&count);
        if(!error&&array)error=ProArrayAlloc(0,sizeof(ProElement),1,(ProArray *)&new_array);
        for(int i=0;!error&&i<count;i++){
            ProElement next=NULL;
            error=replay_element(replay,children[i],&next);
            if(!error&&next){
                error=array?ProArrayObjectAdd((ProArray *)&new_array,PRO_VALUE_UNUSED,1,&next):
                    ProElemtreeElementAdd(fresh,NULL,next);
                if(error)ProElementFree(&next);
            }
        }
        if(!error&&array)error=ProElementArraySet(fresh,NULL,new_array);
        if(new_array){
            if(error){
                int size=0;
                ProArraySizeGet((ProArray)new_array,&size);
                for(int i=0;i<size;i++)ProElementFree(&new_array[i]);
            }
            ProArrayFree((ProArray *)&new_array);
        }
        if(children)ProArrayFree((ProArray *)&children);
    }else if(id==PRO_E_STD_CURVE_COLLECTION_APPL){
        error=replay_curve_collection(replay,old,fresh);
    }else if(id==PRO_E_STD_SURF_COLLECTION_APPL){
        error=replay_surface_collection(replay,old,fresh);
    }else if(multi||id==PRO_E_SMT_FLAT_WALL_ATT_EDGE){
        error=replay_reference_array(replay,old,fresh);
        if(error==PRO_TK_EMPTY){ProElementFree(&fresh);return PRO_TK_NO_ERROR;}
    }else{
        ProValueDataType type;
        error=ProElementValuetypeGet(old,&type);
        if(error==PRO_TK_EMPTY||error==PRO_TK_E_NOT_FOUND){
            ProElementFree(&fresh);
            return PRO_TK_NO_ERROR;
        }
        if(!error)switch(type){
        case PRO_VALUE_TYPE_INT:{
            int value;
            error=ProElementIntegerGet(old,NULL,&value);
            if(!error)error=ProElementIntegerSet(fresh,value);
            break;
        }
        case PRO_VALUE_TYPE_DOUBLE:{
            double value;
            int decimals=9;
            error=ProElementDoubleGet(old,NULL,&value);
            if(!error){
                int original;
                if(!ProElementDecimalsGet(old,&original)&&original>=0&&original<=9)decimals=original;
                error=ProElementDecimalsSet(fresh,decimals);
            }
            if(!error)error=ProElementDoubleSet(fresh,value);
            break;
        }
        case PRO_VALUE_TYPE_WSTRING:{
            wchar_t *value=NULL;
            error=ProElementWstringGet(old,NULL,&value);
            if(!error)error=value&&*value?ProElementWstringSet(fresh,value):PRO_TK_EMPTY;
            if(value)ProWstringFree(value);
            break;
        }
        case PRO_VALUE_TYPE_BOOLEAN:{
            ProBoolean value;
            error=ProElementBooleanGet(old,NULL,&value);
            if(!error)error=ProElementBooleanSet(fresh,value);
            break;
        }
        case PRO_VALUE_TYPE_SELECTION:{
            ProReference original=NULL,reference=NULL;
            error=ProElementReferenceGet(old,NULL,&original);
            if(!error&&original)error=id==PRO_E_STD_CORNER_CHAMF_CORNER?
                replay_corner_reference(replay,original,&reference):replay_map_reference(replay,original,&reference);
            if(!error&&reference)error=ProElementReferenceSet(fresh,reference);
            if(original)ProReferenceFree(original);
            if(reference)ProReferenceFree(reference);
            break;
        }
        default:error=PRO_TK_UNSUPPORTED;
        }
        if(error==PRO_TK_EMPTY){ProElementFree(&fresh);return PRO_TK_NO_ERROR;}
    }
    if(error){
        trace("Replay failed element ID",id);
        ProElementFree(&fresh);
    }else *result=fresh;
    return error;
}

#include "imitation_replay_pattern.inc"
#include "imitation_replay_exclusion.inc"
#include "imitation_replay_hole_profile.inc"

#include "imitation_replay_sheet_cut.inc"

static ProError replay_flange_tree(ImitationReplay *replay,ProElement source,FdTxn *tx){
    static const ProElemId supported[]={
        PRO_E_FEATURE_TYPE,PRO_E_SMT_WALL_TYPE,PRO_E_STD_FEATURE_NAME,PRO_E_SMT_FLANGE_TYPE,
        PRO_E_STD_CURVE_COLLECTION_APPL,PRO_E_SMT_FLANGE_TRAJ_CRV_NORM,PRO_E_STD_SECTION,
        PRO_E_SMT_WALL_SHARPS_TO_BENDS,PRO_E_SMT_FLANGE_SEC_FLIP,PRO_E_SMT_FLANGE_DEPTH,
        PRO_E_SMT_FILLETS,PRO_E_SMT_WALL_HEIGHT,PRO_E_SMT_BEND_RELIEF,PRO_E_SMT_WALL_THICKNESS_FLIP,
        PRO_E_SMT_CORNER_RELIEF,PRO_E_SMT_MTR_CUTS,PRO_E_SMT_AUTO_EXLD_EDGE,PRO_E_SMT_DEV_LEN_CALCULATION
    };
    ProError error=ProElementAlloc(PRO_E_FEATURE_TREE,&tx->root);
    for(int i=0;!error&&i<_countof(supported);i++){
        if(supported[i]==PRO_E_STD_SECTION){
            error=model_element(tx->root,PRO_E_STD_SECTION,0,0,NULL);
            continue;
        }
        ProElement original,fresh=NULL;
        error=mc_tree_element(source,&supported[i],1,&original);
        if(error==PRO_TK_E_NOT_FOUND){error=PRO_TK_NO_ERROR;continue;}
        if(!error)error=replay_element(replay,original,&fresh);
        if(!error&&fresh)error=ProElemtreeElementAdd(tx->root,NULL,fresh);
        if(error&&fresh)ProElementFree(&fresh);
    }
    return error;
}

static ProError replay_first_solid_tree(ImitationReplay *replay,ProElement source,FdTxn *tx){
    const ProElemId ids[]={PRO_E_FEATURE_TYPE,PRO_E_FEATURE_FORM,PRO_E_STD_EXT_DEPTH};
    ProError error=ProElementAlloc(PRO_E_FEATURE_TREE,&tx->root);
    for(int i=0;!error&&i<_countof(ids);i++){
        ProElement original,fresh=NULL;
        error=mc_tree_element(source,&ids[i],1,&original);
        if(!error)error=replay_element(replay,original,&fresh);
        if(!error&&fresh)error=ProElemtreeElementAdd(tx->root,NULL,fresh);
        if(error&&fresh)ProElementFree(&fresh);
    }
    return error;
}

static ProError replay_tree_integer(ProElement tree,ProElemId field,int *value){
    ProElement element=NULL;
    ProError error=mc_tree_element(tree,&field,1,&element);
    if(!error)error=ProElementIntegerGet(element,NULL,value);
    return error;
}

#include "imitation_replay_conversion.inc"
#include "imitation_replay_boolean.inc"
#include "imitation_replay_split.inc"
#include "imitation_replay_quilt_solid.inc"
#include "imitation_replay_round.inc"

static ProError replay_verify_csys(ProFeature *source,ProFeature *target){
    ProGeomitem a,b;
    ProCsysdata src,dst;
    ProError error=fd_feature_geom(source,PRO_CSYS,&a);
    if(!error)error=fd_feature_geom(target,PRO_CSYS,&b);
    if(!error)error=replay_csys_frame(&a,&src);
    if(!error)error=replay_csys_frame(&b,&dst);
    if(!error&&!replay_csys_same(&src,&dst))error=PRO_TK_INCOMPLETE;
    return error;
}

static ProError replay_verify_plane(ProFeature *source,ProFeature *target){
    ProGeomitem a,b;
    ProVector pa,pb,na,nb,offset;
    ProError error=fd_feature_geom(source,PRO_SURFACE,&a);
    if(!error)error=fd_feature_geom(target,PRO_SURFACE,&b);
    if(!error)error=fd_plane_data(&a,pa,na);
    if(!error)error=fd_plane_data(&b,pb,nb);
    if(error)return error;
    ad_sub(pa,pb,offset);
    return ad_unit(na)&&ad_unit(nb)&&ad_dot(na,nb)>0.999999&&fabs(ad_dot(offset,na))<=0.0002?
        PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

typedef struct {ProVector point[64];int count;} ReplayDatumPoints;

static ProError replay_datum_point(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    ReplayDatumPoints *points=data;
    if(points->count==_countof(points->point))return PRO_TK_OUT_OF_RANGE;
    FdRef reference={0};
    reference.item=*item;
    ProError error=ad_point(&reference,points->point[points->count]);
    if(!error)points->count++;
    return error;
}

static ProError replay_verify_points(ProFeature *source,ProFeature *target){
    ReplayDatumPoints a={0},b={0};
    ProError error=ProFeatureGeomitemVisit(source,PRO_POINT,replay_datum_point,NULL,&a);
    if(!error)error=ProFeatureGeomitemVisit(target,PRO_POINT,replay_datum_point,NULL,&b);
    if(error)return error;
    if(!a.count||a.count!=b.count)return PRO_TK_INCOMPLETE;
    for(int i=0;i<a.count;i++)if(!ad_near(a.point[i],b.point[i],0.0002))return PRO_TK_INCOMPLETE;
    return PRO_TK_NO_ERROR;
}

#include "imitation_replay_geometry_mirror.inc"
#include "imitation_replay_sweep.inc"
#include "imitation_replay_extrusion_body.inc"
#include "imitation_replay_quilt_edit.inc"
#include "imitation_replay_flat_wall.inc"
#include "imitation_replay_blend.inc"

static int replay_definition_supported(ProElement tree,ProFeattype type,const wchar_t **label){
    if(type==PRO_FEAT_DEFORM_AREA){
        *label=L"钣金分割区域";
        ProElement section=NULL;
        return !mc_tree_element(tree,(ProElemId[]){PRO_E_STD_SECTION},1,&section);
    }
    if(type==PRO_FEAT_PUNCH_QUILT){
        *label=L"钣金面组成形";
        ProElement tool=NULL;
        return !mc_tree_element(tree,(ProElemId[]){PRO_E_FORM_PUNCH_QUILT},1,&tool);
    }
    if(type==PRO_FEAT_DRAFT){
        int split,angle;
        *label=L"定角拔模";
        return !replay_tree_integer(tree,PRO_E_DRAFT_SPLIT,&split)&&
            (split==PRO_DRAFT_UI_SPLIT_NONE||split==PRO_DRAFT_UI_SPLIT_NEUT)&&
            !replay_tree_integer(tree,PRO_E_DRAFT_CONSTANT_OR_VARIABLE,&angle)&&angle==PRO_DRAFT_UI_CONSTANT;
    }
    if(type==PRO_FEAT_RM_SURF){
        *label=L"移除面";
        int attach=0;
        return !replay_tree_integer(tree,PRO_E_RM_SURF_ATTACH_TYPE,&attach)&&attach==FM_RM_SURF_ATTACH_SAME;
    }
    if(type==PRO_FEAT_SPLIT_BODY){
        *label=L"按基准面分割实体";
        return replay_split_definition(tree);
    }
    if(type==PRO_FEAT_BOOLEANBODIES){
        int operation;
        *label=L"多实体布尔";
        return replay_boolean_definition(tree,&operation);
    }
    if(type==PRO_FEAT_EXTEND){
        *label=L"曲面组延伸";
        return replay_quilt_extend(tree);
    }
    if(type==PRO_FEAT_CUT&&replay_quilt_trim(tree)){
        *label=L"曲面组修剪";
        return 1;
    }
    if(type==PRO_FEAT_ROUND&&replay_variable_round(tree))*label=L"变半径倒圆角";
    if(type==PRO_FEAT_BEND){
        int form;
        if(!replay_tree_integer(tree,PRO_E_SMT_BEND_FORM,&form)&&form==PRO_SMT_BEND_FORM_ROLL)
            *label=L"钣金卷边";
    }
    if(type==PRO_FEAT_DATUM_QUILT){
        int value=0;
        *label=L"曲面组缝合";
        return !replay_tree_integer(tree,PRO_E_SRF_MRG_TYPE,&value)&&
            (value==0||value==PRO_SRF_MRG_JOIN);
    }
    if(type==PRO_FEAT_CURVE){
        int kind;
        if(!replay_tree_integer(tree,PRO_E_CURVE_TYPE,&kind)){
            if(kind==PRO_CURVE_TYPE_FROM_EQUATION)*label=L"方程空间曲线";
            else if(kind==PRO_CURVE_TYPE_INTSRF)*label=L"曲面相交曲线";
        }
    }
    if(type==PRO_FEAT_SRF_MDL){
        int transform=0;
        ProType kind;
        if(replay_tree_integer(tree,PRO_E_SRF_TRANS_TYPE,&transform)||transform!=PRO_SURF_TRANS_TYPE_MIRROR)
            return 0;
        if(replay_mirror_seed(tree,NULL,&kind))return 0;
        *label=kind==PRO_BODY?L"实体镜像":L"曲面组镜像";
        return 1;
    }
    if(type==PRO_FEAT_GROUP_HEAD){
        ProElement mirror;
        if(mc_tree_element(tree,(ProElemId[]){PRO_E_MGF_REFS,PRO_E_MGF_MIRROR_PLANE},2,&mirror))return 0;
        *label=L"特征几何镜像";
        return 1;
    }
    if(type==PRO_FEAT_DATUM_SURF){
        ProElement body=NULL;
        if(!mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_CONV_BODY},1,&body)){
            *label=L"转换为钣金";
            return replay_empty_conversion(tree)||replay_wall_conversion(tree);
        }
        int form=0,material=0;
        if(!replay_tree_integer(tree,PRO_E_DTM_SRF_TYPE,&form)&&form==16)*label=L"曲面组偏移";
        if(!replay_tree_integer(tree,PRO_E_FEATURE_FORM,&form)&&form==PRO_EXTRUDE&&
           !replay_tree_integer(tree,PRO_E_EXT_SURF_CUT_SOLID_TYPE,&material)&&material==PRO_EXT_FEAT_TYPE_SURFACE)
            *label=L"拉伸曲面";
    }
    if(type==PRO_FEAT_WALL){
        int kind=0;
        if(replay_tree_integer(tree,PRO_E_SMT_WALL_TYPE,&kind))return 0;
        if(kind==PRO_SMT_WALL_TYPE_FLAT){*label=L"平整壁";return 1;}
        return kind==PRO_SMT_WALL_TYPE_FLANGE;
    }
    if(type==PRO_FEAT_HOLE){
        ProElement hole=NULL,depth=NULL,to=NULL;
        int kind=0,representation=0,termination=0;
        if(mc_tree_element(tree,(ProElemId[]){PRO_E_HLE_COM},1,&hole)||
           replay_tree_integer(hole,PRO_E_HLE_TYPE_NEW,&kind))return 0;
        if(kind==PRO_HLE_NEW_TYPE_SKETCH){
            ProElement profile,placement;
            *label=L"草绘台阶孔";
            return !replay_hole_profile_element(tree,&profile)&&
                !replay_hole_profile_placement(tree,&placement);
        }
        if(replay_tree_integer(hole,PRO_E_HLE_MAKE_LIGHTWT,&representation)||representation!=PRO_HLE_REGULAR)return 0;
        if(kind==PRO_HLE_CUSTOM_TYPE||kind==PRO_HLE_NEW_TYPE_STANDARD){
            int thread=0,bore=0,sink=0;
            if(replay_tree_integer(hole,PRO_E_HLE_DEPTH,&termination)||
               (termination!=PRO_HLE_STD_VAR_DEPTH&&termination!=PRO_HLE_STD_THRU_ALL_DEPTH))return 0;
            replay_tree_integer(hole,PRO_E_HLE_ADD_THREAD,&thread);
            replay_tree_integer(hole,PRO_E_HLE_ADD_CBORE,&bore);
            replay_tree_integer(hole,PRO_E_HLE_ADD_CSINK,&sink);
            if(kind==PRO_HLE_NEW_TYPE_STANDARD&&thread==PRO_HLE_ADD_THREAD){*label=L"标准螺纹孔";return 1;}
            if(kind==PRO_HLE_CUSTOM_TYPE&&bore==PRO_HLE_ADD_CBORE){*label=L"柱形沉孔";return 1;}
            if(kind==PRO_HLE_CUSTOM_TYPE&&sink==PRO_HLE_ADD_CSINK){*label=L"锥形沉头孔";return 1;}
            return 0;
        }
        if(kind!=PRO_HLE_NEW_TYPE_STRAIGHT||mc_tree_element(hole,(ProElemId[]){PRO_E_HOLE_STD_DEPTH},1,&depth)||
           mc_tree_element(depth,(ProElemId[]){PRO_E_HOLE_DEPTH_TO},1,&to)||
           replay_tree_integer(to,PRO_E_HOLE_DEPTH_TO_TYPE,&termination))return 0;
        if(termination==PRO_HLE_STRGHT_BLIND_DEPTH){*label=L"简单盲孔";return 1;}
        if(termination==PRO_HLE_STRGHT_THRU_ALL_DEPTH){*label=L"简单通孔";return 1;}
        return 0;
    }
    if(type==PRO_FEAT_FIRST_FEAT||type==PRO_FEAT_PROTRUSION||type==PRO_FEAT_CUT||type==PRO_FEAT_CUT_SMT){
        int form=0,material=0;
        if(!replay_tree_integer(tree,PRO_E_FEATURE_FORM,&form)){
            const int removes=type==PRO_FEAT_CUT||type==PRO_FEAT_CUT_SMT||
                (!replay_tree_integer(tree,PRO_E_REMOVE_MATERIAL,&material)&&material==PRO_EXT_MATERIAL_REMOVE);
            if(form==PRO_EXTRUDE)*label=type==PRO_FEAT_CUT_SMT?L"钣金拉伸切口":removes?L"拉伸去料":L"拉伸加料";
            if(form==PRO_REVOLVE)*label=removes?L"旋转去料":L"旋转加料";
            if(form==PRO_USE_SURFS)return replay_quilt_solid_definition(tree,type,label);
            if(form==PRO_SWEEP){
                ReplaySweep sweep={0};
                *label=removes?L"恒截面扫描切除":L"恒截面扫描加料";
                const int supported=!replay_sweep_read(tree,&sweep);
                if(supported&&sweep.section_mode==PRO_SWEEP_VARIABLE_SECTION)
                    *label=removes?L"可变截面扫描切除":L"可变截面扫描加料";
                return supported;
            }
        }
    }
    return 1;
}


static ProError replay_build_feature(ImitationReplay *replay,ProFeature *source,ProFeattype type,
                                    ProFeature *created,ProName label,int *entities,int *dimensions,const wchar_t **operation){
    FdSketch original={0},result={0};
    original.feature=*source;
    FdTxn tx={0};
    ProElement name=NULL,section=NULL,sketch_value=NULL;
    ProSection attachment=NULL,placeholder=NULL;
    const int attached=type==PRO_FEAT_WALL;
    const int first=type==PRO_FEAT_FIRST_FEAT;
    int hole_profile=0;
    int separate_body=0,body_count=0;
    int section_assigned=0;
    int removes=type==PRO_FEAT_CUT_SMT||type==PRO_FEAT_CUT;
    ProMassProperty old_mass,new_mass;
    const char *stage="read native feature definition";
    double extend_distance=0;
    int excluded_member=0;
    if(type==PRO_FEAT_RM_SURF&&!replay_exclusion_member(source,&excluded_member)){
        *operation=L"阵列实例几何排除";
        return replay_exclusion(replay,source,created);
    }
    replay->rebuilding_remove=type==PRO_FEAT_RM_SURF;
    ProError error=type==PRO_FEAT_EXTEND?replay_original_extend(replay,source->id,&extend_distance):PRO_TK_NO_ERROR;
    if(error)goto done;
    error=(type==PRO_FEAT_RM_SURF||type==PRO_FEAT_CORN_CHAMF||type==PRO_FEAT_DATUM_QUILT||type==PRO_FEAT_EXTEND||
                    (type==PRO_FEAT_ROUND&&replay->external_total))?
        replay_history_open(replay,source,&original):
        ProFeatureElemtreeExtract(source,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&original.tree);
    if(error==PRO_TK_INVALID_TYPE&&(type==PRO_FEAT_DATUM_SURF||type==PRO_FEAT_PROTRUSION)){
        stage="rebuild blend with mapped sketch definitions";
        error=replay_blend_missing_tree(replay,source,type,created,label,operation);
        goto done;
    }
    if(error)goto done;
    if(!mc_tree_element(original.tree,(ProElemId[]){PRO_E_STD_FEATURE_NAME},1,&name)){
        wchar_t *value=NULL;
        if(!ProElementWstringGet(name,NULL,&value)&&value){
            wcsncpy_s(label,PRO_NAME_SIZE,value,_TRUNCATE);
            ProWstringFree(value);
        }
    }

    if(attached){
        int kind=0;
        if(!replay_tree_integer(original.tree,PRO_E_SMT_WALL_TYPE,&kind)&&kind==PRO_SMT_WALL_TYPE_FLANGE&&
           replay_flange_has_single_attachment_edge(original.tree)){
            stage="read flange attachment before later sheet-metal changes";
            fd_sketch_free(&original);
            error=replay_history_open(replay,source,&original);
            if(error)goto done;
        }
    }
    if(!replay_definition_supported(original.tree,type,operation)){error=PRO_TK_UNSUPPORTED;goto done;}
    if(type==PRO_FEAT_SPLIT_BODY){
        stage="rebuild native body split";
        error=replay_split(replay,original.tree,label,created);
        goto done;
    }
    if(type==PRO_FEAT_BOOLEANBODIES){
        stage="rebuild native body boolean";
        error=replay_boolean(replay,original.tree,label,created);
        goto done;
    }
    if(type==PRO_FEAT_CUT&&replay_quilt_trim(original.tree))removes=0;
    int form=0;
    if((type==PRO_FEAT_PROTRUSION||type==PRO_FEAT_CUT)&&
       !replay_tree_integer(original.tree,PRO_E_FEATURE_FORM,&form)&&form==PRO_SWEEP){
        error=replay_sweep(replay,source,original.tree,type,label,created,entities,dimensions);
        goto done;
    }
    if(type==PRO_FEAT_DATUM_SURF&&replay_empty_conversion(original.tree)){
        stage="convert original empty body";
        error=replay_convert_empty(replay,label,created);
        goto done;
    }
    if(type==PRO_FEAT_PROTRUSION){
        stage="read original extrusion body";
        error=replay_extrusion_new_body(source,original.tree,&separate_body);
        if(!error&&separate_body)error=sd_body_count(replay->target,&body_count);
        if(!error&&separate_body&&body_count)error=replay_extrusion_body_materials(replay->source);
        if(error)goto done;
        if(separate_body)*operation=L"拉伸新建实体";
    }
    stage="read embedded section";
    hole_profile=type==PRO_FEAT_HOLE&&replay_is_hole_profile(original.tree);
    if(hole_profile){
        error=replay_hole_profile_element(original.tree,&section);
        if(!error)error=ProElementSpecialvalueGet(section,NULL,(ProAppData *)&original.section);
        if(!error&&!original.section)error=PRO_TK_INCOMPLETE;
    }else if(first){
        error=mc_tree_element(original.tree,(ProElemId[]){PRO_E_SKETCHER},1,&section);
        if(!error)error=ProElementSpecialvalueGet(section,NULL,(ProAppData *)&original.section);
    }else error=fd_section_from_tree(original.tree,&original.section);
    if(error==PRO_TK_E_NOT_FOUND||error==PRO_TK_EMPTY)error=PRO_TK_NO_ERROR;
    if(error)goto done;
    if(removes&&(error=qa_geometry_mass(replay->target,&old_mass)))goto done;
    if(replay->reference_pattern){
        stage="create local patterned sketch references";
        if(!original.section){error=PRO_TK_UNSUPPORTED;goto done;}
        error=replay_pattern_build_guide(replay,original.section);
        if(error)goto done;
    }
    if(attached&&original.section&&label[0]){
        int kind=0;
        double start,end,height;
        if(!replay_tree_integer(original.tree,PRO_E_SMT_WALL_TYPE,&kind)&&kind==PRO_SMT_WALL_TYPE_FLANGE){
            stage="recreate native circular wall";
            error=replay_ring_wall(replay,original.tree,original.section,label,created);
            if(error!=PRO_TK_UNSUPPORTED)goto done;
        }
        if(kind==PRO_SMT_WALL_TYPE_FLAT&&
           replay_stock_wall_profile(original.section,&start,&end,&height)){
            stage="recreate stock flat wall with native edge and profile";
            error=replay_stock_wall(replay,source,original.tree,label,start,end,height,created);
            if(error!=PRO_TK_UNSUPPORTED)goto done;
        }
    }
    stage="allocate new feature";
    error=sh_begin(&tx,replay->target,L"BWTL_REPLAY_STEP",type);
    if(error)goto done;
    error=ProElementFree(&tx.root);
    if(!error){
        if(first)error=replay_first_solid_tree(replay,original.tree,&tx);
        else if(hole_profile)error=replay_hole_profile_tree(replay,original.tree,&tx.root);
        else if(type==PRO_FEAT_ROUND&&replay_variable_round(original.tree)){
            error=replay_variable_round_tree(replay,original.tree,label,&tx);
        }
        else if(separate_body)error=replay_extrusion_body_tree(replay,original.tree,&tx);
        else if(type==PRO_FEAT_SRF_MDL)error=replay_geometry_mirror_tree(replay,original.tree,&tx);
        else if(type==PRO_FEAT_EXTEND)error=replay_quilt_extend_tree(replay,original.tree,extend_distance,&tx.root);
        else if(type==PRO_FEAT_CUT_SMT)error=replay_cut_tree(replay,original.tree,&tx);
        else if(attached){
            ProElement wall;
            int kind;
            error=mc_tree_element(original.tree,(ProElemId[]){PRO_E_SMT_WALL_TYPE},1,&wall);
            if(!error)error=ProElementIntegerGet(wall,NULL,&kind);
            if(!error)error=kind==PRO_SMT_WALL_TYPE_FLANGE?replay_flange_tree(replay,original.tree,&tx):
                replay_element(replay,original.tree,&tx.root);
        }else error=replay_element(replay,original.tree,&tx.root);
    }
    if(error)goto done;
    if(original.section&&replay->external_planes&&(type==PRO_FEAT_DATUM_SURF||type==PRO_FEAT_CUT_SMT)){
        stage="orient localized sketch plane";
        error=replay_external_section_orientation(replay,&tx,original.section);
        if(error)goto done;
    }
    if(attached||first||hole_profile){
        stage="draw independent 2D profile";
        if(!original.section){error=PRO_TK_UNSUPPORTED;goto done;}
        error=ProSection2DAlloc(&attachment);
        if(!error)error=ProSectionNameSet(attachment,L"BWTL_REPLAY_PROFILE");
        if(!error)error=replay_draw_section(replay,original.section,attachment,
            hole_profile?REPLAY_HOLE_SECTION:REPLAY_INDEPENDENT_SECTION,entities,dimensions);
        if(!error&&attached)error=mc_tree_element(tx.root,(ProElemId[]){PRO_E_STD_SECTION},1,&section);
        if(!error&&attached)error=model_element(section,PRO_E_SKETCHER,0,0,&sketch_value);
        if(!error&&attached)error=ProElementSpecialvalueSet(sketch_value,attachment);
        if(!error&&hole_profile)error=replay_hole_profile_attach(tx.root,attachment,&sketch_value);
        if(!error&&(attached||hole_profile))section_assigned=1;
        if(error)goto done;
    }
    stage="create target feature";
    if(first){
        tx.options[0]=PRO_FEAT_CR_INCOMPLETE_FEAT;
        error=ProFeatureWithoptionsCreate(tx.model_selection,tx.root,tx.options,PRO_REGEN_NO_FLAGS,&tx.feature,&tx.errors);
    }else error=attached?sh_create(&tx):fd_create(&tx,original.section!=NULL&&!hole_profile);
    if(error)goto done;
    if(first){
        result.feature=tx.feature;
        stage="finish independent first solid extrusion";
        error=ProFeatureElemtreeExtract(&result.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&result.tree);
        if(!error)error=mc_tree_element(result.tree,(ProElemId[]){PRO_E_SKETCHER},1,&sketch_value);
        if(!error)error=ProElementSpecialvalueGet(sketch_value,NULL,(ProAppData *)&placeholder);
        if(!error)error=ProElementSpecialvalueSet(sketch_value,attachment);
        if(!error){
            section_assigned=1;
            tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
            error=ProFeatureWithoptionsRedefine(NULL,&result.feature,result.tree,tx.options,PRO_REGEN_NO_FLAGS,&tx.errors);
        }
        if(error)goto done;
    }else if(original.section&&!attached&&!hole_profile){
        result.feature=tx.feature;
        stage="get new embedded section";
        error=ProFeatureElemtreeExtract(&result.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&result.tree);
        if(!error)error=fd_section_from_tree(result.tree,&result.section);
        if(!error&&!result.section)error=PRO_TK_INCOMPLETE;
        if(!error){
            stage="verify embedded section frame";
            error=replay_section_frame(original.section,result.section);
        }
        if(error)goto done;
        stage="draw new embedded section";
        error=replay_draw_section(replay,original.section,result.section,REPLAY_ASSOCIATIVE_SECTION,entities,dimensions);
        if(error)goto done;
        stage="finish embedded feature";
        if(type==PRO_FEAT_CUT_SMT||type==PRO_FEAT_DATUM_SURF){
            ProElement planned_direction,section_direction;
            int sense;
            error=mc_tree_element(tx.root,(ProElemId[]){PRO_E_STD_DIRECTION},1,&planned_direction);
            if(!error)error=ProElementIntegerGet(planned_direction,NULL,&sense);
            if(!error)error=mc_tree_element(result.tree,(ProElemId[]){PRO_E_STD_DIRECTION},1,&section_direction);
            if(!error)error=ProElementIntegerSet(section_direction,sense);
            if(error)goto done;
        }
        tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
        error=ProFeatureWithoptionsRedefine(NULL,&result.feature,result.tree,tx.options,
            PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors);
        if(error)goto done;
    }
    stage="verify new feature";
    error=part_regenerate(replay->target);
    ProFeattype actual;
    if(!error)error=ProFeatureTypeGet(&tx.feature,&actual);
    if(!error&&actual!=type)error=PRO_TK_INCOMPLETE;
    if(!error&&type==PRO_FEAT_CSYS)error=replay_verify_csys(source,&tx.feature);
    if(!error&&type==PRO_FEAT_DATUM)error=replay_verify_plane(source,&tx.feature);
    if(!error&&type==PRO_FEAT_DATUM_POINT)error=replay_verify_points(source,&tx.feature);
    if(!error&&separate_body){
        int actual_bodies=0;
        error=sd_body_count(replay->target,&actual_bodies);
        if(!error&&actual_bodies!=body_count+1)error=PRO_TK_INCOMPLETE;
    }
    if(!error&&type==PRO_FEAT_CURVE){
        int kind;
        if(!replay_tree_integer(original.tree,PRO_E_CURVE_TYPE,&kind)&&
           (kind==PRO_CURVE_TYPE_FROM_EQUATION||kind==PRO_CURVE_TYPE_INTSRF))
            error=replay_verify_equation_curve(source,&tx.feature);
    }
    if(!error&&removes)error=qa_geometry_mass(replay->target,&new_mass);
    if(!error&&removes&&new_mass.volume>=old_mass.volume-1e-8)error=PRO_TK_INCOMPLETE;
    if(!error)*created=tx.feature;
done:
    if(error){
        trace(stage,error);
        if(tx.errors.error_list)for(int i=0;i<tx.errors.error_number&&i<12;i++){
            trace("Replay create element error ID",tx.errors.error_list[i].err_item_id);
            trace("Replay create element error",tx.errors.error_list[i].error);
        }
    }
    if(section_assigned&&ProElementSpecialvalueSet(sketch_value,placeholder))attachment=NULL;
    fd_sketch_free(&result);
    fd_sketch_free(&original);
    /* The whole newly created scratch model is discarded on failure. */
    fd_cleanup(&tx,PRO_TK_NO_ERROR);
    if(attachment)ProSectionFree(&attachment);
    replay->rebuilding_remove=0;
    if(replay->history_owner){
        ProError cleanup=ProMdlErase(replay->history_owner);
        replay->history_owner=NULL;
        if(!error)error=cleanup;
    }
    return error;
}

static const wchar_t *replay_operation_name(ProFeattype type){
    switch(type){
    case PRO_FEAT_DATUM:return L"基准面";
    case PRO_FEAT_CURVE:return L"草绘";
    case PRO_FEAT_DATUM_SURF:return L"平面首壁";
    case PRO_FEAT_SRF_MDL:return L"实体或曲面镜像";
    case PRO_FEAT_DATUM_QUILT:return L"曲面组缝合";
    case PRO_FEAT_EXTEND:return L"曲面组延伸";
    case PRO_FEAT_BOOLEANBODIES:return L"多实体布尔";
    case PRO_FEAT_SPLIT_BODY:return L"分割实体";
    case PRO_FEAT_CUT_SMT:case PRO_FEAT_CUT:return L"拉伸切除";
    case PRO_FEAT_FIRST_FEAT:case PRO_FEAT_PROTRUSION:return L"拉伸";
    case PRO_FEAT_WALL:return L"凸缘";
    case PRO_FEAT_BEND:return L"钣金折弯";
    case PRO_FEAT_UNBEND:return L"钣金展开";
    case PRO_FEAT_BEND_BACK:return L"钣金折回";
    case PRO_FEAT_FLAT_PAT:return L"钣金展平形态";
    case PRO_FEAT_DEFORM_AREA:return L"钣金分割区域";
    case PRO_FEAT_PUNCH_QUILT:return L"钣金面组成形";
    case PRO_FEAT_SMT_SKETCH_FORM:return L"钣金草绘成形";
    case PRO_FEAT_ROUND:return L"倒圆角";
    case PRO_FEAT_CHAMFER:return L"倒角";
    case PRO_FEAT_CORN_CHAMF:return L"角倒角";
    case PRO_FEAT_SHELL:return L"抽壳";
    case PRO_FEAT_RIB:return L"筋";
    case PRO_FEAT_DRAFT:return L"拔模";
    case PRO_FEAT_RM_SURF:return L"移除面";
    case PRO_FEAT_GROUP_HEAD:return L"特征组或镜像";
    case PRO_FEAT_HOLE:return L"孔";
    case PRO_FEAT_DATUM_AXIS:return L"基准轴";
    case PRO_FEAT_CSYS:return L"坐标系";
    case PRO_FEAT_DATUM_POINT:return L"基准点";

    default:return L"待支持特征";
    }
}

static int replay_type_supported(ProFeattype type){
    return type==PRO_FEAT_DATUM||type==PRO_FEAT_CURVE||type==PRO_FEAT_DATUM_SURF||type==PRO_FEAT_SRF_MDL||
        type==PRO_FEAT_DATUM_QUILT||type==PRO_FEAT_EXTEND||type==PRO_FEAT_BOOLEANBODIES||type==PRO_FEAT_SPLIT_BODY||
        type==PRO_FEAT_CUT_SMT||type==PRO_FEAT_CUT||type==PRO_FEAT_FIRST_FEAT||
        type==PRO_FEAT_PROTRUSION||type==PRO_FEAT_WALL||type==PRO_FEAT_ROUND||type==PRO_FEAT_HOLE||
        type==PRO_FEAT_DATUM_AXIS||type==PRO_FEAT_DATUM_POINT||type==PRO_FEAT_CSYS||
        type==PRO_FEAT_CHAMFER||type==PRO_FEAT_CORN_CHAMF||
        type==PRO_FEAT_SHELL||type==PRO_FEAT_RIB||type==PRO_FEAT_DRAFT||type==PRO_FEAT_RM_SURF||type==PRO_FEAT_GROUP_HEAD||
        type==PRO_FEAT_BEND||type==PRO_FEAT_UNBEND||type==PRO_FEAT_BEND_BACK||
        type==PRO_FEAT_FLAT_PAT||type==PRO_FEAT_SMT_SKETCH_FORM||type==PRO_FEAT_DEFORM_AREA||type==PRO_FEAT_PUNCH_QUILT;
}

static int replay_standard_feature(ProFeattype type,const wchar_t *label){
    return (type==PRO_FEAT_DATUM&&(!_wcsicmp(label,L"RIGHT")||!_wcsicmp(label,L"TOP")||
        !_wcsicmp(label,L"FRONT")))||(type==PRO_FEAT_CSYS&&(!_wcsicmp(label,L"PRT_CSYS_DEF")||!_wcsicmp(label,L"PRT_CSYS")))||
        (type==PRO_FEAT_ZONE&&(!_wcsicmp(label,L"X")||!_wcsicmp(label,L"Y")||!_wcsicmp(label,L"Z")));
}

#include "imitation_replay_local_group.inc"
#include "imitation_replay_preview.inc"

typedef struct {
    int count,derived;
    ReplayPatternSource patterns;
    int generated_datums,read_candidate;
    struct {int id,sections,pattern_type,members,conversion,group,section_link;ProFeattype type;} step[REPLAY_FEATURE_MAX];
} ReplayOrder;

#include "imitation_replay_order_links.inc"

static ProError replay_read_order(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ReplayOrder *order=data;
    ProFeattype type;
    ProBoolean visible,embedded;
    ProPatternStatus pattern;
    ProGrppatternStatus group_pattern;
    ProName name=L"";
    ProError error=ProFeatureTypeGet(feature,&type);
    if(!error)error=ProFeatureVisibilityGet(feature,&visible);
    if(!error)error=ProFeatureIsEmbedded(feature,&embedded);
    if(!error)error=ProFeaturePatternStatusGet(feature,&pattern);
    if(!error)error=ProFeatureGrppatternStatusGet(feature,&group_pattern);
    if(error)return error;
    ProFeatStatus regeneration;
    if(!ProFeatureStatusGet(feature,&regeneration)&&regeneration==PRO_FEAT_FAMTAB_SUPPRESSED)
        return PRO_TK_NO_ERROR;
    if(type==PRO_FEAT_DERIVED_MEMBER){order->derived++;return PRO_TK_NO_ERROR;}
    if(group_pattern==PRO_GRP_PATTERN_MEMBER){order->derived++;return PRO_TK_NO_ERROR;}
    if(!visible||embedded||type==PRO_FEAT_ANNOTATION||
       pattern==PRO_PATTERN_HEADER||pattern==PRO_PATTERN_MEMBER||
       group_pattern==PRO_GRP_PATTERN_HEADER)return PRO_TK_NO_ERROR;
    ProModelitemNameGet((ProModelitem *)feature,name);
    if(replay_standard_feature(type,name))return PRO_TK_NO_ERROR;
    int sequence=0;
    if(order->generated_datums&&type==PRO_FEAT_DATUM&&!wcsncmp(name,L"BWTL_XREF_",10)&&
       part_integer(name+10,&sequence)&&sequence>0&&sequence<=REPLAY_EXTERNAL_MAX)return PRO_TK_NO_ERROR;
    if(order->generated_datums&&type==PRO_FEAT_DATUM_POINT&&!wcsncmp(name,L"BWTL_XPT_",9)&&
       part_integer(name+9,&sequence)&&sequence>0&&sequence<=REPLAY_EXTERNAL_MAX)return PRO_TK_NO_ERROR;
    if(order->generated_datums&&type==PRO_FEAT_CURVE&&
       ((!wcsncmp(name,L"BWTL_XLN_",9)&&pattern==PRO_PATTERN_NONE)||
        (!wcsncmp(name,L"BWTL_REF_POINTS_",16)&&pattern==PRO_PATTERN_NONE)||
        (!wcsncmp(name,L"BWTL_REF_GUIDE_",15)&&pattern==PRO_PATTERN_LEADER)))return PRO_TK_NO_ERROR;
    if(order->count==REPLAY_FEATURE_MAX)return PRO_TK_OUT_OF_RANGE;
    int sections=0,pattern_type=0,members=0;
    ProFeatureNumSectionsGet(feature,&sections);
    if(pattern==PRO_PATTERN_LEADER){
        const ProPatternClass pattern_class=replay_pattern_class(type);
        ProElement tree=NULL;
        ProPattern definition;
        ProFeature *instances=NULL;
        if(order->read_candidate){
            ProElement kind;
            error=ProPatternElemtreeCreate(feature,pattern_class,&tree);
            if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_GENPAT_TYPE},1,&kind);
            if(!error)error=ProElementIntegerGet(kind,NULL,&pattern_type);
        }else error=replay_pattern_definition_class(feature,pattern_class,&order->patterns,&tree,&pattern_type);
        if(tree)ProElementFree(&tree);
        if(!error)error=ProFeaturePatternGet(feature,pattern_class,&definition);
        if(!error)error=ProPatternMembersGet(&definition,&instances);
        if(!error)error=ProArraySizeGet((ProArray)instances,&members);
        if(instances)ProArrayFree((ProArray *)&instances);
        if(error)return error;
    }
    int conversion=0;
    if(type==PRO_FEAT_DATUM_SURF){
        ProElement tree=NULL;
        error=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
        if(!error)conversion=replay_empty_conversion(tree);
        if(error==PRO_TK_INVALID_TYPE&&replay_blend_subtype(feature))error=PRO_TK_NO_ERROR;
        if(tree)ProFeatureElemtreeFree(feature,tree);
        if(error)return error;
    }
    int group=0,section_link=0;
    error=replay_order_group(feature,order,&group);
    if(!error)error=replay_order_section_link(feature,type,sections,order,&section_link);
    if(error)return error;
    order->step[order->count].group=group;
    order->step[order->count].section_link=section_link;
    order->step[order->count].conversion=conversion;
    order->step[order->count].id=feature->id;
    order->step[order->count].type=type;
    order->step[order->count].sections=sections;
    order->step[order->count].pattern_type=pattern_type;
    order->step[order->count++].members=members;

    return PRO_TK_NO_ERROR;
}

static ProError replay_verify_order_impl(ProMdl source,ProMdl result,int read_candidate,ReplayOrder *verified_order,PartText *out){
    ReplayOrder old={0},fresh={0};
    fresh.read_candidate=read_candidate;
    ProExtRefInfo *groups=NULL;
    int count=0;
    if(!ProSolidExternParentsGet((ProSolid)source,PRO_ALL_EXT_REF_TYPES,&groups,&count))
        fresh.generated_datums=count>0;
    if(groups)ProExtRefInfoFree(&groups);
    ProError error=ProSolidFeatVisit((ProSolid)source,replay_read_order,NULL,&old);
    if(!error)error=ProSolidFeatVisit((ProSolid)result,replay_read_order,NULL,&fresh);
    ProError old_cleanup=replay_pattern_source_close(&old.patterns);
    ProError new_cleanup=replay_pattern_source_close(&fresh.patterns);
    if(!error)error=old_cleanup?old_cleanup:new_cleanup;
    if(error)return error;
    if(old.count!=fresh.count){
        part_text_add(out,L"顶层建模步骤数量不一致，原件 %d，结果 %d。\n",old.count,fresh.count);
        return PRO_TK_INCOMPLETE;
    }
    if(old.derived!=fresh.derived){
        part_text_add(out,L"原生派生特征数量不一致，原件 %d，结果 %d。\n",old.derived,fresh.derived);
        return PRO_TK_INCOMPLETE;
    }
    int coincident_patterns=0;
    for(int i=0;i<old.count;i++){
        if(old.step[i].type!=fresh.step[i].type||old.step[i].sections!=fresh.step[i].sections||
           old.step[i].pattern_type!=fresh.step[i].pattern_type||
           old.step[i].conversion!=fresh.step[i].conversion){
            part_text_add(out,L"第 %d 步的原生特征类型、截面数量或阵列定义有差异，源特征 %d，新特征 %d。\n",
                i+1,old.step[i].id,fresh.step[i].id);
            return PRO_TK_INCOMPLETE;
        }
        if(old.step[i].members!=fresh.step[i].members){
            if(old.step[i].pattern_type!=PRO_GENPAT_DIR_DRIVEN||
               replay_verify_coincident_direction(source,result,old.step[i].id,fresh.step[i].id)){
                part_text_add(out,L"第 %d 步阵列实例不一致，原件 %d，结果 %d。\n",
                    i+1,old.step[i].members,fresh.step[i].members);
                return PRO_TK_INCOMPLETE;
            }
            coincident_patterns++;
            part_text_add(out,L"第 %d 步保留方向阵列和孔位。原件 %d 个实例，结果 %d 个，多出的 %d 个与现有孔重合。"
                L"原来的排除位置没有写入；单独修改一个方向的间距后，重合关系可能变化。\n",
                i+1,old.step[i].members,fresh.step[i].members,fresh.step[i].members-old.step[i].members);
        }
        if(old.step[i].group!=fresh.step[i].group||old.step[i].section_link!=fresh.step[i].section_link){
            part_text_add(out,L"第 %d 步的所属特征组或草绘归属不同，源特征 %d，新特征 %d。\n",
                i+1,old.step[i].id,fresh.step[i].id);
            return PRO_TK_INCOMPLETE;
        }
    }
    part_text_add(out,L"逐项核对 %d 道建模步骤的顺序、类型、组成员和草绘归属。%ls\n",old.count,
        coincident_patterns?L"方向阵列的重合位置另行核对。":L"阵列实例数一致。");
    if(old.derived)part_text_add(out,L"另核对 %d 项组内派生特征。\n",old.derived);
    if(verified_order)*verified_order=fresh;
    return PRO_TK_NO_ERROR;
}

static ProError replay_verify_order(ProMdl source,ProMdl result,PartText *out){
    return replay_verify_order_impl(source,result,0,NULL,out);
}

static ProError replay_mirror_members(ImitationReplay *replay,ProFeature *source,ProFeature *created)
{
    ProFeature *original=NULL,*result=NULL;
    int original_count=0,result_count=0;
    ProError error=ProGroupFeaturesCollect((ProGroup *)source,&original);
    if(!error)error=ProGroupFeaturesCollect((ProGroup *)created,&result);
    if(!error)error=ProArraySizeGet((ProArray)original,&original_count);
    if(!error)error=ProArraySizeGet((ProArray)result,&result_count);
    if(!error&&original_count!=result_count)error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<original_count;i++){
        if(original[i].id==source->id&&result[i].id==created->id)continue;
        ProFeattype a,b;
        error=ProFeatureTypeGet(&original[i],&a);
        if(!error)error=ProFeatureTypeGet(&result[i],&b);
        if(!error&&(a!=PRO_FEAT_DERIVED_MEMBER||a!=b))error=PRO_TK_UNSUPPORTED;
        if(!error)error=replay_add_mapping(replay,original[i].id,result[i].id);
    }
    if(original)ProArrayFree((ProArray *)&original);
    if(result)ProArrayFree((ProArray *)&result);
    return error;
}

#include "imitation_replay_diagnostics.inc"

static ProError replay_feature(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ImitationReplay *replay=data;
    ProFeattype type;
    ProFeatStatus state;
    unsigned int state_flags=0;
    const wchar_t *issue=NULL;
    ProPatternStatus pattern;
    ProGrppatternStatus group_pattern;
    ProBoolean embedded=PRO_B_FALSE,visible=PRO_B_TRUE;
    ProName label=L"";
    ProError error=ProFeatureTypeGet(feature,&type);
    if(!error)error=replay_feature_state(feature,&state,&state_flags,&issue);
    if(!error)error=ProFeaturePatternStatusGet(feature,&pattern);
    if(!error)error=ProFeatureGrppatternStatusGet(feature,&group_pattern);
    if(!error)error=ProFeatureIsEmbedded(feature,&embedded);
    if(!error)error=ProFeatureVisibilityGet(feature,&visible);
    if(error)return error;
    if(state==PRO_FEAT_FAMTAB_SUPPRESSED)return PRO_TK_NO_ERROR;
    if(type==PRO_FEAT_DERIVED_MEMBER){
        for(int i=0;i<replay->count;i++)if(replay->mapped[i].source==feature->id)return PRO_TK_NO_ERROR;
    }
    if((!visible&&!issue)||embedded||type==PRO_FEAT_ANNOTATION||
       pattern==PRO_PATTERN_HEADER||pattern==PRO_PATTERN_MEMBER||
       group_pattern==PRO_GRP_PATTERN_HEADER||group_pattern==PRO_GRP_PATTERN_MEMBER)return PRO_TK_NO_ERROR;
    if(replay->count==REPLAY_FEATURE_MAX)return PRO_TK_OUT_OF_RANGE;
    ProModelitemNameGet((ProModelitem *)feature,label);
    if(type==PRO_FEAT_GROUP_HEAD&&!issue){
        ReplayLocalGroup group={0};
        if(!replay_local_group_read(feature,&group)){
            if(replay->local_group.count)return PRO_TK_UNSUPPORTED;
            group.step=++replay->steps;
            replay->local_group=group;
            return PRO_TK_NO_ERROR;
        }
    }
    replay->reference_error=(ReplayReferenceFailure){0};
    ProFeature created;
    ReplayReferencePattern *guide=NULL;
    const int standard=replay_standard_feature(type,label);
    SmartPartSignature checkpoint={0};
    if(!standard&&replay->keep_prefix&&replay->completed&&!replay->local_group.count)
        checkpoint=smart_part_signature(replay->target);
    if(standard&&type==PRO_FEAT_ZONE)return PRO_TK_NO_ERROR;
    if(standard){
        error=ProModelitemByNameInit(replay->target,PRO_FEATURE,label,&created);
        const int alias=error==PRO_TK_E_NOT_FOUND&&type==PRO_FEAT_CSYS&&!_wcsicmp(label,L"PRT_CSYS");
        if(alias)error=ProModelitemByNameInit(replay->target,PRO_FEATURE,L"PRT_CSYS_DEF",&created);
        if(!error&&type==PRO_FEAT_CSYS)error=replay_verify_csys(feature,&created);
        if(!error&&alias)error=ProModelitemNameSet(&created,label);
    }else{
        int entities=0,dimensions=0;
        const wchar_t *operation=replay_operation_name(type);
        replay->steps++;
        replay->current_feature=feature->id;
        replay_diagnostic_begin(replay,feature,type,label,state_flags);
        if(issue)error=PRO_TK_BAD_CONTEXT;
        else if(!replay_type_supported(type))error=PRO_TK_UNSUPPORTED;
        if(!error&&pattern==PRO_PATTERN_LEADER&&replay->external_total){
            ReplayPatternSource read={0};
            ProElement tree=NULL;
            int kind=0;
            error=replay_pattern_definition(feature,&read,&tree,&kind);
            if(tree)ProElementFree(&tree);
            ProError cleanup=replay_pattern_source_close(&read);
            if(!error)error=cleanup;
            if(!error&&kind==PRO_GENPAT_REF_DRIVEN){
                guide=calloc(1,sizeof(*guide));
                error=guide?replay_pattern_read_guide(replay,feature,guide):PRO_TK_OUT_OF_MEMORY;
                if(!error)replay->reference_pattern=guide;
            }
        }
        if(!error)error=replay_build_feature(replay,feature,type,&created,label,&entities,&dimensions,&operation);
        part_text_add(replay->out,L"第 %d 步 %ls%ls%ls，源特征 %d，%ls",
            replay->steps,operation,label[0]?L" ":L"",label,feature->id,
            error?L"重建未完成":L"已在新件创建");
        if(error)part_text_add(replay->out,L"（类型 %d，Creo %d）。\n",type,error);
        if(issue)part_text_add(replay->out,L"源特征状态 %ls（状态 %d，标志 0x%x）。\n",issue,state,state_flags);
        if(!error){
            ProName new_label=L"";
            ProModelitemNameGet((ProModelitem *)&created,new_label);
            part_text_add(replay->out,L"，新特征 %ls%ls%d，Creo 0",
                new_label,new_label[0]?L" ID ":L"ID ",created.id);
            if(entities)part_text_add(replay->out,L"，重画 %d 个轮廓图元，重建 %d 个原草绘尺寸",entities,dimensions);
            part_text_add(replay->out,L"。\n");
        }
    }
    if(!error)error=replay_add_mapping(replay,feature->id,created.id);
    if(!error&&type==PRO_FEAT_GROUP_HEAD)error=replay_mirror_members(replay,feature,&created);
    if(!standard)replay->last_pattern_member=0;
    if(!error&&pattern==PRO_PATTERN_LEADER)error=replay_pattern(replay,feature,&created,PRO_FEAT_PATTERN);
    if(!standard)replay_diagnostic_end(replay,feature,error);
    if(error)replay_report_reference_failure(&replay->reference_error,replay->out);
    if(error&&replay->failure){
        replay->failure->step=standard?0:replay->steps;
        replay->failure->feature=feature->id;
        replay->failure->operation=issue?issue:replay_operation_name(type);
        replay->failure->reference=replay->reference_error;
        wcscpy_s(replay->failure->label,_countof(replay->failure->label),label);
    }
    replay->reference_pattern=NULL;
    free(guide);
    if(!error&&!standard)replay->last_feature=feature->id;
    const int group_count=replay->local_group.count;
    if(!error&&group_count&&feature->id==replay->local_group.members[group_count-1])
        error=replay_local_group_finish(replay);
    if(!error&&!standard&&!replay->local_group.count)replay->completed=replay->steps;
    if(error&&checkpoint.hash&&!checkpoint.error){
        SmartPartSignature current=smart_part_signature(replay->target);
        replay->prefix_intact=!current.error&&current.hash==checkpoint.hash;
    }
    return error;
}

static ProError replay_find_conversion(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ProFeattype type;
    ProBoolean visible,embedded;
    ProName label=L"";
    ProError error=ProFeatureTypeGet(feature,&type);
    if(!error)error=ProFeatureVisibilityGet(feature,&visible);
    if(!error)error=ProFeatureIsEmbedded(feature,&embedded);
    if(error)return error;
    if(!visible||embedded||type==PRO_FEAT_ANNOTATION)return PRO_TK_NO_ERROR;
    ProModelitemNameGet((ProModelitem *)feature,label);
    if(replay_standard_feature(type,label))return PRO_TK_NO_ERROR;
    if(type==PRO_FEAT_DATUM_SURF){
        ProElement tree=NULL;
        error=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
        if(!error)*(int *)data=replay_empty_conversion(tree)||replay_wall_conversion(tree);
        if(tree)ProFeatureElemtreeFree(feature,tree);
        if(error&&error!=PRO_TK_INVALID_TYPE)return error;
        if(*(int *)data)return PRO_TK_USER_ABORT;
    }
    return PRO_TK_NO_ERROR;
}

#include "imitation_replay_relations.inc"
#include "imitation_replay_family.inc"

static ProError imitation_replay_new_impl(ProMdl source,ProMdlsubtype subtype,ProMdl *target,
                                         int *steps,ReplayFailure *failure,int *partial,int manual,PartText *out){
    const char *stage="open blank template";
    ProMdlName name;
    int conversion=0;
    ProError error=subtype==PROMDLSTYPE_PART_SHEETMETAL?
        ProSolidFeatVisit((ProSolid)source,replay_find_conversion,NULL,&conversion):PRO_TK_NO_ERROR;
    if(error==PRO_TK_USER_ABORT||error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    ImitationReplay replay={0};
    if(!error)error=ProSolidFeatVisit((ProSolid)source,replay_capture_extend,NULL,&replay);
    if(!error)error=model_create_empty(subtype==PROMDLSTYPE_PART_SHEETMETAL&&!conversion?2:1,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,target);
    if(!error)error=template_empty((ProSolid)*target);
    double a,b;
    if(!error)error=qa_mm_per_unit(source,&a);
    if(!error)error=qa_mm_per_unit(*target,&b);
    if(!error&&!imitation_metric_same(a,b))error=PRO_TK_UNSUPPORTED;
    ProAccuracyType accuracy_type;
    double accuracy;
    if(!error)error=ProSolidAccuracyGet((ProSolid)source,&accuracy_type,&accuracy);
    if(!error)error=ProSolidAccuracySet((ProSolid)*target,accuracy_type,accuracy);
    replay.source=source;
    replay.target=*target;
    replay.out=out;
    replay.failure=failure;
    replay.keep_prefix=partial!=NULL;
    replay.manual=manual;
    if(partial)*partial=0;
    if(!error)error=replay_external_state(source,&replay.external_total);
    if(!error)error=ProSolidFeatVisit((ProSolid)source,replay_feature,NULL,&replay);
    if(!error&&replay.local_group.count)
        error=replay_local_group_finish(&replay);
    if(!error)error=replay_relations(&replay);
    if(!error)error=replay_family_read(source,*target,out);
    if(!error&&replay.external_total){
        int used=0;
        for(int i=0;i<replay.external_total;i++)used+=replay.external_used[i]!=0;
        part_text_add(out,L"已在新件重建 %d / %d 条外部参照。\n",used,replay.external_total);
        if(used!=replay.external_total)error=PRO_TK_INCOMPLETE;
    }
    if(!error&&!replay.steps)error=PRO_TK_EMPTY;
    if(error&&partial&&replay.prefix_intact)*partial=replay.completed;
    *steps=replay.steps;
    return error;
}

static ProError imitation_replay_new(ProMdl source,ProMdlsubtype subtype,ProMdl *target,int *steps,PartText *out){
    return imitation_replay_new_impl(source,subtype,target,steps,NULL,NULL,0,out);
}
