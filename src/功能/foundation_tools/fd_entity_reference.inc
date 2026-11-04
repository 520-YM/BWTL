static ProError fd_entity_reference(ProSection section,const wchar_t *token,int *id,ProSectionPointType *sense){
    wchar_t number[32];const wchar_t *p=wcschr(token,L':');Pro2dEntdef *entity=NULL;ProError e;
    *sense=PRO_ENT_WHOLE;
    if(p){
        wcsncpy_s(number,32,token,(size_t)(p-token));if(!part_integer(number,id))return PRO_TK_BAD_INPUTS;
        if(!wcscmp(p+1,L"START"))*sense=PRO_ENT_START;
        else if(!wcscmp(p+1,L"END"))*sense=PRO_ENT_END;
        else if(!wcscmp(p+1,L"CENTER"))*sense=PRO_ENT_CENTER;
        else if(wcscmp(p+1,L"WHOLE"))return PRO_TK_BAD_INPUTS;
    }else if(!part_integer(token,id))return PRO_TK_BAD_INPUTS;
    e=ProSectionEntityGet(section,*id,&entity);if(e)return e;
    if((*sense==PRO_ENT_START||*sense==PRO_ENT_END)&&entity->type!=PRO_2D_LINE&&entity->type!=PRO_2D_ARC&&entity->type!=PRO_2D_CENTER_LINE)return PRO_TK_INVALID_TYPE;
    if(*sense==PRO_ENT_CENTER&&entity->type!=PRO_2D_CIRCLE&&entity->type!=PRO_2D_ARC)return PRO_TK_INVALID_TYPE;
    return 0;
}
