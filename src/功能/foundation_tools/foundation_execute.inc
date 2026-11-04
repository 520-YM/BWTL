static ProError foundation_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    const wchar_t *verb=cmd->arg[0];ProError e;ProMdlType type;ProMdlsubtype sub;report[0]=0;
    if((e=ProMdlTypeGet(model,&type))||(e=ProMdlSubtypeGet(model,&sub)))return e;
    if(type!=PRO_MDL_PART||sub==PROMDLSTYPE_PART_SHEETMETAL){wcscpy_s(report,capacity,L"此批基准与草绘功能限普通实体零件，请激活普通零件后重发。");return PRO_TK_BAD_CONTEXT;}
    if(sd_is_command(verb))return !wcsncmp(verb,L"SOLID_",6)?sd_create(cmd,model,report,capacity):sd_edit(cmd,model,report,capacity);
    if(!wcscmp(verb,L"REFERENCES")){
#include "references.inc"
}
    if(!wcsncmp(verb,L"DATUM_",6))return fd_create_datum(cmd,model,report,capacity);
    if(!wcscmp(verb,L"SKETCH_INFO"))return fd_sketch_info(model,cmd->arg[2],report,capacity);
    if(!wcscmp(verb,L"SKETCH_BEGIN")){
#include "sketch_begin.inc"
}
    if(!wcscmp(verb,L"SKETCH_CREATE"))return fd_sketch_create(cmd,model,report,capacity);
    if(!wcscmp(verb,L"SKETCH_ADD")){
#include "sketch_add.inc"
}
    return fd_sketch_edit(cmd,model,report,capacity);
}
