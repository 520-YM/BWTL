#ifndef BWTL_QUICK_PARTS_PLAN_H
#define BWTL_QUICK_PARTS_PLAN_H
#include "model_artifact_request.h"
enum QuickPart {Q_NONE,Q_L,Q_U,Q_POWER,Q_SCREEN,Q_HINGE,Q_LOCK,Q_CLIP,Q_SPACER,Q_STOP,Q_COVER,Q_STOOL,Q_CHAIR};
enum QuickProfile {Q_HOLE,Q_RECT,Q_NOTCH,Q_SLOT};
typedef struct {int shape;double x,y,w,h;} QuickOpening;
typedef struct {
    double width,depth,thickness,height,radius,step_diameter,step_height,bore,leg_width,leg_inset;
    double back_width,back_thickness,back_height;
    int count,walls;
    QuickOpening cut[6];
} QuickPlan;

static int quick_kind(const wchar_t *verb){
    static const wchar_t *const names[]={L"QUICK_L",L"QUICK_U",L"QUICK_POWER",L"QUICK_SCREEN",L"QUICK_HINGE",
        L"QUICK_LOCK",L"QUICK_CLIP",L"QUICK_SPACER",L"QUICK_STOP",L"QUICK_COVER",L"QUICK_STOOL",L"QUICK_CHAIR"};
    for(int i=0;i<_countof(names);i++)if(!wcscmp(verb,names[i]))return i+1;
    return Q_NONE;
}
static const wchar_t *const quick_labels[]={L"",L"L形角码",L"U形安装支架",L"电源安装托架",L"屏幕安装背板",L"铰链垫板",
    L"门锁挡片",L"线束固定夹",L"阶梯隔套",L"可调限位块",L"可拆检修盖板",L"凳子",L"带靠背的凳子"};
static int quick_topic(const wchar_t *goal){
    for(int i=1;i<Q_STOOL;i++)if(wcsstr(goal,quick_labels[i]))return i;
    if(wcsstr(goal,L"L 形角码"))return Q_L;
    if(wcsstr(goal,L"U 形安装支架"))return Q_U;
    const wchar_t *creation=model_artifact_request(goal);
    if(!creation){
        static const wchar_t *const verbs[]={L"新建",L"创建",L"生成",L"建个",L"建一个"};
        for(int i=0;i<_countof(verbs);i++){
            const wchar_t *found=wcsstr(goal,verbs[i]);
            if(found&&(!creation||found<creation))creation=found;
        }
    }
    if(!creation)return Q_NONE;
    const wchar_t *end=creation+wcscspn(creation,L"，,。；;！!？?\r\n");
    static const wchar_t *const stools[]={L"凳子",L"方凳",L"板凳"};
    const wchar_t *noun=NULL;
    for(int i=0;i<_countof(stools);i++){
        const wchar_t *found=wcsstr(creation,stools[i]);
        if(found&&found<end&&(!noun||found<noun))noun=found;
    }
    if(!noun)return Q_NONE;
    static const wchar_t *const comparisons[]={L"参考",L"类似",L"像",L"参照"};
    for(int i=0;i<_countof(comparisons);i++){
        const wchar_t *found=wcsstr(creation,comparisons[i]);
        if(found&&found<noun)return Q_NONE;
    }
    static const wchar_t *const custom[]={L"圆形",L"圆凳",L"圆面",L"三腿",L"三条腿",L"折叠",L"斜靠背",L"弧形靠背",L"曲面靠背",L"横撑",L"横档",L"装配",L"倒圆",L"倒角",L"圆角"};
    for(int i=0;i<_countof(custom);i++)if(wcsstr(goal,custom[i]))return Q_NONE;
    if(wcsstr(creation,L"靠背")){
        static const wchar_t *const without_back[]={L"不带靠背",L"不加靠背",L"不要靠背",L"无靠背",L"别加靠背",L"去掉靠背"};
        for(int i=0;i<_countof(without_back);i++)if(wcsstr(creation,without_back[i]))return Q_STOOL;
        return Q_CHAIR;
    }
    return Q_STOOL;
}
static int quick_intent(const wchar_t *goal,int kind){
    static const wchar_t *const actions[]={L"新建",L"创建",L"生成",L"做一个",L"做个",L"帮我做",L"帮我画",L"画一个",L"建一个",L"建个"};
    static const wchar_t *const current[]={L"不要生成",L"别生成",L"不生成",L"别创建",L"不创建",L"不新建",L"不要新建",L"别新建",L"不另建",L"不要另建",L"现有零件",L"当前零件",L"当前模型",L"原零件",L"已有零件"};
    if(!kind||quick_topic(goal)!=kind)return 0;
    for(int i=0;i<_countof(current);i++)if(wcsstr(goal,current[i]))return 0;
    if((kind==Q_STOOL||kind==Q_CHAIR)&&model_artifact_request(goal))return 1;
    for(int i=0;i<_countof(actions);i++)if(wcsstr(goal,actions[i]))return 1;
    return 0;
}
static void quick_opening(QuickPlan *p,int shape,double x,double y,double width,double height){
    p->cut[p->count++]=(QuickOpening){shape,x,y,width,height};
}
static void quick_pair(QuickPlan *p,double pitch,double diameter){
    quick_opening(p,Q_HOLE,(p->width-pitch)/2,p->depth/2,diameter,diameter);
    quick_opening(p,Q_HOLE,(p->width+pitch)/2,p->depth/2,diameter,diameter);
}
static void quick_grid(QuickPlan *p,double px,double py,double diameter){
    for(int x=-1;x<=1;x+=2)for(int y=-1;y<=1;y+=2)
        quick_opening(p,Q_HOLE,(p->width+x*px)/2,(p->depth+y*py)/2,diameter,diameter);
}

