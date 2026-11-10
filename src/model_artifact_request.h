#ifndef BWTL_MODEL_ARTIFACT_REQUEST_H
#define BWTL_MODEL_ARTIFACT_REQUEST_H
#include <wchar.h>
#include <wctype.h>

/* Recognize a request to make an object at the start of a clause.
   Leading politeness is optional. Bare verbs need a classifier. */
static const wchar_t *model_artifact_request(const wchar_t *text){
    static const wchar_t *const prefixes[]={L"现在",L"我想",L"我需要",L"麻烦",L"请问",L"请",L"你",L"可不可以",L"能不能",L"能否",L"可以",L"能",L"直接",L"帮我",L"给我",L"替我",L"先"};
    static const wchar_t *const verbs[]={L"制作",L"设计并建",L"做",L"画"};
    static const wchar_t *const topics[]={L"怎么",L"如何",L"什么",L"哪些",L"哪个",L"哪种",L"方法",L"教程",L"步骤",L"方案",L"思路",L"流程",L"建议",L"的图",L"的功能",L"只需",L"只用",L"容易吗",L"要多久",L"分析",L"对比",L"比较",L"评估",L"检查",L"说明",L"总结",L"清单",L"报告"};
    const wchar_t *separators=L"，,。；;！!？?\r\n";
    wchar_t close_quote=0;
    while(*text){
        const wchar_t *end=text;
        int quoted=close_quote!=0;
        for(;*end;end++){
            if(close_quote){
                if(*end==close_quote)close_quote=0;
                else if(*end==L'\n')close_quote=0;
                continue;
            }
            if(*end==L'“'||*end==L'「'||*end==L'『'||*end==L'"'){
                close_quote=*end==L'“'?L'”':*end==L'「'?L'」':*end==L'『'?L'』':L'"';
                quoted=1;
            }else if(wcschr(separators,*end))break;
        }
        const wchar_t *p=text;
        int delegated=0;
        if(!quoted){
            for(;;){
                while(p<end&&iswspace(*p))p++;
                size_t i;
                for(i=0;i<_countof(prefixes);i++){
                    size_t n=wcslen(prefixes[i]);
                    if((size_t)(end-p)>=n&&!wcsncmp(p,prefixes[i],n)){p+=n;break;}
                }
                if(i==_countof(prefixes))break;
                if(i!=_countof(prefixes)-1)delegated=1;
            }
            for(size_t i=0;i<_countof(verbs);i++){
                size_t n=wcslen(verbs[i]);
                if((size_t)(end-p)<=n||wcsncmp(p,verbs[i],n))continue;
                const wchar_t *target=p+n,*last=end;
                while(target<last&&iswspace(*target))target++;
                while(last>target&&(iswspace(last[-1])||wcschr(L"了吗呢吧",last[-1])))last--;
                if(target==last)break;
                if(!delegated){
                    if(*target==L'一'||*target==L'1')target++;
                    if(target==last||!wcschr(L"个件套把张块根只台扇条",*target)||++target==last)break;
                }
                size_t j;
                for(j=0;j<_countof(topics);j++){
                    const wchar_t *topic=wcsstr(target,topics[j]);
                    if(topic&&topic<last)break;
                }
                if(j==_countof(topics))return p;
                break;
            }
        }
        text=end+(*end!=0);
    }
    return NULL;
}
#endif
