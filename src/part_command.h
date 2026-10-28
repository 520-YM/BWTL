#ifndef CREO_PART_COMMAND_H
#define CREO_PART_COMMAND_H
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

/* Independent implementation. No generated code, shell, paths or macros are
   accepted. A response can contain exactly ONE executable protocol line. */
#define PART_ARG_MAX 32
#define PART_TOKEN_MAX 256
typedef enum { PART_NONE, PART_READ, PART_DIMS, PART_PARAM, PART_COPY,
    PART_RENAME, PART_REGEN, PART_SAVE, PART_STEP, PART_SUPPRESS, PART_RESUME, PART_DISPLAY, PART_EXTRUDE } PartVerb;
typedef struct { PartVerb verb; int count; wchar_t arg[PART_ARG_MAX][PART_TOKEN_MAX]; } PartCommand;
static int part_number(const wchar_t *s, double *v) {
    wchar_t *end; errno=0; *v=wcstod(s,&end);
    return *s && end!=s && !*end && !errno && isfinite(*v);
}
static int part_integer(const wchar_t *s, int *v) {
    wchar_t *end; long n; errno=0; n=wcstol(s,&end,10);
    if(!*s || end==s || *end || errno || n<0 || n>2147483647L)return 0;
    *v=(int)n;return 1;
}
static int part_name(const wchar_t *s) {
    size_t i,n=wcslen(s); if(!n || n>31)return 0;
    for(i=0;i<n;i++)if(!((s[i]>=L'a'&&s[i]<=L'z')||(s[i]>=L'A'&&s[i]<=L'Z')||
        (s[i]>=L'0'&&s[i]<=L'9')||s[i]==L'_'||s[i]==L'-'))return 0;
    return 1;
}
static int part_tokens(const wchar_t *s,size_t length,PartCommand *out) {
    size_t i=0,j;int quoted;
    while(i<length){
        while(i<length && (s[i]==L' '||s[i]==L'\t'||s[i]==L'\r'))i++;
        if(i==length)break;
        if(out->count>=PART_ARG_MAX)return 0;
        quoted=s[i]==L'"';if(quoted)i++;j=0;
        while(i<length){
            wchar_t c=s[i++];
            if(quoted && c==L'"'){quoted=2;break;}
            if(!quoted && (c==L' '||c==L'\t'||c==L'\r'))break;
            if(c<L' ' || (!quoted && c==L'"'))return 0;
            if(quoted && c==L'\\'){
                if(i==length || (s[i]!=L'"'&&s[i]!=L'\\'))return 0;
                c=s[i++];
            }
            if(j+1>=PART_TOKEN_MAX)return 0;
            out->arg[out->count][j++]=c;
        }
        if(quoted==1 || (quoted==2 && i<length && s[i]!=L' '&&s[i]!=L'\t'&&s[i]!=L'\r'))return 0;
        out->arg[out->count++][j]=0;
    }
    return 1;
}
/* -1: malformed/multiple actions, 0: no action, 1: valid native action.
   Legacy CREO_PLAN is counted too, so mixed protocols cannot both execute. */
static int part_command_parse(const wchar_t *answer,PartCommand *out) {
    const wchar_t *line=answer;int seen=0,has_op=0;
    PartCommand candidate={0};
    memset(out,0,sizeof(*out));
    while(*line){
        const wchar_t *end=wcschr(line,L'\n');size_t n=end?(size_t)(end-line):wcslen(line);
        const wchar_t *start=line; while(n && (*start==L' '||*start==L'\t')){start++;n--;}
        if(n>=10 && (!wcsncmp(start,L"CREO_PLAN ",10)||!wcsncmp(start,L"CREO_OP ",8))){
            if(++seen>1)return -1;
            if(!wcsncmp(start,L"CREO_OP ",8)){
                if(start!=line || !part_tokens(start+8,n-8,&candidate))return -1;
                has_op=1;
            }
        }
        if(!end)break;line=end+1;
    }
    if(!has_op)return 0;
    if(candidate.count<2 || !part_name(candidate.arg[1]))return -1;
    {
        const wchar_t *verb=candidate.arg[0];int n=candidate.count,id,i;double oldv,newv;
        if(!wcscmp(verb,L"READ")&&n==2)candidate.verb=PART_READ;
        else if(!wcscmp(verb,L"EXTRUDE")&&n==7){
            double a,b,h,hole;
            if(wcscmp(candidate.arg[2],L"BOX")&&wcscmp(candidate.arg[2],L"CYLINDER"))return -1;
            if(!part_number(candidate.arg[3],&a)||!part_number(candidate.arg[4],&b)||!part_number(candidate.arg[5],&h)||!part_number(candidate.arg[6],&hole))return -1;
            if(a<0.1||b<0.1||h<0.1||a>10000||b>10000||h>10000||hole<0||(hole>0&&hole<0.1)||(hole>0&&(hole>=a-0.1||hole>=b-0.1)))return -1;
            if(!wcscmp(candidate.arg[2],L"CYLINDER")&&fabs(a-b)>1e-6)return -1;
            candidate.verb=PART_EXTRUDE;
        }
        else if(!wcscmp(verb,L"DIMS") && n>=6 && (n-3)%3==0 && (n-3)/3<=8){
            if(!part_integer(candidate.arg[2],&id))return -1;
            for(i=3;i<n;i+=3){
                int k;if(!part_name(candidate.arg[i]) || !part_number(candidate.arg[i+1],&oldv)||
                    !part_number(candidate.arg[i+2],&newv)||fabs(oldv)>1e7||fabs(newv)>1e7)return -1;
                for(k=3;k<i;k+=3)if(!_wcsicmp(candidate.arg[k],candidate.arg[i]))return -1;
            }candidate.verb=PART_DIMS;
        }
        else if(!wcscmp(verb,L"PARAM")&&n==6 && part_name(candidate.arg[2])){
            if(wcscmp(candidate.arg[3],L"STRING") && wcscmp(candidate.arg[3],L"DOUBLE") &&
                wcscmp(candidate.arg[3],L"INTEGER") && wcscmp(candidate.arg[3],L"BOOLEAN"))return -1;
            candidate.verb=PART_PARAM;
        }
        else if(!wcscmp(verb,L"COPY")&&n==3&&part_name(candidate.arg[2]))candidate.verb=PART_COPY;
        else if(!wcscmp(verb,L"RENAME")&&n==3&&part_name(candidate.arg[2]))candidate.verb=PART_RENAME;
        else if(!wcscmp(verb,L"DISPLAY")&&n==3&&part_name(candidate.arg[2]))candidate.verb=PART_DISPLAY;
        else if(!wcscmp(verb,L"REGEN")&&n==2)candidate.verb=PART_REGEN;
        else if(!wcscmp(verb,L"SAVE")&&n==2)candidate.verb=PART_SAVE;
        else if(!wcscmp(verb,L"STEP")&&n==3&&part_name(candidate.arg[2]))candidate.verb=PART_STEP;
        else if((!wcscmp(verb,L"SUPPRESS")||!wcscmp(verb,L"RESUME"))&&n==4){
            if(!part_integer(candidate.arg[2],&id)||!part_integer(candidate.arg[3],&id))return -1;
            candidate.verb=!wcscmp(verb,L"SUPPRESS")?PART_SUPPRESS:PART_RESUME;
        }else return -1;
    }
    *out=candidate;return 1;
}
#endif
