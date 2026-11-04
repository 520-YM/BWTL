static ProError template_empty(ProSolid solid) {
    ProSolidBody *bodies = NULL;
    ProError error = ProSolidBodiesCollect(solid, &bodies);
    int count = 0, i;
    if (error != PRO_TK_NO_ERROR) return error;
    if (bodies) error = ProArraySizeGet((ProArray)bodies, &count);
    for (i = 0; error == PRO_TK_NO_ERROR && i < count; ++i) {
        ProSolidBodyState state;
        error = ProSolidBodyStateGet(&bodies[i], &state);
        if (error == PRO_TK_NO_ERROR && state == PRO_BODY_STATE_ACTIVE) error = PRO_TK_BAD_CONTEXT;
    }
    if (bodies) ProArrayFree((ProArray *)&bodies);
    return error;
}
