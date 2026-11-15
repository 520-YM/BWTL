#define REPLAY_FEATURE_MAX 256
#define REPLAY_EXTERNAL_MAX 128

typedef struct ReplayReferencePattern ReplayReferencePattern;
typedef struct {int id;double distance;ProError error;} ReplayExtendInput;

typedef struct {
    int item;
    ProType type;
    ProError error;
    ProFamilyMdlName owner;
} ReplayReferenceFailure;

typedef struct {
    int step,feature;
    const wchar_t *operation;
    ProName label;
    ReplayReferenceFailure reference;
} ReplayFailure;

typedef struct {
    int id,step,count,patterned,members[REPLAY_FEATURE_MAX];
    ProName name;
} ReplayLocalGroup;

typedef struct {
    ProMdl source,target,definition_owner,history_owner;
    PartText *out;
    int count,steps,current_feature,last_feature,external_planes,external_datums,external_total;
    unsigned char external_used[REPLAY_EXTERNAL_MAX];
    ProModelitem external_local[REPLAY_EXTERNAL_MAX];
    struct { int source,target; } mapped[REPLAY_FEATURE_MAX];
    ReplayExtendInput extensions[REPLAY_FEATURE_MAX];
    int extend_count;
    ReplayReferencePattern *reference_pattern;
    ReplayFailure *failure;
    int keep_prefix,completed,prefix_intact,manual;
    ReplayReferenceFailure reference_error;
    ReplayLocalGroup local_group;
} ImitationReplay;

#include "imitation_replay_curved_refs.inc"

typedef struct {
    int present,quilt,sheet;
    ProSrftype kind;
    ProVector normal;
} ReplayEdgeFace;

typedef struct {
    ProGeomitem match;
    int count,datum,edge_topology,witness_set;
    ReplayEdgeFace faces[2];
    ProVector samples[3];
    ProVector point,normal,witness;
    ProCsysdata frame;
    ProGeomitem original;
    ReplayTrimmedFace trimmed;
    int sheet_side;
    ReplayCurve curve;
    int ids[REPLAY_FEATURE_MAX];
} ReplayGeometry;

static ProError replay_csys_frame(ProGeomitem *item,ProCsysdata *frame){
    ProCsys csys;
    ProGeomitemdata *raw=NULL;
    ProError error=ProGeomitemToCsys(item,&csys);
    if(!error)error=ProCsysDataGet(csys,&raw);
    if(!error)*frame=*raw->data.p_csys_data;
    if(raw)ProGeomitemdataFree(&raw);
    return error;
}

static int replay_csys_same(const ProCsysdata *a,const ProCsysdata *b){
    return ad_near(a->origin,b->origin,0.0002)&&ad_near(a->x_vector,b->x_vector,1e-7)&&
        ad_near(a->y_vector,b->y_vector,1e-7)&&ad_near(a->z_vector,b->z_vector,1e-7);
}

static ProError replay_edge_samples(ProGeomitem *item,ProVector points[3]){
    ProEdge edge;
    ProError error=ProGeomitemToEdge(item,&edge);
    for(int i=0;!error&&i<3;i++)error=ProEdgeXyzdataEval(edge,i*0.5,points[i],NULL,NULL,NULL);
    return error;
}

static ProError replay_plane_datum(ProGeomitem *item,int *datum){
    ProSurface surface;
    ProBoolean is_datum;
    ProError error=ProGeomitemToSurface(item,&surface);
    if(!error)error=ProSurfaceIsDatumPlane((ProSolid)item->owner,surface,&is_datum);
    if(!error)*datum=is_datum;
    return error;
}

