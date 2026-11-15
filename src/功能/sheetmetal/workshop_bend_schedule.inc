#define SMY_BEND_LIMIT 32

typedef struct {
    FabBends bends;
    unsigned long predecessors[SMY_BEND_LIMIT];
    wchar_t tool[SMY_BEND_LIMIT][32],gauge[SMY_BEND_LIMIT][32],inspection[SMY_BEND_LIMIT][32];
    int first[SMY_BEND_LIMIT],second[SMY_BEND_LIMIT];
} SmyBendOrder;

static int smy_bend_row(const SmyBendOrder *plan,const wchar_t *reference){
    int surface;
    if(wcsncmp(reference,L"SURFACE:",8)||!part_integer(reference+8,&surface))return -1;
    for(int i=0;i<plan->bends.count;i++)if(plan->bends.rows[i].surface==surface)return i;
    return -1;
}

static int smy_bend_sort(const SmyBendOrder *plan,int group_tools,int order[]){
    unsigned long chosen=0;
    int prior=-1;
    for(int position=0;position<plan->bends.count;position++){
        int best=-1;
        for(int i=0;i<plan->bends.count;i++){
            unsigned long bit=1UL<<i;
            if((chosen&bit)||(plan->predecessors[i]&~chosen))continue;
            if(best<0){best=i;continue;}
            int same=group_tools&&prior>=0&&!wcscmp(plan->tool[i],plan->tool[prior]);
            int best_same=group_tools&&prior>=0&&!wcscmp(plan->tool[best],plan->tool[prior]);
            if(same>best_same||(same==best_same&&
               (group_tools?plan->bends.rows[i].length<plan->bends.rows[best].length:plan->bends.rows[i].length>plan->bends.rows[best].length)))best=i;
        }
        if(best<0)return 0;
        order[position]=best;
        chosen|=1UL<<best;
        prior=best;
    }
    return 1;
}

