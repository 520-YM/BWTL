static int part_number(const wchar_t *s, double *v) {
    wchar_t *end; errno=0; *v=wcstod(s,&end);
    return *s && end!=s && !*end && !errno && isfinite(*v);
}
