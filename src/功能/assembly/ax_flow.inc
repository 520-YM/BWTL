
#define AX_FLOW_LIMIT 96

typedef struct {
    wchar_t path[AX_STUDY_PATH],parent[AX_STUDY_PATH];
    ProFamilyMdlName model;
    int state;
} AxFlowStep;
typedef struct {
    AxFlowStep steps[AX_FLOW_LIMIT];
    int sequence[AX_FLOW_LIMIT],used,count;
} AxFlow;

static int ax_flow_key(const wchar_t *source,const wchar_t *label,wchar_t id[AX_STUDY_PATH],PartText *out){
    if(!wcsncmp(label,L"上方",2)||!wcsncmp(label,L"下方",2)||!wcsncmp(label,L"左侧",2)||!wcsncmp(label,L"右侧",2)){
        wchar_t file[MAX_PATH],line[512],header[120];
        FILE *data=ax_study_layout_file(source,file)?ax_learning_open(file):NULL;
        if(!data)return 0;
        swprintf_s(header,_countof(header),L"BWTL-ASSEMBLY-LAYOUT-1 %ls",source);
        int matches=0;ProIdTable ids={0};
        if(fgetws(line,_countof(line),data)){
            line[wcscspn(line,L"\r\n")]=0;
            if(!_wcsicmp(line[0]==0xfeff?line+1:line,header)){
                while(fgetws(line,_countof(line),data)){
                    wchar_t *col[4];
                    if(!ax_study_layout_line(line,col,4)||_wcsicmp(col[1],label+2)||!ax_path_parse(col[3],ids))continue;
                    wchar_t *cut=wcsrchr(col[0],L'-');
                    if(cut&&cut[1]==label[0]&&!cut[2]){wcscpy_s(id,AX_STUDY_PATH,col[3]);matches++;}
                }
            }
        }
        fclose(data);
        if(matches==1)return 1;
    }
    return ax_study_case_id(source,label,id,out);
}

static int ax_flow_rebase(const wchar_t *parent,const wchar_t *path,wchar_t dependency[AX_STUDY_PATH]){
    ProIdTable route={0},root={0};
    int count=ax_path_parse(path,route),prefix=ax_path_parse(parent,root);
    if(count<=0)return 0;
    if(prefix&&count>prefix&&!memcmp(route,root,prefix*sizeof(int)))return ax_path_label(route,prefix+1,dependency,AX_STUDY_PATH);
    if(prefix){
        if(prefix>=AX_PATH_DEPTH)return 0;
        root[prefix]=route[0];
        return ax_path_label(root,prefix+1,dependency,AX_STUDY_PATH);
    }
    return ax_path_label(route,1,dependency,AX_STUDY_PATH);
}

static ProError ax_flow_discover(const wchar_t *source,const wchar_t *path,const wchar_t *only_parent,
    AxFlow *plan,PartText *out){
    for(int i=0;i<plan->used;i++){
        if(wcscmp(plan->steps[i].path,path))continue;
        if(plan->steps[i].state==2)return PRO_TK_NO_ERROR;
        part_text_add(out,L"装配依赖在 %ls 形成循环，需先核对原规则。\n",path);
        return PRO_TK_BAD_CONTEXT;
    }
    if(plan->used>=AX_FLOW_LIMIT)return PRO_TK_OUT_OF_RANGE;
    AxStudyRule *rule=calloc(1,sizeof(*rule));
    if(!rule)return PRO_TK_OUT_OF_MEMORY;
    ProError error=ax_study_read(source,path,rule);
    if(error){free(rule);return error;}
    if(only_parent&&wcscmp(only_parent,rule->parent)){free(rule);return PRO_TK_BAD_CONTEXT;}
    int index=plan->used++;
    AxFlowStep *step=&plan->steps[index];
    wcscpy_s(step->path,AX_STUDY_PATH,path);
    wcscpy_s(step->parent,AX_STUDY_PATH,rule->parent);
    wcscpy_s(step->model,_countof(step->model),rule->name);
    step->state=1;
    if(!only_parent&&wcscmp(step->parent,L"ROOT")){
        /*
         * The parent defines which assembly to edit. Its own placement may
         * require the child interface being planned, so do not make that
         * placement an extra prerequisite of work inside the parent.
         */
        part_text_add(out,L"%ls 在父装配 %ls 内执行。\n",path,step->parent);
    }
    for(int i=0;!error&&i<rule->count;i++){
        wchar_t dependency[AX_STUDY_PATH];
        const wchar_t *base=rule->rows[i].base.path;
        if(!wcscmp(base,L"ROOT"))continue;
        if(!ax_flow_rebase(step->parent,base,dependency)||!wcscmp(dependency,path))continue;
        error=ax_flow_discover(source,dependency,only_parent,plan,out);
        if(error)part_text_add(out,L"%ls 的支撑 %ls 需要复核。\n",path,dependency);
    }
    if(!error){
        step->state=2;
        plan->sequence[plan->count++]=index;
    }
    free(rule);
    return error;
}

