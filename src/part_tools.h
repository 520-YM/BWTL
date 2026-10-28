#ifndef CREO_PART_TOOLS_H
#define CREO_PART_TOOLS_H
/* Developed against the installed PTC Creo 10 Pro/TOOLKIT headers.
   All entry points run on Creo's command thread. No MCP, UI automation or
   third-party repository source is used. */
#include <stdarg.h>
#include <ProDimension.h>
#include <ProAssembly.h>
#include <ProParameter.h>
#include <ProModelitem.h>
#include <ProEdge.h>
#include <ProIntf3Dexport.h>
#include "part_command.h"
#define PART_FEATURE_MAX 256
#define PART_DIM_MAX 256
#define PART_PARAM_MAX 64
#define PART_GEOM_MAX 64
#define PART_TEXT_MAX 28000
typedef struct { int id,type; unsigned flags; ProFeatStatus status; ProName name; } PartFeature;
typedef struct { ProDimension handle; ProName symbol; int feature; ProDimensiontype type; double value; ProBoolean related,negative; } PartDimension;
typedef struct { ProName name; ProParamvalue value; ProLockstatus lock; } PartParameter;
typedef struct { int id,type;double size; } PartGeometry;
typedef struct {
    ProMdl model,window_model; ProMdlName name; ProMdlType type; ProMdlsubtype subtype;
    ProWVerstamp stamp,parent_stamp;int in_assembly;ProPath directory;ProUnititem length_unit,mass_unit;
    double mm_per_native,volume,mass; int has_volume,has_mass,has_outline;
    Pro3dPnt outline[2];
    PartFeature feature[PART_FEATURE_MAX];int features;
    PartDimension dimension[PART_DIM_MAX];int dimensions;
    PartParameter parameter[PART_PARAM_MAX];int parameters;
    PartGeometry edge[PART_GEOM_MAX],surface[PART_GEOM_MAX];int edges,surfaces;
    int truncated,read_errors,unhealthy,empty_solid;
} PartSnapshot;
typedef struct { wchar_t *text;size_t size;int truncated; } PartText;
static void part_text_add(PartText *out,const wchar_t *fmt,...) {
    size_t n=wcslen(out->text);int result;va_list args;
    if(n+2>=out->size){out->truncated=1;return;}
    va_start(args,fmt);result=_vsnwprintf_s(out->text+n,out->size-n,_TRUNCATE,fmt,args);va_end(args);
    if(result<0)out->truncated=1;
}
static void part_text_quote(PartText *out,const wchar_t *s){
    part_text_add(out,L"\"");
    while(*s){
        if(*s==L'"'||*s==L'\\')part_text_add(out,L"\\%lc",*s);
        else if(*s<L' ')part_text_add(out,L" ");
        else part_text_add(out,L"%lc",*s);
        s++;
    }part_text_add(out,L"\"");
}
static int part_close(double a,double b){return isfinite(a)&&isfinite(b)&&fabs(a-b)<=1e-7*fmax(1.0,fmax(fabs(a),fabs(b)));}
static void part_snapshot_clear(PartSnapshot *s){if(s->stamp)ProVerstampFree(&s->stamp);if(s->parent_stamp)ProVerstampFree(&s->parent_stamp);memset(s,0,sizeof(*s));}
static ProError part_geometry_visit(ProGeomitem *item,ProError status,ProAppData data){
    PartSnapshot *s=data;PartGeometry *g;int i,n;
    if(status)return status;
    if(item->type==PRO_EDGE){
        ProEdge edge;ProEnttype type;
        n=s->edges;for(i=0;i<n;i++)if(s->edge[i].id==item->id)return 0;
        if(n==PART_GEOM_MAX){s->truncated=1;return 0;}g=&s->edge[n];
        if(ProGeomitemToEdge(item,&edge)||ProEdgeTypeGet(edge,&type)||ProEdgeLengthEval(edge,&g->size)){s->read_errors++;return 0;}
        g->id=item->id;g->type=type;s->edges++;
    }else if(item->type==PRO_SURFACE){
        ProSurface surface;ProSrftype type;
        n=s->surfaces;for(i=0;i<n;i++)if(s->surface[i].id==item->id)return 0;
        if(n==PART_GEOM_MAX){s->truncated=1;return 0;}g=&s->surface[n];
        if(ProGeomitemToSurface(item,&surface)||ProSurfaceTypeGet(surface,&type)||ProSurfaceAreaEval(surface,&g->size)){s->read_errors++;return 0;}
        g->id=item->id;g->type=type;s->surfaces++;
    }return 0;
}
static ProError part_feature_visit(ProFeature *f,ProError status,ProAppData data){
    PartSnapshot *s=data;PartFeature *p;ProError e;
    if(status)return status;
    if(s->features==PART_FEATURE_MAX){s->truncated=1;return 0;}
    p=&s->feature[s->features];p->id=f->id;
    if(ProFeatureTypeGet(f,&p->type)||ProFeatureStatusGet(f,&p->status)||ProFeatureStatusflagsGet(f,&p->flags)){s->read_errors++;return 0;}
    ProModelitemNameGet((ProModelitem *)f,p->name);s->features++;
    if(p->flags&(PRO_FEAT_STAT_FAILED|PRO_FEAT_STAT_CHILD_OF_FAILED|PRO_FEAT_STAT_CHILD_OF_EXT_FAILED))s->unhealthy=1;
    if(p->status==PRO_FEAT_ACTIVE){
        e=ProFeatureGeomitemVisit(f,PRO_EDGE,part_geometry_visit,NULL,s);if(e && e!=PRO_TK_E_NOT_FOUND)s->read_errors++;
        if(p->type!=PRO_FEAT_DATUM){e=ProFeatureGeomitemVisit(f,PRO_SURFACE,part_geometry_visit,NULL,s);if(e&&e!=PRO_TK_E_NOT_FOUND)s->read_errors++;}
    }return 0;
}
static ProError part_dimension_visit(ProDimension *dim,ProError status,ProAppData data){
    PartSnapshot *s=data;PartDimension *d;ProFeature owner;
    if(status)return status;
    if(s->dimensions==PART_DIM_MAX){s->truncated=1;return 0;}
    d=&s->dimension[s->dimensions];d->handle=*dim;
    if(ProDimensionSymbolGet(dim,d->symbol)||ProDimensionValueGet(dim,&d->value)||
        ProDimensionTypeGet(dim,&d->type)||ProDimensionOwnerfeatureGet(dim,&owner)||
        ProDimensionIsReldriven(dim,&d->related)||ProDimensionIsRegenednegative(dim,&d->negative)){s->read_errors++;return 0;}
    d->feature=owner.id;s->dimensions++;return 0;
}
static ProError part_parameter_visit(ProParameter *param,ProError status,ProAppData data){
    PartSnapshot *s=data;PartParameter *p;
    if(status)return status;
    if(s->parameters==PART_PARAM_MAX){s->truncated=1;return 0;}
    p=&s->parameter[s->parameters];wcscpy_s(p->name,PRO_NAME_SIZE,param->id);
    if(ProParameterValueWithUnitsGet(param,&p->value,NULL)||ProParameterLockstatusGet(param,&p->lock)){s->read_errors++;return 0;}
    s->parameters++;return 0;
}
static ProError part_target_get(ProMdl *target){
    ProMdl window=NULL,active=NULL;ProMdlType type;ProError error;
    if((error=ProMdlCurrentGet(&window)))return error;
    if((error=ProMdlTypeGet(window,&type)))return error;
    if(type!=PRO_MDL_ASSEMBLY){*target=window;return 0;}
    if(!ProMdlActiveGet(&active) && active){*target=active;return 0;}
    return PRO_TK_BAD_CONTEXT;
}
static ProError part_request_target_get(const wchar_t *request,ProMdl *target){
    if(wcsstr(request,L"当前装配")||wcsstr(request,L"顶层装配")||wcsstr(request,L"总装配")){
        ProMdl window=NULL;ProMdlType type;ProError e=ProMdlCurrentGet(&window);
        if(!e&&!ProMdlTypeGet(window,&type)&&type==PRO_MDL_ASSEMBLY){*target=window;return 0;}
    }
    return part_target_get(target);
}
static ProError part_snapshot_take_mode(ProMdl model,PartSnapshot *s,int full){
    ProError e;ProModelitem item;ProUnitsystem units;ProUnititem mm;ProUnitConversion conv;ProMassProperty mass;
    part_snapshot_clear(s);s->model=model;
    if((e=ProMdlCurrentGet(&s->window_model)))return e;
    if(s->window_model!=model){
        ProMdlType window_type;ProMdl active=NULL;
        if(!ProMdlTypeGet(s->window_model,&window_type)&&window_type==PRO_MDL_ASSEMBLY&&!ProMdlActiveGet(&active)&&active==model){
            s->in_assembly=1;if((e=ProMdlVerstampGet(s->window_model,&s->parent_stamp)))return e;
        }
    }
    if((e=ProMdlMdlnameGet(model,s->name))||(e=ProMdlTypeGet(model,&s->type))||
       (e=ProMdlSubtypeGet(model,&s->subtype))||(e=ProDirectoryCurrentGet(s->directory)))return e;
    if(s->type!=PRO_MDL_PART && s->type!=PRO_MDL_ASSEMBLY)return PRO_TK_BAD_CONTEXT;
    if((e=ProMdlVerstampGet(model,&s->stamp)))return e;
    s->empty_solid=s->type==PRO_MDL_PART && s->subtype!=PROMDLSTYPE_PART_SHEETMETAL && !template_empty((ProSolid)model);
    if((e=ProMdlPrincipalunitsystemGet(model,&units)) || (e=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&s->length_unit))||
       (e=ProUnitInit(model,L"mm",&mm))||(e=ProUnitConversionCalculate(&s->length_unit,&mm,&conv)))return e;
    s->mm_per_native=conv.scale;if(!isfinite(conv.scale)||conv.scale<=0||conv.offset!=0)return PRO_TK_BAD_CONTEXT;
    e=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_MASS,&s->mass_unit);if(e)s->mass_unit.name[0]=0;
    if(full){
        e=ProSolidFeatVisit((ProSolid)model,part_feature_visit,NULL,s);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;
        e=ProSolidDimensionVisit((ProSolid)model,PRO_B_FALSE,part_dimension_visit,NULL,s);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;
    }
    if((e=ProMdlToModelitem(model,&item)))return e;
    if(full){e=ProParameterVisit(&item,NULL,part_parameter_visit,s);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;}
    else {
        const wchar_t *keys[]={L"PTC_REPORTED_MATERIAL",L"材料",L"PTC_MATERIAL_NAME",L"材质"};int i;
        for(i=0;i<4;i++){
            ProParameter parameter;PartParameter *p=&s->parameter[s->parameters];
            if(!ProParameterInit(&item,(wchar_t *)keys[i],&parameter)&&!ProParameterValueWithUnitsGet(&parameter,&p->value,NULL)){
                wcscpy_s(p->name,PRO_NAME_SIZE,keys[i]);s->parameters++;
            }
        }
    }
    if(!ProSolidOutlineGet((ProSolid)model,s->outline))s->has_outline=1;
    if(full && !ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)){
        s->volume=mass.volume;s->has_volume=1;
    }
    if(!ProSolidMassPropertyGet((ProSolid)model,NULL,&mass)){s->mass=mass.mass;s->has_mass=1;}
    return 0;
}
static ProError part_snapshot_take(ProMdl model,PartSnapshot *s){return part_snapshot_take_mode(model,s,1);}
static ProError part_snapshot_take_basic(ProMdl model,PartSnapshot *s){return part_snapshot_take_mode(model,s,0);}
static void part_param_format(PartText *out,const ProParamvalue *p){
    switch(p->type){
    case PRO_PARAM_STRING:part_text_quote(out,p->value.s_val);break;
    case PRO_PARAM_DOUBLE:part_text_add(out,L"%.17g",p->value.d_val);break;
    case PRO_PARAM_INTEGER:part_text_add(out,L"%d",p->value.i_val);break;
    case PRO_PARAM_BOOLEAN:part_text_add(out,L"%d",p->value.l_val?1:0);break;
    default:part_text_add(out,L"[不支持的参数类型]");break;
    }
}
static const wchar_t *part_basic_material(const PartSnapshot *s){
    const wchar_t *keys[]={L"PTC_REPORTED_MATERIAL",L"材料",L"PTC_MATERIAL_NAME",L"材质"};int i,j;
    for(i=0;i<4;i++)for(j=0;j<s->parameters;j++){
        const PartParameter *p=&s->parameter[j];
        if(!_wcsicmp(p->name,keys[i])&&p->value.type==PRO_PARAM_STRING&&p->value.value.s_val[0]&&
           _wcsicmp(p->value.value.s_val,L"ASSY")&&_wcsicmp(p->value.value.s_val,L"PTC_SYSTEM_MTRL_PROPS"))return p->value.value.s_val;
    }return L"未指定";
}
static void part_snapshot_format_basic(const PartSnapshot *s,wchar_t *text,size_t capacity){
    PartText out={text,capacity,0};double x,y,z;text[0]=0;
    part_text_add(&out,L"名称：%ls.%ls\n类型：%ls\n",s->name,s->type==PRO_MDL_ASSEMBLY?L"asm":L"prt",
        s->type==PRO_MDL_ASSEMBLY?L"装配":s->subtype==PROMDLSTYPE_PART_SHEETMETAL?L"钣金零件":L"实体零件");
    x=(s->outline[1][0]-s->outline[0][0])*s->mm_per_native;
    y=(s->outline[1][1]-s->outline[0][1])*s->mm_per_native;
    z=(s->outline[1][2]-s->outline[0][2])*s->mm_per_native;
    if(s->empty_solid)part_text_add(&out,L"尺寸：空零件，暂无实体\n");
    else if(s->has_outline&&isfinite(x)&&isfinite(y)&&isfinite(z)&&x>=0&&y>=0&&z>=0)
        part_text_add(&out,L"外形尺寸：%.6g × %.6g × %.6g mm\n",x,y,z);
    else part_text_add(&out,L"外形尺寸：暂未获取\n");
    part_text_add(&out,L"材料：%.80ls\n",part_basic_material(s));
    if(s->has_mass&&isfinite(s->mass)&&s->mass>=0)part_text_add(&out,L"重量：%.6g %ls\n",s->mass,s->mass_unit.name[0]?s->mass_unit.name:L"（模型质量单位）");
    else part_text_add(&out,L"重量：暂未获取\n");
    if(s->in_assembly)part_text_add(&out,L"位置：装配中当前激活的零件\n");
    if(s->unhealthy)part_text_add(&out,L"状态：存在重生成问题\n");
}
static void part_snapshot_format_selected(const PartSnapshot *s,wchar_t *text,size_t capacity,unsigned sections){
    PartText out={text,capacity,0};int i;text[0]=0;
    part_text_add(&out,L"当前操作目标：%ls；类型=%ls；subtype=%d；工作目录：%ls\n单位：%ls；1模型长度单位=%.17g毫米；角度尺寸单位=度。所有尺寸表数值均为模型原生单位。\n",s->name,s->type==PRO_MDL_ASSEMBLY?L"装配":L"零件",s->subtype,s->directory,s->length_unit.name,s->mm_per_native);
    if(s->in_assembly){ProMdlName parent=L"";ProMdlMdlnameGet(s->window_model,parent);part_text_add(&out,L"所在装配窗口：%ls；此目标是实际激活的组件零件；编辑将影响该零件的全部实例。\n",parent);}
    part_text_add(&out,L"EMPTY_SOLID=%d（1=当前目标为空实体零件，可原位创建首个基础拉伸；0不适用）\n",s->empty_solid);
    if(s->has_outline)part_text_add(&out,L"外形范围(模型单位)：X[%.9g,%.9g] Y[%.9g,%.9g] Z[%.9g,%.9g]\n",s->outline[0][0],s->outline[1][0],s->outline[0][1],s->outline[1][1],s->outline[0][2],s->outline[1][2]);
    if(s->has_volume)part_text_add(&out,L"几何体积：%.12g（模型长度单位³）\n",s->volume);
    if(s->has_mass)part_text_add(&out,L"Creo返回质量：%.12g %ls（取决于模型材料/密度设置；未据此核验材料）\n",s->mass,s->mass_unit.name);
    else part_text_add(&out,L"质量属性：API未成功返回，不能推算或声称已知材料质量。\n");
    part_text_add(&out,L"特征(%d条；status=0活动,5普通抑制)：\n",s->features);
    if(sections&1){
    for(i=0;i<s->features;i++){const PartFeature *f=&s->feature[i];part_text_add(&out,L"FID=%d TYPE=%d STATUS=%d FLAGS=%u NAME=",f->id,f->type,f->status,f->flags);part_text_quote(&out,f->name);part_text_add(&out,L"\n");}
    }else part_text_add(&out,L"本次未展开特征列表，不得猜测ID。\n");
    part_text_add(&out,L"尺寸(%d条；TYPE=9线性,3半径,4直径,21角度,72弧长；RELATED=1不可修改)：\n",s->dimensions);
    if(sections&2){
    for(i=0;i<s->dimensions;i++){const PartDimension *d=&s->dimension[i];part_text_add(&out,L"FID=%d DIM=%ls VALUE=%.17g TYPE=%d RELATED=%d NEGATIVE=%d\n",d->feature,d->symbol,d->value,d->type,d->related,d->negative);}
    }else part_text_add(&out,L"本次未展开尺寸表，不得猜测符号或值。\n");
    part_text_add(&out,L"模型参数(%d条；LOCK=0可写；类型50浮点,51字符串,52整数,53布尔)：\n",s->parameters);
    if(sections&4){
    for(i=0;i<s->parameters;i++){const PartParameter *p=&s->parameter[i];part_text_add(&out,L"PARAM=%ls TYPE=%d LOCK=%d VALUE=",p->name,p->value.type,p->lock);part_param_format(&out,&p->value);part_text_add(&out,L"\n");}
    }else part_text_add(&out,L"本次未展开参数表，不得猜测参数值。\n");
    part_text_add(&out,L"几何摘要（边长度/面面积均为模型单位）：\n");
    if(sections&8){
    for(i=0;i<s->edges;i++)part_text_add(&out,L"EDGE=%d TYPE=%d LENGTH=%.9g\n",s->edge[i].id,s->edge[i].type,s->edge[i].size);
    for(i=0;i<s->surfaces;i++)part_text_add(&out,L"SURFACE=%d TYPE=%d AREA=%.9g\n",s->surface[i].id,s->surface[i].type,s->surface[i].size);
    }else part_text_add(&out,L"本次未展开边面列表；需要时明确请求读取几何。\n");
    if(s->truncated||out.truncated)part_text_add(&out,L"注意：模型数据超过当前读取上限，结果不完整，不得猜测未列出的特征或尺寸。\n");
    if(s->read_errors)part_text_add(&out,L"注意：%d项API读取未成功，其信息未知。\n",s->read_errors);
    if(s->unhealthy)part_text_add(&out,L"注意：存在失败特征，写入操作已禁止。\n");
    if(out.truncated){
        const wchar_t *marker=L"\n注意：输出空间已满，以上数据不完整；禁止猜测未显示的数据。\n";
        size_t length=wcslen(marker);if(capacity>length+1)wcscpy_s(text+capacity-length-1,length+1,marker);
    }
}
static void part_snapshot_format(const PartSnapshot *s,wchar_t *text,size_t capacity){part_snapshot_format_selected(s,text,capacity,15);}
/* Avoid appending entire geometry and parameter tables to every ordinary
   chat request. The native snapshot remains complete for validation. */
