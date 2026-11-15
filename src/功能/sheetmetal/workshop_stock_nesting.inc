typedef struct {
    double width,height,edge,clamp,gap;
} SmyStockBoard;

typedef struct {
    int part,sheet,turn;
    double x,y,w,h;
} SmyStockPlace;

typedef struct {
    int group;
    double x,y,row,net,boxes;
} SmyStockSheet;

typedef struct {
    SmyStockSheet sheets[SMY_STOCK_ITEMS];
    SmyStockPlace places[SMY_STOCK_ITEMS];
    int sheet_count,placed,unplaced[SMY_STOCK_PARTS],group[SMY_STOCK_PARTS],groups;
} SmyStockPlan;

static SmyStockBoard smy_board(const PartCommand *cmd){
    return (SmyStockBoard){as_arg(cmd,3),as_arg(cmd,4),as_arg(cmd,5),as_arg(cmd,6),as_arg(cmd,7)};
}
static double smy_board_bottom(const SmyStockBoard *b){return b->edge+b->clamp;}
static int smy_board_valid(const SmyStockBoard *b){
    return b->width>2*b->edge&&b->height>b->edge+smy_board_bottom(b);
}

static int smy_stock_orient(const SmyStockPart *p,const SmyStockBoard *b,int turn,double *w,double *h){
    *w=turn?p->height:p->width;
    *h=turn?p->width:p->height;
    return (!turn||p->rotate)&&*w<=b->width-2*b->edge+1e-8&&*h<=b->height-b->edge-smy_board_bottom(b)+1e-8;
}

static void smy_stock_groups(const SmyStockList *list,SmyStockPlan *plan){
    for(int i=0;i<list->count;i++){
        int earlier=0;
        while(earlier<i&&!smy_same_stock(&list->parts[i].material,&list->parts[earlier].material))earlier++;
        plan->group[i]=earlier<i?plan->group[earlier]:plan->groups++;
    }
}

static int smy_try_row(const SmyStockPart *part,const SmyStockBoard *board,const SmyStockSheet *sheet,SmyStockPlace *best){
    double best_height=INFINITY,best_right=INFINITY;
    int found=0;
    for(int turn=0;turn<=1;turn++){
        double w,h;
        if(!smy_stock_orient(part,board,turn,&w,&h))continue;
        for(int next_row=0;next_row<=1;next_row++){
            if(next_row&&!sheet->row)continue;
            double x=next_row?board->edge:sheet->x;
            double y=next_row?sheet->y+sheet->row+board->gap:sheet->y;
            double top=y+(next_row?h:fmax(h,sheet->row)),right=x+w;
            if(right>board->width-board->edge+1e-8||top>board->height-board->edge+1e-8)continue;
            if(top<best_height-1e-8||(fabs(top-best_height)<1e-8&&right<best_right)){
                *best=(SmyStockPlace){0,0,turn,x,y,w,h};
                found=1;
                best_height=top;
                best_right=right;
            }
        }
    }
    return found;
}

static void smy_nest(const SmyStockList *list,const SmyStockBoard *board,SmyStockPlan *plan){
    int order[SMY_STOCK_PARTS];
    smy_stock_groups(list,plan);
    for(int i=0;i<list->count;i++){
        double area=list->parts[i].width*list->parts[i].height;
        int j=i;
        for(;j>0;j--){
            const SmyStockPart *prev=&list->parts[order[j-1]];
            if(prev->width*prev->height>=area)break;
            order[j]=order[j-1];
        }
        order[j]=i;
    }
    for(int i=0;i<list->count;i++){
        int type=order[i];
        const SmyStockPart *part=&list->parts[type];
        SmyStockSheet empty={plan->group[type],board->edge,smy_board_bottom(board),0,0,0};
        for(int j=0;j<part->quantity;j++){
            SmyStockPlace position;
            int sheet=0;
            for(;sheet<plan->sheet_count;sheet++){
                if(plan->sheets[sheet].group==empty.group&&smy_try_row(part,board,&plan->sheets[sheet],&position))break;
            }
            if(sheet==plan->sheet_count){
                if(!smy_try_row(part,board,&empty,&position)){
                    plan->unplaced[type]+=part->quantity-j;
                    break;
                }
                plan->sheets[plan->sheet_count++]=empty;
            }
            SmyStockSheet *chosen=&plan->sheets[sheet];
            if(position.y>chosen->y+1e-8)chosen->row=0;
            chosen->x=position.x+position.w+board->gap;
            chosen->y=position.y;
            chosen->row=fmax(chosen->row,position.h);
            chosen->net+=part->net;
            chosen->boxes+=position.w*position.h;
            position.part=type;
            position.sheet=sheet;
            plan->places[plan->placed++]=position;
        }
    }
}

