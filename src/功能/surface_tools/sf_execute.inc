static ProError sf_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    const wchar_t *labels[]={L"",L"拉伸曲面",L"旋转曲面",L"边界混合曲面",L"闭合边界补面",L"曲面等距偏移",L"曲面裁剪",L"曲面延伸",L"曲面缝合",L"封闭曲面实体化",L"曲面加厚",L"独立实体拉伸",L"多实体布尔运算"};
    FdTxn tx={0};
    SfArea area={0};
    double original_area=0;
    FdRef original={0};
    int kind=sf_kind(cmd->arg[0]),bodies=0,material=0;
    ProFeattype type=kind==SF_EXTEND?PRO_FEAT_EXTEND:
        kind==SF_TRIM?PRO_FEAT_CUT:kind==SF_MERGE?PRO_FEAT_DATUM_QUILT:
        kind==SF_BOOLEAN?PRO_FEAT_BOOLEANBODIES:kind>=SF_SOLIDIFY?PRO_FEAT_PROTRUSION:PRO_FEAT_DATUM_SURF;
    ProError e;
    if(kind==SF_INFO)return sf_info(model,report,capacity);
    if(surface_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
#define SF_TRY(label,call) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    SF_TRY("prepare surface operation",fd_begin(&tx,model,cmd->arg[2],type));
    SF_TRY("read pre-operation body count",sd_body_count(model,&bodies));
    if(kind==SF_TRIM||kind==SF_EXTEND){
        SF_TRY("read original quilt",sf_ref(model,cmd->arg[3],&original));
        SF_TRY("measure original quilt",sf_area(&original,&area));
        original_area=area.area;
        fd_ref_free(&original);
    }
    if(kind==SF_BOOLEAN&&bodies!=2){e=PRO_TK_BAD_CONTEXT;goto done;}
    if((kind==SF_SOLIDIFY||kind==SF_THICKEN)&&bodies){e=PRO_TK_BAD_CONTEXT;goto done;}
    if(kind==SF_BODY_EXTRUDE&&bodies>=8){e=PRO_TK_BAD_CONTEXT;goto done;}
    switch(kind){
    case SF_EXTRUDE:case SF_REVOLVE:case SF_FILL:case SF_BODY_EXTRUDE:
        SF_TRY("define surface or independent body section",sf_extrude(&tx,cmd,kind));break;
    case SF_BOUNDARY:SF_TRY("define boundary curves",sf_boundary(&tx,cmd));break;
    case SF_OFFSET:SF_TRY("define quilt offset",sf_offset(&tx,cmd));break;
    case SF_TRIM:SF_TRY("define quilt trim",sf_trim(&tx,cmd));break;
    case SF_EXTEND:SF_TRY("define surface extension",sf_extend(&tx,cmd));break;
    case SF_MERGE:SF_TRY("define quilt join",sf_merge(&tx,cmd));break;
    case SF_SOLIDIFY:case SF_THICKEN:SF_TRY("define surface material feature",sf_thicken(&tx,cmd,kind==SF_SOLIDIFY));break;
    case SF_BOOLEAN:SF_TRY("define body boolean operation",sf_boolean(&tx,cmd));break;
    default:e=PRO_TK_BAD_INPUTS;goto done;
    }
    if(!tx.created)SF_TRY("create native surface feature",fd_create(&tx,0));
    if(kind==SF_BOOLEAN)SF_TRY("verify native body references",sf_boolean_verify(&tx,cmd));
    material=kind==SF_BODY_EXTRUDE?bodies+1:kind>=SF_SOLIDIFY?1:0;
    SF_TRY("verify surface geometry, parents and material",sf_verify(&tx,bodies,material,(kind==SF_TRIM||kind==SF_EXTEND||kind==SF_MERGE)?cmd->arg[3]:NULL,&area));
    if((kind==SF_TRIM&&area.area>=original_area-1e-8*tx.scale*tx.scale)||
       (kind==SF_EXTEND&&area.area<=original_area+1e-8*tx.scale*tx.scale)){
        e=PRO_TK_GENERAL_ERROR;tx.stage="verify actual quilt area change";goto done;
    }
    if(material)swprintf_s(report,capacity,L"已完成%ls %ls。已核对实际实体、参照和重生成，未保存。",labels[kind],cmd->arg[2]);
    else swprintf_s(report,capacity,L"已完成%ls %ls，%d 个面，面积 %.9g mm²。已核对实际曲面、参照和重生成，未保存。",labels[kind],cmd->arg[2],area.count,area.area/pow(tx.scale,2));
done:
    fd_ref_free(&original);
    if(e&&tx.errors.error_list)for(int i=0;i<tx.errors.error_number&&i<32;i++){
        trace("surface element id",tx.errors.error_list[i].err_item_id);
        trace("surface element error",tx.errors.error_list[i].error);
    }
    sf_finish(&tx,e,bodies,report,capacity);
    return e;
#undef SF_TRY
}
