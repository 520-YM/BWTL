static void org_coordinates(const ProVector point,const ProCsysdata *frame,double scale,ProVector xyz){
    ProVector offset;
    const double *axes[]={frame->x_vector,frame->y_vector,frame->z_vector};
    ad_sub(point,frame->origin,offset);
    for(int i=0;i<3;i++)xyz[i]=ad_dot(offset,axes[i])*scale;
}

static ProError org_holes(const OrganizeView *view,const PartCommand *cmd,PartText *out){
    ProCsysdata frame={0};
    frame.x_vector[0]=frame.y_vector[1]=frame.z_vector[2]=1;
    if(wcscmp(cmd->arg[2],L"MODEL")){
        ProError error=in_box_frame(view->model,cmd->arg[2],&frame);
        if(error)return error;
    }
    DeliveryBores *scan=NULL;
    ProError error=org_bores(view,&scan);
    if(error)return error;
    int limit=(int)as_arg(cmd,3),count=0,partial=0;
    part_text_add(out,L"孔位坐标表（%ls，mm）\n孔壁｜端点 A (X,Y,Z)｜端点 B (X,Y,Z)｜直径｜孔段类型\n",
        !wcscmp(cmd->arg[2],L"MODEL")?L"模型默认 XYZ":cmd->arg[2]);
    for(int i=0;i<scan->count;i++){
        const DeliveryBore *b=&scan->bores[i];
        if(!b->classification){partial++;continue;}
        if(++count>limit)continue;
        ProVector first,last,a,c;
        cm_point(b->centerline,b->axis,b->low,first);
        cm_point(b->centerline,b->axis,b->high,last);
        org_coordinates(first,&frame,view->mm,a);
        org_coordinates(last,&frame,view->mm,c);
        part_text_add(out,L"SURFACE:%d｜(%.8g, %.8g, %.8g)｜(%.8g, %.8g, %.8g)｜Ø%.8g｜%ls\n",
            b->faces[0],a[0],a[1],a[2],c[0],c[1],c[2],2*b->radius*view->mm,org_bore_class(b->classification));
    }
    part_text_add(out,L"共核实 %d 个完整圆柱孔段，列出 %d 个。%ls",
        count,count<limit?count:limit,scan->limited?L"扫描达到上限。":L"");
    if(partial)part_text_add(out,L"%d 组局部内凹圆柱面未列入孔位表。",partial);
    free(scan);
    return PRO_TK_NO_ERROR;
}