static ProError smy_nest_audit(const SmyStockList *list,const SmyStockBoard *b,const SmyStockPlan *p,
    double *minimum_edge,double *minimum_gap){
    int quantities[SMY_STOCK_PARTS]={0};
    *minimum_edge=*minimum_gap=INFINITY;
    for(int i=0;i<p->placed;i++){
        const SmyStockPlace *a=&p->places[i];
        const SmyStockPart *part=&list->parts[a->part];
        double w,h;
        if(!smy_stock_orient(part,b,a->turn,&w,&h)||fabs(a->w-w)>1e-7||fabs(a->h-h)>1e-7||
           p->group[a->part]!=p->sheets[a->sheet].group)return PRO_TK_BAD_CONTEXT;
        double edge=fmin(fmin(a->x-b->edge,b->width-b->edge-a->x-w),
            fmin(a->y-smy_board_bottom(b),b->height-b->edge-a->y-h));
        *minimum_edge=fmin(*minimum_edge,edge);
        if(edge<-1e-7)return PRO_TK_BAD_CONTEXT;
        quantities[a->part]++;
        for(int j=0;j<i;j++){
            const SmyStockPlace *other=&p->places[j];
            if(other->sheet!=a->sheet)continue;
            double dx=fmax(a->x-other->x-other->w,other->x-a->x-a->w);
            double dy=fmax(a->y-other->y-other->h,other->y-a->y-a->h);
            if(dx< -1e-7&&dy< -1e-7)return PRO_TK_BAD_CONTEXT;
            double gap=hypot(fmax(0,dx),fmax(0,dy));
            *minimum_gap=fmin(*minimum_gap,gap);
            if(gap+1e-7<b->gap)return PRO_TK_BAD_CONTEXT;
        }
    }
    for(int i=0;i<list->count;i++)if(quantities[i]+p->unplaced[i]!=list->parts[i].quantity)return PRO_TK_BAD_CONTEXT;
    return PRO_TK_NO_ERROR;
}

static int smy_grid_capacity(double length,double item,double gap){
    double n=floor((length+gap+1e-8)/(item+gap));
    return (int)fmin(n,1000000000);
}

static void smy_stock_grid(const SmyStockList *list,const SmyStockBoard *b,PartText *out){
    double bw=b->width-2*b->edge,bh=b->height-b->edge-smy_board_bottom(b);
    for(int i=0;i<list->count;i++){
        const SmyStockPart *p=&list->parts[i];
        double best=0;
        for(int turn=0;turn<=1;turn++){
            double w,h;
            if(!smy_stock_orient(p,b,turn,&w,&h))continue;
            int columns=smy_grid_capacity(bw,w,b->gap),rows=smy_grid_capacity(bh,h,b->gap);
            double normal=(double)rows*columns;
            double offset=floor((bw+ b->gap-(w+b->gap)/2+1e-8)/(w+b->gap));
            double stagger=(rows-rows/2.0+fmod(rows,2)/2)*columns+(rows/2)*fmax(offset,0);
            part_text_add(out,L"%ls，%d°：正排 %.0f 件/张，半节距交错 %.0f 件/张。\n",p->name,turn*90,normal,stagger);
            best=fmax(best,fmax(normal,stagger));
        }
        if(best)part_text_add(out,L"%ls 最佳安全包络阵列 %.0f 件/张，按净面积利用率 %.4g%%。\n",
            p->name,best,100*best*p->net/(b->width*b->height));
        else part_text_add(out,L"%ls 的允许方向均无法放入指定板材。\n",p->name);
    }
}

static ProError smy_stock_output(const SmyStockList *list,const SmyStockBoard *board,const SmyStockPlan *plan,
    const wchar_t *leaf,PartText *out){
    DeliveryOutput work={0};
    FILE *places=NULL,*orders=NULL,*boards=NULL;
    ProError error=dv_stage_begin(leaf,&work);
    if(!error)error=fab_csv_open(work.stage,L"placements.csv",&places);
    if(!error)error=fab_csv_open(work.stage,L"order-reconciliation.csv",&orders);
    if(!error)error=fab_csv_open(work.stage,L"stock-summary.csv",&boards);
    if(!error){
        fwprintf(places,L"板序,组别,零件,左右,X_mm,Y_mm,旋转_deg,包络宽_mm,包络高_mm\n");
        for(int i=0;i<plan->placed;i++){
            const SmyStockPlace *p=&plan->places[i];
            const SmyStockPart *part=&list->parts[p->part];
            fwprintf(places,L"%d,%d,",p->sheet+1,plan->group[p->part]+1);
            if(!dv_csv_text(places,part->name)){error=PRO_TK_CANT_WRITE;break;}
            fwprintf(places,L",%lc,%.10g,%.10g,%d,%.10g,%.10g\n",part->hand,p->x,p->y,p->turn*90,p->w,p->h);
        }
        fwprintf(orders,L"零件,订单数,已排数,未排数,未排原因\n");
        for(int i=0;!error&&i<list->count;i++){
            if(!dv_csv_text(orders,list->parts[i].name)){error=PRO_TK_CANT_WRITE;break;}
            fwprintf(orders,L",%d,%d,%d,%ls\n",list->parts[i].quantity,list->parts[i].quantity-plan->unplaced[i],
                plan->unplaced[i],plan->unplaced[i]?L"允许方向的包络超过有效板材":L"");
        }
        fwprintf(boards,L"板序,组别,材料,板厚_mm,纹向,覆膜,板宽_mm,板长_mm,零件净面积_mm2,余料理论面积_mm2\n");
        for(int i=0;!error&&i<plan->sheet_count;i++){
            const SmyStockSheet *sheet=&plan->sheets[i];
            int part=0;
            while(part<list->count&&plan->group[part]!=sheet->group)part++;
            const SmyStockMaterial *stock=&list->parts[part].material;
            fwprintf(boards,L"%d,%d,",i+1,sheet->group+1);
            if(!dv_csv_text(boards,stock->grade)){error=PRO_TK_CANT_WRITE;break;}
            fwprintf(boards,L",%.10g,%ls,%ls,%.10g,%.10g,%.10g,%.10g\n",stock->thickness,stock->grain,stock->film,
                board->width,board->height,sheet->net,board->width*board->height-sheet->net);
        }
    }
    error=fab_close(places,error);
    error=fab_close(orders,error);
    error=fab_close(boards,error);
    error=dv_stage_finish(&work,error,out);
    if(!error)part_text_add(out,L"排样坐标、逐板汇总及订单核对：%ls\n",work.target);
    return error;
}
