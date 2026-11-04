static int foundation_request(const wchar_t *text){
    const wchar_t *words[]={L"基准",L"平面",L"草绘",L"参照",L"坐标",L"曲线",L"直线",L"矩形",L"圆",L"点",L"轴",L"偏移",L"约束",L"对称",L"尺寸",L"SKETCH",L"DATUM",L"参看"};
    int i;const wchar_t *more[]={L"拉伸",L"旋转",L"切除",L"投影",L"编辑",L"移动",L"删除",L"引用",L"轮廓"};for(i=0;i<sizeof(more)/sizeof(more[0]);i++)if(wcsstr(text,more[i]))return 1;for(i=0;i<sizeof(words)/sizeof(words[0]);i++)if(wcsstr(text,words[i]))return 1;return 0;
}
