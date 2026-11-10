#ifndef BWTL_QUALITY_PLAN_H
#define BWTL_QUALITY_PLAN_H

enum QualityCommand {
    QA_NONE, QA_MEASURE, QA_WALL, QA_HOLES, QA_DRAFT, QA_CONTINUITY,
    QA_MASS, QA_SYMMETRY, QA_COMPARE, QA_DEPS, QA_RULES
};
enum QualityRule {
    QR_MIN_VOLUME, QR_MAX_VOLUME, QR_REQUIRE_PARAM, QR_MIN_WALL,
    QR_MIN_HOLE_GAP, QR_MIN_EDGE_GAP, QR_MIN_DRAFT
};

static int qa_kind(const wchar_t *verb){
    static const wchar_t *const names[]={
        L"QA_MEASURE",L"QA_WALL",L"QA_HOLES",L"QA_DRAFT",L"QA_CONTINUITY",
        L"QA_MASS",L"QA_SYMMETRY",L"QA_COMPARE",L"QA_DEPS",L"QA_RULES"
    };
    for(int i=0;i<_countof(names);i++)
        if(!wcscmp(verb,names[i]))return i+1;
    return QA_NONE;
}

static int qa_number(const wchar_t *token,double low,double high){
    double value;
    return part_number(token,&value)&&value>=low&&value<=high;
}

static int qa_ref(const wchar_t *token,const wchar_t *prefix){
    size_t length=wcslen(prefix);
    int id;
    return !wcsncmp(token,prefix,length)&&part_integer(token+length,&id)&&id>0;
}

static int qa_surface_token(const wchar_t *token){return qa_ref(token,L"SURFACE:");}
static int qa_edge_token(const wchar_t *token){return qa_ref(token,L"EDGE:");}
static int qa_plane_token(const wchar_t *token){
    return qa_surface_token(token)||(!wcsncmp(token,L"PLANE:",6)&&fd_name(token+6));
}

static int qa_measure_ref(const wchar_t *token){
    return qa_plane_token(token)||qa_edge_token(token)||
        (!wcsncmp(token,L"POINT:",6)&&fd_name(token+6))||
        (!wcsncmp(token,L"AXIS:",5)&&fd_name(token+5));
}

static int qa_axis(const wchar_t *token){
    return wcslen(token)==1&&wcschr(L"XYZ",token[0])!=NULL;
}

typedef struct {
    const wchar_t *name;
    int arguments;
} QualityRuleSpec;

static const QualityRuleSpec qa_rule_specs[]={
    {L"MIN_VOLUME",1},{L"MAX_VOLUME",1},{L"REQUIRE_PARAM",1},
    {L"MIN_WALL",3},{L"MIN_HOLE_GAP",3},{L"MIN_EDGE_GAP",3},{L"MIN_DRAFT",3}
};

static int qa_rule_find(const wchar_t *name){
    for(int i=0;i<_countof(qa_rule_specs);i++)
        if(!wcscmp(name,qa_rule_specs[i].name))return i;
    return -1;
}

static int qa_rules_valid(int count,wchar_t args[][PART_TOKEN_MAX]){
    for(int i=2;i<count;){
        int rule=qa_rule_find(args[i]);
        if(rule<0||i+qa_rule_specs[rule].arguments>=count)return 0;
        if(rule==QR_REQUIRE_PARAM){
            if(!fd_name(args[i+1]))return 0;
        }else if(rule==QR_MIN_VOLUME||rule==QR_MAX_VOLUME){
            if(!qa_number(args[i+1],0,1e15))return 0;
        }else{
            if(!qa_surface_token(args[i+1]))return 0;
            if(rule==QR_MIN_DRAFT){
                if(!qa_axis(args[i+2])||!qa_number(args[i+3],0,90))return 0;
            }else if((rule==QR_MIN_EDGE_GAP?!qa_edge_token(args[i+2]):!qa_surface_token(args[i+2]))||
                     !qa_number(args[i+3],0,1e6))return 0;
        }
        i+=qa_rule_specs[rule].arguments+1;
    }
    return count>=4;
}

static int quality_plan_valid(int count,wchar_t args[][PART_TOKEN_MAX]){
    int kind=qa_kind(args[0]),resolution;
    if(!kind)return 0;
    switch(kind){
    case QA_MEASURE:
        if(count==4){
            if(!wcscmp(args[2],L"LENGTH"))return qa_edge_token(args[3])?1:-1;
            if(!wcscmp(args[2],L"AREA")||!wcscmp(args[2],L"DIAMETER"))
                return qa_surface_token(args[3])?1:-1;
        }
        if(count==5){
            if(!wcscmp(args[2],L"DISTANCE"))return qa_measure_ref(args[3])&&qa_measure_ref(args[4])?1:-1;
            if(!wcscmp(args[2],L"ANGLE"))return qa_edge_token(args[3])&&qa_edge_token(args[4])?1:-1;
        }
        return -1;
    case QA_WALL:
        if(count<5||count>15||count%2!=1||!qa_number(args[2],0,1e6))return -1;
        for(int i=3;i<count;i++)if(!qa_surface_token(args[i]))return -1;
        return 1;
    case QA_HOLES:
        if(count!=6||!qa_surface_token(args[3])||!qa_number(args[5],0,1e6))return -1;
        if(!wcscmp(args[2],L"PAIR"))return qa_surface_token(args[4])?1:-1;
        if(!wcscmp(args[2],L"EDGE"))return qa_edge_token(args[4])?1:-1;
        return -1;
    case QA_DRAFT:
        return count==6&&qa_surface_token(args[2])&&qa_axis(args[3])&&
            (!wcscmp(args[4],L"POS")||!wcscmp(args[4],L"NEG"))&&qa_number(args[5],0,90)?1:-1;
    case QA_CONTINUITY:
        return count==5&&qa_edge_token(args[2])&&qa_number(args[3],0.001,30)&&qa_number(args[4],0.000001,100)?1:-1;
    case QA_MASS:
        return count==2?1:-1;
    case QA_SYMMETRY:case QA_COMPARE:
        return count==5&&(kind==QA_SYMMETRY?qa_plane_token(args[2]):part_name(args[2]))&&
            qa_number(args[3],0.000001,100)&&part_integer(args[4],&resolution)&&resolution>=7&&resolution<=25?1:-1;
    case QA_DEPS:
        return count==5&&(qa_ref(args[2],L"ID:")||fd_name(args[2]))&&
            (!wcscmp(args[3],L"UP")||!wcscmp(args[3],L"DOWN")||!wcscmp(args[3],L"BOTH"))&&
            part_integer(args[4],&resolution)&&resolution>=1&&resolution<=5?1:-1;
    case QA_RULES:
        return qa_rules_valid(count,args)?1:-1;
    default:
        return -1;
    }
}
#endif
