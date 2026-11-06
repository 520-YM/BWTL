static int model_plan_valid(const ModelPlan *p) {
    if (p->centered && (p->empty_kind || !p->cylinder || p->hole != 0)) return 0;
    if (p->empty_kind) return p->empty_kind >= 1 && p->empty_kind <= 3;
    if (!isfinite(p->a) || !isfinite(p->b) || !isfinite(p->height) || !isfinite(p->hole)) return 0;
    if (p->a < 0.1 || p->b < 0.1 || p->height < 0.1 || p->a > 10000 || p->b > 10000 || p->height > 10000) return 0;
    if (p->hole < 0 || (p->hole > 0 && p->hole < 0.1)) return 0;
    if (p->hole > 0 && (p->hole >= p->a - 0.1 || p->hole >= p->b - 0.1)) return 0;
    if (p->cylinder && fabs(p->a - p->b) > 0.000001) return 0;
    return 1;
}