static ProError ax_flow_plan(const wchar_t *source,const wchar_t *const *labels,int count,PartText *out){
    AxFlow *plan=calloc(1,sizeof(*plan));
    if(!plan)return PRO_TK_OUT_OF_MEMORY;
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<count;i++){
        wchar_t path[AX_STUDY_PATH];
        if(!ax_flow_key(source,labels[i],path,out))error=PRO_TK_E_NOT_FOUND;
        else error=ax_flow_discover(source,path,NULL,plan,out);
    }
    if(!error){
        part_text_add(out,L"执行顺序共 %d 步。先在各自父装配定位支撑，再定位关联组件。父装配已用来源路径标识。\n",plan->count);
        for(int i=0;i<plan->count;i++){
            const AxFlowStep *step=&plan->steps[plan->sequence[i]];
            part_text_add(out,L"%d. 父装配 %ls，%ls %ls。\n",i+1,step->parent,step->path,step->model);
        }
    }
    free(plan);
    return error;
}

static ProError ax_flow_supplied(ProMdl target,const wchar_t *name){
    AxScan *scan=NULL;
    ProError error=ax_scan(target,0,&scan);
    if(error)return error;
    int hits=0;
    for(int i=0;i<scan->count;i++){
        AxOccurrence *row=&scan->occurrences[i];
        if(row->depth==1&&row->model&&row->state==PRO_FEAT_ACTIVE&&!_wcsicmp(row->name,name))hits++;
    }
    free(scan);
    return hits==1?PRO_TK_NO_ERROR:hits?PRO_TK_E_FOUND:PRO_TK_E_NOT_FOUND;
}

static ProError ax_flow_install(ProMdl target,const wchar_t *source,const wchar_t *label,const wchar_t *name,
    int prerequisites,PartText *out){
    if(!target)return PRO_TK_BAD_CONTEXT;
    wchar_t path[AX_STUDY_PATH];
    AxStudyRule last={0};
    if(!ax_flow_key(source,label,path,out))return PRO_TK_E_NOT_FOUND;
    ProError error=ax_study_read(source,path,&last);
    if(error)return error;
    const wchar_t *selected=name&&*name?name:last.name;
    if(prerequisites){
        AxFlow *plan=calloc(1,sizeof(*plan));
        if(!plan)return PRO_TK_OUT_OF_MEMORY;
        error=ax_flow_discover(source,path,last.parent,plan,out);
        for(int i=0;!error&&i+1<plan->count;i++){
            const AxFlowStep *step=&plan->steps[plan->sequence[i]];
            ProError exists=ax_flow_supplied(target,step->model);
            if(exists==PRO_TK_E_NOT_FOUND){
                part_text_add(out,L"先补支撑 %ls %ls。\n",step->path,step->model);
                error=ax_study_apply(target,source,step->path,step->model,NULL,0,out);
            }else if(exists)error=exists;
        }
        free(plan);
        if(error)return error;
    }
    return ax_study_apply(target,source,path,selected,NULL,0,out);
}

static int ax_flow_sentence(const wchar_t *p,wchar_t label[MAX_PATH]){
    if(*p!=L'把')return 0;
    const wchar_t *end=wcsstr(++p,L"装上");
    if(!end||end==p||end-p>=MAX_PATH)return 0;
    const wchar_t *tail=end+2;
    if(*tail&&wcscmp(tail,L"。")&&wcscmp(tail,L"。直接执行"))return 0;
    wcsncpy_s(label,MAX_PATH,p,end-p);
    or_trim(label);
    return !!*label;
}