static ProError smy_bend_table(const wchar_t *name,SmyBendOrder *plan,PartText *out){
    ProPath directory,path;
    ProError error=ProDirectoryCurrentGet(directory);
    if(!error)error=smy_stock_file(directory,name,L".tsv",path);
    if(error)return error;
    FILE *file=_wfopen(path,L"rt, ccs=UTF-8");
    if(!file)return PRO_TK_CANT_OPEN;
    wchar_t line[512];
    int row=0;
    while(!error&&fgetws(line,_countof(line),file)){
        if(++row>2048){error=PRO_TK_OUT_OF_RANGE;break;}
        if(!wcschr(line,L'\n')&&!feof(file)){error=PRO_TK_LINE_TOO_LONG;break;}
        wchar_t *p=line+wcsspn(line,L" \t\r\n"),*cursor=NULL,*f[4];
        if(!*p||*p==L'#')continue;
        int n=0;
        for(wchar_t *t=wcstok_s(p,L" \t\r\n",&cursor);t;t=wcstok_s(NULL,L" \t\r\n",&cursor)){
            if(n==_countof(f)){n++;break;}
            f[n++]=t;
        }
        if(n==3&&!wcscmp(f[0],L"BEFORE")){
            int first=smy_bend_row(plan,f[1]),second=smy_bend_row(plan,f[2]);
            if(first<0||second<0||first==second){error=PRO_TK_BAD_INPUTS;break;}
            plan->predecessors[second]|=1UL<<first;
        }else if(n==4){
            int i=smy_bend_row(plan,f[0]);
            if(i<0||plan->tool[i][0]||!dv_safe_leaf(f[1])||!dv_safe_leaf(f[2])||!dv_safe_leaf(f[3])){
                error=PRO_TK_BAD_INPUTS;break;
            }
            wcscpy_s(plan->tool[i],_countof(plan->tool[i]),f[1]);
            wcscpy_s(plan->gauge[i],_countof(plan->gauge[i]),f[2]);
            wcscpy_s(plan->inspection[i],_countof(plan->inspection[i]),f[3]);
        }else error=PRO_TK_BAD_INPUTS;
    }
    if(!error&&ferror(file))error=PRO_TK_CANT_OPEN;
    fclose(file);
    if(error){part_text_add(out,L"折弯工艺表 %ls.tsv 第 %d 行无效。\n",name,row);return error;}
    for(int i=0;i<plan->bends.count;i++)if(!plan->tool[i][0]){
        part_text_add(out,L"工艺表缺少折弯面 %d 的设备、挡料基准或检验项。\n",plan->bends.rows[i].surface);
        return PRO_TK_E_NOT_FOUND;
    }
    if(!smy_bend_sort(plan,0,plan->first)||!smy_bend_sort(plan,1,plan->second)){
        part_text_add(out,L"先后要求互相矛盾，找不到可执行的顺序。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}

static ProError smy_bend_segments(const SmyBendOrder *plan,const PartCommand *cmd,PartText *out){
    int i=smy_bend_row(plan,cmd->arg[2]);
    if(i<0)return PRO_TK_E_NOT_FOUND;
    double length=plan->bends.rows[i].length,last=0;
    int count=0;
    for(int at=3;at<cmd->count;at+=2){
        double start=as_arg(cmd,at),finish=as_arg(cmd,at+1);
        if(start<last-1e-7||finish<=start||finish>length+1e-7)return PRO_TK_BAD_INPUTS;
        if(start>last+1e-7){
            count++;
            part_text_add(out,L"沿弯线 %.8g–%.8g mm 为无模具覆盖区。\n",last,start);
        }else if(at>3)part_text_add(out,L"沿弯线 %.8g mm 为接刀位置，外观面接痕待复核。\n",start);
        last=finish;
    }
    if(last<length-1e-7){count++;part_text_add(out,L"沿弯线 %.8g–%.8g mm 为无模具覆盖区。\n",last,length);}
    part_text_add(out,L"原生折弯长度 %.8g mm，%d 处空档。\n",length,count);
    return PRO_TK_NO_ERROR;
}

static void smy_bend_candidate(const SmyBendOrder *plan,const int *order,const wchar_t *label,PartText *out){
    int changes=0,signs=0;
    part_text_add(out,L"%ls：",label);
    for(int i=0;i<plan->bends.count;i++){
        int index=order[i];
        if(i){
            int prior=order[i-1];
            changes+=wcscmp(plan->tool[index],plan->tool[prior])!=0;
            signs+=plan->bends.rows[index].angle*plan->bends.rows[prior].angle<0;
        }
        part_text_add(out,L"%lsSURFACE:%d",i?L" → ":L"",plan->bends.rows[index].surface);
    }
    part_text_add(out,L"。换模 %d 次，连续反向折弯 %d 处。需要用实际模具检查中间状态与空间。\n",changes,signs);
}

static ProError smy_bend_output(ProMdl model,const PartCommand *cmd,int kind,const SmyBendOrder *plan,PartText *out){
    DeliveryOutput work={0};
    FILE *file=NULL;
    const wchar_t *filename=kind==SMY_BEND_NUMBER?L"bend-identities.csv":kind==SMY_BEND_ORDER?L"bend-dependencies.csv":L"bend-operation-card.csv";
    ProError error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,filename,&file);
    if(!error&&kind==SMY_BEND_ORDER){
        fwprintf(file,L"前序弯面,后序弯面\n");
        for(int i=0;i<plan->bends.count;i++)for(int j=0;j<plan->bends.count;j++)
            if(plan->predecessors[i]&(1UL<<j))
                fwprintf(file,L"SURFACE:%d,SURFACE:%d\n",plan->bends.rows[j].surface,plan->bends.rows[i].surface);
    }
    if(!error&&kind!=SMY_BEND_ORDER){
        fwprintf(file,L"工序,折弯号,特征ID,名称,弯面,有符号角度_deg,内R_mm,长度_mm,展开弧长_mm,板厚_mm,模具,挡料基准,首件检查,AX_mm,AY_mm,AZ_mm,BX_mm,BY_mm,BZ_mm\n");
        const int *order=kind==SMY_BEND_CARD&&!wcscmp(cmd->arg[4],L"TOOL")?plan->second:plan->first;
        for(int i=0;!error&&i<plan->bends.count;i++){
            int index=kind==SMY_BEND_NUMBER?i:order[i];
            const FabBend *row=&plan->bends.rows[index];
            ProSurface face;
            ProVector axis,a,b;
            error=smy_bend_span(model,row->surface,&face,axis,a,b);
            if(error)break;
            double mm=plan->bends.mm;
            fwprintf(file,L"%d,B%03d,%d,",i+1,index+1,row->id);
            if(!dv_csv_text(file,row->name)){error=PRO_TK_CANT_WRITE;break;}
            fwprintf(file,L",%d,%.10g,%.10g,%.10g,%.10g,%.10g,",row->surface,row->angle,row->radius,row->length,row->developed,plan->bends.thickness);
            if(!dv_csv_text(file,plan->tool[index])||fputwc(L',',file)==WEOF||
               !dv_csv_text(file,plan->gauge[index])||fputwc(L',',file)==WEOF||
               !dv_csv_text(file,plan->inspection[index])){error=PRO_TK_CANT_WRITE;break;}
            if(fwprintf(file,L",%.10g,%.10g,%.10g,%.10g,%.10g,%.10g\n",a[0]*mm,a[1]*mm,a[2]*mm,b[0]*mm,b[1]*mm,b[2]*mm)<0)
                error=PRO_TK_CANT_WRITE;
        }
    }
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"已按 %d 道原生折弯写出 %ls\\%ls。\n",plan->bends.count,work.target,filename);
    return error;
}

static ProError smy_bend_execute(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    SmyBendOrder plan={0};
    ProError error=smy_bends_load(model,&plan.bends);
    if(error)return error;
    if(!plan.bends.count)return PRO_TK_E_NOT_FOUND;
    if(plan.bends.count>SMY_BEND_LIMIT)return PRO_TK_OUT_OF_RANGE;
    if(kind==SMY_BEND_SEGMENTS)return smy_bend_segments(&plan,cmd,out);
    if(kind!=SMY_BEND_NUMBER){
        error=smy_bend_table(cmd->arg[kind==SMY_BEND_CANDIDATES?2:3],&plan,out);
        if(error)return error;
    }
    if(kind==SMY_BEND_CANDIDATES){
        smy_bend_candidate(&plan,plan.first,L"优先长折弯",out);
        smy_bend_candidate(&plan,plan.second,L"优先同模具",out);
        return PRO_TK_NO_ERROR;
    }
    return smy_bend_output(model,cmd,kind,&plan,out);
}
