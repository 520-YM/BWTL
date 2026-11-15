static void smy_stock_group_report(const SmyStockList *list,PartText *out){
    SmyStockPlan plan={0};
    smy_stock_groups(list,&plan);
    for(int group=0;group<plan.groups;group++){
        int count=0,first=-1;
        for(int i=0;i<list->count;i++)if(plan.group[i]==group){
            if(first<0)first=i;
            count+=list->parts[i].quantity;
        }
        const SmyStockMaterial *s=&list->parts[first].material;
        part_text_add(out,L"组 %d：%ls，%.8g mm，纹向 %ls，覆膜 %ls，共 %d 件。\n",group+1,s->grade,s->thickness,s->grain,s->film,count);
        for(int i=0;i<list->count;i++)if(plan.group[i]==group)
            part_text_add(out,L"  %ls × %d。\n",list->parts[i].name,list->parts[i].quantity);
    }
}

static void smy_stock_fit(const SmyStockList *list,const SmyStockBoard *board,PartText *out){
    for(int i=0;i<list->count;i++){
        const SmyStockPart *p=&list->parts[i];
        double w,h;
        int normal=smy_stock_orient(p,board,0,&w,&h),turned=smy_stock_orient(p,board,1,&w,&h);
        part_text_add(out,L"%ls，包络 %.8g × %.8g mm：%ls。纹向 %ls，覆膜 %ls。\n",p->name,p->width,p->height,
            normal&&turned?L"0° 和 90° 均可放入":normal?L"可用 0°":turned?L"可用 90°":L"无可用方向",
            p->material.grain,p->material.film);
    }
}

static void smy_stock_hand(const SmyStockList *list,PartText *out){
    for(int i=0;i<list->count;i++){
        const SmyStockPart *p=&list->parts[i];
        part_text_add(out,L"%ls × %d，%lc 件，纹向 %ls，覆膜 %ls，允许 %ls，镜像禁止。\n",
            p->name,p->quantity,p->hand,p->material.grain,p->material.film,p->rotate?L"0° 和 90°":L"0°");
    }
}

static void smy_stock_common(const SmyStockList *list,double kerf,PartText *out){
    int count=0;
    for(int i=0;i<list->count;i++){
        const SmyStockPart *p=&list->parts[i];
        if(!p->rectangle)continue;
        for(int j=i;j<list->count;j++){
            const SmyStockPart *q=&list->parts[j];
            if((i==j&&p->quantity<2)||!q->rectangle||!smy_same_stock(&p->material,&q->material))continue;
            for(int turn=0;turn<=q->rotate;turn++){
                double w=turn?q->height:q->width,h=turn?q->width:q->height;
                if(fabs(p->height-h)>FAB_JOIN&&fabs(p->width-w)>FAB_JOIN)continue;
                count++;
                part_text_add(out,L"%ls / %ls（第二件 %d°）：%ls 直边可对齐。输入割缝 %.8g mm，候选共边仍须 CAM 复核实际补偿及边尺寸。\n",
                    p->name,q->name,90*turn,fabs(p->height-h)<=FAB_JOIN&&fabs(p->width-w)<=FAB_JOIN?L"横、纵":fabs(p->height-h)<=FAB_JOIN?L"纵向":L"横向",kerf);
            }
        }
    }
    part_text_add(out,L"共 %d 个等长矩形直边候选。未改排样或输出切割程序。\n",count);
}

static ProError smy_stock_remnants(const SmyStockList *job,const wchar_t *base,double border,PartText *out){
    SmyStockList remnants={0};
    ProError error=smy_stock_list(base,1,&remnants,out);
    if(error)return error;
    for(int i=0;i<remnants.count;i++){
        const SmyStockPart *r=&remnants.parts[i];
        if(!r->rectangle){
            part_text_add(out,L"库存 %ls：%.8g mm²，非完整矩形，未自动匹配。\n",r->name,r->net);
            continue;
        }
        SmyStockBoard b={r->width,r->height,border,0,0};
        if(!smy_board_valid(&b)){
            part_text_add(out,L"库存 %ls：扣除留边后无可用区域。\n",r->name);
            continue;
        }
        int matches=0;
        for(int j=0;j<job->count;j++){
            const SmyStockPart *p=&job->parts[j];
            if(!smy_same_stock(&p->material,&r->material))continue;
            double w,h;
            int base=smy_stock_orient(p,&b,0,&w,&h),turn=smy_stock_orient(p,&b,1,&w,&h);
            if(!base&&!turn)continue;
            matches++;
            part_text_add(out,L"库存 %ls（%ls，%.8g mm²）：%ls 单件包络可按 %ls 放入。\n",r->name,r->drawing,r->net,p->name,
                base&&turn?L"0° 或 90°":base?L"0°":L"90°");
        }
        if(!matches)part_text_add(out,L"库存 %ls 未匹配当前订单。\n",r->name);
    }
    return PRO_TK_NO_ERROR;
}

static ProError smy_stock_execute(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    if(kind==SMY_STOCK_MEASURE)return smy_stock_measure(model,cmd,out);
    SmyStockList list={0};
    ProError error=smy_stock_list(cmd->arg[2],0,&list,out);
    if(error)return error;
    switch(kind){
    case SMY_STOCK_GROUP:smy_stock_group_report(&list,out);return PRO_TK_NO_ERROR;
    case SMY_STOCK_HAND:smy_stock_hand(&list,out);return PRO_TK_NO_ERROR;
    case SMY_STOCK_COMMON:smy_stock_common(&list,as_arg(cmd,3),out);return PRO_TK_NO_ERROR;
    case SMY_STOCK_REMNANT:return smy_stock_remnants(&list,cmd->arg[3],as_arg(cmd,4),out);
    }
    SmyStockBoard board=smy_board(cmd);
    if(!smy_board_valid(&board))return PRO_TK_BAD_INPUTS;
    if(kind==SMY_STOCK_FIT){smy_stock_fit(&list,&board,out);return PRO_TK_NO_ERROR;}
    if(kind==SMY_STOCK_GRID){smy_stock_grid(&list,&board,out);return PRO_TK_NO_ERROR;}
    SmyStockPlan plan={0};
    smy_nest(&list,&board,&plan);
    double clearance,gap;
    error=smy_nest_audit(&list,&board,&plan,&clearance,&gap);
    if(error)return error;
    part_text_add(out,L"板材 %d 张，订单 %d 件，已排 %d 件，未排 %d 件。坐标、夹持禁布区、板边、件间距及用量均已核对。\n",
        plan.sheet_count,list.quantity,plan.placed,list.quantity-plan.placed);
    if(kind==SMY_STOCK_AUDIT){
        if(isfinite(clearance))part_text_add(out,L"相对于允许排版区域边界的最小剩余裕量 %.8g mm。\n",clearance);
        if(isfinite(gap))part_text_add(out,L"同一板上包络之间的最小实测间距 %.8g mm。\n",gap);
        for(int i=0;i<list.count;i++)if(plan.unplaced[i])part_text_add(out,L"%ls 有 %d 件超出有效板材。\n",list.parts[i].name,plan.unplaced[i]);
        return PRO_TK_NO_ERROR;
    }
    return smy_stock_output(&list,&board,&plan,cmd->arg[8],out);
}
