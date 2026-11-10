static ProError as_ellipse(ProSection section,const PartCommand *cmd,int first,double scale,int *id){
    const int corner[9][2]={{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1},{1,0}};
    const double knots[12]={0,0,0,0.25,0.25,0.5,0.5,0.75,0.75,1,1,1};
    const double w=0.7071067811865475244,weights[9]={1,w,1,w,1,w,1,w,1};
    Pro2dPnt points[9];
    AsPoint center=as_point(as_arg(cmd,first)*scale,as_arg(cmd,first+1)*scale);
    AsPoint direction=as_ray(as_arg(cmd,first+4)*AS_PI/180);
    AsPoint major=as_mul(direction,as_arg(cmd,first+2)*scale/2);
    AsPoint minor=as_mul(as_left(direction),as_arg(cmd,first+3)*scale/2);
    int i;
    /* Creo 10 evaluates rational section control coordinates in weighted form. */
    for(i=0;i<9;i++){
        AsPoint point=as_plus(center,as_plus(as_mul(major,corner[i][0]),as_mul(minor,corner[i][1])));
        as_xy(points[i],as_mul(point,weights[i]));
    }
    return as_add_bspline(section,2,points,9,knots,weights,id);
}
