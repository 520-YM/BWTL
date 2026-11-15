#ifndef BWTL_SMART_CHECKPOINT_H
#define BWTL_SMART_CHECKPOINT_H
typedef struct {
    ProMdl model,window;
    ProWVerstamp stamp,parent;
    ProMdlName name;
    ProPath directory;
} SmartCheckpoint;
static SmartCheckpoint smart_checkpoint;

static void smart_binding_clear(void){
    smart_committed_part=(SmartPartSignature){0};
    if(smart_checkpoint.stamp)ProVerstampFree(&smart_checkpoint.stamp);
    if(smart_checkpoint.parent)ProVerstampFree(&smart_checkpoint.parent);
    memset(&smart_checkpoint,0,sizeof(smart_checkpoint));
}

static const wchar_t *smart_binding_problem(void){
    ProMdl current=NULL,active=NULL;
    ProPath directory;
    ProMdlName name;
    ProWVerstamp stamp=NULL;
    SmartCheckpoint *c=&smart_checkpoint;
    if(!c->stamp)return L"本轮缺少可核对的模型版本记录";
    if(ProMdlCurrentGet(&current)||current!=c->window)return L"Creo 活动窗口与本轮目标不一致";
    if(ProDirectoryCurrentGet(directory))return L"无法核对 Creo 工作目录";
    if(_wcsicmp(directory,c->directory))return L"Creo 工作目录已变化";
    active=current;
    if(c->model!=current&&ProMdlActiveGet(&active))return L"无法读取装配中的活动零件";
    if(active!=c->model||ProMdlMdlnameGet(active,name)||_wcsicmp(name,c->name))return L"活动模型与本轮目标不一致";
    if(ProMdlVerstampGet(active,&stamp))return L"无法读取当前模型版本";
    int same=ProVerstampEqual(c->stamp,stamp);
    ProVerstampFree(&stamp);
    if(!same)return L"当前模型版本已变化，已保留本轮完成的步骤";
    if(c->parent){
        if(ProMdlVerstampGet(current,&stamp))return L"无法读取父装配的版本";
        same=ProVerstampEqual(c->parent,stamp);
        ProVerstampFree(&stamp);
        if(!same)return L"父装配版本已变化，已保留本轮完成的步骤";
    }
    return NULL;
}

static int smart_binding_valid(void){return !smart_binding_problem();}

static int smart_binding_set(ProMdl model,ProMdl window,const wchar_t *directory){
    ProPath current_dir;
    ProMdl current=NULL;
    smart_binding_clear();
    if(!model||!window||!directory[0]||ProDirectoryCurrentGet(current_dir)||_wcsicmp(current_dir,directory)||
       ProMdlCurrentGet(&current)||current!=window)return 0;
    SmartCheckpoint *c=&smart_checkpoint;
    c->model=model;c->window=window;
    wcscpy_s(c->directory,_countof(c->directory),current_dir);
    if(ProMdlMdlnameGet(model,c->name)||ProMdlVerstampGet(model,&c->stamp)||
       (model!=window&&ProMdlVerstampGet(window,&c->parent))||!smart_binding_valid()){
        smart_binding_clear();
        return 0;
    }
    return 1;
}
static const wchar_t *smart_continue_binding(void){
    SmartPartSignature committed=smart_committed_part;
    smart_committed_part=(SmartPartSignature){0};
    const wchar_t *problem=smart_binding_problem();
    SmartCheckpoint *checkpoint=&smart_checkpoint;
    if(!problem||!committed.hash||committed.error||checkpoint->model!=checkpoint->window||checkpoint->parent)
        return problem;
    ProMdl active=NULL;
    ProMdlName name;
    ProPath directory;
    if(ProMdlCurrentGet(&active)||active!=checkpoint->model||ProMdlMdlnameGet(active,name)||
       _wcsicmp(name,checkpoint->name)||ProDirectoryCurrentGet(directory)||_wcsicmp(directory,checkpoint->directory))
        return problem;
    SmartPartSignature current=smart_part_signature(active);
    if(current.error||current.hash!=committed.hash)return problem;
    /* Creo can publish the new version stamp when its native command returns.
       Accept that stamp only if the just-verified part still matches. */
    if(!smart_binding_set(active,active,directory))return problem;
    trace("Verified committed part at continuation",0);
    return NULL;
}
#endif
