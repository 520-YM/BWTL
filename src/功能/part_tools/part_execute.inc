static ProError part_execute(const PartCommand *cmd,const PartSnapshot *snapshot,wchar_t *text,size_t capacity){
    ProError e;ProMdl model=NULL;ProMdlName name;text[0]=0;
    if((e=part_check_binding(cmd,snapshot,&model))){swprintf_s(text,capacity,L"未执行：活动模型、工作目录或模型版本与请求时不一致。请针对当前模型重新发送指令（错误码%d）。",e);return e;}
    if(cmd->verb!=PART_READ && (snapshot->unhealthy || part_health(model))){wcscpy_s(text,capacity,L"未执行：当前零件存在失败特征，先修复重生成问题。");return PRO_TK_BAD_CONTEXT;}
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
