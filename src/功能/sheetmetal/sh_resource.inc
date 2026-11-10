static ProError sh_stock_section(int kind,ProSection *profile){
    wchar_t module[MAX_PATH],*end;ProPath path;
    if(!GetModuleFileNameW(NULL,module,MAX_PATH))return PRO_TK_GENERAL_ERROR;
    for(int i=0;i<3;i++){end=wcsrchr(module,L'\\');if(!end)return PRO_TK_BAD_CONTEXT;*end=0;}
    swprintf_s(path,PRO_PATH_SIZE,L"%ls\\text\\smt\\%ls",module,kind==0?L"flat_shapes\\rect_flat_wall.sec":kind==1?L"flange_shapes\\open_hem.sec":L"flange_shapes\\c_hem.sec");
    return ProMdlFiletypeLoad(path,PRO_MDLFILE_2DSECTION,PRO_B_FALSE,(ProMdl *)profile);
}
