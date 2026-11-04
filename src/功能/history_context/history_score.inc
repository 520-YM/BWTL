static int history_score(const wchar_t *text,const wchar_t *query){
    size_t i,n=wcslen(query);int score=0;
    for(i=0;i+1<n;i++)if(query[i]>=0x4e00&&query[i]<=0x9fff&&query[i+1]>=0x4e00&&query[i+1]<=0x9fff){wchar_t word[3]={query[i],query[i+1],0};if(wcsstr(text,word))score++;}
    for(i=0;i<n;){wchar_t word[64];size_t j=0;if(!iswalnum(query[i])||query[i]>127){i++;continue;}
        while(i<n&&query[i]<128&&iswalnum(query[i])){if(j<63)word[j++]=query[i];i++;}word[j]=0;if(j>=3&&wcsstr(text,word))score+=3;
    }return score;
}
