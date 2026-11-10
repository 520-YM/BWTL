static ProError qa_measure(ProMdl model,const PartCommand *cmd,double scale,PartText *out){
    const wchar_t *mode=cmd->arg[2];
    FdRef first={0},second={0};
    ProSurface surface;
    ProEdge edge;
    ProError error;
    double value;
    const wchar_t *unit=L"mm";
    if((error=fd_ref_resolve(model,cmd->arg[3],PRO_TYPE_UNUSED,&first)))goto done;
    if(cmd->count==5&&(error=fd_ref_resolve(model,cmd->arg[4],PRO_TYPE_UNUSED,&second)))goto done;

    if(!wcscmp(mode,L"LENGTH")){
        if(!(error=ProGeomitemToEdge(&first.item,&edge)))error=ProEdgeLengthEval(edge,&value);
    }else if(!wcscmp(mode,L"AREA")){
        if(!(error=ProGeomitemToSurface(&first.item,&surface)))error=ProSurfaceAreaEval(surface,&value);
        unit=L"mm²";
    }else if(!wcscmp(mode,L"DIAMETER")){
        ProSrftype type;
        if((error=ProGeomitemToSurface(&first.item,&surface))||(error=ProSurfaceTypeGet(surface,&type)))goto done;
        if(type!=PRO_SRF_CYL){error=PRO_TK_INVALID_TYPE;goto done;}
        ProUvParam uv={0,0};
        error=ProSurfaceDiameterEval(surface,uv,&value);
    }else if(!wcscmp(mode,L"ANGLE")){
        ProEdge other;
        ProEnttype a,b;
        if((error=ProGeomitemToEdge(&first.item,&edge))||(error=ProGeomitemToEdge(&second.item,&other))||
           (error=ProEdgeTypeGet(edge,&a))||(error=ProEdgeTypeGet(other,&b)))goto done;
        if(a!=PRO_ENT_LINE||b!=PRO_ENT_LINE){error=PRO_TK_INVALID_TYPE;goto done;}
        /* The acute angle between the undirected supporting lines. */
        ProVector start,finish,delta[2];
        ProEdge edges[]={edge,other};
        for(int i=0;i<2;i++){
            if((error=ProEdgeXyzdataEval(edges[i],0,start,NULL,NULL,NULL))||
               (error=ProEdgeXyzdataEval(edges[i],1,finish,NULL,NULL,NULL)))goto done;
            for(int j=0;j<3;j++)delta[i][j]=finish[j]-start[j];
            if(!ad_unit(delta[i])){error=PRO_TK_BAD_CONTEXT;goto done;}
        }
        value=acos(fabs(qa_clamp(ad_dot(delta[0],delta[1]))))*180.0/AS_PI;
        unit=L"°";
    }else{
        Pro2dPnt param_a,param_b;
        Pro3dPnt point_a,point_b;
        error=ProSelectionWithOptionsDistanceEval(first.selection,PRO_B_FALSE,second.selection,PRO_B_FALSE,
            param_a,param_b,point_a,point_b,&value);
        if(!error)
            part_text_add(out,L"最近点 A=(%.9g, %.9g, %.9g) mm，B=(%.9g, %.9g, %.9g) mm\n",
                scale*point_a[0],scale*point_a[1],scale*point_a[2],scale*point_b[0],scale*point_b[1],scale*point_b[2]);
    }
    if(!error){
        if(!wcscmp(unit,L"mm"))value*=scale;
        else if(!wcscmp(unit,L"mm²"))value*=scale*scale;
        if(!isfinite(value)||value<0){error=PRO_TK_BAD_CONTEXT;goto done;}
        part_text_add(out,L"%ls %ls%ls%ls = %.12g %ls\n",mode,cmd->arg[3],
            cmd->count==5?L" ↔ ":L"",cmd->count==5?cmd->arg[4]:L"",value,unit);
    }
done:
    fd_ref_free(&second);
    fd_ref_free(&first);
    return error;
}
