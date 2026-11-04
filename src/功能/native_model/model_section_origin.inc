static ProError model_section_origin(ProSection section) {
    ProIntlist ids = NULL; int count = 0, i, found = 0, id;
    ProError error = ProSectionEntityIdsGet(section, &ids, &count);
    if (error != PRO_TK_NO_ERROR && error != PRO_TK_E_NOT_FOUND) return error;
    for (i = 0; i < count; ++i) {
        Pro2dEntdef *entity = NULL;
        error = ProSectionEntityGet(section, ids[i], &entity);
        if (error != PRO_TK_NO_ERROR) break;
        if (entity->type == PRO_2D_COORD_SYS) found = 1;
    }
    if (ids) ProArrayFree((ProArray *)&ids);
    if (error != PRO_TK_NO_ERROR && error != PRO_TK_E_NOT_FOUND) return error;
    if (!found) {
        Pro2dCoordSysdef origin = {0};
        origin.type = PRO_2D_COORD_SYS;
        return ProSectionEntityAdd(section, (Pro2dEntdef *)&origin, &id);
    }
    return PRO_TK_NO_ERROR;
}
