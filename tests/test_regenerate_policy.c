#include <assert.h>
#include <stdio.h>
typedef void *ProMdl;
typedef void *ProSolid;
typedef int ProError;
#define PRO_REGEN_NO_RESOLVE_MODE 128
#define PRO_REGEN_UNDO_IF_FAIL 1024
#define PRO_TK_REGEN_AGAIN (-56)
static int responses[4],calls,health_calls,health_status;
static int ProSolidRegenerate(ProSolid model,int flags){
    assert(model);assert(flags==(128|1024));return responses[calls++];
}
static int part_health(ProMdl model){health_calls++;return health_status;}
#include "../src/功能/part_tools/part_regenerate.inc"
static void prepare(int a,int b,int c,int health){
    responses[0]=a;responses[1]=b;responses[2]=c;calls=health_calls=0;health_status=health;
}
int main(void){
    void *model=(void *)1;
    prepare(0,0,0,0);assert(!part_regenerate(model)&&calls==1&&health_calls==1);
    prepare(-56,0,0,0);assert(!part_regenerate(model)&&calls==2&&health_calls==1);
    prepare(-56,-56,0,0);assert(!part_regenerate(model)&&calls==3&&health_calls==1);
    prepare(-56,-56,-56,0);assert(part_regenerate(model)==-56&&calls==3&&!health_calls);
    prepare(-1,0,0,0);assert(part_regenerate(model)==-1&&calls==1&&!health_calls);
    prepare(-56,-48,0,0);assert(part_regenerate(model)==-48&&calls==2&&!health_calls);
    prepare(-56,0,0,-1);assert(part_regenerate(model)==-1&&calls==2&&health_calls==1);
    puts("REGEN POLICY PASS: retry success, bounded exhaustion, genuine failures and feature-health rejection");return 0;
}
