
        FdRef refs[2]={{0}};ProVector p,n,q,m;ProModelitem existing;
        if(!ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)cmd->arg[2],&existing))return PRO_TK_E_FOUND;
        e=fd_ref_resolve(model,cmd->arg[3],PRO_SURFACE,&refs[0]);if(!e)e=fd_ref_resolve(model,cmd->arg[4],PRO_SURFACE,&refs[1]);
        if(!e)e=fd_plane_data(&refs[0].item,p,n);if(!e)e=fd_plane_data(&refs[1].item,q,m);
        if(!e&&fabs(n[0]*m[0]+n[1]*m[1]+n[2]*m[2])>1-1e-9)e=PRO_TK_BAD_INPUTS;
        fd_ref_free(&refs[0]);fd_ref_free(&refs[1]);if(e)return e;
        if(fd_draft.stamp)ProVerstampFree(&fd_draft.stamp);memset(&fd_draft,0,sizeof(fd_draft));
        e=ProMdlVerstampGet(model,&fd_draft.stamp);if(e)return e;fd_draft.model=model;
        wcscpy_s(fd_draft.name,PRO_NAME_SIZE,cmd->arg[2]);wcscpy_s(fd_draft.plane,256,cmd->arg[3]);wcscpy_s(fd_draft.orient,256,cmd->arg[4]);
        swprintf_s(report,capacity,L"已准备草绘 %ls，平面%ls、方向参照%ls。尚未创建空特征；收到第一组图元后会原子创建并验证关联草绘。",cmd->arg[2],cmd->arg[3],cmd->arg[4]);return 0;
    
