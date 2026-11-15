typedef struct {
    ProMdl model;
    ProSolidBody body;
    double mm,thickness;
    FILE *file;
    int rows,failed;
} SmyFirstCard;

static int smy_card_read(wchar_t **f,void *context){
    SmyFirstCard *card=context;
    double nominal,lo,hi,actual;
    if(!dv_safe_leaf(f[0])||!dv_safe_leaf(f[2])||!part_number(f[3],&nominal)||
       !smx_number(f[4],0,1000)||!smx_number(f[5],0,1000))return 0;
    lo=nominal-wcstod(f[4],NULL);hi=nominal+wcstod(f[5],NULL);
    const wchar_t *unit=L"mm";
    ProError error=PRO_TK_NO_ERROR;
    if(!wcscmp(f[1],L"T"))actual=card->thickness;
    else if(!wcsncmp(f[1],L"BEND:",5)||!wcsncmp(f[1],L"BORE:",5)){
        int id;
        if(!part_integer(f[1]+5,&id)||id<=0)return 0;
        wchar_t token[40];
        swprintf_s(token,_countof(token),L"SURFACE:%d",id);
        if(f[1][1]==L'E'){
            ProSmtDvlLenCalcInfo calculation;
            double radius,length,thickness,mm;
            error=smx_bend_values(card->model,token,&calculation,&radius,&actual,&length,&thickness,&mm);
            if(!error)actual=fabs(actual);
            unit=L"deg";
        }else{
            FdRef ref={0};
            QualityBore hole;
            error=qa_bore(card->model,token,&ref,&hole);
            if(!error)actual=2*hole.radius*card->mm;
            fd_ref_free(&ref);
        }
    }else{
        if(!fd_name(f[1]))return 0;
        error=fab_measure(card->model,&card->body,card->mm,f[1],&actual,&unit);
    }
    if(error)return 0;
    int pass=actual>=lo-1e-7&&actual<=hi+1e-7;
    card->failed+=!pass;card->rows++;
    FILE *file=card->file;
    if(!dv_csv_text(file,f[0])||fputwc(L',',file)==WEOF||!dv_csv_text(file,f[1])||fputwc(L',',file)==WEOF||
       !dv_csv_text(file,f[2]))return 0;
    return fwprintf(file,L",%ls,%.10g,%.10g,%.10g,%.10g,%ls,,\n",unit,nominal,lo,hi,actual,pass?L"模型值符合":L"模型值超差")>=0;
}

static ProError smy_first_card(ProMdl model,const PartCommand *cmd,PartText *out){
    SmyFirstCard card={0};
    card.model=model;
    ProError error=qa_single_body(model,&card.body);
    if(!error)error=fab_thickness(model,&card.mm,&card.thickness);
    DeliveryOutput work={0};
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,L"first-article.csv",&card.file);
    if(!error&&fwprintf(card.file,L"项目,Creo测量参照,车间测量方法,单位,名义值,下限,上限,模型实测,模型判定,车间实测,人员签名\n")<0)
        error=PRO_TK_CANT_WRITE;
    wchar_t source[64];
    swprintf_s(source,_countof(source),L"%ls.tsv",cmd->arg[3]);
    if(!error)error=smx_table(source,6,smy_card_read,&card,out);
    error=dv_stage_finish(&work,fab_close(card.file,error),out);
    if(!error)part_text_add(out,L"已生成 %d 项首件检验记录，其中 %d 项模型尺寸超差。车间实测和签字列留空：%ls\\first-article.csv。\n",
        card.rows,card.failed,work.target);
    return error;
}

static ProError smy_stamp(ProMdl model,wchar_t stamp[256]){
    ProWVerstamp version=NULL;
    char *value=NULL;
    ProError error=ProMdlVerstampGet(model,&version);
    if(!error)error=ProVerstampStringGet(version,&value);
    if(!error&&(!value||swprintf_s(stamp,256,L"%hs",value)<0||wcspbrk(stamp,L"\t\r\n ")))error=PRO_TK_BAD_CONTEXT;
    if(value)ProVerstampStringFree(&value);
    if(version)ProVerstampFree(&version);
    return error;
}

