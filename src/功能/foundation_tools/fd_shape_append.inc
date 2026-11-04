static ProError fd_shape_append(ProSection section,const PartCommand *cmd,int start,double scale,int ids[4],int *count){
    double v[5]={0};int n=cmd->count-start-1,i,dim;ProError e;const wchar_t *shape=cmd->arg[start];
    ProBoolean im=PRO_B_FALSE;ProSectionPointType whole=PRO_ENT_WHOLE;
    *count=0;
    if((e=ProSectionIntentManagerModeGet(section,&im)))return e;
    if(im&&(e=ProSectionIntentManagerModeSet(section,PRO_B_FALSE)))return e;
    /* A feature-owned 3D section already carries its sketch frame. A
       standalone 2D coordinate entity must not be inserted into that frame. */
    for(i=0;i<n;i++){part_number(cmd->arg[start+1+i],&v[i]);if(i<3||!wcscmp(shape,L"LINE")||!wcscmp(shape,L"CENTERLINE")||!wcscmp(shape,L"RECT"))v[i]*=scale;}
    if(!wcscmp(shape,L"LINE")||!wcscmp(shape,L"CENTERLINE")||!wcscmp(shape,L"RECT")){
        Pro2dLinedef line={0};double pts[5][2];
        line.type=PRO_2D_LINE;
        if(wcscmp(shape,L"RECT")){
#include "shape_line.inc"
}else{
#include "shape_rectangle.inc"
}
    }else if(!wcscmp(shape,L"CIRCLE")){
#include "shape_circle.inc"
}else if(!wcscmp(shape,L"ARC")){
#include "shape_arc.inc"
}else return PRO_TK_BAD_INPUTS;
    return 0;
}