static ProError replay_edge_faces(ProGeomitem *item,ProVector midpoint,ReplayEdgeFace faces[2]){
    ProEdge edge;
    ProSurface adjacent[2]={0};
    ProError error=ProGeomitemToEdge(item,&edge);
    if(!error)error=ProEdgeNeighborsGet(edge,NULL,NULL,&adjacent[0],&adjacent[1]);
    for(int i=0;!error&&i<2;i++){
        if(!adjacent[i])continue;
        ReplayEdgeFace *face=&faces[i];
        ProQuilt quilt;
        ProUvParam uv;
        ProVector point;
        face->present=1;
        face->sheet=-1;
        error=ProSurfaceTypeGet(adjacent[i],&face->kind);
        if(!error){
            ProError attached=ProSurfaceQuiltGet((ProSolid)item->owner,adjacent[i],&quilt);
            if(attached==PRO_TK_NO_ERROR)face->quilt=1;
            else if(attached!=PRO_TK_E_NOT_FOUND)error=attached;
        }
        ProSmtSurfType sheet;
        if(!ProSmtSurfaceTypeGet((ProPart)item->owner,adjacent[i],&sheet))face->sheet=sheet;
        if(!error)error=ProSurfaceParamEval((ProSolid)item->owner,adjacent[i],midpoint,uv);
        if(!error)error=ProSurfaceXyzdataEval(adjacent[i],uv,point,NULL,NULL,face->normal);
        if(!error&&!ad_unit(face->normal))error=PRO_TK_INCOMPLETE;
    }
    return error;
}

static int replay_same_edge_face(const ReplayEdgeFace *a,const ReplayEdgeFace *b){
    if(a->present!=b->present)return 0;
    return !a->present||(a->kind==b->kind&&a->quilt==b->quilt&&a->sheet==b->sheet&&
        ad_dot(a->normal,b->normal)>0.999999);
}


static ProError replay_match_geometry(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    ReplayGeometry *search=data;
    const double tol=0.0002;
    if(item->type==PRO_EDGE){
        ProVector points[3];
        if(replay_edge_samples(item,points)||!ad_near(points[1],search->samples[1],tol))return PRO_TK_NO_ERROR;
        if(!(ad_near(points[0],search->samples[0],tol)&&ad_near(points[2],search->samples[2],tol))&&
           !(ad_near(points[0],search->samples[2],tol)&&ad_near(points[2],search->samples[0],tol)))return PRO_TK_NO_ERROR;
        if(search->edge_topology){
            ReplayEdgeFace faces[2]={0};
            if(replay_edge_faces(item,points[1],faces))return PRO_TK_NO_ERROR;
            if(!(replay_same_edge_face(&search->faces[0],&faces[0])&&replay_same_edge_face(&search->faces[1],&faces[1]))&&
               !(replay_same_edge_face(&search->faces[0],&faces[1])&&replay_same_edge_face(&search->faces[1],&faces[0])))
                return PRO_TK_NO_ERROR;
        }
    }else if(item->type==PRO_AXIS){
        FdRef reference={0};
        reference.item=*item;
        ProVector point,direction,delta,cross;
        if(ad_line(&reference,point,direction)||ad_dot(direction,search->normal)<0.999999)return PRO_TK_NO_ERROR;
        ad_sub(point,search->point,delta);
        ad_cross(delta,search->normal,cross);
        if(sqrt(ad_dot(cross,cross))>tol)return PRO_TK_NO_ERROR;
    }else if(item->type==PRO_POINT){
        FdRef reference={0};
        reference.item=*item;
        ProVector point;
        if(ad_point(&reference,point)||!ad_near(point,search->point,tol))return PRO_TK_NO_ERROR;
    }else if(item->type==PRO_CSYS){
        ProCsysdata frame;
        if(replay_csys_frame(item,&frame)||!replay_csys_same(&frame,&search->frame))return PRO_TK_NO_ERROR;
    }else if(item->type==PRO_CURVE){
        ReplayCurve curve;
        if(replay_curve_read(item,&curve)||!replay_curve_same(&curve,&search->curve))return PRO_TK_NO_ERROR;
    }else if(search->trimmed.count){
        if(!replay_trimmed_face_matches(item,&search->original,&search->trimmed))return PRO_TK_NO_ERROR;
    }else{
        int datum;
        ProVector point,normal,delta;
        if(replay_plane_datum(item,&datum)||datum!=search->datum||fd_plane_data(item,point,normal)||
           !ad_unit(normal)||ad_dot(normal,search->normal)<0.999999)return PRO_TK_NO_ERROR;
        ad_sub(point,search->point,delta);
        if(fabs(ad_dot(delta,search->normal))>tol)return PRO_TK_NO_ERROR;
    }
    if(search->sheet_side>=0){
        ProSurface surface;
        ProSmtSurfType side;
        if(ProGeomitemToSurface(item,&surface)||
           ProSmtSurfaceTypeGet((ProPart)item->owner,surface,&side)||side!=search->sheet_side)return PRO_TK_NO_ERROR;
    }
    if(search->witness_set){
        ProSurface surface;
        ProUvParam uv;
        ProUvStatus inside;
        ProVector xyz;
        if(ProGeomitemToSurface(item,&surface)||
           ProSurfaceParamEval((ProSolid)item->owner,surface,search->witness,uv)||
           ProSurfaceUvpntVerify((ProSolid)item->owner,surface,uv,&inside)||inside!=PRO_UV_INSIDE||
           ProSurfaceXyzdataEval(surface,uv,xyz,NULL,NULL,NULL)||!ad_near(xyz,search->witness,tol))return PRO_TK_NO_ERROR;
    }
    for(int i=0;i<search->count;i++)if(search->ids[i]==item->id)return PRO_TK_NO_ERROR;
    if(search->count==_countof(search->ids))return PRO_TK_OUT_OF_RANGE;
    search->ids[search->count++]=item->id;
    search->match=*item;
    return PRO_TK_NO_ERROR;
}

