#include <io.h>
#include <fcntl.h>
static ProError sh_count_bends(ProSurface surface,ProError status,ProAppData data){
    ShBendInfo *s=data;ProSmtSurfType type;double angle;if(status)return status;
    if(!ProSmtSurfaceTypeGet((ProPart)s->model,surface,&type)&&type==PRO_SMT_SURF_FACE&&!ProSmtBendsrfInfoGet((ProPart)s->model,surface,NULL,NULL,&angle,NULL,NULL)&&fabs(angle)>.01){ProSrftype shape;s->count++;if(!ProSurfaceTypeGet(surface,&shape)&&shape!=PRO_SRF_PLANE)s->inside++;}return 0;
}
static ProError sh_export(const PartCommand *cmd,ProMdl source,wchar_t *report,size_t capacity){
    ProPath directory,final,temp;ProMdl copy=NULL;ProMdlName copy_name;ProWVerstamp before=NULL,after=NULL;ProError e;FdTxn tx={0};FdRef fixed={0};
    ShDxf *s=NULL;ShBendInfo bends={0};ProMassProperty source_mass,mass;HANDLE handle=INVALID_HANDLE_VALUE;FILE *file=NULL;int fd=-1,temp_owned=0,entities=0;
#define DX(call) do{if((e=(call)))goto done;}while(0)
    DX(ProDirectoryCurrentGet(directory));if(!part_target_available(source,cmd->arg[2],L"dxf"))return PRO_TK_E_FOUND;
    if(swprintf_s(final,PRO_PATH_SIZE,L"%ls\\%ls.dxf",directory,cmd->arg[2])<0||swprintf_s(temp,PRO_PATH_SIZE,L"%ls\\.bwtl_%08lx_%08lx.tmp",directory,GetCurrentProcessId(),GetTickCount())<0)return PRO_TK_BAD_INPUTS;
    DX(ProMdlVerstampGet(source,&before));DX(ProSolidMassPropertyWithDensityGet((ProSolid)source,NULL,PRO_MP_DENS_USE_ALWAYS,1,&source_mass));
    swprintf_s(copy_name,PRO_MDLNAME_SIZE,L"bwtl_flat_%08lx_%08lx",GetCurrentProcessId(),GetTickCount());DX(ProMdlnameCopy(source,copy_name,&copy));
    DX(sh_begin(&tx,copy,L"BWTL_EXPORT_FLAT",PRO_FEAT_FLAT_PAT));bends.model=copy;DX(ProSolidSurfaceVisit((ProSolid)copy,sh_count_bends,NULL,&bends));
    if(bends.inside){DX(sh_flatten(&tx,cmd->arg[3],0));DX(part_regenerate(copy));DX(part_health(copy));}
    DX(md_solid_face(&tx,cmd->arg[3],&fixed));
    s=calloc(1,sizeof(*s));if(!s){e=PRO_TK_OUT_OF_MEMORY;goto done;}s->model=copy;s->scale=tx.scale;DX(fd_plane_data(&fixed.item,s->origin,s->normal));
    {ProVector seed={1,0,0};if(fabs(s->normal[0])>.9){seed[0]=0;seed[1]=1;}
     double dot=ad_dot(seed,s->normal);for(int j=0;j<3;j++)s->u[j]=seed[j]-dot*s->normal[j];if(!ad_unit(s->u)){e=PRO_TK_BAD_CONTEXT;goto done;}ad_cross(s->normal,s->u,s->v);}
    DX(ProSolidSurfaceVisit((ProSolid)copy,sh_dxf_surface,NULL,s));if(!s->faces||!s->count||s->bend_count!=bends.count){e=PRO_TK_BAD_CONTEXT;goto done;}
    handle=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(handle==INVALID_HANDLE_VALUE){e=PRO_TK_CANT_WRITE;goto done;}temp_owned=1;
    fd=_open_osfhandle((intptr_t)handle,_O_TEXT);if(fd<0){e=PRO_TK_CANT_WRITE;goto done;}handle=INVALID_HANDLE_VALUE;
    file=_fdopen(fd,"w");if(!file){_close(fd);e=PRO_TK_CANT_WRITE;goto done;}
    fprintf(file,"0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n70\n3\n");
    {const char *layers[]={"OUTLINE","HOLES","BEND"};for(int i=0;i<3;i++)fprintf(file,"0\nLAYER\n2\n%s\n70\n0\n62\n%d\n6\nCONTINUOUS\n",layers[i],i+1);}
    fprintf(file,"0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n");
    for(int i=0;i<s->count;i++)if(s->edges[i].seen==1){DX(sh_dxf_write_edge(file,s,&s->edges[i]));entities++;}
    for(int i=0;i<s->bend_count;i++){sh_dxf_line(file,"BEND",s->bends[i].a,s->bends[i].b);entities++;}
    fprintf(file,"0\nENDSEC\n0\nEOF\n");if(ferror(file)||fflush(file)||_commit(_fileno(file))){e=PRO_TK_CANT_WRITE;goto done;}
    if(fclose(file)){file=NULL;e=PRO_TK_CANT_WRITE;goto done;}file=NULL;
    DX(ProMdlVerstampGet(source,&after));DX(ProSolidMassPropertyWithDensityGet((ProSolid)source,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass));
    if(!ProVerstampEqual(before,after)||!part_close(source_mass.volume,mass.volume)){e=PRO_TK_GENERAL_ERROR;goto done;}
    fd_ref_free(&fixed);fd_cleanup(&tx,0);DX(ProMdlErase(copy));copy=NULL;
    if(!MoveFileExW(temp,final,MOVEFILE_WRITE_THROUGH)){e=PRO_TK_CANT_WRITE;goto done;}temp_owned=0;
    swprintf_s(report,capacity,L"已导出下料 DXF：%ls。毫米单位，OUTLINE／HOLES／BEND 分层，共%d个图元、%d条真实展开折弯中线。原零件未修改、未保存，临时展开副本已清理。",final,entities,s->bend_count);
done:
    if(file)fclose(file);if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);if(temp_owned)DeleteFileW(temp);
    fd_ref_free(&fixed);if(tx.root)fd_cleanup(&tx,0);if(copy){ProError erase=ProMdlErase(copy);trace("erase export copy",erase);if(erase)sh_recovery_uncertain=1;}
    if(s)free(s);if(before)ProVerstampFree(&before);if(after)ProVerstampFree(&after);return e;
#undef DX
}
