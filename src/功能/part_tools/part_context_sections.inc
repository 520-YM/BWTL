static unsigned part_context_sections(const PartSnapshot *s,const wchar_t *request){
    unsigned sections=0;int i;
    if(wcsstr(request,L"拉伸")||wcsstr(request,L"旋转")||wcsstr(request,L"切除")||wcsstr(request,L"投影")||wcsstr(request,L"编辑")||wcsstr(request,L"删除"))sections|=9;
    if(wcsstr(request,L"详细")||wcsstr(request,L"全部")||wcsstr(request,L"分析"))return 15;
    if(wcsstr(request,L"尺寸")||wcsstr(request,L"毫米")||wcsstr(request,L"mm")||wcsstr(request,L"改")||wcsstr(request,L"调整")||wcsstr(request,L"长")||wcsstr(request,L"宽")||wcsstr(request,L"厚")||wcsstr(request,L"高"))sections|=3;
    if(wcsstr(request,L"特征")||wcsstr(request,L"抑制")||wcsstr(request,L"恢复")||wcsstr(request,L"圆角")||wcsstr(request,L"倒角")||wcsstr(request,L"孔"))sections|=1;
    if(wcsstr(request,L"参数")||wcsstr(request,L"材质")||wcsstr(request,L"材料"))sections|=4;
    if(wcsstr(request,L"几何")||wcsstr(request,L"边")||wcsstr(request,L"面")||wcsstr(request,L"面积")||wcsstr(request,L"坐标"))sections|=8;
    for(i=0;i<s->dimensions;i++)if(s->dimension[i].symbol[0]&&wcsstr(request,s->dimension[i].symbol)){sections|=3;break;}
    for(i=0;i<s->parameters;i++)if(s->parameter[i].name[0]&&wcsstr(request,s->parameter[i].name)){sections|=4;break;}
    return sections;
}