typedef struct {ProMdl model;int owner;ReplayGeometry *search;} ReplayCurrentFaces;

static ProError replay_current_face(ProSurface surface,ProError status,ProAppData data)
{
    if(status)return status;
    ReplayCurrentFaces *visit=data;
    ProGeomitem item;
    ProFeature owner;
    if(ProSurfaceToGeomitem((ProSolid)visit->model,surface,&item)||
       ProGeomitemFeatureGet(&item,&owner)||owner.id!=visit->owner)return PRO_TK_NO_ERROR;
    return replay_match_geometry(&item,PRO_TK_NO_ERROR,visit->search);
}

#include "imitation_replay_external.inc"
#include "imitation_replay_dimensions.inc"

#include "imitation_replay_manual.inc"

static ProError replay_reference_find(ImitationReplay *replay,ProReference ref,ProModelitem *target,int edge_endpoint){
    ProType type;
    int id;
    ProMdl owner=NULL;
    ProError error=ProReferenceIdGet(ref,&id);
    if(error||id<=0)return PRO_TK_EMPTY;
    error=ProReferenceOwnerGet(ref,&owner);
    if(!error&&owner&&owner!=replay->source&&owner!=replay->definition_owner&&owner!=replay->history_owner){
        if((error=ProReferenceTypeGet(ref,&type)))return error;
        if(type==PRO_SURFACE)return replay_external_surface(replay,ref,target);
        if(type==PRO_POINT)return replay_external_point(replay,ref,target);
        if(type==PRO_CURVE){
            ReplayExternalReference ext={0};
            error=replay_external_reference(replay,ref,&ext);
            return error?error:replay_external_line(replay,&ext,target);
        }
        return PRO_TK_UNSUPPORTED;
    }
    if(error||!owner){
        ProFamilyMdlName referenced=L"";
        ProMdlName source=L"";
        if(ProReferenceOwnerMdlnameGet(ref,referenced)||ProMdlMdlnameGet(replay->source,source)||
           _wcsicmp(referenced,source))return PRO_TK_BAD_CONTEXT;
    }
    if((error=ProReferenceTypeGet(ref,&type)))return error;
    if(edge_endpoint&&(type==PRO_EDGE_START||type==PRO_EDGE_END||type==PRO_EDGE_PNT))type=PRO_EDGE;
    if(type==PRO_DIMENSION)return replay_dimension_item(replay,owner?owner:replay->source,id,target);
    if(type==PRO_BODY){
        error=qa_single_body(replay->target,target);
        if(error!=PRO_TK_BAD_CONTEXT)return error;
        int active=0;
        ProSolidBody source_body;
        if(sd_body_count(replay->target,&active)||active||
           ProSolidDefaultBodyGet((ProSolid)(owner?owner:replay->source),&source_body)||
           source_body.id!=id)return error;
        return ProSolidDefaultBodyGet((ProSolid)replay->target,target);
    }
    if(type==PRO_QUILT){
        SfQuiltFind original={0};
        original.model=owner&&owner==replay->history_owner?owner:replay->source;
        original.id=id;
        ProFeature source_feature,created;
        error=ProSolidQuiltVisit((ProSolid)original.model,sf_find_quilt,NULL,&original);
        if(!error&&original.count!=1)error=PRO_TK_E_NOT_FOUND;
        if(!error)error=ProGeomitemFeatureGet(&original.item,&source_feature);
        if(error)return error;
        for(int i=0;i<replay->count;i++)if(replay->mapped[i].source==source_feature.id){
            error=ProFeatureInit((ProSolid)replay->target,replay->mapped[i].target,&created);
            return error?error:fd_feature_geom(&created,PRO_QUILT,target);
        }
        return PRO_TK_E_NOT_FOUND;
    }
    if(type==PRO_FEATURE){
        for(int i=0;i<replay->count;i++)if(replay->mapped[i].source==id)
            return ProModelitemInit(replay->target,replay->mapped[i].target,PRO_FEATURE,target);
        return PRO_TK_E_NOT_FOUND;
    }
    if(type!=PRO_SURFACE&&type!=PRO_EDGE&&type!=PRO_AXIS&&type!=PRO_POINT&&type!=PRO_CSYS&&type!=PRO_CURVE)
        return PRO_TK_UNSUPPORTED;

    ProGeomitem original;
    ProFeature source_feature,created;
    if((error=ProModelitemInit(owner&&owner==replay->history_owner?owner:replay->source,id,type,&original))||
       (error=ProGeomitemFeatureGet(&original,&source_feature)))return error;
    int created_id=0;
    for(int i=0;i<replay->count;i++)if(replay->mapped[i].source==source_feature.id){
        created_id=replay->mapped[i].target;break;
    }
    if(!created_id)return PRO_TK_E_NOT_FOUND;
    if((error=ProFeatureInit((ProSolid)replay->target,created_id,&created)))return error;
    ReplayGeometry search={.sheet_side=-1};
    search.original=original;
    if(type==PRO_EDGE)error=replay_edge_samples(&original,search.samples);
    else if(type==PRO_CURVE)error=replay_curve_read(&original,&search.curve);
    else if(type==PRO_CSYS)error=replay_csys_frame(&original,&search.frame);
    else if(type==PRO_AXIS||type==PRO_POINT){
        FdRef reference={0};
        reference.item=original;
        error=type==PRO_AXIS?ad_line(&reference,search.point,search.normal):ad_point(&reference,search.point);
    }
    else{
        ProSurface surface;
        ProSrftype kind;
        error=ProGeomitemToSurface(&original,&surface);
        if(!error)error=ProSurfaceTypeGet(surface,&kind);
        if(!error&&kind==PRO_SRF_PLANE){
            ProSmtSurfType side;
            ProMdlsubtype subtype;
            if(!ProMdlSubtypeGet(replay->target,&subtype)&&subtype==PROMDLSTYPE_PART_SHEETMETAL&&
               !ProSmtSurfaceTypeGet((ProPart)original.owner,surface,&side))search.sheet_side=side;
            error=replay_plane_datum(&original,&search.datum);
            if(!error)error=fd_plane_data(&original,search.point,search.normal);
            if(!error&&!ad_unit(search.normal))error=PRO_TK_INCOMPLETE;
        }else if(!error&&(kind==PRO_SRF_CYL||kind==PRO_SRF_CONE)){
            error=replay_trimmed_face_read(surface,(ProSolid)original.owner,&search.trimmed);
        }else if(!error)error=PRO_TK_UNSUPPORTED;
    }
    if(!error)error=ProFeatureGeomitemVisit(&created,type,replay_match_geometry,NULL,&search);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error&&type==PRO_SURFACE&&!search.trimmed.count&&search.count>1&&!search.datum&&
       !ProReferencePointGet(ref,search.witness)){
        search.witness_set=1;
        search.count=0;
        error=ProFeatureGeomitemVisit(&created,type,replay_match_geometry,NULL,&search);
        if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
        if(!search.count){
            search.witness_set=0;
            if(!error)error=ProFeatureGeomitemVisit(&created,type,replay_match_geometry,NULL,&search);
        }
    }
    if(!error&&type==PRO_SURFACE&&!search.trimmed.count&&search.count>1&&!search.datum){
        ProSurface surface;
        if(!ProGeomitemToSurface(&original,&surface)&&
           !replay_trimmed_face_read(surface,(ProSolid)original.owner,&search.trimmed)){
            search.count=0;
            error=ProFeatureGeomitemVisit(&created,type,replay_match_geometry,NULL,&search);
        }
    }
    if(!error&&type==PRO_EDGE&&search.count>1&&
       !replay_edge_faces(&original,search.samples[1],search.faces)){
        search.edge_topology=1;
        search.count=0;
        error=ProFeatureGeomitemVisit(&created,type,replay_match_geometry,NULL,&search);
    }
    if(error)return error;
    if(type==PRO_SURFACE&&!search.count&&!search.datum){
        ReplayCurrentFaces visit={replay->target,created.id,&search};
        error=ProSolidSurfaceVisit((ProSolid)replay->target,replay_current_face,NULL,&visit);
        if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    }
    if(type==PRO_SURFACE&&!search.count&&!search.datum&&!search.trimmed.count&&search.sheet_side>=0){
        CabinetFace wall;
        ProFeattype feature_type;
        ProModelitem item;
        if(!ProFeatureTypeGet(&created,&feature_type)&&feature_type==PRO_FEAT_WALL&&
           !cm_wall_face(&created,&wall)&&!ProModelitemInit(replay->target,wall.id,PRO_SURFACE,&item)){
            error=replay_match_geometry(&item,PRO_TK_NO_ERROR,&search);
            if(error)return error;
        }
    }
    if(search.count>1&&replay->manual&&(type==PRO_SURFACE||type==PRO_EDGE))
        return replay_manual_reference(replay,&created,type,&search,target);
    if(search.count!=1){
        trace("Replay original owning feature",source_feature.id);
        trace("Replay remapped owning feature",created.id);
        trace("Replay geometry reference type",type);
        trace("Replay geometry reference ID",id);
        trace("Replay geometry candidates",search.count);
        return search.count?PRO_TK_E_AMBIGUOUS:PRO_TK_E_NOT_FOUND;
    }
    *target=search.match;
    return PRO_TK_NO_ERROR;
}

