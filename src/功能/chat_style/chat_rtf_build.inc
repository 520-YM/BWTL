static char *chat_rtf_build(const wchar_t *source,int cell_width){
    size_t count=wcslen(source);const wchar_t *line=source;
    ChatRtf r={0};int in_cell=0,body_started=0,pending_blank=0;char format[512];
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
            if(in_cell)chat_rtf_add(&r,"\\cell\\row\\pard\\highlight0\\fs4\\sb0\\sa0\\sl80\\slmult0\\par ");
            sprintf_s(format,sizeof(format),"\\trowd\\trgaph0\\trleft0\\clcbpat%d"
                "\\clpadl160\\clpadfl3\\clpadr160\\clpadfr3\\clpadt60\\clpadft3\\clpadb60\\clpadfb3"
                "\\cellx%d\\pard\\intbl\\li140\\ri140\\sb0\\sa40\\sl240\\slmult1"
                "\\f0\\fs20\\cf7\\highlight%d\\b ",role+1,cell_width,role+1);
            chat_rtf_add(&r,format);chat_rtf_text(&r,headings[role],wcslen(headings[role]));
            chat_rtf_add(&r,"\\b0\\par ");in_cell=1;body_started=0;pending_blank=0;
            if(n>prefix){chat_rtf_text(&r,line+prefix,n-prefix);body_started=1;}
        }else {
            size_t k=0;while(k<n&&(line[k]==L' '||line[k]==L'\t'))k++;
            if(in_cell){
                /* Keep at most one interior blank line, never padding at the
                   beginning/end of a message. Insert breaks before content. */
                if(k==n){if(body_started)pending_blank=1;}
                else {if(body_started)chat_rtf_add(&r,pending_blank?"\\par\\par ":"\\par ");
                    chat_rtf_text(&r,line,n);body_started=1;pending_blank=0;}
            }else {chat_rtf_text(&r,line,n);if(end)chat_rtf_add(&r,"\\par ");}
        }
        if(!end)break;line=end+1;
    }
    if(in_cell)chat_rtf_add(&r,"\\cell\\row\\pard\\highlight0 ");
    chat_rtf_add(&r,"}");
    if(r.failed){free(r.text);return NULL;}return r.text;
}