#include "ax_flow_memory.inc"
#include "ax_flow_hole_group.inc"
#include "ax_flow_replay.inc"
#include "ax_flow_resume.inc"
static int ax_flow_chat(const wchar_t *request){
    static const wchar_t *const commands[]={L"按装配记忆",L"规划跨层装配",L"先补支撑再装",L"记录装配方向",L"读取装配方向",L"记录装配学习修订",L"查看装配学习历史",L"匹配装配孔组",L"核对阵列站位",L"按新孔组补装",L"比较装配试装",L"建立装配续做任务",L"继续装配任务",L"查看装配任务"};
    size_t n=0;
    int action=-1;
    for(int i=0;i<_countof(commands);i++){
        size_t len=wcslen(commands[i]);
        if(!wcsncmp(request,commands[i],len)&&(!request[len]||iswspace(request[len]))){n=len;action=i;break;}
    }
    if(action<0)return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    wchar_t words[9][MAX_PATH]={0},report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    int count=ax_study_words(request+n,words);
    ProMdl current=NULL;
    ProMdlCurrentGet(&current);
    ProError error=PRO_TK_BAD_INPUTS;
    native_set_busy(3);
    if(action==3&&count==4)error=ax_flow_direction_save(current,words[0],words[1],words[2],words[3],&out);
    else if(action==4&&(count==2||count==3))error=ax_flow_direction_get(current,words[0],words[1],count==3?words[2]:NULL,&out);
    else if(action==5&&count==2)error=ax_flow_revision_note(current,words[0],words[1],&out);
    else if(action==6&&count==1)error=ax_flow_history(words[0],&out);
    else if(action==7&&(count==7||count==8)){
        double tolerance=0,anchor=0;
        if(or_number(words[6],&tolerance)&&(count==7||
            (or_number(words[7],&anchor)&&anchor>=1&&anchor<=AX_GROUP_HOLES&&floor(anchor)==anchor)))
            error=ax_group_compare(current,words[0],words[1],words[2],words[3],words[4],words[5],tolerance,(int)anchor-1,&out);
    }else if((action==8||action==9)&&count==6){
        for(int i=0;i<5;i++)if(wcslen(words[i])>=PART_TOKEN_MAX){count=0;break;}
        if(count)error=ax_group_stations(current,words[0],words[1],words[2],words[3],words[4],words[5],action==9,&out);
    }
    else if(action==10&&count==5)error=ax_flow_compare_branches(current,words[0],words[1],words[2],words[3],words[4],&out);
    else if(action==11&&count==3)error=ax_resume_begin(current,words[0],words[1],words[2],&out);
    else if(action==12&&(count==1||count==2)){
        double steps=1;
        if(count==1||(or_number(words[1],&steps)&&steps>=1&&steps<=AX_RESUME_LIMIT&&floor(steps)==steps))
            error=ax_resume_run(current,words[0],(int)steps,&out);
    }else if(action==13&&count==1)error=ax_resume_run(current,words[0],0,&out);
    else if(count>=2&&action<3){
        if(action==1){
            const wchar_t *labels[8];
            for(int i=1;i<count;i++)labels[i-1]=words[i];
            error=ax_flow_plan(words[0],labels,count-1,&out);
        }else if(action==2&&count<=3){
            error=ax_flow_install(current,words[0],words[1],count==3?words[2]:NULL,1,&out);
        }else if(action==0){
            const wchar_t *text=request+n;
            while(iswspace(*text))text++;
            while(*text&&!iswspace(*text))text++;
            while(iswspace(*text))text++;
            wchar_t label[MAX_PATH];
            if(ax_flow_sentence(text,label))error=ax_flow_install(current,words[0],label,NULL,0,&out);
        }
    }
    if(error&&!report[0])part_text_add(&out,L"请核对总装案例、安装目标和活动父装配。示例「按装配记忆 M26G2000000 把上方导向轮支架装上」。\n");
    if(error)part_text_add(&out,L"步骤未全部通过（Creo %d）。\n",error);
    native_prompt_set(L"");
    append_card(L"你",request);save_turn(L"用户",request);
    append_card(L"装配学习",report);save_turn(L"装配学习",report);
    native_set_busy(0);
    return 1;
}