static int smy_package_directory(const wchar_t *path){
    DWORD attrs=GetFileAttributesW(path);
    return attrs!=INVALID_FILE_ATTRIBUTES&&(attrs&FILE_ATTRIBUTE_DIRECTORY)&&!(attrs&FILE_ATTRIBUTE_REPARSE_POINT);
}

static ProError smy_copy_checked(const wchar_t *source,const wchar_t *destination){
    wchar_t from[65],to[65];
    ULONGLONG count,actual;
    if(!dv_sha256(source,from,&count))return PRO_TK_CANT_OPEN;
    if(!CopyFileW(source,destination,TRUE))return PRO_TK_CANT_WRITE;
    return dv_sha256(destination,to,&actual)&&count==actual&&!wcscmp(from,to)?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError smy_pdf_signature(const wchar_t *source){
    FILE *file=_wfopen(source,L"rb");
    if(!file)return PRO_TK_CANT_OPEN;
    char mark[5];
    int valid=fread(mark,1,sizeof(mark),file)==sizeof(mark)&&!memcmp(mark,"%PDF-",sizeof(mark));
    fclose(file);
    return valid?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static const wchar_t *const smy_package_type[]={L"PRT",L"STEP",L"DXF",L"PDF",L"BENDS",L"FIRST",L"FASTENERS"};
static const wchar_t *const smy_package_file[]={NULL,NULL,L"flat.dxf",L"drawing.pdf",L"bends.csv",L"first-article.csv",L"fasteners.csv"};

static ProError smy_package_bundle(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput work={0};
    ProPath source[4],leaves[7]={{0}};
    wchar_t stamp[256],after[256];
    ProError error=dv_no_external_dependencies(model);
    if(!error)error=smy_stamp(model,stamp);
    if(!error)error=ProDirectoryCurrentGet(work.parent);
    for(int i=0;!error&&i<4;i++){
        error=smy_stock_file(work.parent,cmd->arg[5+i],i?L".csv":L".pdf",source[i]);
        if(error)part_text_add(out,L"缺少或无法读取 %ls%ls。\n",cmd->arg[5+i],i?L".csv":L".pdf");
        if(!error&&!i)error=smy_pdf_signature(source[i]);
    }
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=dv_native_copy(model,&work,leaves[0]);
    if(!error)error=dv_export_file(model,work.stage,PRO_INTF_EXPORT_STEP,leaves[1]);
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d&&!error)error=PRO_TK_OUT_OF_MEMORY;
    ProPath target;
    if(!error)error=fab_flat_read(model,work.stage,L"flat",cmd->arg[4],d,target);
    free(d);
    for(int i=2;i<7;i++)wcscpy_s(leaves[i],_countof(leaves[i]),smy_package_file[i]);
    for(int i=0;!error&&i<4;i++){
        error=fab_leaf_path(work.stage,leaves[3+i],L"",target);
        if(!error)error=smy_copy_checked(source[i],target);
    }
    if(!error)error=smy_stamp(model,after);
    if(!error&&wcscmp(stamp,after))error=PRO_TK_BAD_CONTEXT;
    FILE *manifest=NULL;
    if(!error)error=fab_csv_open(work.stage,L"manifest.tsv",&manifest);
    if(!error)fwprintf(manifest,L"MODEL\t%ls\t%ls\t%ls\n",cmd->arg[1],cmd->arg[3],stamp);
    for(int i=0;!error&&i<7;i++){
        wchar_t digest[65];
        ULONGLONG bytes;
        error=fab_leaf_path(work.stage,leaves[i],L"",target);
        if(!error&&!dv_sha256(target,digest,&bytes))error=PRO_TK_CANT_OPEN;
        if(!error&&fwprintf(manifest,L"%ls\t%ls\t%llu\t%ls\n",smy_package_type[i],leaves[i],bytes,digest)<0)
            error=PRO_TK_CANT_WRITE;
    }
    error=dv_stage_finish(&work,fab_close(manifest,error),out);
    if(!error)part_text_add(out,L"已生成交付包 %ls，原生零件副本、STEP 和 DXF 已生成，输入的 PDF 与三份工艺文件已按字节和 SHA-256 核对。\n",
        work.target);
    return error;
}

static int smy_package_leaf(const wchar_t *leaf){
    if(!iswalnum(leaf[0])||wcslen(leaf)>159)return 0;
    for(const wchar_t *p=leaf;*p;p++)if(!iswalnum(*p)&&!wcschr(L"_.-",*p))return 0;
    return !wcsstr(leaf,L"..")&&leaf[wcslen(leaf)-1]!=L'.';
}

static ProError smy_package_check(ProMdl model,const PartCommand *cmd,PartText *out){
    ProPath directory,folder,manifest_path,path,flat;
    ProError error=ProDirectoryCurrentGet(directory);
    if(!error)error=fab_leaf_path(directory,cmd->arg[2],L"",folder);
    if(!error&&!smy_package_directory(folder))error=PRO_TK_CANT_OPEN;
    if(!error)error=fab_leaf_path(folder,L"manifest.tsv",L"",manifest_path);
    if(error)return error;
    if(!dv_file_nonempty(manifest_path))return PRO_TK_CANT_OPEN;
    FILE *manifest=_wfopen(manifest_path,L"rt, ccs=UTF-8");
    if(!manifest)return PRO_TK_CANT_OPEN;
    wchar_t stamp[256],line[512];
    error=smy_stamp(model,stamp);
    for(int i=-1;!error&&i<7;i++){
        if(!fgetws(line,_countof(line),manifest)||!wcschr(line,L'\n')){error=PRO_TK_CANT_OPEN;break;}
        wchar_t *save=NULL,*f[4];
        for(int j=0;j<4;j++)f[j]=wcstok_s(j?NULL:line,L"\t\r\n",&save);
        if(!f[3]||wcstok_s(NULL,L"\t\r\n",&save)){error=PRO_TK_BAD_INPUTS;break;}
        if(i<0){
            if(wcscmp(f[0],L"MODEL")||_wcsicmp(f[1],cmd->arg[1])||wcscmp(f[2],cmd->arg[3])||wcscmp(f[3],stamp)){
                part_text_add(out,L"模型名称、指定发布版本或原生模型状态已变化，关联文件需要重新核对。\n");
                error=PRO_TK_BAD_CONTEXT;
            }
            continue;
        }
        if(wcscmp(f[0],smy_package_type[i])||!smy_package_leaf(f[1])||(i>=2&&wcscmp(f[1],smy_package_file[i]))||
           wcslen(f[3])!=64||wcsspn(f[3],L"0123456789abcdef")!=64){error=PRO_TK_BAD_INPUTS;break;}
        wchar_t *end=NULL,digest[65];
        errno=0;
        ULONGLONG wanted=_wcstoui64(f[2],&end,10),actual;
        if(end==f[2]||*end||!wanted||errno){error=PRO_TK_BAD_INPUTS;break;}
        error=fab_leaf_path(folder,f[1],L"",path);
        if(!error&&(!dv_sha256(path,digest,&actual)||actual!=wanted||wcscmp(digest,f[3]))){
            part_text_add(out,L"%ls 缺失、被修改或校验不一致。\n",f[1]);
            error=PRO_TK_BAD_CONTEXT;
        }
        if(!error&&i==2)wcscpy_s(flat,_countof(flat),path);
    }
    if(!error&&(fgetws(line,_countof(line),manifest)||ferror(manifest)))error=PRO_TK_BAD_INPUTS;
    fclose(manifest);
    if(!error){
        PartCommand compare={0};
        wcscpy_s(compare.arg[3],PART_TOKEN_MAX,cmd->arg[4]);
        wcscpy_s(compare.arg[4],PART_TOKEN_MAX,L"0.0001");
        error=smy_flat_match(model,&compare,flat,out);
    }
    if(!error)part_text_add(out,L"七份交付文件的散列、原生模型状态与 DXF 展开一致。图纸及工艺文字的内容对应关系以签发记录为准。\n");
    return error;
}
