typedef struct {ProMdl model;PartText *text;double scale;int count;} ShInfo;
static ProError sh_info_surface(ProSurface surface,ProError status,ProAppData data){
    ShInfo *s=data;ProSmtSurfType side;ProSrftype type;int id;double r,a,d;ProBool inside;ProSmtDvlLenCalcInfo calc;
    if(status)return status;if(++s->count>256)return PRO_TK_OUT_OF_RANGE;
    if(ProSmtSurfaceTypeGet((ProPart)s->model,surface,&side)||ProSurfaceTypeGet(surface,&type)||ProSurfaceIdGet(surface,&id))return 0;
    part_text_add(s->text,L"SURFACE:%d 钣金侧别=%d (2驱动/3偏置/1厚度侧)，面类型=%d",id,side,type);
    if((side==PRO_SMT_SURF_FACE||side==PRO_SMT_SURF_OFFSET)&&!ProSmtBendsrfInfoGet((ProPart)s->model,surface,&r,&inside,&a,&d,&calc))
        part_text_add(s->text,L"；折弯%ls半径=%.9g mm，角度=%.9g°，展开弧长=%.9g mm，计算方式=%d，因子=%.9g",inside?L"内":L"外",r/s->scale,a,d/s->scale,calc.method,calc.y_factor_value);
    part_text_add(s->text,L"\n");return 0;
}
static ProError sh_info(ProMdl model,wchar_t *report,size_t capacity){
    PartText text={report,capacity,0};ShInfo info={model,&text,1,0};ProDimension thickness;double value;ProError e;ProMdlsubtype subtype;
    ProUnitsystem units;ProUnititem mm,native;ProUnitConversion conversion;
    report[0]=0;
    if((e=ProMdlPrincipalunitsystemGet(model,&units))||(e=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&native))||
       (e=ProUnitInit(model,L"mm",&mm))||(e=ProUnitConversionCalculate(&mm,&native,&conversion)))return e;
    info.scale=conversion.scale;ProMdlSubtypeGet(model,&subtype);
    if(subtype==PROMDLSTYPE_PART_SHEETMETAL&&!ProSmtPartThicknessGet((ProPart)model,&thickness)&&!ProDimensionValueGet(&thickness,&value))
        part_text_add(&text,L"当前钣金厚度=%.9g mm。\n",value/info.scale);
    if(subtype==PROMDLSTYPE_PART_SHEETMETAL){
        int bodies;
        if((e=sd_body_count(model,&bodies)))return e;
        if(!bodies){
            part_text_add(&text,L"当前为钣金零件，尚无实体板壁。可在闭合草绘上创建首壁，或在空钣金零件中创建托盘。\n");
            wchar_t refs[20000];
            e=foundation_references(model,refs,_countof(refs));
            if(!e)part_text_add(&text,L"%ls",refs);
            return e==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:e;
        }
    }
    {wchar_t refs[24000];if(!(e=am_info(model,refs,_countof(refs))))part_text_add(&text,L"%ls\n",refs);else return e;}
    if(subtype==PROMDLSTYPE_PART_SHEETMETAL){e=ProSolidSurfaceVisit((ProSolid)model,sh_info_surface,NULL,&info);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;}
    {wchar_t refs[20000];if(!(e=foundation_references(model,refs,_countof(refs))))part_text_add(&text,L"%ls",refs);}
    return e;
}
