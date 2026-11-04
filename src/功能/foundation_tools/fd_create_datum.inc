static ProError fd_create_datum(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};FdRef ref[2]={{0}};ProElement array=NULL,entry=NULL,el=NULL;ProError e;ProFeattype type;
    ProGeomitem created_item;int i;double v[6]={0},length,scale;ProVector p0,n0,p1,n1;wchar_t equation[1024];
    type=!wcscmp(cmd->arg[0],L"DATUM_PLANE")?PRO_FEAT_DATUM:!wcscmp(cmd->arg[0],L"DATUM_AXIS")?PRO_FEAT_DATUM_AXIS:!wcscmp(cmd->arg[0],L"DATUM_POINT")?PRO_FEAT_DATUM_POINT:!wcscmp(cmd->arg[0],L"DATUM_CSYS")?PRO_FEAT_CSYS:PRO_FEAT_CURVE;
    FD_TRY("prepare datum transaction",fd_begin(&tx,model,cmd->arg[2],type));scale=tx.scale;
    if(type==PRO_FEAT_DATUM){
#include "datum_plane.inc"
}else if(type==PRO_FEAT_DATUM_AXIS){
#include "datum_axis.inc"
}else{
        FD_TRY("resolve coordinate system",fd_ref_resolve(model,cmd->arg[3],PRO_CSYS,&ref[0]));
        for(i=0;i<(type==PRO_FEAT_DATUM_POINT?3:6);i++)part_number(cmd->arg[4+i],&v[i]);
        if(type==PRO_FEAT_DATUM_POINT){
#include "datum_point.inc"
}else if(type==PRO_FEAT_CSYS){
#include "datum_csys.inc"
}else{
#include "datum_curve.inc"
}
    }
    FD_TRY("create datum feature",fd_create(&tx,0));FD_TRY("verify datum state and parents",fd_verify(&tx));
    FD_TRY("verify created datum geometry",fd_feature_geom(&tx.feature,type==PRO_FEAT_DATUM?PRO_SURFACE:type==PRO_FEAT_DATUM_AXIS?PRO_AXIS:type==PRO_FEAT_DATUM_POINT?PRO_POINT:type==PRO_FEAT_CSYS?PRO_CSYS:PRO_CURVE,&created_item));
    if(type==PRO_FEAT_DATUM){
        double offset;FD_TRY("read created plane",fd_plane_data(&created_item,p1,n1));offset=(p1[0]-p0[0])*n0[0]+(p1[1]-p0[1])*n0[1]+(p1[2]-p0[2])*n0[2];
        if(!part_close(offset,v[0]*scale)){tx.stage="verify signed plane offset";e=PRO_TK_GENERAL_ERROR;goto done;}
    }
    swprintf_s(report,capacity,L"已在当前零件创建%ls：%ls。特征、参照父子关系及重生成检查通过，实体体积保持，未保存。",type==PRO_FEAT_DATUM?L"偏移基准面":type==PRO_FEAT_DATUM_AXIS?L"基准轴":type==PRO_FEAT_DATUM_POINT?L"基准点":type==PRO_FEAT_CSYS?L"坐标系":L"直线基准曲线",cmd->arg[2]);
done:
    fd_cleanup(&tx,e);fd_ref_free(&ref[0]);fd_ref_free(&ref[1]);
    if(e)swprintf_s(report,capacity,L"基准操作未完成：%hs，错误码%d。%ls；未保存。",tx.stage,e,tx.rollback_ok?L"未创建或已核对移除本次新增特征":L"回滚未能验证，请检查当前零件");
    return e;
}
