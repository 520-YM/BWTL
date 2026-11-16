static ProError foundation_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    const wchar_t *verb=cmd->arg[0];ProError e;ProMdlType type;ProMdlsubtype sub;report[0]=0;
    if((e=ProMdlTypeGet(model,&type))||(e=ProMdlSubtypeGet(model,&sub)))return e;
    if(type==PRO_MDL_PART&&dv_kind(verb))return dv_execute(model,cmd,report,capacity);
    if(type==PRO_MDL_PART&&qa_kind(verb))return qa_execute(model,cmd,report,capacity);
    if(type==PRO_MDL_PART&&ux_kind(verb))return ux_execute(cmd,model,report,capacity);
    if(type==PRO_MDL_PART&&smx_kind(verb)){
        PartText out={report,capacity,0};
        return smx_execute(model,cmd,smx_kind(verb),&out);
    }
    if(type==PRO_MDL_PART&&sh_kind(verb))return sh_execute(cmd,model,report,capacity);
    if(type==PRO_MDL_PART&&pn_kind(verb))return pn_execute(cmd,model,sub==PROMDLSTYPE_PART_SHEETMETAL,report,capacity);
    if(type==PRO_MDL_PART&&org_kind(verb))return org_execute(cmd,model,report,capacity);
    if(type==PRO_MDL_PART&&!wcscmp(verb,L"STOOL_BACK"))return quick_backrest(model,cmd,report,capacity);
    if(type==PRO_MDL_PART&&fab_kind(verb)){PartText output={report,capacity,0};return fab_execute(model,cmd,&output);}
    if(type==PRO_MDL_PART&&in_kind(verb))return in_execute(cmd,model,sub==PROMDLSTYPE_PART_SHEETMETAL,report,capacity);
    if(type==PRO_MDL_PART&&rv_kind(verb))return rv_execute(cmd,model,sub==PROMDLSTYPE_PART_SHEETMETAL,report,capacity);
    if(type==PRO_MDL_PART&&vt_kind(verb))return vt_execute(cmd,model,sub==PROMDLSTYPE_PART_SHEETMETAL,report,capacity);
    if(type==PRO_MDL_PART&&fx_kind(verb))return fx_execute(cmd,model,sub==PROMDLSTYPE_PART_SHEETMETAL,report,capacity);
    if(type==PRO_MDL_PART&&cm_kind(verb)){
        if(sub==PROMDLSTYPE_PART_SHEETMETAL)return cm_execute(cmd,model,report,capacity);
        wcscpy_s(report,capacity,L"请单独打开原生钣金零件后重发。");
        return PRO_TK_BAD_CONTEXT;
    }
    if(type!=PRO_MDL_PART||sub==PROMDLSTYPE_PART_SHEETMETAL){wcscpy_s(report,capacity,L"此批基准与草绘功能限普通实体零件，请激活普通零件后重发。");return PRO_TK_BAD_CONTEXT;}
    if(!wcscmp(verb,L"PLATE_LEGS"))return quick_plate_legs(model,cmd,report,capacity);
    if(mp_kind(verb))return mp_execute(cmd,model,report,capacity);
    if(sf_kind(verb))return sf_execute(cmd,model,report,capacity);
    if(am_kind(verb))return am_execute(cmd,model,report,capacity);
    if(md_kind(verb))return md_execute(cmd,model,report,capacity);
    if(mc_is_command(verb))return mc_execute(cmd,model,report,capacity);
    if(refinement_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)==1)return rf_execute(cmd,model,report,capacity);
    if(ad_kind(verb))return ad_execute(cmd,model,report,capacity);
    if(as_edit_kind(verb))return as_execute(cmd,model,report,capacity);
    if(sx_kind(verb)>=SX_SYMMETRIC&&sx_kind(verb)<=SX_THIN)return sd_create(cmd,model,report,capacity);
    if(sx_kind(verb)==SX_VARIABLE)return sx_variable(cmd,model,report,capacity);
    if(!wcscmp(verb,L"SOLID_SWEEP"))return sd_sweep(cmd,model,report,capacity);
    if(sx_kind(verb)>=SX_PATH&&sx_kind(verb)<=SX_CUT)return sx_sweep(cmd,model,report,capacity);
    if(sx_kind(verb)==SX_HELIX)return sx_helix(cmd,model,report,capacity);
    if(sx_kind(verb)==SX_LOFT)return sx_loft(cmd,model,report,capacity);
    if(!wcscmp(verb,L"SOLID_BLEND"))return sd_blend(cmd,model,report,capacity);
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
