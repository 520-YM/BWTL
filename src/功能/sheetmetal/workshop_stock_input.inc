#define SMY_STOCK_PARTS 24
#define SMY_STOCK_ITEMS 256

typedef struct {
    wchar_t grade[32],grain[8],film[8];
    double thickness;
} SmyStockMaterial;

typedef struct {
    wchar_t name[PRO_NAME_SIZE],drawing[PRO_NAME_SIZE];
    SmyStockMaterial material;
    wchar_t hand;
    int quantity,rotate,rectangle;
    double width,height,net;
} SmyStockPart;

typedef struct {
    ProPath directory;
    SmyStockPart parts[SMY_STOCK_PARTS];
    int count,quantity;
} SmyStockList;

static int smy_same_stock(const SmyStockMaterial *a,const SmyStockMaterial *b){
    return fabs(a->thickness-b->thickness)<1e-7&&!_wcsicmp(a->grade,b->grade)&&
        !wcscmp(a->grain,b->grain)&&!wcscmp(a->film,b->film);
}

static double smy_integral(const FabEntity *edge){
    if(edge->type==1)return (edge->a[0]*edge->b[1]-edge->a[1]*edge->b[0])/2;
    if(edge->type==3)return AS_PI*edge->radius*edge->radius;
    double d=edge->sweep*AS_PI/180;
    return (edge->radius*edge->radius*d+edge->center[0]*(edge->b[1]-edge->a[1])-
        edge->center[1]*(edge->b[0]-edge->a[0]))/2;
}

static ProError smy_cut_area(const FabDxf *d,double *net,int *rectangle){
    unsigned char done[FAB_DXF_MAX]={0};
    double area[2]={0};
    int loops[2]={0},straight=1;
    for(int i=0;i<d->count;i++){
        const FabEntity *first=&d->entities[i];
        if(first->layer==3||done[i])continue;
        int layer=first->layer-1;
        if(layer<0||layer>1)return PRO_TK_BAD_CONTEXT;
        loops[layer]++;
        done[i]=1;
        double integral=smy_integral(first);
        if(first->type!=3){
            const double *head=first->a,*tail=first->b;
            for(int steps=1;!fab_close_xy(head,tail,FAB_JOIN);steps++){
                if(steps>=d->count)return PRO_TK_BAD_CONTEXT;
                int next=-1,forward=0;
                for(int j=0;j<d->count;j++){
                    const FabEntity *edge=&d->entities[j];
                    if(done[j]||edge->type==3||edge->layer!=first->layer)continue;
                    if(fab_close_xy(tail,edge->a,FAB_JOIN)){next=j;forward=1;break;}
                    if(fab_close_xy(tail,edge->b,FAB_JOIN)){next=j;break;}
                }
                if(next<0)return PRO_TK_BAD_CONTEXT;
                const FabEntity *edge=&d->entities[next];
                integral+=(forward?1:-1)*smy_integral(edge);
                tail=forward?edge->b:edge->a;
                done[next]=1;
            }
        }
        if(fabs(integral)<=FAB_JOIN*FAB_JOIN)return PRO_TK_BAD_CONTEXT;
        area[layer]+=fabs(integral);
    }
    if(loops[0]!=1||area[0]<=area[1])return PRO_TK_BAD_CONTEXT;
    *net=area[0]-area[1];
    for(int i=0;i<d->count;i++){
        const FabEntity *edge=&d->entities[i];
        if(edge->layer!=1)continue;
        if(edge->type!=1||(fabs(edge->a[0]-edge->b[0])>FAB_JOIN&&fabs(edge->a[1]-edge->b[1])>FAB_JOIN))straight=0;
    }
    double box=(d->high[0]-d->low[0])*(d->high[1]-d->low[1]);
    *rectangle=straight&&d->outline==4&&!d->holes&&fabs(area[0]-box)<=1e-7*fmax(box,1);
    return PRO_TK_NO_ERROR;
}

static ProError smy_stock_file(const ProPath directory,const wchar_t *base,const wchar_t *ext,ProPath path){
    if(!dv_safe_leaf(base))return PRO_TK_BAD_INPUTS;
    ProError error=fab_leaf_path(directory,base,ext,path);
    if(!error&&!dv_file_nonempty(path))error=PRO_TK_CANT_OPEN;
    return error;
}

static int smy_material_fields(wchar_t **f,SmyStockMaterial *material){
    if(!smx_word(f[0])||!smx_number(f[1],0.01,100)||
       (wcscmp(f[2],L"FREE")&&wcscmp(f[2],L"X")&&wcscmp(f[2],L"Y"))||
       (wcscmp(f[3],L"UP")&&wcscmp(f[3],L"DOWN")&&wcscmp(f[3],L"NONE")))return 0;
    wcscpy_s(material->grade,_countof(material->grade),f[0]);
    wcscpy_s(material->grain,_countof(material->grain),f[2]);
    wcscpy_s(material->film,_countof(material->film),f[3]);
    part_number(f[1],&material->thickness);
    return 1;
}

