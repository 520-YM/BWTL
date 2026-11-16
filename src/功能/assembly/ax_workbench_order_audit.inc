/* Batch input is a fixed UTF-8 CSV schema with plain identifiers and numeric values. */
static int aw_order_fields(wchar_t *line,wchar_t *columns[8]){
    int count=0;
    wchar_t *p=line;
    wchar_t *end=p+wcscspn(p,L"\r\n");
    *end=0;
    while(count<8){
        columns[count++]=p;
        wchar_t *separator=wcschr(p,L',');
        if(!separator)break;
        if(count==8)return 0;
        *separator=0;
        p=separator+1;
    }
    return count==8&&!wcschr(columns[7],L',');
}

static ProError aw_order_line(ProMdl root,wchar_t *columns[8],PartText *out,int *pass){
    *pass=0;
    if(!fd_name(columns[0])||!fd_name(columns[1])||!qa_number(columns[2],0.001,1e6)||
       !qa_number(columns[3],0.001,1e6)||!qa_number(columns[4],0.001,1e6)||!qa_number(columns[5],0,1e6)||
       !aw_path(columns[6])||!aw_path(columns[7])||!wcscmp(columns[6],columns[7]))return PRO_TK_BAD_INPUTS;
    ProMdlName current;
    ProMdl model=NULL;
    int loaded=0;
    ProError error=ProMdlMdlnameGet(root,current);
    if(error)return error;
    if(!_wcsicmp(current,columns[1]))model=root;
    else if((error=ax_load_subassembly(columns[1],&model,&loaded)))return error;
    if((error=ax_master(model)))goto done;
    double mm;
    ProOutline bounds;
    const AxDeviceFrame frame={{0,0,0},{{1,0,0},{0,1,0},{0,0,1}}};
    if((error=qa_mm_per_unit(model,&mm))||(error=aw_part_bounds(model,L"ROOT",&frame,mm,bounds)))goto done;
    int limits=1;
    double size[3],minimum=wcstod(columns[5],NULL);
    for(int i=0;i<3;i++){
        size[i]=(bounds[1][i]-bounds[0][i])*mm;
        if(!ax_q_within(size[i],wcstod(columns[2+i],NULL)))limits=0;
    }

    AxComponent a={0},b={0};
    ProSelection selections[2]={0};
    error=ax_hw_occurrence(model,columns[6],mm,&a);
    if(!error)error=ax_hw_occurrence(model,columns[7],mm,&b);
    if(!error)error=ax_part_selection(&a,&selections[0]);
    if(!error)error=ax_part_selection(&b,&selections[1]);
    int interfering=0;
    double clearance=0;
    if(!error)error=ax_motion_sample_fit(selections[0],selections[1],mm,&clearance,&interfering);
    for(int i=0;i<2;i++)if(selections[i])ProSelectionFree(&selections[i]);
    if(error)goto done;
    *pass=limits&&!interfering&&qa_at_least(clearance,minimum);
    part_text_add(out,L"%ls 的 %ls：外廓 %.8g×%.8g×%.8g mm，%ls；指定实例最小净空 %.8g mm（要求 %.8g），%ls。\n",
        columns[0],columns[1],size[0],size[1],size[2],limits?L"外廓符合":L"外廓超限",clearance,minimum,
        interfering?L"有干涉":qa_at_least(clearance,minimum)?L"净空符合":L"净空不足");
done:
    if(loaded&&model){ProError erase=ProMdlErase(model);if(!error)error=erase;}
    return error;
}

static ProError aw_order_audit(ProMdl root,const PartCommand *cmd,PartText *out){
    ProPath path,directory;
    WIN32_FILE_ATTRIBUTE_DATA info;
    ProError error=ProDirectoryCurrentGet(directory);
    if(error)return error;
    if(!aw_csv_leaf(cmd->arg[2])||swprintf_s(path,_countof(path),L"%ls\\%ls",directory,cmd->arg[2])<0)return PRO_TK_BAD_INPUTS;
    if(!GetFileAttributesExW(path,GetFileExInfoStandard,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       info.nFileSizeHigh||!info.nFileSizeLow||info.nFileSizeLow>128000)return PRO_TK_CANT_OPEN;
    FILE *file=_wfopen(path,L"rt, ccs=UTF-8");
    if(!file)return PRO_TK_CANT_OPEN;
    wchar_t line[1024];
    int rows=0,verified=0,failed=0,unknown=0;
    if(!fgetws(line,_countof(line),file)||
       wcscmp(line,L"ORDER,ASSEMBLY,MAX_X,MAX_Y,MAX_Z,MIN_GAP,COMPONENT_A,COMPONENT_B\n")){
        part_text_add(out,L"订单表表头应为 ORDER,ASSEMBLY,MAX_X,MAX_Y,MAX_Z,MIN_GAP,COMPONENT_A,COMPONENT_B。\n");
        error=PRO_TK_BAD_INPUTS;
        goto done;
    }
    while(fgetws(line,_countof(line),file)){
        if(++rows>80){error=PRO_TK_INCOMPLETE;break;}
        wchar_t *columns[8];
        if(!wcschr(line,L'\n')&&!feof(file)){
            error=PRO_TK_BAD_INPUTS;break;
        }
        int pass=0;
        ProError result=aw_order_fields(line,columns)?aw_order_line(root,columns,out,&pass):PRO_TK_BAD_INPUTS;
        if(result){unknown++;part_text_add(out,L"订单表第 %d 行无法核实（Creo %d）。\n",rows+1,result);}
        else{verified++;failed+=!pass;}
    }
    if(ferror(file))error=PRO_TK_CANT_OPEN;
    if(!rows&&!error)error=PRO_TK_E_NOT_FOUND;
    part_text_add(out,L"订单表核对通过 %d 行，有超差 %d 行，无法核实 %d 行。限给定装配范围和逐行间隙对。\n",verified-failed,failed,unknown);
done:
    fclose(file);
    return error?error:unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
