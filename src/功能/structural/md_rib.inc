static ProError md_rib(FdTxn *tx,const PartCommand *cmd){
    FdRef base={0},point={0};SxPath path={0};ProCurve curve;ProEnttype type;ProVector base_point,normal,start,end,direction,across;
    ProName point_name,plane_name,sketch_name;ProMdlName model;wchar_t plane_token[64],point_token[64];
    FdTxn sketch_tx={0};FdSketch sketch={0};ProMatrix frame;ProError e;int id,sketch_id=0;
    double length,width=as_arg(cmd,5)*tx->scale,height=as_arg(cmd,6)*tx->scale,angle=as_arg(cmd,7)*AS_PI/180;
    double top=width-2*height*tan(angle),embed=fmin(height/100,0.01*tx->scale),bottom=width+2*embed*tan(angle);
#define RB(call) do{if((e=(call)))goto done;}while(0)
    if(top<=0.01*tx->scale)return PRO_TK_BAD_INPUTS;
    RB(fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&base));RB(fd_plane_data(&base.item,base_point,normal));
    RB(sx_path_resolve(tx->model,cmd->arg[3]+6,tx->scale,&path));
    if(path.count!=1){e=PRO_TK_BAD_INPUTS;goto done;}
    RB(ProGeomitemToCurve(&path.items[0].item,&curve));RB(ProCurveTypeGet(curve,&type));if(type!=PRO_ENT_LINE){e=PRO_TK_BAD_INPUTS;goto done;}
    RB(ProCurveXyzdataEval(curve,0,start,NULL,NULL));RB(ProCurveXyzdataEval(curve,1,end,NULL,NULL));
    ad_sub(end,start,direction);length=sqrt(ad_dot(direction,direction));if(!ad_unit(direction)){e=PRO_TK_BAD_INPUTS;goto done;}
    {ProVector delta;ad_sub(start,base_point,delta);
     if(fabs(ad_dot(direction,normal))>1e-6||fabs(ad_dot(delta,normal))>0.001*tx->scale){e=PRO_TK_BAD_INPUTS;goto done;}}
    ad_cross(direction,normal,across);if(!ad_unit(across)){e=PRO_TK_BAD_INPUTS;goto done;}
    RB(ProMdlMdlnameGet(tx->model,model));
    swprintf_s(point_name,PRO_NAME_SIZE,L"%.23ls_PT",tx->name);swprintf_s(plane_name,PRO_NAME_SIZE,L"%.23ls_PL",tx->name);swprintf_s(sketch_name,PRO_NAME_SIZE,L"%.23ls_SK",tx->name);
    RB(am_sub(tx->model,L"CREO_OP DATUM_POINT_CURVE %ls %ls %ls START 0",model,point_name,cmd->arg[3]));
    RB(am_sub(tx->model,L"CREO_OP DATUM_PLANE_NORMAL %ls %ls %ls POINT:%ls",model,plane_name,cmd->arg[3],point_name));
    swprintf_s(plane_token,_countof(plane_token),L"PLANE:%ls",plane_name);swprintf_s(point_token,_countof(point_token),L"POINT:%ls",point_name);
    RB(fd_ref_resolve(tx->model,point_token,PRO_POINT,&point));RB(ad_point(&point,start));
    RB(fd_begin(&sketch_tx,tx->model,sketch_name,PRO_FEAT_CURVE));RB(fd_sketch_setup(&sketch_tx,plane_token,cmd->arg[4]));RB(fd_create(&sketch_tx,1));
    sketch.feature=sketch_tx.feature;sketch_id=sketch.feature.id;
    RB(ProFeatureElemtreeExtract(&sketch.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&sketch.tree));RB(fd_section_from_tree(sketch.tree,&sketch.section));RB(ProSectionLocationGet(sketch.section,frame));
    {
        AsPoint p[4];double sizes[]={-bottom/2,bottom/2,top/2,-top/2},heights[]={-embed,-embed,height,height};
        for(int i=0;i<4;i++){
            ProVector world,delta;
            for(int j=0;j<3;j++)world[j]=start[j]+sizes[i]*across[j]+heights[i]*normal[j];ad_sub(world,frame[3],delta);
            p[i]=as_point(ad_dot(delta,frame[0]),ad_dot(delta,frame[1]));
        }
        double area=0;for(int i=0;i<4;i++)area+=p[i].x*p[(i+1)%4].y-p[i].y*p[(i+1)%4].x;
        for(int i=0;i<4;i++){int a=area>0?i:3-i,b=area>0?(i+1)%4:(6-i)%4;RB(as_add_line(sketch.section,p[a],p[b],&id));}
    }
    RB(fd_section_solve(sketch.section,tx->scale));sketch_tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
    RB(ProFeatureWithoptionsRedefine(NULL,&sketch.feature,sketch.tree,sketch_tx.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&sketch_tx.errors));RB(fd_verify(&sketch_tx));
    RB(md_extrude(tx,tx->name,sketch_name,frame,direction,length/tx->scale,0));
    RB(ProModelitemByNameInit(tx->model,PRO_FEATURE,tx->name,(ProModelitem *)&tx->feature));tx->created=1;tx->parents[tx->parent_count++]=sketch_id;
    e=md_expected_volume(tx,(width-height*tan(angle))*height*length/pow(tx->scale,3));
done:fd_sketch_free(&sketch);fd_cleanup(&sketch_tx,0);fd_ref_free(&base);fd_ref_free(&point);sx_path_free(&path);return e;
#undef RB
}
