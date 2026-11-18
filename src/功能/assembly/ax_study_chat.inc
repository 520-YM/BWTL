
static void ax_study_help(PartText *out){
    part_text_add(out,L"装配学习从当前 Creo 总装保存定位规则。以下命令可直接发送。\n"
        L"查看装配规则 总装名 ID:实例路径\n"
        L"对照装配图纸 总装名\n"
        L"导入装配图纸对照 总装名 \"PDF 文件\" \"对照表.tsv\"\n"
        L"按装配规则安装 总装名 ID:实例路径 新组件名 [支撑实例路径] [保存]\n"
        L"组件名可填「自动」或「自动换件」。多个支撑使用 ID:旧=ID:新,ID:旧=ID:新；旧路径从规则卡查看。已有图纸对照时可用编号或用途代替实例路径。\n"
        L"批量按装配规则安装 总装名 \"批量表路径\"\n"
        L"记录装配示范 / 结束装配示范 / 取消装配示范\n"
        L"检查装配学习结果 [ID:件1 ID:件2 最小净空毫米]\n"
        L"检查装配学习轨迹 移动件ID 障碍件ID AXIS:名称 ROTATE或SLIDE 起点 终点 样本数 净空毫米\n"
        L"验收装配规则 总装名 ID:实例路径\n");
}

static int ax_study_words(const wchar_t *p,wchar_t word[9][MAX_PATH]){
    int n=0;
    while(*p){
        while(iswspace(*p))p++;
        if(!*p)break;
        if(n==9)return -1;
        int quoted=*p==L'"';
        if(quoted)p++;
        const wchar_t *begin=p;
        while(*p&&(quoted?*p!=L'"':!iswspace(*p)))p++;
        size_t length=p-begin;
        if(!length||length>=MAX_PATH||(quoted&&(!*p||(p[1]&&!iswspace(p[1])))))return -1;
        wcsncpy_s(word[n++],MAX_PATH,begin,length);
        if(quoted)p++;
    }
    return n;
}

static int ax_study_chat(const wchar_t *request){
    static const wchar_t *const actions[]={L"装配学习工具",L"查看装配规则",L"对照装配图纸",L"按装配规则安装",
        L"批量按装配规则安装",L"记录装配示范",L"结束装配示范",L"取消装配示范",L"检查装配学习结果",L"验收装配规则",L"导入装配图纸对照",L"检查装配学习轨迹"};
    int action=-1;
    const wchar_t *args=NULL;
    for(int i=0;i<_countof(actions);i++){
        size_t n=wcslen(actions[i]);
        if(!wcsncmp(request,actions[i],n)&&(!request[n]||iswspace(request[n]))){action=i;args=request+n;break;}
    }
    if(action<0)return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    wchar_t words[9][MAX_PATH]={0},report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    int n=ax_study_words(args,words);
    ProMdl active=NULL;
    ProMdlCurrentGet(&active);
    ProError error=PRO_TK_BAD_INPUTS;
    native_set_busy(3);
    switch(action){
    case 0:ax_study_help(&out);error=PRO_TK_NO_ERROR;break;
    case 1:if(n==2)error=ax_study_card(words[0],words[1],&out);break;
    case 2:if(n==1)error=ax_study_case_id(words[0],L"",words[2],&out)?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;break;
    case 3:
        if(n>=3&&n<=5){
            int save=n>3&&!wcscmp(words[n-1],L"保存");
            int extras=n-save-3;
            ProIdTable path;
            if(extras==0||(extras==1&&(ax_path_parse(words[3],path)||wcschr(words[3],L'='))))
                error=active?ax_study_apply(active,words[0],words[1],words[2],extras?words[3]:NULL,save,&out):PRO_TK_BAD_CONTEXT;
        }
        break;
    case 4:if(n==2&&active)error=ax_study_batch(active,words[0],words[1],&out);break;
    case 5:if(!n)error=ax_study_demo_start(active,&out);break;
    case 6:if(!n)error=ax_study_demo_end(active,&out);break;
    case 7:
        if(!n){
            ax_study_demo.model=NULL;
            ProNotificationUnset(PRO_FEATURE_CREATE_POST);
            ProNotificationUnset(PRO_FEATURE_REDEFINE_POST);
            part_text_add(&out,L"装配示范已取消。\n");
            error=PRO_TK_NO_ERROR;
        }
        break;
    case 8:
        if(!n||(n==3&&qa_number(words[2],0,100000)))
            error=ax_study_audit(active,n?words[0]:NULL,n?words[1]:NULL,n?wcstod(words[2],NULL):0,&out);
        break;
    case 9:if(n==2&&active)error=ax_study_mark(active,words[0],words[1],&out);break;
    case 10:if(n==3)error=ax_study_import_layout(words[0],words[1],words[2],&out);break;
    case 11:
        if(n==8&&active){
            PartCommand protocol;
            ProMdlName assembly_name;
            wchar_t request[1024];
            ProError model=ProMdlMdlnameGet(active,assembly_name);
            if(!model&&swprintf_s(request,_countof(request),L"CREO_OP ASM_MOTION_SWEEP %ls %ls %ls %ls %ls %ls %ls %ls %ls",
                assembly_name,words[0],words[1],words[2],words[3],words[4],words[5],words[6],words[7])>=0&&
                part_command_parse(request,&protocol)==1)error=ax_execute(active,&protocol,report,_countof(report));
        }
        break;
    }
    if(error==PRO_TK_BAD_INPUTS)ax_study_help(&out);
    else if(error==PRO_TK_E_NOT_FOUND&&!report[0])part_text_add(&out,L"找不到对应的案例、图纸对照或组件。请核对名称和文件。\n");
    if(error)part_text_add(&out,L"操作未完成（Creo %d）。\n",error);
    native_prompt_set(L"");
    append_card(L"你",request);
    save_turn(L"用户",request);
    append_card(L"装配学习",report);
    save_turn(L"装配学习",report);
    native_set_busy(0);
    return 1;
}
