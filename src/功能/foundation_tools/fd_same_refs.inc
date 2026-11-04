static int fd_same_refs(int n,const int *a,const ProSectionPointType *sa,const int *b,const ProSectionPointType *sb){
    int i,same=1;for(i=0;i<n;i++)if(a[i]!=b[i]||sa[i]!=sb[i])same=0;
    if(same)return 1;
    return n==2&&a[0]==b[1]&&a[1]==b[0]&&sa[0]==sb[1]&&sa[1]==sb[0];
}
