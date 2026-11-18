
#include <ProVerstamp.h>

typedef struct {
    unsigned long pid;
    unsigned long stamp_hi,stamp_lo;
    int id,native_rows;
    ProMdlName child;
    char model_stamp[240];
    ProMatrix pose;
} AxStudyReceipt;

static int ax_study_receipt_file(const wchar_t *source,const wchar_t *key,const wchar_t *target,
    wchar_t path[MAX_PATH]){
    wchar_t directory[MAX_PATH],suffix[AX_STUDY_PATH];
    ProIdTable ids;
    if(!ax_study_key(source)||!ax_study_key(target)||!ax_path_parse(key,ids)||!ax_learning_directory(directory,1))return 0;
    wcscpy_s(suffix,_countof(suffix),key+3);
    for(wchar_t *p=suffix;*p;p++)if(*p==L'/')*p=L'-';
    return swprintf_s(path,MAX_PATH,L"%ls\\%ls-%ls-%ls.verify.txt",directory,source,suffix,target)>=0;
}

static int ax_study_case_stamp(const wchar_t *source,FILETIME *stamp){
    wchar_t path[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA data;
    if(!ax_study_casefile(source,path,0)||!GetFileAttributesExW(path,GetFileExInfoStandard,&data)||
       data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return 0;
    *stamp=data.ftLastWriteTime;
    return 1;
}

static ProError ax_study_verstamp(ProMdl model,char text[240]){
    ProWVerstamp stamp=NULL;
    char *str=NULL;
    ProError error=ProMdlVerstampGet(model,&stamp);
    if(!error)error=ProVerstampStringGet(stamp,&str);
    if(!error&&(!str||strnlen(str,240)>=240||strchr(str,'\n')||strchr(str,' ')))error=PRO_TK_BAD_CONTEXT;
    if(!error)strcpy_s(text,240,str);
    if(str)ProVerstampStringFree(&str);
    if(stamp)ProVerstampFree(&stamp);
    return error;
}

static ProError ax_study_inspect(ProMdl parent,int id,AxStudyReceipt *data){
    wchar_t key[32];
    swprintf_s(key,_countof(key),L"ID:%d",id);
    AxComponent comp={0};
    ProError error=ax_component_resolve(parent,key,&comp);
    ProBoolean under=PRO_B_TRUE,packaged=PRO_B_TRUE;
    if(!error)error=ProAsmcompIsPackaged(&comp.feature,&packaged);
    if(!error)error=ProAsmcompIsUnderconstrained(&comp.feature,&under);
    if(!error)error=ax_constraint_count(&comp.feature,&data->native_rows);
    if(!error)error=ProMdlMdlnameGet(comp.model,data->child);
    if(!error)error=ax_actual_pose(parent,&comp.feature,data->pose);
    if(!error)error=ax_study_verstamp(parent,data->model_stamp);
    if(error)return error;
    data->id=id;
    return under||packaged||!data->native_rows?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static int ax_study_write_receipt(const wchar_t *file,const AxStudyReceipt *data,int verified,const FILETIME *source){
    wchar_t temp[MAX_PATH];
    FILE *out=ax_study_atomic(file,temp);
    if(!out)return 0;
    fwprintf(out,L"BWTL-ASSEMBLY-REPLAY-1 %ls\n%lu %lu %lu %d %d %ls %hs\n",
        verified?L"REOPENED":L"SAVED",GetCurrentProcessId(),source->dwHighDateTime,source->dwLowDateTime,
        data->id,data->native_rows,data->child,data->model_stamp);
    for(int i=0;i<4;i++)for(int j=0;j<3;j++)fwprintf(out,L"%.17g ",data->pose[i][j]);
    fputwc(L'\n',out);
    return ax_study_commit(out,temp,file);
}

static int ax_study_read_receipt(const wchar_t *file,AxStudyReceipt *data){
    FILE *in=ax_learning_open(file);
    if(!in)return 0;
    wchar_t line[512],state[16],name[180],stamp[240];
    int ok=fgetws(line,_countof(line),in)&&swscanf_s(line,L"BWTL-ASSEMBLY-REPLAY-1 %15ls",state,(unsigned)_countof(state))==1&&
      (!wcscmp(state,L"SAVED")||!wcscmp(state,L"REOPENED"))&&fgetws(line,_countof(line),in)&&
      swscanf_s(line,L"%lu %lu %lu %d %d %179ls %239ls",&data->pid,&data->stamp_hi,&data->stamp_lo,&data->id,
        &data->native_rows,name,(unsigned)_countof(name),stamp,(unsigned)_countof(stamp))==7&&
      wcslen(name)<_countof(data->child)&&fgetws(line,_countof(line),in)&&
      swscanf_s(line,L"%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
        &data->pose[0][0],&data->pose[0][1],&data->pose[0][2],&data->pose[1][0],&data->pose[1][1],&data->pose[1][2],
        &data->pose[2][0],&data->pose[2][1],&data->pose[2][2],&data->pose[3][0],&data->pose[3][1],&data->pose[3][2])==12;
    if(ok){
        wcscpy_s(data->child,_countof(data->child),name);
        ok=WideCharToMultiByte(CP_UTF8,0,stamp,-1,data->model_stamp,_countof(data->model_stamp),NULL,NULL)>0;
        data->pose[3][3]=1;
        for(int i=0;i<4;i++)for(int j=0;j<3;j++)if(!isfinite(data->pose[i][j]))ok=0;
    }
    fclose(in);
    return ok;
}

static ProError ax_study_audit(ProMdl assembly,const wchar_t *first,const wchar_t *second,double clearance,PartText *out){
    if(!assembly)return PRO_TK_BAD_CONTEXT;
    if(first&&second){
        AxComponent a={0},b={0};
        ProSelection left=NULL,right=NULL;
        double mm=0,distance=0;
        ProBoolean touching=PRO_B_FALSE;
        Pro3dPnt nearest[2];
        ProError error=qa_mm_per_unit(assembly,&mm);
        if(!error)error=ax_component_resolve(assembly,first,&a);
        if(!error)error=ax_component_resolve(assembly,second,&b);
        if(!error&&ax_same_occurrence(&a,&b))error=PRO_TK_BAD_INPUTS;
        if(!error)error=ax_part_selection(&a,&left);
        if(!error)error=ax_part_selection(&b,&right);
        if(!error)error=ProFitClearanceCompute(left,PRO_FIT_INCLUDE_NONE,right,PRO_FIT_INCLUDE_NONE,
            PRO_FIT_WHOLE_SURFACE,NULL,PRO_B_FALSE,&distance,NULL,&touching,nearest);
        if(left)ProSelectionFree(&left);
        if(right)ProSelectionFree(&right);
        if(error)return error;
        int pass=!touching&&qa_at_least(distance*mm,clearance);
        part_text_add(out,L"%ls 与 %ls 的实测净空 %.7g mm，要求 %.7g mm，%ls。\n",
            first,second,distance*mm,clearance,touching?L"Creo 报告干涉":pass?L"通过":L"低于要求");
        return pass?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
    }
    AxScan *scan=NULL;
    ProError error=ax_scan(assembly,0,&scan);
    if(error)return error;
    int positioned=0,under=0,misses=0,duplicates=0;
    for(int i=0;i<scan->count;i++){
        const AxOccurrence *entry=&scan->occurrences[i];
        if(entry->state!=PRO_FEAT_ACTIVE)continue;
        if(!entry->model){misses++;continue;}
        wchar_t id[AX_STUDY_PATH];
        ax_path_label(entry->ids,entry->depth,id,_countof(id));
        AxComponent comp={0};
        ProBoolean p=PRO_B_TRUE,u=PRO_B_TRUE;
        ProMatrix pose;
        if(ax_component_resolve(assembly,id,&comp)||ProAsmcompIsPackaged(&comp.feature,&p)||
          ProAsmcompIsUnderconstrained(&comp.feature,&u)||ProAsmcomppathTrfGet(&comp.path,PRO_B_TRUE,pose)){misses++;continue;}
        if(p||u){under++;part_text_add(out,L"%ls %ls %ls。\n",id,entry->name,p?L"未定位":L"欠约束");}
        else positioned++;
        for(int j=i-1;j>=0;j--){
            const AxOccurrence *prev=&scan->occurrences[j];
            if(prev->model!=entry->model||prev->state!=PRO_FEAT_ACTIVE)continue;
            ProAsmcomppath at;
            ProMatrix previous;
            ProIdTable ids={0};
            memcpy(ids,prev->ids,prev->depth*sizeof(int));
            if(!ProAsmcomppathInit((ProSolid)assembly,ids,prev->depth,&at)&&
                !ProAsmcomppathTrfGet(&at,PRO_B_TRUE,previous)&&ax_same_pose(previous,pose,1)){
                wchar_t other[AX_STUDY_PATH];
                ax_path_label(prev->ids,prev->depth,other,_countof(other));
                duplicates++;
                part_text_add(out,L"%ls 和 %ls 使用同一模型且位姿相同，请核实是否重复安装。\n",other,id);
                break;
            }
        }
    }
    int limited=scan->limited;
    free(scan);
    part_text_add(out,L"定位通过 %d，欠约束或未定位 %d，无法读取 %d，重叠的同模型实例 %d。\n",
        positioned,under,misses,duplicates);
    if(limited)part_text_add(out,L"已达到扫描上限。\n");
    return under||misses||duplicates||limited?PRO_TK_INCOMPLETE:part_health(assembly);
}

static ProError ax_study_mark(ProMdl assembly,const wchar_t *source,const wchar_t *key,PartText *out){
    ProMdlName target;
    ProBoolean dirty=PRO_B_TRUE;
    ProError error=ProMdlMdlnameGet(assembly,target);
    wchar_t id[AX_STUDY_PATH],receipt[MAX_PATH];
    AxStudyReceipt saved={0},current={0};
    AxStudyRule rule={0};
    FILETIME stamp;
    if(error)return error;
    if(!ax_study_case_id(source,key,id,out)||!ax_study_receipt_file(source,id,target,receipt)||
       !ax_study_case_stamp(source,&stamp)||!ax_study_read_receipt(receipt,&saved))return PRO_TK_E_NOT_FOUND;
    if(saved.stamp_hi!=stamp.dwHighDateTime||saved.stamp_lo!=stamp.dwLowDateTime||
       (error=ax_study_read(source,id,&rule)))return error?error:PRO_TK_INCOMPLETE;
    if(saved.pid==GetCurrentProcessId()||ProMdlModificationVerify(assembly,&dirty)||dirty){
        part_text_add(out,L"需要在另一 Creo 会话打开保存的总装后再验收换件。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    if((error=ax_study_inspect(assembly,saved.id,&current)))return error;
    if(!_wcsicmp(rule.name,current.child)){
        part_text_add(out,L"当前使用了与示例相同的零件号。要验证可复用性，请使用不同零件并再次装配。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    double mm=0;
    if((error=qa_mm_per_unit(assembly,&mm)))return error;
    if(strcmp(saved.model_stamp,current.model_stamp)||_wcsicmp(saved.child,current.child)||
       saved.native_rows!=current.native_rows||!ax_same_pose(saved.pose,current.pose,mm))return PRO_TK_INCOMPLETE;
    if(!ax_study_write_receipt(receipt,&current,1,&stamp))return PRO_TK_CANT_WRITE;
    part_text_add(out,L"跨会话换件验收通过。%ls 的 ID:%d 共 %d 条定位约束，保存重开后的位姿一致。\n验收文件 %ls\n",
        current.child,current.id,current.native_rows,receipt);
    return PRO_TK_NO_ERROR;
}

static ProError ax_study_apply(ProMdl assembly,const wchar_t *source,const wchar_t *key,const wchar_t *component,
    const wchar_t *base,int save,PartText *out){
    wchar_t id[AX_STUDY_PATH],receipt[MAX_PATH]=L"";
    ProMdlName target;
    AxStudyRule rule={0};
    AxStudyReceipt before={0};
    FILETIME case_stamp;
    ProError error=ProMdlMdlnameGet(assembly,target);
    if(error)return error;
    if(!_wcsicmp(target,source)){part_text_add(out,L"请激活用于试装的目标装配。当前打开的是学习来源本身。\n");return PRO_TK_BAD_CONTEXT;}
    if(!ax_study_case_id(source,key,id,out))return PRO_TK_E_NOT_FOUND;
    if((error=ax_study_read(source,id,&rule)))return error;
    if(!rule.count||rule.count!=rule.reported||rule.under){
        part_text_add(out,L"原模块的定位读回不完整。先打开样件用「装配学习」重采，或补齐缺失的支撑。\n");
        return PRO_TK_INCOMPLETE;
    }
    if(save&&(!ax_study_receipt_file(source,id,target,receipt)||!ax_study_case_stamp(source,&case_stamp)||!storage_prepare_output())){
        part_text_add(out,L"保存需要选好 BWTL 当前数据盘上的 Creo 工作目录。\n");
        return PRO_TK_CANT_WRITE;
    }
    ProMdl child=NULL;
    if((error=ax_study_find_child(assembly,&rule,component,&child,out)))return error;
    int placed=0;
    if((error=ax_study_place(assembly,child,&rule,base,&placed,out))||!save)return error;
    if((error=ProMdlSave(assembly))||(error=ax_study_inspect(assembly,placed,&before)))return error;
    if(!ax_study_write_receipt(receipt,&before,0,&case_stamp))return PRO_TK_CANT_WRITE;
    ProPath where=L"";
    ProMdlOriginGet(assembly,where);
    part_text_add(out,L"已保存装配 %ls。重新打开后发送「验收装配规则 %ls %ls」，即可登记换件通过。\n",where,source,key);
    return PRO_TK_NO_ERROR;
}

typedef struct {
    wchar_t key[AX_STUDY_PATH],model[PRO_NAME_SIZE],base[AX_STUDY_PATH];
    ProError outcome;
} AxStudyTask;

static ProError ax_study_batch(ProMdl assembly,const wchar_t *source,const wchar_t *file,PartText *out){
    FILE *in=ax_learning_open(file);
    if(!in)return PRO_TK_E_NOT_FOUND;
    AxStudyTask *tasks=calloc(AX_STUDY_BATCH,sizeof(*tasks));
    if(!tasks){fclose(in);return PRO_TK_OUT_OF_MEMORY;}
    int count=0,done=0,malformed=0;
    wchar_t line[512];
    while(fgetws(line,_countof(line),in)){
        wchar_t *start=line[0]==0xfeff?line+1:line;
        if(*start==L'#'||*start==L'\r'||*start==L'\n')continue;
        wchar_t *col[3];
        if(count==AX_STUDY_BATCH||!ax_study_layout_line(start,col,3)||wcslen(col[0])>=AX_STUDY_PATH||
           wcslen(col[1])>=PRO_NAME_SIZE||wcslen(col[2])>=AX_STUDY_PATH){malformed=1;break;}
        wcscpy_s(tasks[count].key,AX_STUDY_PATH,col[0]);
        wcscpy_s(tasks[count].model,PRO_NAME_SIZE,col[1]);
        if(wcscmp(col[2],L"-"))wcscpy_s(tasks[count].base,AX_STUDY_PATH,col[2]);
        tasks[count++].outcome=PRO_TK_E_NOT_FOUND;
    }
    fclose(in);
    if(malformed||!count){free(tasks);return PRO_TK_BAD_INPUTS;}
    int before=-1;
    ax_recovery_uncertain=0;
    while(done!=before&&done<count&&!ax_recovery_uncertain){
        before=done;
        for(int i=0;i<count;i++){
            AxStudyTask *task=&tasks[i];
            if(!task->outcome||(task->outcome!=PRO_TK_E_NOT_FOUND&&task->outcome!=PRO_TK_E_FOUND))continue;
            wchar_t buffer[2400]=L"";
            PartText info={buffer,_countof(buffer),0};
            task->outcome=ax_study_apply(assembly,source,task->key,task->model,task->base,0,&info);
            if(!task->outcome){done++;part_text_add(out,L"%ls %ls\n%ls",task->key,task->model,buffer);}
            else if(task->outcome!=PRO_TK_E_NOT_FOUND&&task->outcome!=PRO_TK_E_FOUND)
                part_text_add(out,L"%ls %ls\n%ls",task->key,task->model,buffer);
        }
    }
    for(int i=0;i<count;i++)if(tasks[i].outcome)
        part_text_add(out,L"待处理 %ls · %ls（Creo %d）。\n",tasks[i].key,tasks[i].model,tasks[i].outcome);
    part_text_add(out,L"已定位 %d/%d 项，已完成的留在当前装配里。缺失参照会在支撑件装好后复查。\n",done,count);
    free(tasks);
    return done==count?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}