static unsigned part_context_sections(const PartSnapshot *s,const wchar_t *request){
    unsigned sections=0;int i;
    if(wcsstr(request,L"详细")||wcsstr(request,L"全部")||wcsstr(request,L"分析"))return 15;
    if(wcsstr(request,L"尺寸")||wcsstr(request,L"毫米")||wcsstr(request,L"mm")||wcsstr(request,L"改")||wcsstr(request,L"调整")||wcsstr(request,L"长")||wcsstr(request,L"宽")||wcsstr(request,L"厚")||wcsstr(request,L"高"))sections|=3;
    if(wcsstr(request,L"特征")||wcsstr(request,L"抑制")||wcsstr(request,L"恢复")||wcsstr(request,L"圆角")||wcsstr(request,L"倒角")||wcsstr(request,L"孔"))sections|=1;
    if(wcsstr(request,L"参数")||wcsstr(request,L"材质")||wcsstr(request,L"材料"))sections|=4;
    if(wcsstr(request,L"几何")||wcsstr(request,L"边")||wcsstr(request,L"面")||wcsstr(request,L"面积")||wcsstr(request,L"坐标"))sections|=8;
    for(i=0;i<s->dimensions;i++)if(s->dimension[i].symbol[0]&&wcsstr(request,s->dimension[i].symbol)){sections|=3;break;}
    for(i=0;i<s->parameters;i++)if(s->parameter[i].name[0]&&wcsstr(request,s->parameter[i].name)){sections|=4;break;}
    return sections;
}
static ProError part_check_binding(const PartCommand *cmd,const PartSnapshot *s,ProMdl *model){
    ProMdlName name;ProWVerstamp stamp=NULL;ProPath dir;ProError e;int equal;
    {ProMdl window=NULL;if((e=ProMdlCurrentGet(&window)))return e;if(window!=s->window_model)return PRO_TK_BAD_CONTEXT;}
    if(s->model==s->window_model){if((e=ProMdlCurrentGet(model)))return e;}
    else if((e=part_target_get(model)))return e;
    if(*model!=s->model || (e=ProMdlMdlnameGet(*model,name)) || _wcsicmp(name,cmd->arg[1]) || _wcsicmp(name,s->name))return PRO_TK_BAD_CONTEXT;
    if((e=ProDirectoryCurrentGet(dir)) || _wcsicmp(dir,s->directory))return PRO_TK_BAD_CONTEXT;
    if((e=ProMdlVerstampGet(*model,&stamp)))return e;
    equal=ProVerstampEqual(stamp,s->stamp);ProVerstampFree(&stamp);
    if(equal && s->in_assembly){
        if((e=ProMdlVerstampGet(s->window_model,&stamp)))return e;
        equal=ProVerstampEqual(stamp,s->parent_stamp);ProVerstampFree(&stamp);
    }
    return equal?0:PRO_TK_BAD_CONTEXT;
}
static ProError part_health_visit(ProFeature *f,ProError status,ProAppData data){
    unsigned flags;ProError e;if(status)return status;
    if((e=ProFeatureStatusflagsGet(f,&flags)))return e;
    return flags&(PRO_FEAT_STAT_FAILED|PRO_FEAT_STAT_CHILD_OF_FAILED|PRO_FEAT_STAT_CHILD_OF_EXT_FAILED)?PRO_TK_GENERAL_ERROR:0;
}
static ProError part_health(ProMdl model){ProError e=ProSolidFeatVisit((ProSolid)model,part_health_visit,NULL,NULL);return e==PRO_TK_E_NOT_FOUND?0:e;}
static ProError part_regenerate(ProMdl model){ProError e=ProSolidRegenerate((ProSolid)model,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);return e?e:part_health(model);}
static ProError part_regenerate_context(ProMdl model,const PartSnapshot *snapshot){ProError e=part_regenerate(model);if(!e&&snapshot->in_assembly)e=part_regenerate(snapshot->window_model);return e;}
static const PartFeature *part_find_feature(const PartSnapshot *s,int id){int i;for(i=0;i<s->features;i++)if(s->feature[i].id==id)return &s->feature[i];return NULL;}
static ProError part_change_dimensions(const PartCommand *cmd,const PartSnapshot *s,ProMdl model,wchar_t *text,size_t capacity){
    ProDimension dims[8];double old[8],next[8],value;ProFeature feature;ProFeattype type;ProFeatStatus status;
    int n=(cmd->count-3)/3,id,i,j,changed=0;ProError e=0,rollback=0;const PartFeature *expected;
    part_integer(cmd->arg[2],&id);expected=part_find_feature(s,id);
    if(!expected||expected->status!=PRO_FEAT_ACTIVE)return PRO_TK_BAD_CONTEXT;
    if((e=ProFeatureInit((ProSolid)model,id,&feature))||(e=ProFeatureTypeGet(&feature,&type))||
       (e=ProFeatureStatusGet(&feature,&status)))return e;
    if(type!=expected->type||status!=expected->status)return PRO_TK_BAD_CONTEXT;
    for(i=0;i<n;i++){
        const PartDimension *d=NULL;ProBoolean related,negative;ProFeature owner;ProName symbol;
        part_number(cmd->arg[4+3*i],&old[i]);part_number(cmd->arg[5+3*i],&next[i]);
        for(j=0;j<s->dimensions;j++)if(!_wcsicmp(s->dimension[j].symbol,cmd->arg[3+3*i])){d=&s->dimension[j];break;}
        if(!d||d->feature!=id||d->related||d->negative||next[i]<0||old[i]<0||!part_close(d->value,old[i]))return PRO_TK_BAD_CONTEXT;
        if(d->type!=PRODIMTYPE_LINEAR&&d->type!=PRODIMTYPE_RADIUS&&d->type!=PRODIMTYPE_DIAMETER&&d->type!=PRODIMTYPE_ANGLE&&d->type!=PRODIMTYPE_ARC_LENGTH)return PRO_TK_BAD_CONTEXT;
        if((d->type==PRODIMTYPE_DIAMETER||d->type==PRODIMTYPE_RADIUS||d->type==PRODIMTYPE_ARC_LENGTH)&&next[i]<=0)return PRO_TK_BAD_INPUTS;
        if(d->type==PRODIMTYPE_ANGLE && fabs(next[i])>360)return PRO_TK_BAD_INPUTS;
        dims[i]=d->handle;
        if((e=ProDimensionValueGet(&dims[i],&value))||(e=ProDimensionIsReldriven(&dims[i],&related))||
           (e=ProDimensionIsRegenednegative(&dims[i],&negative))||(e=ProDimensionOwnerfeatureGet(&dims[i],&owner))||(e=ProDimensionSymbolGet(&dims[i],symbol)))return e;
        if(related||negative||owner.id!=id||!part_close(value,old[i])||_wcsicmp(symbol,cmd->arg[3+3*i]))return PRO_TK_BAD_CONTEXT;
    }
    for(i=0;i<n;i++){
        /* Include even a failed setter in restoration: the SDK may have
           entered a partially changed state before returning its error. */
        changed=i+1;e=ProDimensionValueSet(&dims[i],next[i]);if(e)goto undo;
    }
    if((e=part_regenerate_context(model,s)))goto undo;
    if(s->has_volume && s->volume>0){
        ProMassProperty mass;e=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass);
        if(e||!isfinite(mass.volume)||mass.volume<=0){if(!e)e=PRO_TK_GENERAL_ERROR;goto undo;}
    }
    for(i=0;i<n;i++)if((e=ProDimensionValueGet(&dims[i],&value))||!part_close(value,next[i])){if(!e)e=PRO_TK_GENERAL_ERROR;goto undo;}
    swprintf_s(text,capacity,L"已修改零件 %ls 的特征 %d，共%d个尺寸；重生成及逐项读回通过。修改保留在内存，未自动保存。",s->name,id,n);return 0;