#include "功能/quick_parts/qp_l.inc"
#include "功能/quick_parts/qp_u.inc"
#include "功能/quick_parts/qp_power.inc"
#include "功能/quick_parts/qp_screen.inc"
#include "功能/quick_parts/qp_hinge.inc"
#include "功能/quick_parts/qp_lock.inc"
#include "功能/quick_parts/qp_clip.inc"
#include "功能/quick_parts/qp_spacer.inc"
#include "功能/quick_parts/qp_stop.inc"
#include "功能/quick_parts/qp_cover.inc"

static int quick_profiles_valid(const QuickPlan *p){
    double edges[6][4];
    const double web=fmax(0.3,p->thickness*.25);
    const double bend=p->walls?p->radius+2*p->thickness:0;
    if(p->walls&&(p->height<=p->radius+p->thickness||p->depth<=p->walls*bend+web))return 0;
    for(int i=0;i<p->count;i++){
        const QuickOpening *o=&p->cut[i];
        double *a=edges[i];
        a[0]=o->shape==Q_NOTCH?0:o->x-o->w/2;
        a[1]=o->shape==Q_NOTCH?o->w:o->x+o->w/2;
        a[2]=o->y-o->h/2;a[3]=o->y+o->h/2;
        if((o->shape!=Q_NOTCH&&a[0]<web)||a[1]>p->width-web||a[2]<web+bend||
           a[3]>p->depth-web-(p->walls==2?bend:0)||(o->shape==Q_SLOT&&o->w-o->h<0.2))return 0;
        for(int j=0;j<i;j++){
            const double *b=edges[j];
            if(a[1]+web>b[0]&&b[1]+web>a[0]&&a[3]+web>b[2]&&b[3]+web>a[2])return 0;
        }
    }
    return 1;
}

static int quick_plan(int n,wchar_t a[][PART_TOKEN_MAX],QuickPlan *out){
    static const int sizes[]={0,7,7,8,8,6,7,7,5,6,8,6,9};
    static void (*const recipe[])(QuickPlan *,const double *)={
        NULL,qp_l,qp_u,qp_power,qp_screen,qp_hinge,qp_lock,qp_clip,qp_spacer,qp_stop,qp_cover};
    int kind=quick_kind(a[0]);
    double v[9];
    if(!kind)return 0;
    if(n!=2+sizes[kind]||!dv_safe_leaf(a[1]))return -1;
    for(int i=0;i<n-2;i++)if(!part_number(a[i+2],&v[i])||v[i]<0.1||v[i]>2000)return -1;
    QuickPlan p={0};
    p.width=v[0];p.depth=v[1];p.thickness=v[2];
    if(kind==Q_STOOL||kind==Q_CHAIR){
        p.height=v[3];p.leg_width=v[4];p.leg_inset=v[5];
        if(p.thickness+0.1>p.height||2*(p.leg_width+p.leg_inset)+0.3>fmin(p.width,p.depth))return -1;
        if(kind==Q_CHAIR){
            p.back_width=v[6];p.back_thickness=v[7];p.back_height=v[8];
            if(p.back_width>p.width||p.back_thickness>=p.depth)return -1;
        }
        if(out)*out=p;
        return 1;
    }
    recipe[kind](&p,v);
    if(kind==Q_COVER&&(2*v[3]>=p.width||2*v[4]>=p.depth))return -1;
    if(kind==Q_SPACER){
        double wall=fmax(0.3,p.thickness*.1);
        if((p.width-p.step_diameter)/2<wall||(p.step_diameter-p.bore)/2<wall)return -1;
    }else if(!quick_profiles_valid(&p))return -1;
    if(out)*out=p;
    return 1;
}
#endif
