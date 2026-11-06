/* A small literal-safe Markdown subset; all content still passes RTF escaping. */
static void chat_rtf_markup(ChatRtf *r,const wchar_t *s,size_t n){
    size_t i=0,start=0,j;
    if(n>2&&s[0]==L'#'){while(i<n&&i<4&&s[i]==L'#')i++;if(i<n&&s[i]==L' '){chat_rtf_add(r,"\\b ");chat_rtf_text(r,s+i+1,n-i-1);chat_rtf_add(r,"\\b0 ");return;}i=0;}
    if(n>2&&(s[0]==L'-'||s[0]==L'*')&&s[1]==L' '){chat_rtf_text(r,L"• ",2);s+=2;n-=2;}
    while(i<n){
        int bold=i+1<n&&s[i]==L'*'&&s[i+1]==L'*',code=s[i]==L'`';size_t marker=bold?2:1;
        if(bold||code){
            for(j=i+marker;j+marker<=n;j++)if((bold&&s[j]==L'*'&&s[j+1]==L'*')||(code&&s[j]==L'`'))break;
            if(j+marker<=n){chat_rtf_text(r,s+start,i-start);chat_rtf_add(r,bold?"\\b ":"\\f1 ");chat_rtf_text(r,s+i+marker,j-i-marker);chat_rtf_add(r,bold?"\\b0 ":"\\f0 ");i=j+marker;start=i;continue;}
        }i++;
    }chat_rtf_text(r,s+start,n-start);
}
