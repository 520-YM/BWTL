
typedef struct {
    const wchar_t *sketch,*feature;
    PanelProfile profile[4];
    int count;
} MountCut;

static double mount_height(const PanelProfile *item){return item->shape==PN_CIRCLE?item->width:item->height;}

static int mount_validate(const MountCut *first,const MountCut *second,double w,double h){
    const MountCut *groups[]={first,second};
    const PanelProfile *used[8]={0};
    int count=0;
    for(int g=0;g<2;g++){
        for(int i=0;i<groups[g]->count;i++){
            const PanelProfile *p=&groups[g]->profile[i];
            double x=p->width/2,y=mount_height(p)/2;
            if(!isfinite(p->x)||!isfinite(p->y)||x<=0||y<=0||
               p->x-x<OR_TOL||p->x+x>w-OR_TOL||p->y-y<OR_TOL||p->y+y>h-OR_TOL)return 0;
            for(int j=0;j<count;j++){
                const PanelProfile *a=used[j];
                if(p->shape==PN_CIRCLE&&a->shape==PN_CIRCLE){
                    if(hypot(p->x-a->x,p->y-a->y)<(p->width+a->width)/2+OR_TOL)return 0;
                }else if(fabs(p->x-a->x)<x+a->width/2+OR_TOL&&
                         fabs(p->y-a->y)<y+mount_height(a)/2+OR_TOL)return 0;
            }
            used[count++]=p;
        }
    }
    return 1;
}

static ProError mount_cut_group(FdTxn *tx,MountCut *group,double t){
    PartCommand cmd={0};
    ProMatrix location;
    PanelFrame frame={0};
    ProError error=quick_frame(tx,t,&cmd,&frame);
    ProMassProperty mass;
    if(!error)error=qa_geometry_mass(tx->model,&mass);
    if(error)return error;
    tx->volume=mass.volume;
    double area=0;
    for(int i=0;!error&&i<group->count;i++){
        area+=pn_area(&group->profile[i]);
        error=pn_profile_probes(tx,&frame,&group->profile[i],t,0);
    }
    if(!error)error=pn_sketch(tx->model,&cmd,&frame,group->sketch,group->profile,group->count,1,location);
    if(!error)error=pn_cut(tx,&frame,group->feature,group->sketch,location,t,1);
    if(!error)error=md_expected_volume(tx,-area*t);
    for(int i=0;!error&&i<group->count;i++)error=pn_profile_probes(tx,&frame,&group->profile[i],t,1);
    return error;
}

static ProError mount_create(const wchar_t *name,double w,double h,double t,MountCut *first,MountCut *second,
    ProMdl *built,PartText *out){
    *built=NULL;
    if(!ax_study_key(name)||w<=0||h<=0||t<=0||
       !mount_validate(first,second,w,h)){
        part_text_add(out,L"孔位超出板边、相互重叠，或板厚与外形不合。请核对两组接口的尺寸。\n");
        return PRO_TK_BAD_INPUTS;
    }
    if(!storage_prepare_output()){part_text_add(out,L"%ls\n",storage_output_message);return PRO_TK_BAD_CONTEXT;}
    if(!part_target_available(NULL,name,L"prt"))return PRO_TK_E_FOUND;
    if(cm_recovery_uncertain||sh_recovery_uncertain)return PRO_TK_BAD_CONTEXT;
    FdTxn tx={0};
    ProMdlName label;
    wcscpy_s(label,_countof(label),name);
    const ModelPlan box={.a=w,.b=h,.height=t,.ready=1};
    const char *stage="";
    ProError error=quick_seed(label,&tx.model);
    if(!error)error=model_build_internal(&box,label,&stage,tx.model,NULL,NULL);
    double mm=0;
    if(!error)error=qa_mm_per_unit(tx.model,&mm);
    if(!error)tx.scale=1/mm;
    int face=0;
    const wchar_t *where=L"底板与板厚";
    if(!error)error=quick_top(&tx,t,&face);
    if(!error)error=cm_native_command(tx.model,L"CREO_OP SMT_CONVERT %ls MOUNT_SHEET SURFACE:%d %.12g",label,face,t);
    if(!error){where=first->feature;error=mount_cut_group(&tx,first,t);}
    if(!error){where=second->feature;error=mount_cut_group(&tx,second,t);}
    if(!error)error=part_regenerate(tx.model);
    if(!error)error=part_health(tx.model);
    if(!error)error=quick_box_check(&tx,w,h,t,0);
    double got=0;
    if(!error)error=fab_thickness(tx.model,&mm,&got);
    if(!error&&fabs(got-t)>0.001)error=PRO_TK_INCOMPLETE;
    if(error){
        part_text_add(out,L"%ls 未通过 Creo 原生校验，错误 %d。操作到 %ls 时停止。\n",label,error,where);
        if(tx.model)ProMdlErase(tx.model);
        return error;
    }
    *built=tx.model;
    part_text_add(out,L"已创建 %ls，%.6g × %.6g mm，板厚 %.6g mm。\n%ls 共 %d 处，%ls 共 %d 处。两组切口均通过贯穿、体积及外形复测。待保存。\n",
        label,w,h,t,first->feature,first->count,second->feature,second->count);
    return PRO_TK_NO_ERROR;
}

