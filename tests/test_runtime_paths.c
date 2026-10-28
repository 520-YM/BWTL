#define _CRT_SECURE_NO_WARNINGS
#include "../src/runtime_paths.h"
int main(void){
    wchar_t engine[MAX_PATH];FILE *log;
    if(!runtime_init())return 1;
    log=runtime_open(L"runtime-path-check.log",L"a");if(!log)return 2;
    if(fprintf(log,"portable runtime ASCII log: %d\n",18)<0)return 3;
    if(fclose(log))return 4;
    printf("PORTABLE ROOT / DATA / LOG PASS; native Codex path found: %d\n",runtime_codex(engine));
    return 0;
}
