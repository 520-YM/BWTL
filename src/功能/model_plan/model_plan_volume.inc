static double model_plan_volume(const ModelPlan *p) {
    double area = p->cylinder ? 3.14159265358979323846 * p->a * p->a / 4 : p->a * p->b;
    return (area - 3.14159265358979323846 * p->hole * p->hole / 4) * p->height;
}
