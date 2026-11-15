#include <ProDrawing.h>

static ProError smy_draw_template(ProMdl model,const PartCommand *cmd,PartText *out){
    ProPath directory,path;
    ProMdl existing=NULL;
    ProDrawing drawing=NULL;
    ProDwgcreateErrs diagnostics=NULL;
    ProMdlName name,template_name;
    ProMdlnameShortdata part={0};
    ProView *views=NULL;
    int count=0,attached=0;
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=ProDirectoryCurrentGet(directory);
    if(!error)error=smy_stock_file(directory,cmd->arg[3],L".drw",path);
    if(!error&&!part_target_available(model,cmd->arg[2],L"drw"))error=PRO_TK_E_FOUND;
    if(!error&&!ProMdlnameInit((wchar_t *)cmd->arg[2],PRO_MDL_DRAWING,&existing))error=PRO_TK_E_FOUND;
    if(!error)error=ProMdlMdlnameGet(model,part.name);
    if(!error&&wcslen(part.name)>31)error=PRO_TK_INVALID_NAME;
    if(error)return error;
    wcscpy_s(part.type,_countof(part.type),L"prt");
    wcscpy_s(name,_countof(name),cmd->arg[2]);
    wcscpy_s(template_name,_countof(template_name),cmd->arg[3]);
    error=ProDrawingFromTemplateCreate(name,template_name,&part,0,&drawing,&diagnostics);
    if(diagnostics){
        int reports;
        if(!ProDwgcreateErrsCountGet(diagnostics,&reports)&&reports>0)
            part_text_add(out,L"Creo 工程图模板报告 %d 项错误或警告。\n",reports);
        ProDwgcreateErrsFree(&diagnostics);
    }
    if(!error)error=ProDrawingViewsCollect(drawing,&views);
    if(!error)error=ProArraySizeGet((ProArray)views,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolid source=NULL;
        ProError view_error=ProDrawingViewSolidGet(drawing,views[i],&source);
        if(view_error==PRO_TK_NO_ERROR&&source==(ProSolid)model)attached++;
    }
    if(!error&&attached<3){
        part_text_add(out,L"模板只生成 %d 个关联当前钣金件的视图，要求至少三个。\n",attached);
        error=PRO_TK_BAD_CONTEXT;
    }
    if(!error){
        wchar_t t[32];
        swprintf_s(t,_countof(t),L"%.9g",thickness);
        SmxParameter attributes[]={{L"BWTL_DRAW_PART_NO",cmd->arg[4]},{L"BWTL_DRAW_REV",cmd->arg[5]},
            {L"BWTL_DRAW_MATERIAL",cmd->arg[6]},{L"BWTL_DRAW_THICKNESS_MM",t},{L"BWTL_DRAW_SOURCE",part.name}};
        error=smx_params((ProMdl)drawing,attributes,_countof(attributes),0,out);
    }
    if(views)ProArrayFree((ProArray *)&views);
    if(error&&drawing&&ProMdlErase((ProMdl)drawing)){
        dv_recovery_uncertain=1;
        part_text_add(out,L"未完成的工程图仍留在独立会话，请检查后再进行新的交付操作。\n");
    }
    if(!error)part_text_add(out,L"已在会话中由 %ls 创建 %ls，实际关联当前钣金件的视图 %d 个，已登记料号、发布版本、材料和实测板厚。未保存。\n",
        template_name,name,attached);
    return error;
}
