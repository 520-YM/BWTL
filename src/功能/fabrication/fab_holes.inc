static ProError fab_holes(ProMdl model,const PartCommand *cmd,PartText *out){
    OrganizeView view={model,0};
    ProCsysdata frame={0};
    frame.x_vector[0]=frame.y_vector[1]=frame.z_vector[2]=1;
    ProError error=qa_single_body(model,NULL);
    if(!error)error=qa_mm_per_unit(model,&view.mm);
    if(!error&&wcscmp(cmd->arg[3],L"MODEL"))error=in_box_frame(model,cmd->arg[3],&frame);
    DeliveryBores *scan=NULL;
    if(!error)error=org_bores(&view,&scan);
    if(!error&&scan->limited)error=PRO_TK_OUT_OF_RANGE;
    DeliveryOutput work={0};
    FILE *file=NULL;
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,L"hole-operations.csv",&file);
    int rows=0,partial=0;
    if(!error){
        fwprintf(file,L"参考坐标系,孔壁面ID,直径_mm,圆柱段长度_mm,AX_mm,AY_mm,AZ_mm,BX_mm,BY_mm,BZ_mm,孔段类型,工艺复核\n");
        for(int i=0;i<scan->count;i++){
            const DeliveryBore *b=&scan->bores[i];
            if(!b->classification){partial++;continue;}
            ProVector p,q,a,c;
            cm_point(b->centerline,b->axis,b->low,p);
            cm_point(b->centerline,b->axis,b->high,q);
            org_coordinates(p,&frame,view.mm,a);org_coordinates(q,&frame,view.mm,c);
            if(!dv_csv_text(file,cmd->arg[3]))error=PRO_TK_CANT_WRITE;
            fwprintf(file,L",%d,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%ls,%ls\n",
                b->faces[0],2*b->radius*view.mm,(b->high-b->low)*view.mm,
                a[0],a[1],a[2],c[0],c[1],c[2],org_bore_class(b->classification),
                b->classification==2?L"通孔工艺候选":b->classification==3?L"平底加工工艺候选":L"核对入口及底型");
            rows++;
        }
    }
    free(scan);
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"已列出 %d 个完整圆柱孔段。局部内凹面 %d 组单独排除。螺纹、刀具和加工深度需结合图纸核对。\n%ls\\hole-operations.csv",rows,partial,work.target);
    return error;
}
