static ProError as_spline(ProSection section,const PartCommand *cmd,int first,double scale,int *id){
    Pro2dPnt points[8];
    Pro2dSplinedef def={0};
    int i,j,n=(cmd->count-first-1)/2;
    for(i=0;i<n;i++){
        points[i][0]=as_arg(cmd,first+1+2*i)*scale;points[i][1]=as_arg(cmd,first+2+2*i)*scale;
        for(j=0;j<i;j++)if(as_dist(as_p(points[i]),as_p(points[j]))<0.001*scale)return PRO_TK_BAD_INPUTS;
    }
    if(!wcscmp(cmd->arg[first],L"CONTROL")){
        int degree=n>3?3:n-1;
        double knots[12];
        for(i=0;i<n+degree+1;i++)knots[i]=i<=degree?0:i>=n?1:(double)(i-degree)/(n-degree);
        return as_add_bspline(section,degree,points,n,knots,NULL,id);
    }
    def.type=PRO_2D_SPLINE;def.tangency_type=PRO_2D_SPLINE_TAN_NONE;def.n_points=n;def.point_arr=points;
    return ProSectionEntityAdd(section,(Pro2dEntdef *)&def,id);
}
