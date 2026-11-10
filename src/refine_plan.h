#ifndef BWTL_REFINE_PLAN_H
#define BWTL_REFINE_PLAN_H

static int rf_plane_token(const wchar_t *token){
    return (!wcsncmp(token,L"PLANE:",6)||!wcsncmp(token,L"SURFACE:",8))&&fd_reference_token(token);
}

static int rf_side(const wchar_t *token){return !wcscmp(token,L"POS")||!wcscmp(token,L"NEG");}

static int refinement_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    const wchar_t *verb=a[0];
    double size;
    int count,id,circular=!wcscmp(verb,L"FEATURE_PATTERN_CIRCULAR");
    if(!wcscmp(verb,L"SOLID_DRAFT"))
        return n==8&&fd_name(a[2])&&!wcsncmp(a[3],L"SURFACE:",8)&&part_integer(a[3]+8,&id)&&
            rf_plane_token(a[4])&&rf_plane_token(a[5])&&part_number(a[6],&size)&&size>=0.1&&size<=30&&rf_side(a[7])?1:-1;
    if(!wcscmp(verb,L"SOLID_RIB"))
        return n==7&&fd_name(a[2])&&fd_name(a[3])&&part_number(a[4],&size)&&size>=0.01&&size<=10000&&
            rf_side(a[5])&&(!wcscmp(a[6],L"SYM")||rf_side(a[6]))?1:-1;
    if(!wcscmp(verb,L"FEATURE_MIRROR"))
        return n==5&&fd_name(a[2])&&fd_name(a[3])&&rf_plane_token(a[4])?1:-1;
    if(circular||!wcscmp(verb,L"FEATURE_PATTERN_LINEAR")){
        if(n!=8||!fd_name(a[2])||!fd_name(a[3])||!part_integer(a[5],&count)||count<2||count>20||
           !part_number(a[6],&size)||size<0.01||!rf_side(a[7]))return -1;
        if(circular)return !wcsncmp(a[4],L"AXIS:",5)&&fd_reference_token(a[4])&&(count-1)*size<360?1:-1;
        return rf_plane_token(a[4])&&size<=10000?1:-1;
    }
    if(!wcscmp(verb,L"FEATURE_DELETE"))
        return n==4&&fd_name(a[2])&&!wcsncmp(a[3],L"FEATURE:",8)&&part_integer(a[3]+8,&id)&&id>0?1:-1;
    return 0;
}
#endif
