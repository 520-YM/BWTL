static ProError part_execute(const PartCommand *cmd,const PartSnapshot *snapshot,wchar_t *text,size_t capacity){
    ProError e;ProMdl model=NULL;ProMdlName name;text[0]=0;
    if(cmd->verb==PART_FOUNDATION&&quick_kind(cmd->arg[0]))return quick_execute(cmd,snapshot,text,capacity);
    int delivery=cmd->verb==PART_FOUNDATION?dv_kind(cmd->arg[0]):DV_NONE;
    if(delivery==DV_IMPORT){
        ProPath directory;
        if(!snapshot->directory[0]||ProDirectoryCurrentGet(directory)||_wcsicmp(directory,snapshot->directory)){
            wcscpy_s(text,capacity,L"导入未执行。Creo 工作目录已变化或不可用，请重新发送。");
            return PRO_TK_BAD_CONTEXT;
        }
        if(snapshot->model){
            PartCommand bound=*cmd;
            wcscpy_s(bound.arg[1],PART_TOKEN_MAX,snapshot->name);
            e=part_check_binding(&bound,snapshot,&model);
        }else{
            e=ProMdlCurrentGet(&model);
            if(e==PRO_TK_BAD_CONTEXT||e==PRO_TK_E_NOT_FOUND)e=PRO_TK_NO_ERROR;
            if(model)e=PRO_TK_BAD_CONTEXT;
        }
        if(e){
            wcscpy_s(text,capacity,L"导入未执行。活动模型已变化，请重新发送。");
            return e;
        }
        return dv_execute(model,cmd,text,capacity);
    }
    if((e=part_check_binding(cmd,snapshot,&model))){swprintf_s(text,capacity,L"未执行：活动模型、工作目录或模型版本与请求时不一致。请针对当前模型重新发送指令（错误码%d）。",e);return e;}
    int assembly=cmd->verb==PART_FOUNDATION?ax_kind(cmd->arg[0]):AX_NONE;
    if(assembly){
        if(snapshot->type!=PRO_MDL_ASSEMBLY||snapshot->window_model!=model){
            wcscpy_s(text,capacity,L"请激活目标顶层装配后重发。");
            return PRO_TK_BAD_CONTEXT;
        }
        if(!ax_read_diagnostic(assembly)&&assembly!=AX_REPAIR_REF&&assembly!=AX_LAYOUT_REFS&&assembly!=AX_LAYOUT_AUDIT&&!ax_delete_preview(cmd)&&(snapshot->unhealthy||part_health(model))){
            wcscpy_s(text,capacity,L"装配存在失败特征，请先核对重生成状态。");
            return PRO_TK_BAD_CONTEXT;
        }
        return ax_execute(model,cmd,text,capacity);
    }
    int repair=delivery==DV_HEAL||(delivery==DV_REF&&!wcscmp(cmd->arg[3],L"MISSING"));
    int status_read=cmd->verb==PART_FOUNDATION&&org_kind(cmd->arg[0])==ORG_STATUS;
    if(cmd->verb!=PART_READ && !repair && !status_read && (snapshot->unhealthy || part_health(model))){wcscpy_s(text,capacity,L"未执行：当前零件存在失败特征，先修复重生成问题。");return PRO_TK_BAD_CONTEXT;}
    if(snapshot->type==PRO_MDL_ASSEMBLY && cmd->verb!=PART_READ && cmd->verb!=PART_DIMS && cmd->verb!=PART_PARAM && cmd->verb!=PART_REGEN && cmd->verb!=PART_SAVE){
        wcscpy_s(text,capacity,L"当前活动目标是装配本身。已支持装配信息读取、驱动尺寸/参数修改及重生成/保存；拉伸需要先激活装配中的实体零件，不会自动挑选组件。");return PRO_TK_BAD_CONTEXT;
    }
    switch(cmd->verb){
    case PART_FOUNDATION:
        if(snapshot->in_assembly&&wcscmp(cmd->arg[0],L"REFERENCES")&&wcscmp(cmd->arg[0],L"SKETCH_INFO")){
            wcscpy_s(text,capacity,L"本批基准与关联草绘写入先用于独立普通零件；请单独打开该零件后重发。装配内组件写入尚未验证，未执行。");e=PRO_TK_BAD_CONTEXT;
        }else e=foundation_execute(cmd,model,text,capacity);break;
    case PART_EXTRUDE:e=part_extrude_current(cmd,snapshot,model,text,capacity);break;
    case PART_READ:part_snapshot_format_basic(snapshot,text,capacity);return 0;
    case PART_DIMS:e=part_change_dimensions(cmd,snapshot,model,text,capacity);break;
    case PART_PARAM:e=part_change_parameter(cmd,snapshot,model,text,capacity);break;
    case PART_SUPPRESS:case PART_RESUME:e=part_toggle_feature(cmd,snapshot,model,text,capacity);break;
    case PART_REGEN:e=part_regenerate(model);if(!e)swprintf_s(text,capacity,L"零件 %ls 已重生成，特征状态检查通过；未保存。",snapshot->name);break;
    case PART_DISPLAY:{
#include "part_display.inc"
}
    case PART_COPY:{
#include "part_copy.inc"
}
    case PART_RENAME:{
#include "part_rename.inc"
}
    case PART_SAVE:{
#include "part_save.inc"
}
    case PART_STEP:{
#include "part_step.inc"
}
    default:e=PRO_TK_BAD_INPUTS;break;
    }
    if(e&&!text[0])swprintf_s(text,capacity,L"原生操作未完成：%ls，错误码%d。未宣称成功，未自动保存。可能原因包括数据已变化、参数受限或输出重名；请读取模型后核对。",cmd->arg[0],e);
    trace("Independent part tool result",e);return e;
}