static ProError mount_strip(const wchar_t *name,const double mm[8],ProMdl *built,PartText *out){
    const double w=mm[0],h=mm[1],t=mm[2],inset=mm[3],dia=mm[4],pitch=mm[5],length=mm[6],slot=mm[7];
    if(inset>=w/2||pitch<=0||dia<=0||slot<=0||length<=slot)return PRO_TK_BAD_INPUTS;
    MountCut fixed={L"MOUNT_END_PROFILE",L"MOUNT_END_HOLES",
        {{PN_CIRCLE,inset,h/2,dia,dia},{PN_CIRCLE,w-inset,h/2,dia,dia}},2};
    MountCut adjust={L"MOUNT_ADJUST_PROFILE",L"MOUNT_SLIDING_SLOTS",
        {{PN_CAPSULE,(w-pitch)/2,h/2,length,slot},{PN_CAPSULE,(w+pitch)/2,h/2,length,slot}},2};
    return mount_create(name,w,h,t,&fixed,&adjust,built,out);
}

static MountCut mount_rect(double w,double h,const double mm[3],const wchar_t *sketch,const wchar_t *feature){
    MountCut cut={.sketch=sketch,.feature=feature,.count=4};
    for(int i=0;i<4;i++){
        double x=(w+((i&1)?mm[0]:-mm[0]))/2,y=(h+((i&2)?mm[1]:-mm[1]))/2;
        cut.profile[i]=(PanelProfile){PN_CIRCLE,x,y,mm[2],mm[2]};
    }
    return cut;
}

static ProError mount_adapter(const wchar_t *name,const double mm[9],ProMdl *built,PartText *out){
    for(int i=3;i<9;i++)if(mm[i]<=0)return PRO_TK_BAD_INPUTS;
    MountCut unit=mount_rect(mm[0],mm[1],mm+3,L"EQUIPMENT_HOLE_PROFILE",L"EQUIPMENT_HOLES");
    MountCut site=mount_rect(mm[0],mm[1],mm+6,L"FLOOR_ANCHOR_PROFILE",L"FLOOR_ANCHOR_HOLES");
    return mount_create(name,mm[0],mm[1],mm[2],&unit,&site,built,out);
}

static int mount_chat(const wchar_t *request){
    static const wchar_t *labels[]={L"新建可换式背板安装带",L"新建地脚转接板"};
    int kind=-1;
    size_t offset=0;
    for(int i=0;i<_countof(labels);i++){
        size_t n=wcslen(labels[i]);
        if(!wcsncmp(request,labels[i],n)&&(!request[n]||iswspace(request[n]))){kind=i;offset=n;break;}
    }
    if(kind<0)return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    wchar_t command[2800];
    if(wcslen(request+offset)>=_countof(command)){append(L"请输入零件名和毫米尺寸。");return 1;}
    wcscpy_s(command,_countof(command),request+offset);
    wchar_t *next=NULL;
    wchar_t *name=wcstok_s(command,L" \t",&next);
    const int required=kind?9:8;
    double dims[9];
    int n=0;
    wchar_t *word;
    while(name&&(word=wcstok_s(NULL,L" \t",&next))!=NULL){
        if(n==required||!or_number(word,&dims[n])||dims[n]<=0){n=-1;break;}
        n++;
    }
    wchar_t text[PART_TEXT_MAX]=L"";
    PartText out={text,_countof(text),0};
    ProMdl result=NULL;
    ProError error=PRO_TK_BAD_INPUTS;
    native_set_busy(3);
    if(n==required)error=kind?mount_adapter(name,dims,&result,&out):mount_strip(name,dims,&result,&out);
    if(!error&&result){
        ProError shown=dv_display_new(result);
        if(shown)part_text_add(&out,L"零件已在会话创建，显示返回 Creo %d。\n",shown);
    }
    if(error&&!text[0])part_text_add(&out,kind?
        L"请输入「新建地脚转接板 名称 长 宽 板厚 设备X孔距 设备Y孔距 设备孔径 地脚X孔距 地脚Y孔距 地脚孔径」。\n":
        L"请输入「新建可换式背板安装带 名称 长 宽 板厚 端孔退距 端孔径 槽中心间距 槽长 槽宽」。\n");
    if(error)part_text_add(&out,L"操作未完成（Creo %d）。\n",error);
    native_prompt_set(L"");
    append_card(L"你",request);save_turn(L"用户",request);
    append_card(L"安装板",text);save_turn(L"安装板",text);
    native_set_busy(0);
    return 1;
}