undo:
    for(i=changed-1;i>=0;i--)if(ProDimensionValueSet(&dims[i],old[i]))rollback=1;
    if(part_regenerate_context(model,s))rollback=1;
    for(i=0;i<changed;i++)if(ProDimensionValueGet(&dims[i],&value)||!part_close(value,old[i]))rollback=1;
    swprintf_s(text,capacity,L"尺寸修改失败，错误码%d；%ls。未保存。",e,rollback?L"回滚未能完整验证，请检查当前模型":L"旧尺寸已恢复并读回验证");return e;
}
static int part_param_equal(const ProParamvalue *a,const ProParamvalue *b){
    if(a->type!=b->type)return 0;
    switch(a->type){case PRO_PARAM_STRING:return !wcscmp(a->value.s_val,b->value.s_val);
    case PRO_PARAM_DOUBLE:return part_close(a->value.d_val,b->value.d_val);
    case PRO_PARAM_INTEGER:return a->value.i_val==b->value.i_val;
    case PRO_PARAM_BOOLEAN:return a->value.l_val==b->value.l_val;default:return 0;}
}
static int part_param_parse(const wchar_t *kind,const wchar_t *value,ProParamvalue *out){
    double number;
    memset(out,0,sizeof(*out));
    if(!wcscmp(kind,L"STRING")){out->type=PRO_PARAM_STRING;return wcscpy_s(out->value.s_val,PRO_LINE_SIZE,value)==0;}
    if(!part_number(value,&number))return 0;
    if(!wcscmp(kind,L"DOUBLE")){out->type=PRO_PARAM_DOUBLE;out->value.d_val=number;return fabs(number)<=1e12;}
    if(!wcscmp(kind,L"INTEGER")){if(number<-2147483647.0||number>2147483647.0||floor(number)!=number)return 0;out->type=PRO_PARAM_INTEGER;out->value.i_val=(int)number;return 1;}
    if(!wcscmp(kind,L"BOOLEAN")&&(number==0||number==1)){out->type=PRO_PARAM_BOOLEAN;out->value.l_val=(short)number;return 1;}return 0;
}
static ProError part_change_parameter(const PartCommand *cmd,const PartSnapshot *s,ProMdl model,wchar_t *text,size_t capacity){
    ProModelitem item;ProParameter parameter;ProParamvalue old,next,current;ProLockstatus lock;int i,found=0;ProError e,rollback=0;
    if(!part_param_parse(cmd->arg[3],cmd->arg[4],&old)||!part_param_parse(cmd->arg[3],cmd->arg[5],&next))return PRO_TK_BAD_INPUTS;
    for(i=0;i<s->parameters;i++)if(!_wcsicmp(s->parameter[i].name,cmd->arg[2])){
        if(s->parameter[i].lock!=PRO_PARAMLOCKSTATUS_UNLOCKED||!part_param_equal(&s->parameter[i].value,&old))return PRO_TK_BAD_CONTEXT;found=1;break;
    }if(!found)return PRO_TK_BAD_CONTEXT;
    if((e=ProMdlToModelitem(model,&item))||(e=ProParameterInit(&item,(wchar_t *)cmd->arg[2],&parameter))||
       (e=ProParameterLockstatusGet(&parameter,&lock))||(e=ProParameterValueWithUnitsGet(&parameter,&current,NULL)))return e;
    if(lock!=PRO_PARAMLOCKSTATUS_UNLOCKED||!part_param_equal(&current,&old))return PRO_TK_BAD_CONTEXT;
    e=ProParameterValueWithUnitsSet(&parameter,&next,NULL);
    if(!e)e=part_regenerate_context(model,s);
    if(!e)e=ProParameterValueWithUnitsGet(&parameter,&current,NULL);
    if(!e&&!part_param_equal(&current,&next))e=PRO_TK_GENERAL_ERROR;
    if(e){
        if(ProParameterValueWithUnitsSet(&parameter,&old,NULL)||part_regenerate_context(model,s)||
           ProParameterValueWithUnitsGet(&parameter,&current,NULL)||!part_param_equal(&current,&old))rollback=1;
        swprintf_s(text,capacity,L"参数修改失败，错误码%d；%ls；未保存。",e,rollback?L"回滚未能完整验证，请检查当前模型":L"已恢复原值并读回验证");return e;
    }
    swprintf_s(text,capacity,L"已修改 %ls 的参数 %ls，重生成及读回通过；未自动保存。",s->name,cmd->arg[2]);return 0;
}
/* Refuse both session-name collisions and existing versioned disk files. */
static int part_target_available(ProMdl model,const wchar_t *base,const wchar_t *extension){
    ProMdl existing=NULL;ProMdlName name;ProPath dir;wchar_t path[PRO_PATH_SIZE];WIN32_FIND_DATAW data;HANDLE find;
    wcscpy_s(name,PRO_NAME_SIZE,base);
    if(!_wcsicmp(extension,L"prt") && (!ProMdlnameInit(name,PRO_MDL_PART,&existing)||!ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&existing)))return 0;
    if(ProDirectoryCurrentGet(dir))return 0;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls.%ls*",dir,base,extension)<0)return 0;
    find=FindFirstFileW(path,&data);if(find!=INVALID_HANDLE_VALUE){FindClose(find);return 0;}
    return GetLastError()==ERROR_FILE_NOT_FOUND;
}
static ProError part_toggle_feature(const PartCommand *cmd,const PartSnapshot *s,ProMdl model,wchar_t *text,size_t capacity){
    int id,type,*ids=NULL,*children=NULL,n=0,i;ProFeature f;ProFeatStatus state,desired;
    ProFeatureDeleteOptions *del=NULL;ProFeatureResumeOptions *resume=NULL;ProError e,rollback=0;const PartFeature *old;
    part_integer(cmd->arg[2],&id);part_integer(cmd->arg[3],&type);old=part_find_feature(s,id);
    /* First solids and datums are deliberately excluded. */
    if(type!=PRO_FEAT_HOLE&&type!=PRO_FEAT_ROUND&&type!=PRO_FEAT_CHAMFER&&type!=PRO_FEAT_CUT)return PRO_TK_BAD_INPUTS;
    desired=cmd->verb==PART_SUPPRESS?PRO_FEAT_SUPPRESSED:PRO_FEAT_ACTIVE;
    if(!old||old->type!=type||old->status!=(cmd->verb==PART_SUPPRESS?PRO_FEAT_ACTIVE:PRO_FEAT_SUPPRESSED))return PRO_TK_BAD_CONTEXT;
    if((e=ProFeatureInit((ProSolid)model,id,&f))||(e=ProFeatureTypeGet(&f,&i))||i!=type)return PRO_TK_BAD_CONTEXT;
    if((e=ProFeatureChildrenGet(&f,&children,&n)))return e;
    if(children)ProArrayFree((ProArray *)&children);if(n)return PRO_TK_BAD_CONTEXT;
    if((e=ProArrayAlloc(1,sizeof(int),1,(ProArray *)&ids)))goto done;ids[0]=id;
    if((e=ProArrayAlloc(1,sizeof(*del),1,(ProArray *)&del)))goto done;del[0]=PRO_FEAT_DELETE_NO_OPTS;
    if((e=ProArrayAlloc(1,sizeof(*resume),1,(ProArray *)&resume)))goto done;resume[0]=PRO_FEAT_RESUME_NO_OPTS;
    e=cmd->verb==PART_SUPPRESS?ProFeatureWithoptionsSuppress((ProSolid)model,ids,del,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL):
        ProFeatureWithoptionsResume((ProSolid)model,ids,resume,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);
    if(!e)e=part_regenerate_context(model,s);
    if(!e)e=ProFeatureStatusGet(&f,&state);
    if(!e&&state!=desired)e=PRO_TK_GENERAL_ERROR;
    if(e){
        if(ProFeatureStatusGet(&f,&state))rollback=1;
        else if(state!=old->status){
            ProError r=cmd->verb==PART_SUPPRESS?ProFeatureWithoptionsResume((ProSolid)model,ids,resume,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL):
                ProFeatureWithoptionsSuppress((ProSolid)model,ids,del,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);
            if(r)rollback=1;
        }
        if(part_regenerate_context(model,s)||ProFeatureStatusGet(&f,&state)||state!=old->status)rollback=1;
        swprintf_s(text,capacity,L"特征状态修改失败，错误码%d；%ls；未保存。",e,rollback?L"未能完整验证回滚，请检查模型":L"原状态已恢复并验证");
    }else swprintf_s(text,capacity,L"零件 %ls 的特征%d已%ls，重生成及状态读回通过；未保存。",s->name,id,cmd->verb==PART_SUPPRESS?L"抑制":L"恢复");
done:
    if(ids)ProArrayFree((ProArray *)&ids);if(del)ProArrayFree((ProArray *)&del);if(resume)ProArrayFree((ProArray *)&resume);return e;
}
static ProError part_extrude_current(const PartCommand *cmd,const PartSnapshot *snapshot,ProMdl model,wchar_t *text,size_t capacity){
    ModelPlan plan={0};ProMdlName name;ProFeature created={0};const char *stage="validate current extrusion";
    ProError error;int restored=1,window;TemplateDatums before={0},after={0};
    if(snapshot->type!=PRO_MDL_PART || snapshot->subtype==PROMDLSTYPE_PART_SHEETMETAL){wcscpy_s(text,capacity,L"未执行：当前目标是装配或钣金模型。请在装配内激活要编辑的实体零件；不能把装配本身当实体拉伸。");return PRO_TK_BAD_CONTEXT;}
    plan.cylinder=!wcscmp(cmd->arg[2],L"CYLINDER");plan.ready=1;
    part_number(cmd->arg[3],&plan.a);part_number(cmd->arg[4],&plan.b);part_number(cmd->arg[5],&plan.height);part_number(cmd->arg[6],&plan.hole);
    if(!model_plan_valid(&plan))return PRO_TK_BAD_INPUTS;
    if((error=template_empty((ProSolid)model))){wcscpy_s(text,capacity,L"未执行：当前零件已有实体，现阶段的当前零件拉伸只支持空实体零件的第一个基础拉伸；没有另建零件代替。");return error;}
    if((error=ProSolidFeatVisit((ProSolid)model,template_feature,NULL,&before)))return error;
    error=model_build_internal(&plan,name,&stage,model,&restored,&created);
    if(!error && snapshot->in_assembly){
        error=part_regenerate(snapshot->window_model);
        if(error){
            restored=!model_remove_feature((ProSolid)model,created.id);
            if(part_regenerate(model))restored=0;
            if(part_regenerate(snapshot->window_model))restored=0;
            stage="Regenerate owning assembly";
        }
    }
    if(error){
        if(restored && (ProSolidFeatVisit((ProSolid)model,template_feature,NULL,&after)||after.total!=before.total||after.planes!=before.planes||after.csys!=before.csys||template_empty((ProSolid)model)))restored=0;
        swprintf_s(text,capacity,L"当前模型拉伸失败：%hs，错误码%d；%ls；未保存。",stage,error,restored?L"目标空零件状态已检查保留/恢复":L"回滚未能验证，请检查模型，不应继续操作");return error;
    }
    if(!ProWindowCurrentGet(&window))ProWindowRepaint(window);
    swprintf_s(text,capacity,L"已在当前零件 %ls 内创建%ls基础拉伸，特征ID=%d；体积及重生成校验通过。未新建零件，未自动保存。%ls 基础拉伸位于零件局部XY平面、沿+Z；保留原基准但不随基准面移动。",name,plan.cylinder?L"圆形":L"矩形",created.id,snapshot->in_assembly?L"已重生成所在装配；此修改影响同一零件的全部实例。":L"");
    return 0;
}
static ProError part_execute(const PartCommand *cmd,const PartSnapshot *snapshot,wchar_t *text,size_t capacity){
    ProError e;ProMdl model=NULL;ProMdlName name;text[0]=0;
    if((e=part_check_binding(cmd,snapshot,&model))){swprintf_s(text,capacity,L"未执行：活动模型、工作目录或模型版本与请求时不一致。请针对当前模型重新发送指令（错误码%d）。",e);return e;}
    if(cmd->verb!=PART_READ && (snapshot->unhealthy || part_health(model))){wcscpy_s(text,capacity,L"未执行：当前零件存在失败特征，先修复重生成问题。");return PRO_TK_BAD_CONTEXT;}
    if(snapshot->type==PRO_MDL_ASSEMBLY && cmd->verb!=PART_READ && cmd->verb!=PART_DIMS && cmd->verb!=PART_PARAM && cmd->verb!=PART_REGEN && cmd->verb!=PART_SAVE){
        wcscpy_s(text,capacity,L"当前活动目标是装配本身。已支持装配信息读取、驱动尺寸/参数修改及重生成/保存；拉伸需要先激活装配中的实体零件，不会自动挑选组件。");return PRO_TK_BAD_CONTEXT;
    }
    switch(cmd->verb){
    case PART_EXTRUDE:e=part_extrude_current(cmd,snapshot,model,text,capacity);break;
    case PART_READ:part_snapshot_format_basic(snapshot,text,capacity);return 0;
    case PART_DIMS:e=part_change_dimensions(cmd,snapshot,model,text,capacity);break;
    case PART_PARAM:e=part_change_parameter(cmd,snapshot,model,text,capacity);break;
    case PART_SUPPRESS:case PART_RESUME:e=part_toggle_feature(cmd,snapshot,model,text,capacity);break;
    case PART_REGEN:e=part_regenerate(model);if(!e)swprintf_s(text,capacity,L"零件 %ls 已重生成，特征状态检查通过；未保存。",snapshot->name);break;
    case PART_DISPLAY:{
        ProMdl target=NULL;ProMdlName target_name;ProPath dir,path;int window;
        wcscpy_s(target_name,PRO_NAME_SIZE,cmd->arg[2]);e=ProMdlnameInit(target_name,PRO_MDL_PART,&target);
        if(e){
            e=ProDirectoryCurrentGet(dir);
            if(!e&&swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls.prt",dir,target_name)<0)e=PRO_TK_BAD_INPUTS;
            if(!e)e=ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&target);
        }
        if(!e)e=ProMdlMdlnameGet(target,name);
        if(!e&&_wcsicmp(name,target_name))e=PRO_TK_BAD_CONTEXT;
        if(!e)e=ProObjectwindowMdlnameCreate(target_name,PRO_PART,&window);
        if(!e)e=ProWindowCurrentSet(window);
        if(!e)e=ProMdlDisplay(target);
        if(!e)e=ProWindowActivate(window);
        if(!e)swprintf_s(text,capacity,L"已请求显示并激活零件 %ls，未保存或修改模型数据。",target_name);break;
    }
    case PART_COPY:{
        ProMdl copy=NULL;ProMdlName target;ProMdlType type;
        if(!part_target_available(model,cmd->arg[2],L"prt")){e=PRO_TK_BAD_CONTEXT;break;}
        wcscpy_s(target,PRO_NAME_SIZE,cmd->arg[2]);e=ProMdlnameCopy(model,target,&copy);
        if(!e)e=ProMdlMdlnameGet(copy,name);if(!e)e=ProMdlTypeGet(copy,&type);
        if(!e&&(_wcsicmp(name,target)||type!=PRO_MDL_PART))e=PRO_TK_GENERAL_ERROR;
        if(!e)e=part_regenerate(copy);
        if(!e)swprintf_s(text,capacity,L"已复制 %ls 为 %ls.prt，类型及重生成检查通过；副本未保存，也未切换当前窗口。",snapshot->name,name);break;
    }
    case PART_RENAME:{
        ProMdlName target;wcscpy_s(target,PRO_NAME_SIZE,cmd->arg[2]);
        if(!part_target_available(model,target,L"prt")){e=PRO_TK_BAD_CONTEXT;break;}
        e=ProMdlnameRename(model,target);if(!e)e=ProMdlMdlnameGet(model,name);
        if(!e&&_wcsicmp(name,target))e=PRO_TK_GENERAL_ERROR;
        if(e){ProMdlName old;wcscpy_s(old,PRO_NAME_SIZE,snapshot->name);if(ProMdlnameRename(model,old))wcscpy_s(text,capacity,L"重命名失败且恢复原名未成功，请检查模型；未保存。");}
        else swprintf_s(text,capacity,L"已将 %ls 重命名为 %ls，名称读回通过；未保存，也未删除原磁盘文件。",snapshot->name,name);break;
    }
    case PART_SAVE:{
        ProPath before,after;
        e=ProDirectoryCurrentGet(before);if(!e)e=part_regenerate(model);if(!e)e=ProMdlSave(model);
        if(!e)e=ProDirectoryCurrentGet(after);
        if(!e&&_wcsicmp(before,after))e=PRO_TK_BAD_CONTEXT;
        if(!e)swprintf_s(text,capacity,L"零件 %ls 重生成检查通过，Creo 已报告保存成功；工作目录：%ls。未清理旧版本。",snapshot->name,before);break;
    }
    case PART_STEP:{
        ProPath dir,path,base;WIN32_FILE_ATTRIBUTE_DATA info;
        if(!part_target_available(model,cmd->arg[2],L"stp")||!part_target_available(model,cmd->arg[2],L"step")){e=PRO_TK_BAD_CONTEXT;break;}
        e=ProDirectoryCurrentGet(dir);
        if(!e&&swprintf_s(base,PRO_PATH_SIZE,L"%ls\\%ls",dir,cmd->arg[2])<0)e=PRO_TK_BAD_INPUTS;
        /* PTC explicitly requires a filename WITHOUT its extension. */
        if(!e)e=ProIntf3DFileWriteWithDefaultProfile((ProSolid)model,PRO_INTF_EXPORT_STEP,base);
        if(!e&&swprintf_s(path,PRO_PATH_SIZE,L"%ls.stp",base)<0)e=PRO_TK_BAD_INPUTS;
        if(!e&&GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES){if(swprintf_s(path,PRO_PATH_SIZE,L"%ls.step",base)<0)e=PRO_TK_BAD_INPUTS;}
        if(!e&&(!GetFileAttributesExW(path,GetFileExInfoStandard,&info)||(!info.nFileSizeLow&&!info.nFileSizeHigh)))e=PRO_TK_GENERAL_ERROR;
        if(!e)swprintf_s(text,capacity,L"STEP导出成功，已核对输出文件非空：%ls。原零件未自动保存。",path);break;
    }
    default:e=PRO_TK_BAD_INPUTS;break;
    }
    if(e&&!text[0])swprintf_s(text,capacity,L"原生操作未完成：%ls，错误码%d。未宣称成功，未自动保存。可能原因包括数据已变化、参数受限或输出重名；请读取模型后核对。",cmd->arg[0],e);
    trace("Independent part tool result",e);return e;
}
#endif