static ProError smy_stock_list(const wchar_t *base,int remnant,SmyStockList *list,PartText *out){
    ProPath path,dxf_path;
    ProError error=ProDirectoryCurrentGet(list->directory);
    if(!error)error=smy_stock_file(list->directory,base,L".tsv",path);
    if(error)return error;
    FILE *file=_wfopen(path,L"rt, ccs=UTF-8");
    if(!file)return PRO_TK_CANT_OPEN;
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d){fclose(file);return PRO_TK_OUT_OF_MEMORY;}
    wchar_t line[512];
    int lineno=0,columns=remnant?6:9;
    while(!error&&fgetws(line,_countof(line),file)){
        lineno++;
        if(!wcschr(line,L'\n')&&!feof(file)){error=PRO_TK_LINE_TOO_LONG;break;}
        wchar_t *p=line+wcsspn(line,L" \t\r\n");
        if(!*p||*p==L'#')continue;
        if(list->count==_countof(list->parts)){error=PRO_TK_OUT_OF_RANGE;break;}
        wchar_t *fields[9],*cursor=NULL;
        int count=0;
        for(wchar_t *token=wcstok_s(p,L" \t\r\n",&cursor);token;token=wcstok_s(NULL,L" \t\r\n",&cursor)){
            if(count==columns){count++;break;}
            fields[count++]=token;
        }
        SmyStockPart *part=&list->parts[list->count];
        if(count!=columns||!dv_safe_leaf(fields[0])||!dv_safe_leaf(fields[1])||
           wcslen(fields[0])>=_countof(part->name)||wcslen(fields[1])>=_countof(part->drawing)||
           !smy_material_fields(fields+2,&part->material)){error=PRO_TK_BAD_INPUTS;break;}
        part->quantity=1;
        part->hand=L'N';
        if(!remnant){
            if(!part_integer(fields[6],&part->quantity)||part->quantity<1||
               part->quantity>SMY_STOCK_ITEMS-list->quantity||wcslen(fields[7])!=1||!wcschr(L"LRN",fields[7][0])||
               (wcscmp(fields[8],L"LOCK")&&wcscmp(fields[8],L"TURN"))){
                error=PRO_TK_BAD_INPUTS;break;
            }
            part->hand=fields[7][0];
            part->rotate=!wcscmp(fields[8],L"TURN")&&!wcscmp(part->material.grain,L"FREE");
        }
        for(int i=0;i<list->count;i++)if(!_wcsicmp(list->parts[i].name,fields[0]))error=PRO_TK_E_FOUND;
        if(error)break;
        wcscpy_s(part->name,_countof(part->name),fields[0]);
        wcscpy_s(part->drawing,_countof(part->drawing),fields[1]);
        error=smy_stock_file(list->directory,part->drawing,L".dxf",dxf_path);
        if(!error)error=fab_dxf_read(dxf_path,d);
        if(!error)error=smy_cut_area(d,&part->net,&part->rectangle);
        if(error)break;
        part->width=d->high[0]-d->low[0];
        part->height=d->high[1]-d->low[1];
        list->count++;
        list->quantity+=part->quantity;
    }
    if(!error&&(ferror(file)||!list->count))error=PRO_TK_BAD_INPUTS;
    if(error)part_text_add(out,L"清单 %ls.tsv 第 %d 行或对应 DXF 未通过校验（Creo %d）。\n",base,lineno,error);
    fclose(file);
    free(d);
    return error;
}

static ProError smy_stock_measure(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput scratch={0};
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d)return PRO_TK_OUT_OF_MEMORY;
    double mm,thickness,net=0;
    int rectangle;
    ProPath path;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=fab_scratch(&scratch);
    if(!error)error=dv_stage_enter(&scratch);
    if(!error)error=fab_flat_read(model,scratch.stage,L"stock_measure",cmd->arg[2],d,path);
    if(!error)error=smy_cut_area(d,&net,&rectangle);
    if(!error){
        double box=(d->high[0]-d->low[0])*(d->high[1]-d->low[1]);
        part_text_add(out,L"原生展开净料 %.8g mm²、毛坯包络 %.8g mm²、板厚 %.8g mm；按输入密度 %.8g kg/m³，单件估重 %.8g kg。\n",
            net,box,thickness,as_arg(cmd,3),net*thickness*as_arg(cmd,3)*1e-9);
    }
    free(d);
    return fab_discard(&scratch,error,out);
}
