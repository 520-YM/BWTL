/* Some Creo 10 blend feature definitions return PRO_TK_INVALID_TYPE.
   Recover the ordered sketch parents, creating the native operation with
   BWTL's independent feature template and validating the delivered geometry. */
static int replay_blend_subtype(ProFeature *source)
{
    ProLine name=L"";
    return !ProFeatureSubtypeGet(source,name)&&
        (!_wcsicmp(name,L"Blend")||!wcscmp(name,L"混合")||
         !_wcsicmp(name,L"Boundary Blend")||!wcscmp(name,L"边界混合"));
}

static ProError replay_blend_sections(ProFeature *source,ProFeature sections[2],int boundary)
{
    if(!replay_blend_subtype(source))return PRO_TK_INVALID_TYPE;
    int *parents=NULL,count=0,found=0;
    ProError error=ProFeatureParentsGet(source,&parents,&count);
    for(int i=0;!error&&i<count;i++){
        ProFeature parent;
        ProFeattype type;
        error=ProFeatureInit((ProSolid)source->owner,parents[i],&parent);
        if(!error)error=ProFeatureTypeGet(&parent,&type);
        if(error||type!=PRO_FEAT_CURVE)continue;
        if(found==2){error=PRO_TK_UNSUPPORTED;break;}
        sections[found++]=parent;
    }
    if(parents)ProArrayFree((ProArray *)&parents);
    if(error)return error;
    if(found!=2||count!=2)return PRO_TK_UNSUPPORTED;
    int first,second;
    error=ProFeatureNumberGet(&sections[0],&first);
    if(!error)error=ProFeatureNumberGet(&sections[1],&second);
    if(error)return error;
    if(first>second){ProFeature swap=sections[0];sections[0]=sections[1];sections[1]=swap;}
    int section_count=0;
    error=ProFeatureNumSectionsGet(source,&section_count);
    if(!error&&section_count!=(boundary?0:2))error=PRO_TK_UNSUPPORTED;
    return error;
}

static ProError replay_blend_missing_tree(ImitationReplay *replay,ProFeature *source,ProFeattype type,
    ProFeature *created,ProName label,const wchar_t **operation)
{
    const int boundary=type==PRO_FEAT_DATUM_SURF;
    ProFeature sections[2];
    ProError error=replay_blend_sections(source,sections,boundary);
    PartCommand build={0};
    for(int i=0;!error&&i<2;i++){
        ProFeature parent={0};
        ProName name=L"";
        for(int j=0;j<replay->count;j++)if(replay->mapped[j].source==sections[i].id){
            error=ProFeatureInit((ProSolid)replay->target,replay->mapped[j].target,&parent);
            break;
        }
        if(!error&&!parent.id)error=PRO_TK_E_NOT_FOUND;
        if(!error)error=ProModelitemNameGet(&parent,name);
        if(!error&&swprintf_s(build.arg[i+3],PART_TOKEN_MAX,boundary?L"CURVE:%ls":L"%ls",name)<0)
            error=PRO_TK_BAD_INPUTS;
    }
    if(error)return error;
    if(!label[0]||!fd_name(label))return PRO_TK_BAD_INPUTS;
    wcscpy_s(build.arg[2],PART_TOKEN_MAX,label);
    FdTxn tx={0};
    wchar_t report[1024]=L"";
    if(boundary){
        *operation=L"两边自由边界混合曲面";
        error=sh_begin(&tx,replay->target,label,type);
        if(!error)error=sf_boundary(&tx,&build);
        if(!error)*created=tx.feature;
        fd_cleanup(&tx,0);
    }else{
        *operation=L"两截面平行实体混合";
        error=sd_blend_sections(&build,replay->target,report,_countof(report),0);
        if(!error)error=ux_named_feature(replay->target,label,created);
    }
    if(!error)error=part_regenerate(replay->target);
    if(!error)error=part_health(replay->target);
    if(!error)part_text_add(replay->out,L"按源件的两条父级截面重建原生命令，成品继续核对局部轮廓。\n");
    return error;
}

