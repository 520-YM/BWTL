/* Native fast path for one selected pressing face and one or more hole walls. */
static ProError fastener_bso_selection(ProMdl assembly,double mm,AxHardwareSite sites[12],int *count,PartText *out){
    ProSelection *selection=NULL;
    int selected=0,face=0,holes[12];
    AxComponent panel={0};
    wchar_t key[AX_PATH_DEPTH*12+4]=L"";
    ProError error=ProSelbufferSelectionsGet(&selection);
    if(!error)error=ProArraySizeGet((ProArray)selection,&selected);
    if(!error&&(selected<2||selected>13))error=PRO_TK_BAD_INPUTS;
    for(int i=0;!error&&i<selected;i++){
        ProModelitem item;
        ProAsmcomppath path;
        wchar_t occurrence[AX_PATH_DEPTH*12+4];
        AxGeometry shape;
        error=ProSelectionModelitemGet(selection[i],&item);
        if(!error)error=ProSelectionAsmcomppathGet(selection[i],&path);
        if(!error&&(item.type!=PRO_SURFACE||path.owner!=(ProSolid)assembly||path.table_num<1||
            path.table_num>AX_PATH_DEPTH||!ax_path_label(path.comp_id_table,path.table_num,occurrence,_countof(occurrence))))
            error=PRO_TK_BAD_CONTEXT;
        if(error)break;
        if(!key[0]){
            wcscpy_s(key,_countof(key),occurrence);
            error=ax_hw_occurrence(assembly,key,mm,&panel);
        }
        if(!error&&(item.owner!=panel.model||wcscmp(key,occurrence)))error=PRO_TK_BAD_CONTEXT;
        if(!error)error=ax_geometry(&item,&shape);
        if(error)break;
        if(shape.type==AX_GEOM_PLANE&&!face)face=item.id;
        else if(shape.type==AX_GEOM_CYLINDER&&*count<12)holes[(*count)++]=item.id;
        else error=PRO_TK_BAD_INPUTS;
    }
    if(selection)ProSelectionarrayFree(selection);
    if(!error&&(!face||!*count))error=PRO_TK_BAD_INPUTS;
    if(error){
        part_text_add(out,L"请在当前装配里用 Ctrl 同时选中同一块板的压铆头贴合面和 1–12 个圆孔的孔壁，再发送装配指令。");
        return error;
    }
    wchar_t plane[32],hole[32];
    swprintf_s(plane,_countof(plane),L"SURFACE:%d",face);
    for(int i=0;i<*count;i++){
        double start,end;
        swprintf_s(hole,_countof(hole),L"SURFACE:%d",holes[i]);
        error=ax_hw_site(&panel,hole,plane,mm,&sites[i]);
        if(!error)error=ax_hw_world(&panel,&sites[i]);
        if(!error){
            ax_hw_axial_span(&sites[i],sites[i].contact,sites[i].plane.direction,mm,&start,&end);
            if(!sites[i].bore||start>=-0.001||end>0.001)error=PRO_TK_BAD_CONTEXT;
        }
        if(error){
            part_text_add(out,L"第 %d 个选中孔壁与安装面不构成可核对的孔口。",i+1);
            return error;
        }
        if(!part_close(sites[i].cylinder.radius*2*mm,5.4)){
            part_text_add(out,L"第 %d 个底孔读回直径 %.6g mm，本库件定位用的底孔为 Ø5.4 mm。",i+1,sites[i].cylinder.radius*2*mm);
            return PRO_TK_BAD_CONTEXT;
        }
        for(int j=0;j<i;j++)if(ad_near(sites[j].contact,sites[i].contact,0.001/mm)){
            part_text_add(out,L"有两个选面落在同一个孔口，请每个孔只选一面。");
            return PRO_TK_E_FOUND;
        }
    }
    return 0;
}

static ProError fastener_bso_place(ProMdl assembly,ProMdl part,AxHardwareSite sites[12],int count,double mm,PartText *out){
    AxComponent component={0};
    AxHardwareSite fitting={0};
    AxBatchItem items[12]={0};
    double units=0,start=0,end=0;
    component.model=part;
    ProError error=qa_mm_per_unit(part,&units);
    /* The published M3×6 family member uses this head face and relief cylinder.
       Read geometry each time so an incompatible library revision stops here. */
    if(!error)error=ax_hw_site(&component,L"SURFACE:139",L"SURFACE:90",units,&fitting);
    if(!error)ax_hw_axial_span(&fitting,fitting.contact,fitting.plane.direction,units,&start,&end);
    if(error||!part_close(units,mm)||fitting.bore||!part_close(fitting.cylinder.radius*units,2.5)||
       !part_close(start,0)||!part_close(end,0.3)||!part_close(fitting.contact[2]*units,0.5)){
        part_text_add(out,L"库件的压铆肩和定位轴无法按当前模型版本复核。");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    for(int i=0;i<count;i++){
        items[i].model=part;
        ax_hw_seat_pose(&fitting,&sites[i],items[i].pose);
    }
    return ax_batch_create(assembly,items,count,mm,1,out);
}
