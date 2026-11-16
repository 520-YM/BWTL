typedef struct {
    int checked;
    int ambiguous;
    int material_rays;
    int different;
    int topology;
    double max_shift;
} QualityComparison;

static void qa_reflect(ProVector vector,const ProVector normal,double offset){
    double amount=2.0*(ad_dot(vector,normal)-offset);
    for(int i=0;i<3;i++)vector[i]-=amount*normal[i];
}

typedef int (*QualityCrossingMatch)(ProMdl,ProMdl,double,double,const ProRay *,const QualityCrossing *);

static int qa_crossings_match(QualityCrossingMatch match,ProMdl first,ProMdl second,
                              double scale_a,double scale_b,const ProRay *ray,const QualityRay *hits){
    for(int i=0;i<hits->count;i++)
        if(!match(first,second,scale_a,scale_b,ray,&hits->crossing[i]))return 0;
    return 1;
}

static ProError qa_compare_rays_with_check(ProMdl first,ProMdl second,double scale_a,double scale_b,
    ProVector lower,ProVector upper,const ProVector mirror,double offset,
    int grid,double tolerance,QualityComparison *comparison,QualityCrossingMatch match){
    memset(comparison,0,sizeof(*comparison));
    for(int axis=0;axis<3;axis++){
        int u=(axis+1)%3,v=(axis+2)%3;
        for(int i=0;i<grid;i++)for(int j=0;j<grid;j++){
            ProRay a={0},b={0};
            QualityRay hits_a,hits_b;
            ProVector origin={0};
            origin[axis]=lower[axis]-fmax(1.0,0.05*(upper[axis]-lower[axis]));
            origin[u]=lower[u]+(upper[u]-lower[u])*(i+0.5)/grid;
            origin[v]=lower[v]+(upper[v]-lower[v])*(j+0.5)/grid;
            for(int k=0;k<3;k++)a.start_point[k]=origin[k]/scale_a;
            a.dir_vector[axis]=1;
            b.dir_vector[axis]=1;
            if(mirror){
                qa_reflect(origin,mirror,offset);
                qa_reflect(b.dir_vector,mirror,0);
            }
            for(int k=0;k<3;k++)b.start_point[k]=origin[k]/scale_b;
            ProError ea=qa_ray(first,&a,1e-8/scale_a,&hits_a);
            ProError eb=qa_ray(second,&b,1e-8/scale_b,&hits_b);
            if(ea&&ea!=PRO_TK_INCOMPLETE)return ea;
            if(eb&&eb!=PRO_TK_INCOMPLETE)return eb;
            if(ea||eb){comparison->ambiguous++;continue;}
            comparison->checked++;
            if(hits_a.count||hits_b.count)comparison->material_rays++;
            if(hits_a.count!=hits_b.count){
                if(match&&qa_crossings_match(match,first,second,scale_a,scale_b,&a,&hits_a)&&
                   qa_crossings_match(match,second,first,scale_b,scale_a,&b,&hits_b))continue;
                comparison->different++;
                comparison->topology++;
                continue;
            }
            double shift=0;
            for(int k=0;k<hits_a.count;k++)
                shift=fmax(shift,fabs(hits_a.crossing[k].depth*scale_a-hits_b.crossing[k].depth*scale_b));
            comparison->max_shift=fmax(comparison->max_shift,shift);
            if(!qa_at_least(tolerance,shift))comparison->different++;
        }
    }
    return PRO_TK_NO_ERROR;
}

static ProError qa_compare_rays(ProMdl first,ProMdl second,double scale_a,double scale_b,
    ProVector lower,ProVector upper,const ProVector mirror,double offset,
    int grid,double tolerance,QualityComparison *comparison){
    return qa_compare_rays_with_check(first,second,scale_a,scale_b,lower,upper,mirror,offset,
        grid,tolerance,comparison,NULL);
}

static void qa_comparison_report(PartText *out,const QualityComparison *result,double tolerance,int additional_difference){
    const wchar_t *judgment=L"采样范围内未发现超出容差的差异";
    if(result->different||additional_difference)judgment=L"检测到几何差异";
    else if(!result->material_rays||result->ambiguous)judgment=L"有效覆盖不足，结果未定";
    part_text_add(out,L"%ls。容差 %.9g mm。\n"
        L"有效射线 %d（其中穿过实体 %d），未判定 %d，差异 %d（穿透段数不同 %d）。"
        L"交点数量相同的射线最大轴向偏差 %.9g mm。\n",
        judgment,tolerance,result->checked,result->material_rays,result->ambiguous,
        result->different,result->topology,result->max_shift);
}

static ProError qa_symmetry(ProMdl model,const PartCommand *cmd,double scale,PartText *out){
    FdRef plane={0};
    ProSolidBody body;
    ProOutline bounds;
    ProVector point,normal,lower,upper,center,mirror_center;
    ProMassProperty mass;
    QualityComparison comparison;
    ProError error;
    if((error=qa_single_body(model,&body))||
       (error=fd_ref_resolve(model,cmd->arg[2],PRO_SURFACE,&plane))||
       (error=fd_plane_data(&plane.item,point,normal))||
       (error=ProSolidBodyOutlineGet(&body,bounds))||
       (error=qa_geometry_mass(model,&mass)))goto done;
    if(!ad_unit(normal)){error=PRO_TK_BAD_CONTEXT;goto done;}
    double offset=scale*ad_dot(point,normal);
    double tolerance=wcstod(cmd->arg[3],NULL);
    int grid;
    part_integer(cmd->arg[4],&grid);
    for(int i=0;i<3;i++){
        lower[i]=bounds[0][i]*scale;
        upper[i]=bounds[1][i]*scale;
        center[i]=mirror_center[i]=mass.center_of_gravity[i]*scale;
    }
    for(int corner=0;corner<8;corner++){
        ProVector reflected;
        for(int i=0;i<3;i++)reflected[i]=bounds[(corner>>i)&1][i]*scale;
        qa_reflect(reflected,normal,offset);
        for(int i=0;i<3;i++){
            lower[i]=fmin(lower[i],reflected[i]);
            upper[i]=fmax(upper[i],reflected[i]);
        }
    }
    qa_reflect(mirror_center,normal,offset);
    for(int i=0;i<3;i++)mirror_center[i]-=center[i];
    double center_shift=sqrt(ad_dot(mirror_center,mirror_center));
    error=qa_compare_rays(model,model,scale,scale,lower,upper,normal,offset,grid,tolerance,&comparison);
    if(!error){
        part_text_add(out,L"镜像平面 %ls，XYZ 三向各 %d×%d 射线。原始与镜像几何形心间距 %.9g mm。\n",
            cmd->arg[2],grid,grid,center_shift);
        qa_comparison_report(out,&comparison,tolerance,!qa_at_least(tolerance,center_shift));
    }
done:
    fd_ref_free(&plane);
    return error;
}
