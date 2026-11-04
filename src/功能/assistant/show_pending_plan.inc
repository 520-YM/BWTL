static void show_pending_plan(void) {
    wchar_t summary[512];
    if (!pending_plan.ready) { append(L"当前没有待执行方案，请重新说明要新建的零件尺寸。"); return; }
    if(pending_plan.empty_kind){append(pending_plan.empty_kind==2?L"按当前钣金模板新建空零件…":pending_plan.empty_kind==3?L"按当前装配模板新建空装配…":L"按当前实体模板新建空零件…");return;}
    swprintf_s(summary, 512, L"本次建模：%ls；%g × %g × %g mm；中心通孔直径 %g mm。将按 Creo 当前默认实体模板直接创建。", pending_plan.cylinder ? L"圆柱（前两项为外径）" : L"矩形板/长方体", pending_plan.a, pending_plan.b, pending_plan.height, pending_plan.hole);
    append(summary);
}
