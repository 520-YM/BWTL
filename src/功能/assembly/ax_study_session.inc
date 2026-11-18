
#include <ProNotify.h>

#define AX_STUDY_EVENTS 256
#define AX_STUDY_BATCH 64

typedef struct {
    int id,edit,count,types[AX_STUDY_ROWS],flags[AX_STUDY_ROWS],sides[AX_STUDY_ROWS][2];
    int refs[AX_STUDY_ROWS][2];
    double values[AX_STUDY_ROWS];
} AxStudyEvent;
static struct {
    ProMdl model;
    int count,overflow;
    AxStudyEvent events[AX_STUDY_EVENTS];
} ax_study_demo;

static ProError ax_study_event(ProFeature *feature,int edit){
    if(!ax_study_demo.model||feature->owner!=ax_study_demo.model)return PRO_TK_NO_ERROR;
    ProFeattype type;
    if(ProFeatureTypeGet(feature,&type)||type!=PRO_FEAT_COMPONENT)return PRO_TK_NO_ERROR;
    if(ax_study_demo.count==AX_STUDY_EVENTS){ax_study_demo.overflow=1;return PRO_TK_NO_ERROR;}
    AxStudyEvent *event=&ax_study_demo.events[ax_study_demo.count++];
    memset(event,0,sizeof(*event));
    event->id=feature->id;
    event->edit=edit;
    ProAsmcompconstraint *rows=NULL;
    int count=0;
    if(!ax_read_constraints(feature,NULL,&rows,&count)&&count<=AX_STUDY_ROWS){
        event->count=count;
        for(int i=0;i<count;i++){
            ProAsmcompConstrType type;
            ProSelection refs[2]={NULL,NULL};
            ProDatumside sides[2]={0,0};
            if(!ProAsmcompconstraintTypeGet(rows[i],&type)){
                event->types[i]=type;
                if(ax_offset_type(type))ProAsmcompconstraintOffsetGet(rows[i],&event->values[i]);
            }
            ProAsmcompconstraintAttributesGet(rows[i],&event->flags[i]);
            ProAsmcompconstraintAsmreferenceGet(rows[i],&refs[0],&sides[0]);
            ProAsmcompconstraintCompreferenceGet(rows[i],&refs[1],&sides[1]);
            for(int side=0;side<2;side++){
                ProModelitem item;
                if(refs[side]&&!ProSelectionModelitemGet(refs[side],&item))event->refs[i][side]=item.id;
                event->sides[i][side]=sides[side];
            }
        }
    }
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_created(ProFeature *feature){return ax_study_event(feature,0);}
static ProError ax_study_edited(ProFeature *feature){return ax_study_event(feature,1);}

static ProError ax_study_demo_start(ProMdl assembly,PartText *out){
    if(ax_study_demo.model)return PRO_TK_E_IN_USE;
    ProMdlType type;
    if(!assembly||ProMdlTypeGet(assembly,&type)||type!=PRO_MDL_ASSEMBLY)return PRO_TK_BAD_CONTEXT;
    ProError error=ProNotificationSet(PRO_FEATURE_CREATE_POST,(ProFunction)ax_study_created);
    if(error)return error;
    error=ProNotificationSet(PRO_FEATURE_REDEFINE_POST,(ProFunction)ax_study_edited);
    if(error){ProNotificationUnset(PRO_FEATURE_CREATE_POST);return error;}
    memset(&ax_study_demo,0,sizeof(ax_study_demo));
    ax_study_demo.model=assembly;
    part_text_add(out,L"已开始记录当前总装的组件创建和重新定向。亲自装一次，完成后发送「结束装配示范」。\n");
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_demo_end(ProMdl assembly,PartText *out){
    if(!assembly||assembly!=ax_study_demo.model)return PRO_TK_BAD_CONTEXT;
    ProMdlName name=L"";
    ProError error=ProMdlMdlnameGet(assembly,name);
    double mm=0;
    if(!error)error=qa_mm_per_unit(assembly,&mm);
    wchar_t folder[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    SYSTEMTIME now;
    GetLocalTime(&now);
    FILE *file=NULL;
    if(!error&&ax_study_key(name)&&ax_learning_directory(folder,1)&&
       swprintf_s(path,MAX_PATH,L"%ls\\%ls-demonstration-%04u%02u%02u-%02u%02u%02u-%03u-%lx.txt",
         folder,name,now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,GetCurrentProcessId())>=0)
        file=ax_study_atomic(path,temp);
    if(!file)return error?error:PRO_TK_CANT_WRITE;
    fwprintf(file,L"BWTL-ASSEMBLY-DEMONSTRATION-1 %ls\n",name);
    for(int i=0;i<ax_study_demo.count;i++){
        const AxStudyEvent *event=&ax_study_demo.events[i];
        fwprintf(file,L"STEP %d %ls ID:%d %d\n",i+1,event->edit?L"REDEFINE":L"CREATE",event->id,event->count);
        for(int j=0;j<event->count;j++)
            fwprintf(file,L"RULE %d %d BASE %d SIDE %d CHILD %d SIDE %d OFFSET_MM %.12g\n",
                event->types[j],event->flags[j],event->refs[j][0],event->sides[j][0],
                event->refs[j][1],event->sides[j][1],event->values[j]*mm);
    }
    fwprintf(file,L"END %d TRUNCATED %d\n",ax_study_demo.count,ax_study_demo.overflow);
    if(!ax_study_commit(file,temp,path))return PRO_TK_CANT_WRITE;
    int count=ax_study_demo.count,overflow=ax_study_demo.overflow;
    ax_study_demo.model=NULL;
    ProNotificationUnset(PRO_FEATURE_CREATE_POST);
    ProNotificationUnset(PRO_FEATURE_REDEFINE_POST);
    part_text_add(out,L"已保存 %d 个成功的组件操作事件及当时可以读取的约束。%ls\n%ls\n",
        count,overflow?L"记录达到上限。":L"",path);
    return ax_learning_save(assembly,out);
}

static int ax_study_layout_file(const wchar_t *source,wchar_t path[MAX_PATH]){
    wchar_t directory[MAX_PATH];
    return ax_study_key(source)&&ax_learning_directory(directory,0)&&
       swprintf_s(path,MAX_PATH,L"%ls\\%ls.layout.tsv",directory,source)>=0;
}

static int ax_study_layout_line(wchar_t *line,wchar_t **column,int columns){
    line[wcscspn(line,L"\r\n")]=0;
    for(int i=0;i<columns;i++){
        column[i]=line;
        if(i==columns-1)return *line&&wcschr(line,L'\t')==NULL;
        wchar_t *tab=wcschr(line,L'\t');
        if(!tab||tab==line)return 0;
        *tab=0;
        line=tab+1;
    }
    return 0;
}

static int ax_study_case_id(const wchar_t *source,const wchar_t *key,wchar_t id[AX_STUDY_PATH],PartText *out){
    ProIdTable ids;
    if(ax_path_parse(key,ids)){wcscpy_s(id,AX_STUDY_PATH,key);return 1;}
    wchar_t file[MAX_PATH],line[512],header[120];
    FILE *map=ax_study_layout_file(source,file)?ax_learning_open(file):NULL;
    if(!map)return 0;
    swprintf_s(header,_countof(header),L"BWTL-ASSEMBLY-LAYOUT-1 %ls",source);
    if(!fgetws(line,_countof(line),map)){fclose(map);return 0;}
    line[wcscspn(line,L"\r\n")]=0;
    if(_wcsicmp(line[0]==0xfeff?line+1:line,header)){fclose(map);return 0;}
    int found=0,pending=0;
    while(fgetws(line,_countof(line),map)){
        wchar_t *col[4];
        if(!ax_study_layout_line(line,col,4)||!wcscmp(col[0],L"PDF"))continue;
        const int matches=!_wcsicmp(key,col[0])||!_wcsicmp(key,col[1])||!_wcsicmp(key,col[2]);
        if(ax_path_parse(col[3],ids)){
            if(matches){wcscpy_s(id,AX_STUDY_PATH,col[3]);found++;}
            if(out&&!*key)part_text_add(out,L"%ls %ls · %ls · %ls\n",col[0],col[1],col[2],col[3]);
        }else{
            if(matches)pending=1;
            if(out&&!*key)part_text_add(out,L"%ls %ls · 待定位 Creo 实例\n",col[0],col[1]);
        }
    }
    fclose(map);
    if(out&&!*key)part_text_add(out,L"本地 PDF 布局对照已核实的实例使用原总装路径。\n%ls\n",file);
    else if(out&&(found>1||pending))part_text_add(out,L"该编号或用途存在多个位置，或只有图纸标注。请使用具体的上、下、左右编号。\n");
    return *key?found==1&&!pending:1;
}


#include <wincrypt.h>

static int ax_study_pdf_digest(const wchar_t *path,wchar_t key[65]){
    HANDLE input=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(input==INVALID_HANDLE_VALUE)return 0;
    BY_HANDLE_FILE_INFORMATION info;
    HCRYPTPROV provider=0;
    HCRYPTHASH hash=0;
    BYTE digest[32],block[4096];
    DWORD size=0,length=sizeof(digest);
    int ok=GetFileInformationByHandle(input,&info)&&
        !(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))&&
        !info.nFileSizeHigh&&info.nFileSizeLow<=200*1024*1024&&
        CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
        CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
    int header=0;
    while(ok){
        ok=ReadFile(input,block,sizeof(block),&size,NULL);
        if(!ok||!size)break;
        if(!header){
            ok=size>=5&&!memcmp(block,"%PDF-",5);
            header=1;
        }
        if(ok)ok=CryptHashData(hash,block,size,0);
    }
    ok=ok&&header&&CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0)&&length==sizeof(digest);
    if(ok)for(DWORD i=0;i<length;i++)swprintf_s(key+i*2,65-i*2,L"%02X",digest[i]);
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(input);
    return ok;
}

static ProError ax_study_import_layout(const wchar_t *source,const wchar_t *pdf,const wchar_t *table,PartText *out){
    wchar_t path[MAX_PATH],temp[MAX_PATH],digest[65]=L"",line[512];
    if(!ax_study_layout_file(source,path)||!ax_study_pdf_digest(pdf,digest))return PRO_TK_BAD_INPUTS;
    FILE *in=ax_learning_open(table);
    if(!in)return PRO_TK_E_NOT_FOUND;
    FILE *file=ax_study_atomic(path,temp);
    if(!file){fclose(in);return PRO_TK_CANT_WRITE;}
    fwprintf(file,L"BWTL-ASSEMBLY-LAYOUT-1 %ls\nPDF\t校验通过\t%ls\t图纸留在本机\n",source,digest);
    int matched=0,pending=0,error=0;
    while(fgetws(line,_countof(line),in)){
        wchar_t *start=line[0]==0xfeff?line+1:line,*col[4];
        if(!*start||*start==L'#'||*start==L'\r'||*start==L'\n')continue;
        if(!ax_study_layout_line(start,col,4)){error=1;break;}
        ProIdTable ids;
        AxStudyRule rule={0};
        int good=ax_path_parse(col[3],ids)&&!ax_study_read(source,col[3],&rule)&&!_wcsicmp(rule.name,col[2]);
        fwprintf(file,L"%ls\t%ls\t%ls\t%ls\n",col[0],col[1],col[2],good?col[3]:L"-");
        if(good)matched++;else pending++;
    }
    fclose(in);
    if(error||(!matched&&!pending)){
        fclose(file);DeleteFileW(temp);return PRO_TK_BAD_INPUTS;
    }
    if(!ax_study_commit(file,temp,path))return PRO_TK_CANT_WRITE;
    part_text_add(out,L"PDF 已读取并记录校验值。图纸表与样件规则逐项核对，匹配 %d 项，待核对 %d 项。\n%ls\n",
        matched,pending,path);
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_card(const wchar_t *source,const wchar_t *key,PartText *out){
    wchar_t id[AX_STUDY_PATH];
    AxStudyRule rule={0};
    if(!ax_study_case_id(source,key,id,out))return PRO_TK_E_NOT_FOUND;
    ProError error=ax_study_read(source,id,&rule);
    if(error)return error;
    part_text_add(out,L"%ls %ls · %ls · 父装配 %ls\n定位约束 %d/%d，Creo 阵列状态 %d。\n",
        source,id,rule.name,rule.parent,rule.count,rule.reported,rule.pattern);
    part_text_add(out,L"位置 %.5g / %.5g / %.5g mm（相对父装配）。局部 X=(%.4g,%.4g,%.4g)，Y=(%.4g,%.4g,%.4g)，Z=(%.4g,%.4g,%.4g)。\n",
        rule.pose[3][0],rule.pose[3][1],rule.pose[3][2],rule.pose[0][0],rule.pose[0][1],rule.pose[0][2],
        rule.pose[1][0],rule.pose[1][1],rule.pose[1][2],rule.pose[2][0],rule.pose[2][1],rule.pose[2][2]);
    for(int i=0;i<rule.count;i++){
        AxStudyRow *row=&rule.rows[i];
        part_text_add(out,L"%d. %ls，先装参照 %ls %ls %ls [%d]，再接 %ls %ls [%d]%ls",
            i+1,ax_constraint_label(row->type),row->base.path,row->base.model,row->base.name,row->base.id,row->part.model,row->part.name,row->part.id,
            ax_offset_type(row->type)?L"，偏距 ":L"。\n");
        if(ax_offset_type(row->type))part_text_add(out,L"%.7g mm。\n",row->offset);
        if(row->has_array_gap)part_text_add(out,L"   阵列成员实测配合偏距 %.7g mm。\n",row->array_gap);
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_find_child(ProMdl assembly,const AxStudyRule *rule,const wchar_t *requested,ProMdl *child,PartText *out){
    const int variant=!wcscmp(requested,L"自动换件");
    if(wcscmp(requested,L"自动")&&!variant){
        if(!ax_study_key(requested))return PRO_TK_BAD_INPUTS;
        if(rule->type==PRO_MDL_ASSEMBLY){
            int loaded=0;
            return ax_load_subassembly(requested,child,&loaded);
        }
        ProError error=ProMdlnameInit((wchar_t *)requested,PRO_MDL_PART,child);
        if(error==PRO_TK_E_NOT_FOUND)error=ProMdlnameRetrieve((wchar_t *)requested,PRO_MDL_PART,child);
        return error;
    }
    ProMdl *candidates=NULL;
    int count=0,best=-1,ties=0;
    ProError error=ProSessionMdlList(rule->type,&candidates,&count);
    if(error)return error;
    for(int i=0;i<count;i++){
        if(candidates[i]==assembly)continue;
        AxScan *nested=NULL;
        ProMdlName name;
        if(ProMdlMdlnameGet(candidates[i],name)||(variant&&!_wcsicmp(name,rule->name)))continue;
        if(rule->type==PRO_MDL_ASSEMBLY&&ax_scan(candidates[i],0,&nested))continue;
        int score=0;
        for(int j=0;j<rule->count;j++){
            int level,side,id;
            ProSelection ref=NULL;
            if(ax_study_selection(candidates[i],nested,&rule->rows[j].part,NULL,0,&ref,&side,&level,&id)){score=-1;break;}
            score+=level;
            ProSelectionFree(&ref);
        }
        free(nested);
        if(score<=0)continue;
        if(!_wcsicmp(name,rule->name))score+=1;
        if(score>best){*child=candidates[i];best=score;ties=1;}
        else if(score==best)ties++;
    }
    if(candidates)ProArrayFree((ProArray *)&candidates);
    if(ties!=1){
        *child=NULL;
        part_text_add(out,L"当前会话里%ls符合这个用途的组件。打开新组件，或给出要安装的 Creo 模型名后再试。\n",
            ties?L"有多个同样匹配":L"缺少可唯一识别的");
        return ties?PRO_TK_E_FOUND:PRO_TK_E_NOT_FOUND;
    }
    ProMdlName selected;
    if(!ProMdlMdlnameGet(*child,selected))part_text_add(out,L"用途匹配到 %ls，接着核对安装参照。\n",selected);
    return PRO_TK_NO_ERROR;
}