#include "imitation_replay_reference_retry.inc"

static ProError replay_map_reference(ImitationReplay *replay,ProReference source,ProReference *target){
    ProModelitem item;
    ProError error=replay_reference_item(replay,source,&item,0);
    if(error&&error!=PRO_TK_EMPTY&&!replay->reference_error.error){
        ReplayReferenceFailure *failure=&replay->reference_error;
        failure->error=error;
        if(ProReferenceIdGet(source,&failure->item))ProReferenceOriginalidGet(source,&failure->item);
        ProReferenceTypeGet(source,&failure->type);
        if(ProReferenceOwnerMdlnameGet(source,failure->owner))
            ProReferenceOriginalownerMdlnameGet(source,failure->owner);
    }
    if(!error)error=ProReferenceAlloc(target);
    if(!error)error=ProReferenceSet(*target,NULL,&item);
    if(error&&*target){ProReferenceFree(*target);*target=NULL;}
    return error;
}

static void replay_report_reference_failure(const ReplayReferenceFailure *failure,PartText *out){
    if(!failure->error)return;
    part_text_add(out,L"参照 %ls，类型 %d，ID %d，Creo %d。\n",
        failure->owner[0]?failure->owner:L"Creo 未返回模型名",failure->type,failure->item,failure->error);
}

