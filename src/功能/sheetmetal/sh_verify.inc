static ProError sh_verify_detail(FdTxn *tx,const PartCommand *cmd,int k,PartText *out){
    ProError e;
    ProFeatStatus state;
    ProFeattype actual,expected;
    ProName name;
    ProMassProperty mass;
    ProDimension thickness;
    double t;
    int bodies,*parents=NULL,n=0;
    expected=k==SH_SKETCH?PRO_FEAT_CURVE:k==SH_FLANGE?PRO_FEAT_WALL:k==SH_BEND||k==SH_HEM?PRO_FEAT_BEND:k==SH_CUT?PRO_FEAT_CUT_SMT:
        k==SH_UNBEND?PRO_FEAT_UNBEND:k==SH_REBEND?PRO_FEAT_BEND_BACK:k==SH_FORM?PRO_FEAT_SMT_SKETCH_FORM:PRO_FEAT_DATUM_SURF;
#define SHV(call,label,plain) do{tx->stage=label;e=(call);trace(label,e); \
    if(e){if(out)part_text_add(out,L"%ls",plain);return e;}}while(0)
    SHV(ProFeatureStatusGet(&tx->feature,&state),"sheet verification read feature status",L"插件没能读到新特征的启用状态");
    SHV(ProFeatureTypeGet(&tx->feature,&actual),"sheet verification read feature type",L"插件没能读到新特征的原生类型");
    if(state!=PRO_FEAT_ACTIVE||actual!=expected){
        tx->stage="sheet verification feature status or type mismatch";
        if(out)part_text_add(out,L"新特征的状态为 %d，类型为 %d，验收要求启用状态和原生类型 %d",state,actual,expected);
        return PRO_TK_GENERAL_ERROR;
    }
    tx->stage="sheet verification read feature name";
    e=ProModelitemNameGet((ProModelitem *)&tx->feature,name);
    if(e==PRO_TK_E_NOT_FOUND&&k==SH_FLANGE&&tx->created){
        /* Redefining a stock wall profile can discard its initial name. */
        e=ProModelitemNameSet((ProModelitem *)&tx->feature,tx->name);
        if(!e)e=ProModelitemNameGet((ProModelitem *)&tx->feature,name);
    }
    trace(tx->stage,e);
    if(e){
        if(out)part_text_add(out,L"插件没能读到已生成特征的名称");
        return e;
    }
    if(_wcsicmp(name,tx->name)){
        tx->stage="sheet verification feature name mismatch";
        if(out)part_text_add(out,L"新特征实际名为 %ls，和请求的 %ls 不一致",name,tx->name);
        return PRO_TK_GENERAL_ERROR;
    }
    SHV(ProFeatureParentsGet(&tx->feature,&parents,&n),"sheet verification read feature parents",L"插件没能读取新特征引用的上游特征");
    for(int i=0;i<tx->parent_count;i++){
        int j=0;
        while(j<n&&parents[j]!=tx->parents[i])j++;
        if(j==n){
            if(parents)ProArrayFree((ProArray *)&parents);
            tx->stage="sheet verification missing parent";
            if(out)part_text_add(out,L"新特征没有保留预期的上游参照（ID %d）",tx->parents[i]);
            return PRO_TK_GENERAL_ERROR;
        }
    }
    if(parents)ProArrayFree((ProArray *)&parents);
    if(k==SH_SKETCH){
        SHV(fd_verify(tx),"sheet verification sketch references and dimensions",L"草绘的几何、尺寸或参照没有通过回读校验");
        return 0;
    }
    SHV(sd_body_count(tx->model,&bodies),"sheet verification read body count",L"插件没能读取钣金实体数量");
    SHV(ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass),
        "sheet verification read volume",L"插件没能测得加工后的实体体积");
    SHV(ProSmtPartThicknessGet((ProPart)tx->model,&thickness),"sheet verification locate part thickness",L"插件没能找到钣金板厚尺寸");
    SHV(ProDimensionValueGet(&thickness,&t),"sheet verification read thickness value",L"插件没能读到钣金板厚数值");
    if(bodies!=1||!isfinite(mass.volume)||mass.volume<=0||!isfinite(t)||t<=0){
        tx->stage="sheet verification invalid body count, volume or thickness";
        if(out)part_text_add(out,L"加工结果须有一个钣金实体及正体积和板厚，实测实体 %d 个、体积 %.9g mm³、板厚 %.9g mm",
            bodies,mass.volume/(tx->scale*tx->scale*tx->scale),t/tx->scale);
        return PRO_TK_BAD_CONTEXT;
    }
    if((k==SH_FIRST||k==SH_CONVERT)&&!part_close(t,as_arg(cmd,4)*tx->scale)){
        tx->stage="sheet verification thickness mismatch";
        if(out)part_text_add(out,L"实际板厚 %.8g mm，未达到指定的 %.8g mm",t/tx->scale,as_arg(cmd,4));
        return PRO_TK_GENERAL_ERROR;
    }
    if(k==SH_CONVERT&&(!tx->has_volume||!part_close(mass.volume,tx->volume))){
        tx->stage="sheet verification conversion changed volume";
        if(out)part_text_add(out,L"转钣金后没有核对到原实体体积保持");
        return PRO_TK_GENERAL_ERROR;
    }
    if(k==SH_FLANGE&&mass.volume<=tx->volume){
        tx->stage="sheet verification flange added no material";
        if(out)part_text_add(out,L"新凸缘没有增加实际钣金材料");
        return PRO_TK_GENERAL_ERROR;
    }
    if(k==SH_CUT&&mass.volume>=tx->volume){
        tx->stage="sheet verification cut removed no material";
        const double scale=tx->scale*tx->scale*tx->scale;
        if(out)part_text_add(out,L"切口已生成并通过重生成，但没有切掉板料（切前 %.9g mm³，切后 %.9g mm³）。草绘是 %ls，切除方向 %ls；要根据这个草绘方向和实际板面核对刀路是否穿过板厚",
            tx->volume/scale,mass.volume/scale,cmd->arg[3]+7,cmd->arg[4]);
        return PRO_TK_GENERAL_ERROR;
    }
    if(k==SH_FORM){
        ProElemId form[]={PRO_E_FEATURE_FORM},depth[]={PRO_E_SKETCH_FORM_DEPTH_PUNCH};
        SHV(mc_tree_value(&tx->feature,form,1,PRO_EXTRUDE,1),"sheet verification form type",L"成型特征没有保留预期的原生形式");
        SHV(mc_tree_value(&tx->feature,depth,1,as_arg(cmd,4)*tx->scale,0),"sheet verification form depth",L"成型特征的实际深度未通过核对");
    }
    return 0;
#undef SHV
}

static ProError sh_verify(FdTxn *tx,const PartCommand *cmd,int k){
    return sh_verify_detail(tx,cmd,k,NULL);
}
