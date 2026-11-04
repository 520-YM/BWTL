static ProError model_circle(ProSection section, double x, double y, double radius, int *entity_id) {
    Pro2dCircledef circle;
    int id;
    memset(&circle, 0, sizeof(circle));
    circle.type = PRO_2D_CIRCLE; circle.center[0] = x; circle.center[1] = y; circle.radius = radius;
    ProError error = ProSectionEntityAdd(section, (Pro2dEntdef *)&circle, &id);
    if (error == PRO_TK_NO_ERROR && entity_id) *entity_id = id;
    return error;
}
