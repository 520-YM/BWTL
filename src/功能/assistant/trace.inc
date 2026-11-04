static void trace(const char *operation, int result) {
    FILE *f = runtime_open(L"runtime.log",L"a");
    if (f) { fprintf(f, "%s: %d\n", operation, result); fclose(f); }
}
