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
        }else {if(foundation_plan_valid(n,candidate.arg)!=1)return -1;candidate.verb=PART_FOUNDATION;}
    }
    *out=candidate;return 1;
}
