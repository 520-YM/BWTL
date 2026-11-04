static int fd_shape_valid(int n,wchar_t args[][PART_TOKEN_MAX],int start){
    double v[5]={0};int count,i;
    if(start>=n)return 0;
    if(!wcscmp(args[start],L"LINE")||!wcscmp(args[start],L"CENTERLINE")||!wcscmp(args[start],L"RECT"))count=4;
    else if(!wcscmp(args[start],L"CIRCLE"))count=3;
    else if(!wcscmp(args[start],L"ARC"))count=5;
    else return 0;
    if(n!=start+count+1)return 0;
    for(i=0;i<count;i++)if(!part_number(args[start+1+i],&v[i])||fabs(v[i])>1000000)return 0;
    if(!wcscmp(args[start],L"RECT"))return v[2]>=0.01&&v[3]>=0.01;
    if(!wcscmp(args[start],L"LINE")||!wcscmp(args[start],L"CENTERLINE"))return hypot(v[2]-v[0],v[3]-v[1])>=0.01;
    if(v[2]<0.01)return 0;
    if(!wcscmp(args[start],L"ARC"))return fabs(v[3])<=360&&fabs(v[4])<=360&&fabs(v[3]-v[4])>0.001&&fabs(v[3]-v[4])<359.999;
    return 1;
}
