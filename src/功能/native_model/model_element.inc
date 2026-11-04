static ProError model_element(ProElement parent, ProElemId id, int kind, double value, ProElement *out) {
    ProElement child = NULL;
    ProError error = ProElementAlloc(id, &child);
    if (error != PRO_TK_NO_ERROR) return error;
    if (kind == 1) error = ProElementIntegerSet(child, (int)value);
    if (kind == 2) error = ProElementDoubleSet(child, value);
    if (error == PRO_TK_NO_ERROR) error = ProElemtreeElementAdd(parent, NULL, child);
    if (error != PRO_TK_NO_ERROR) { ProElementFree(&child); return error; }
    if (out) *out = child;
    return error;
}
