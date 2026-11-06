static void model_plan_format(const ModelPlan *p, wchar_t text[256]) {
    if (!p->ready) { text[0] = 0; return; }
    if(p->empty_kind){swprintf_s(text,256,L"CREO_PLAN %ls",p->empty_kind==1?L"EMPTY_SOLID":p->empty_kind==2?L"EMPTY_SHEETMETAL":L"EMPTY_ASSEMBLY");return;}
    if(p->centered){swprintf_s(text,256,L"CREO_PLAN CENTERED_CYLINDER %.17g %.17g",p->a,p->height);return;}
    swprintf_s(text, 256, L"CREO_PLAN %ls %.17g %.17g %.17g %.17g", p->cylinder ? L"CYLINDER" : L"BOX", p->a, p->b, p->height, p->hole);
}