static ProError replay_reference_array(ImitationReplay *replay,ProElement old,ProElement fresh){
    ProReference *input=NULL,*output=NULL;
    int count=0;
    ProError error=ProElementReferencesGet(old,NULL,&input);
    if(!error&&input)error=ProArraySizeGet((ProArray)input,&count);
    if(!error&&!count)error=PRO_TK_EMPTY;
    if(!error)error=ProArrayAlloc(0,sizeof(ProReference),1,(ProArray *)&output);
    for(int i=0;!error&&i<count;i++){
        ProReference mapped=NULL;
        error=replay_map_reference(replay,input[i],&mapped);
        if(!error)error=ProArrayObjectAdd((ProArray *)&output,PRO_VALUE_UNUSED,1,&mapped);
        if(error&&mapped)ProReferenceFree(mapped);
    }
    if(!error)error=ProElementReferencesSet(fresh,output);
    if(input)ProReferencearrayFree(input);
    if(output)ProReferencearrayFree(output);
    return error;
}

#include "imitation_replay_history.inc"

static ProError replay_curve_collection(ImitationReplay *replay,ProElement source,ProElement target){
    ProCollection old=NULL,fresh=NULL;
    ProCrvcollinstr *instructions=NULL;
    int count=0;
    ProError error=ProElementCollectionGet(source,NULL,&old);
    if(!error)error=ProCrvcollectionInstructionsGet(old,&instructions);
    if(!error&&instructions)error=ProArraySizeGet((ProArray)instructions,&count);
    if(error==PRO_TK_EMPTY||error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error&&count)error=ProCrvcollectionAlloc(&fresh);
    if(!error&&count&&replay->history_owner)error=replay_history_chain(replay,old,fresh);
    for(int i=0;!error&&!replay->history_owner&&i<count;i++){
        ProCrvcollinstr next=NULL;
        ProCrvcollinstrType type;
        ProReference *refs=NULL;
        int size=0;
        error=ProCrvcollinstrTypeGet(instructions[i],&type);
        if(!error&&type!=PRO_CURVCOLL_ADD_ONE_INSTR&&type!=PRO_CURVCOLL_ADD_TANGENT_INSTR&&
           type!=PRO_CURVCOLL_TAN_INSTR){
            trace("Replay unsupported curve instruction",type);
            error=PRO_TK_UNSUPPORTED;
        }
        if(!error)error=ProCrvcollinstrAlloc(type,&next);
        if(!error)error=ProCrvcollinstrReferencesGet(instructions[i],&refs);
        if(error==PRO_TK_EMPTY||error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
        if(!error&&refs)error=ProArraySizeGet((ProArray)refs,&size);
        for(int j=0;!error&&j<size;j++){
            ProReference reference=NULL;
            error=replay_map_reference(replay,refs[j],&reference);
            if(!error)error=ProCrvcollinstrReferenceAdd(next,reference);
            if(reference)ProReferenceFree(reference);
        }
        for(int attr=PRO_CURVCOLL_ALL;!error&&attr<=PRO_CURVCOLL_CONCAVE;attr++){
            ProBoolean set=PRO_B_FALSE;
            if(!ProCrvcollinstrAttributeIsSet(instructions[i],attr,&set)&&set)
                error=ProCrvcollinstrAttributeSet(next,attr);
        }
        if(!error)error=ProCrvcollectionInstructionAdd(fresh,next);
        if(refs)ProReferencearrayFree(refs);
        if(next)ProCrvcollinstrFree(next);
    }
    if(!error&&fresh)error=ProElementCollectionSet(target,fresh);
    if(instructions)ProCrvcollinstrArrayFree(instructions);
    if(old)ProCollectionFree(&old);
    if(fresh)ProCollectionFree(&fresh);
    return error;
}
