static ProError model_section(const ModelPlan *p, double scale, ProSection *section, const char **stage) {
    ProError error;
    ProWSecerror errors = NULL;
    ProBoolean intent_manager = PRO_B_FALSE;
    int i, id, edges[4] = {0}, outer_id = 0, hole_id = 0;
    double a = p->a * scale, b = p->b * scale;
    double points[5][2] = {{0,0},{1,0},{1,1},{0,1},{0,0}};
    if (!*section) return PRO_TK_BAD_INPUTS;
    SECTION_TRY("Read sketch intent-manager mode", ProSectionIntentManagerModeGet(*section, &intent_manager));
    if (intent_manager) SECTION_TRY("Disable interactive sketch solver", ProSectionIntentManagerModeSet(*section, PRO_B_FALSE));
    SECTION_TRY("Initialize sketch coordinate system", model_section_origin(*section));
    if (p->cylinder) {
        SECTION_TRY("Sketch outer circle", model_circle(*section, a/2, b/2, a/2, &outer_id));
    }
    else for (i = 0; i < 4; ++i) {
        Pro2dLinedef line;
        memset(&line, 0, sizeof(line)); line.type = PRO_2D_LINE;
        line.end1[0] = points[i][0]*a; line.end1[1] = points[i][1]*b;
        line.end2[0] = points[i+1][0]*a; line.end2[1] = points[i+1][1]*b;
        SECTION_TRY("Sketch rectangle edge", ProSectionEntityAdd(*section, (Pro2dEntdef *)&line, &id));
        edges[i] = id;
    }
    if (p->hole > 0) SECTION_TRY("Sketch center hole", model_circle(*section, a/2, b/2, p->hole*scale/2, &hole_id));
    if (p->cylinder) {
        SECTION_TRY("Dimension outer diameter", model_section_dimension(*section, outer_id, PRO_TK_DIM_DIA, a, a*1.1, b*0.8));
    } else {
        SECTION_TRY("Dimension rectangle width", model_section_dimension(*section, edges[0], PRO_TK_DIM_LINE, a, a/2, -b*0.1));
        SECTION_TRY("Dimension rectangle height", model_section_dimension(*section, edges[1], PRO_TK_DIM_LINE, b, a*1.1, b/2));
    }
    if (p->hole > 0) SECTION_TRY("Dimension hole diameter", model_section_dimension(*section, hole_id, PRO_TK_DIM_DIA, p->hole*scale, a*0.8, b*0.8));
    SECTION_TRY("Allocate sketch diagnostics", ProSecerrorAlloc(&errors));
    /* Autodim both adds remaining dimensions and solves; calling Solve on
       an undimensioned rectangle first can fail before it is constrained. */
    SECTION_TRY("Set scale-aware sketch tolerance", ProSectionEpsilonSet(*section, 0.00001 * ((p->hole > 0 && p->hole*scale < (a < b ? a : b)) ? p->hole*scale : (a < b ? a : b))));
    SECTION_TRY("Autodimension and solve dimensioned sketch", ProSectionAutodim(*section, &errors));
    SECTION_TRY("Regenerate solved sketch", ProSectionRegenerate(*section, &errors));
done:
    if (errors) {
        if (error != PRO_TK_NO_ERROR) model_section_errors(&errors, *stage);
        ProSecerrorFree(&errors);
    }
    return error;
}
