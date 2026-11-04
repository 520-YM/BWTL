static ProError model_section_dimension(ProSection section, int entity, ProSecdimType type, double value, double x, double y) {
    ProSectionPointType point_type = PRO_ENT_WHOLE;
    Pro2dPnt placement = {x,y};
    int dimension;
    ProError error = ProSecdimCreate(section, &entity, &point_type, 1, type, placement, &dimension);
    if (error == PRO_TK_NO_ERROR) error = ProSecdimValueSet(section, dimension, value);
    return error;
}
