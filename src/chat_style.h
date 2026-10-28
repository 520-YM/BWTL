#ifndef CREO_CHAT_STYLE_H
#define CREO_CHAT_STYLE_H
#include <stdio.h>
#include <string.h>
/* Native RichEdit RTF: shaded table cells fill the message width. Nothing
   is rendered in a browser; message text is escaped, never treated as RTF. */
typedef struct { char *text;size_t capacity,length;int failed; } ChatRtf;
typedef struct { const char *text;size_t length,offset; } ChatStream;
static void chat_rtf_add(ChatRtf *r,const char *s){
    size_t n=strlen(s);if(r->length+n>=r->capacity){r->failed=1;return;}
    memcpy(r->text+r->length,s,n);r->length+=n;r->text[r->length]=0;
}
static void chat_rtf_text(ChatRtf *r,const wchar_t *s,size_t n){
    size_t i;char value[32];
    for(i=0;i<n;i++){
        unsigned c=(unsigned short)s[i];
        if(c=='\\'||c=='{'||c=='}'){value[0]='\\';value[1]=(char)c;value[2]=0;}
        else if(c=='\t')strcpy_s(value,sizeof(value),"\\tab ");
        else if(c>=32&&c<127){value[0]=(char)c;value[1]=0;}
        else {sprintf_s(value,sizeof(value),"\\u%d?",(int)(short)c);}
        chat_rtf_add(r,value);
    }
}
/* Also recognize persisted, plain-text histories from earlier versions. */
static int chat_line_role(const wchar_t *s,size_t n,size_t *prefix){
    const wchar_t *labels[]={L"【你】",L"【AI】",L"【执行结果】",L"【模型信息】",L"【提示】",
        L"用户：",L"助手：",L"原生执行结果：",L"你：",L"AI：",L"AI:",L"自检结果："};
    const int roles[]={1,2,3,4,5,1,2,3,1,2,2,3};size_t i;
    for(i=0;i<sizeof(roles)/sizeof(roles[0]);i++){
        size_t length=wcslen(labels[i]);if(n>=length&&!wcsncmp(s,labels[i],length)){*prefix=length;return roles[i];}
    }*prefix=0;return 0;
}
static char *chat_rtf_build(const wchar_t *source,int cell_width){
    size_t count=wcslen(source);const wchar_t *line=source;
    ChatRtf r={0};int in_cell=0;char format[512];
    r.capacity=count*64+16384;r.text=(char *)calloc(r.capacity,1);if(!r.text)return NULL;
    chat_rtf_add(&r,"{\\rtf1\\ansi\\ansicpg1252\\deff0\\uc1{\\fonttbl{\\f0 Microsoft YaHei UI;}}"
        "{\\colortbl;\\red248\\green250\\blue252;\\red232\\green241\\blue255;"
        "\\red232\\green246\\blue240;\\red242\\green236\\blue255;"
        "\\red255\\green246\\blue226;\\red235\\green240\\blue245;"
        "\\red34\\green52\\blue66;}\\viewkind4\\f0\\fs20\\cf7 ");
    while(*line){
        const wchar_t *end=wcschr(line,L'\n');size_t n=end?(size_t)(end-line):wcslen(line),prefix=0;
        int role;if(n&&line[n-1]==L'\r')n--;role=chat_line_role(line,n,&prefix);
        if(role){
            const wchar_t *headings[]={L"",L"你",L"AI",L"执行结果",L"模型信息",L"提示"};
            if(in_cell)chat_rtf_add(&r,"\\cell\\row\\pard\\highlight0\\sa90\\par ");
            sprintf_s(format,sizeof(format),"\\trowd\\trgaph0\\trleft0\\clcbpat%d"
                "\\clpadl160\\clpadfl3\\clpadr160\\clpadfr3\\clpadt100\\clpadft3\\clpadb100\\clpadfb3"
                "\\cellx%d\\pard\\intbl\\li140\\ri140\\sb80\\sa80\\sl280\\slmult1"
                "\\f0\\fs20\\cf7\\highlight%d\\b ",role+1,cell_width,role+1);
            chat_rtf_add(&r,format);chat_rtf_text(&r,headings[role],wcslen(headings[role]));
            chat_rtf_add(&r,"\\b0\\par ");in_cell=1;
            if(n>prefix){chat_rtf_text(&r,line+prefix,n-prefix);if(end)chat_rtf_add(&r,"\\par ");}
        }else {
            chat_rtf_text(&r,line,n);if(end)chat_rtf_add(&r,"\\par ");
        }
        if(!end)break;line=end+1;
    }
    if(in_cell)chat_rtf_add(&r,"\\cell\\row\\pard\\highlight0 ");
    chat_rtf_add(&r,"}");
    if(r.failed){free(r.text);return NULL;}return r.text;
}
static DWORD CALLBACK chat_stream_read(DWORD_PTR cookie,LPBYTE buffer,LONG size,LONG *written){
    ChatStream *s=(ChatStream *)cookie;size_t n=s->length-s->offset;
    if(n>(size_t)size)n=(size_t)size;memcpy(buffer,s->text+s->offset,n);s->offset+=n;*written=(LONG)n;return 0;
}
static int chat_render(HWND history,const wchar_t *source){
    RECT area;HDC dc;int dpi,width;char *rtf;ChatStream stream;EDITSTREAM edit={0};
    GetClientRect(history,&area);dc=GetDC(history);dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(history,dc);
    width=MulDiv(area.right-16,1440,dpi?dpi:96);if(width<1800)width=1800;
    rtf=chat_rtf_build(source,width);if(!rtf)return 0;
    stream.text=rtf;stream.length=strlen(rtf);stream.offset=0;
    edit.dwCookie=(DWORD_PTR)&stream;edit.pfnCallback=chat_stream_read;
    SendMessageW(history,WM_SETREDRAW,FALSE,0);
    SendMessageW(history,EM_STREAMIN,SF_RTF,(LPARAM)&edit);
    SendMessageW(history,WM_SETREDRAW,TRUE,0);InvalidateRect(history,NULL,TRUE);free(rtf);
    return !edit.dwError;
}
#endif
