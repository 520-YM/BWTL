/* Rebuild into the sheet-metal component already created inside the new ASM. */
static ProError cad_rebuild_bracket(ProMdl source, ProMdl target,
                                    ImitationReplay *replay, PartText *out)
{
    if (!source || !target || source == target || !replay || !out)
        return PRO_TK_BAD_INPUTS;

    *replay = (ImitationReplay){0};
    replay->source = source;
    replay->target = target;
    replay->out = out;
    replay->keep_prefix = 1;

    ReplayFailure failure = {0};
    ImitationShapeMetrics original_shape = {0};
    ProMdlsubtype source_subtype, target_subtype;
    ProAccuracyType accuracy_type;
    ProPath stage;
    double source_units, target_units, accuracy;
    int conversion = 0;
    ProError error;
    const wchar_t *operation;

#define CAD_REBUILD_STEP(call, label) do { \
    operation = label; \
    error = (call); \
    if (error) goto done; \
} while (0)

    CAD_REBUILD_STEP(ProMdlSubtypeGet(source, &source_subtype), L"读取原件类型");
    CAD_REBUILD_STEP(ProMdlSubtypeGet(target, &target_subtype), L"读取新建件类型");
    if (source_subtype != PROMDLSTYPE_PART_SHEETMETAL ||
        target_subtype != PROMDLSTYPE_PART_SHEETMETAL) {
        error = PRO_TK_BAD_CONTEXT;
        operation = L"核对原件和新件均为原生钣金";
        goto done;
    }
    CAD_REBUILD_STEP(template_empty((ProSolid)target), L"检查新件仍为空白");
    CAD_REBUILD_STEP(ProDirectoryCurrentGet(stage), L"读取 Creo 工作目录");
    if (!storage_directory_on_drive(stage,storage_data_drive())) {
        error = PRO_TK_BAD_CONTEXT;
        operation = L"检查 Creo 工作目录位于所选数据盘";
        goto done;
    }
    CAD_REBUILD_STEP(replay_source_dependencies(source, &replay->external_total),
                     L"核对原件参照");
    CAD_REBUILD_STEP(imitation_shape_read(source, &original_shape), L"读取原件几何");
    error = ProSolidFeatVisit((ProSolid)source, replay_find_conversion, NULL, &conversion);
    if (error != PRO_TK_NO_ERROR && error != PRO_TK_USER_ABORT &&
        error != PRO_TK_E_NOT_FOUND) {
        operation = L"检查原件是否包含实体转钣金";
        goto done;
    }
    if (conversion) {
        error = PRO_TK_UNSUPPORTED;
        operation = L"原件首建步骤包含实体转钣金，无法在已有的空钣金件上重做该转换";
        goto done;
    }
    CAD_REBUILD_STEP(ProSolidFeatVisit((ProSolid)source, replay_capture_extend, NULL, replay),
                     L"读取上游特征");
    CAD_REBUILD_STEP(qa_mm_per_unit(source, &source_units), L"读取原件单位");
    CAD_REBUILD_STEP(qa_mm_per_unit(target, &target_units), L"读取新件单位");
    if (!imitation_metric_same(source_units, target_units)) {
        error = PRO_TK_UNSUPPORTED;
        operation = L"核对原件和空钣金模板使用相同单位";
        goto done;
    }
    CAD_REBUILD_STEP(ProSolidAccuracyGet((ProSolid)source, &accuracy_type, &accuracy),
                     L"读取原件公差设置");
    CAD_REBUILD_STEP(ProSolidAccuracySet((ProSolid)target, accuracy_type, accuracy),
                     L"应用原件公差设置");

    replay->failure = &failure;
    CAD_REBUILD_STEP(ProSolidFeatVisit((ProSolid)source, replay_feature, NULL, replay),
                     L"逐步重建首壁、翻边和切口");
    if (replay->local_group.count)
        CAD_REBUILD_STEP(replay_local_group_finish(replay), L"收尾特征组");
    CAD_REBUILD_STEP(replay_relations(replay), L"重建特征参数关系");
    CAD_REBUILD_STEP(replay_family_read(source, target, out), L"核对族表");

    if (replay->external_total) {
        int used = 0;
        for (int i = 0; i < replay->external_total; ++i)
            used += replay->external_used[i] != 0;
        part_text_add(out, L"已在新件重建 %d / %d 条外部参照。\n",
                      used, replay->external_total);
        if (used != replay->external_total) {
            error = PRO_TK_INCOMPLETE;
            operation = L"核对所有已使用的源参照";
            goto done;
        }
    }
    if (!replay->steps) {
        error = PRO_TK_EMPTY;
        operation = L"核对实际建模步骤";
        goto done;
    }

    CAD_REBUILD_STEP(imitation_material_replay(source, target, source_subtype, stage, out),
                     L"应用原件材料");
    CAD_REBUILD_STEP(replay_regenerate_model_relations(target), L"执行再生");
    CAD_REBUILD_STEP(imitation_bend_table_replay(source, target, source_subtype, out),
                     L"应用折弯参数");
    CAD_REBUILD_STEP(dv_no_external_dependencies(target), L"核对新件独立参照");
    CAD_REBUILD_STEP(imitation_shape_verify_mode(source, target, &original_shape, 0, out),
                     L"核对板形和孔位");
    CAD_REBUILD_STEP(replay_verify_order_impl(source, target, 1, NULL, out),
                     L"核对特征顺序、内嵌草绘和阵列");
    CAD_REBUILD_STEP(imitation_sheet_compare(source, target, out), L"核对板厚和折弯");
    CAD_REBUILD_STEP(imitation_material_verify(source, target, source_subtype),
                     L"核对材料");
    part_text_add(out, L"已按原件特征树完成 %d 道原生建模步骤。\n", replay->steps);

done:
    replay->failure = NULL;
    if (error) {
        part_text_add(out, L"%ls失败（Creo %d），新件已完成 %d 步。\n",
                      operation, error, replay->completed);
        if (failure.feature)
            part_text_add(out, L"源特征 %d，第 %d 步，%ls %ls。\n",
                          failure.feature, failure.step,
                          failure.operation ? failure.operation : L"", failure.label);
    }
#undef CAD_REBUILD_STEP
    return error;
}
