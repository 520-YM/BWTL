static const wchar_t *model_request_first(const wchar_t *text,const wchar_t *const *terms,size_t count){
    const wchar_t *first=NULL;
    size_t i;
    for(i=0;i<count;i++){
        const wchar_t *match=wcsstr(text,terms[i]);
        if(match&&(!first||match<first))first=match;
    }
    return first;
}

static const wchar_t *const model_existing_targets[]={L"当前零件",L"当前模型",L"当前装配",L"这个零件",L"这个模型",L"该零件",L"现有零件",L"现有模型",L"已有零件",L"已有模型",
        L"原零件",L"原模型",L"不新建",L"不要新建",L"别新建",L"不另建",L"不要另建"};

/* Find a base-model creation clause before considering downstream features. */
static const wchar_t *model_request_new_base(const wchar_t *text){
    const wchar_t *verbs[]={L"新建",L"另建",L"创建",L"建立",L"画",L"做",L"建",L"生成"};
    const wchar_t *bases[]={L"圆柱",L"长方体",L"正方体",L"立方体",L"方块",L"矩形板",L"板",L"圆筒",L"水杯",L"轴套",L"零件",L"装配",L"示例件",L"样件",L"试件"};
    const wchar_t *feature_suffixes[]={L"面",L"壁",L"族",L"孔",L"形孔",L"形的孔",L"形凸台",L"凸台",L"切口",L"轴",L"特征",L"草绘",L"加工信息表",L"上的",L"的"};
    const wchar_t *verb,*base,*end,*suffix,*artifact=model_artifact_request(text);
    if(artifact){
        const wchar_t *existing=model_request_first(artifact,model_existing_targets,_countof(model_existing_targets));
        if(existing&&existing<artifact+wcscspn(artifact,L"，,。；;！!？?\r\n"))artifact=NULL;
    }
    while((verb=model_request_first(text,verbs,_countof(verbs)))!=NULL){
        end=verb+wcscspn(verb,L"，,。；;！!？?\r\n");
        base=model_request_first(verb+1,bases,_countof(bases));
        if(base&&base<end){
            size_t i;
            for(i=0;i<_countof(bases);i++)if(!wcsncmp(base,bases[i],wcslen(bases[i])))break;
            suffix=base+wcslen(bases[i]);
            if(model_request_first(suffix,feature_suffixes,_countof(feature_suffixes))!=suffix)return artifact&&artifact<verb?artifact:verb;
        }
        text=end+(*end!=0);
    }
    return artifact;
}

static int model_request_has_operations(const wchar_t *text){
    const wchar_t *operations[]={L"拉伸",L"曲面",L"多实体",L"布尔",L"缝合",L"加厚",L"补面",L"扫描",L"放样",L"变径",L"旋转切除",L"角度基准面",L"三点基准面",L"中分面",L"相切基准面",L"路径法向基准面",L"两点基准轴",
        L"曲线定位点",L"曲面交线",L"三维样条",L"对称基准坐标系",L"孔",L"键槽",L"卡簧槽",L"点平",L"螺钉柱",L"卡扣",L"密封",L"毛坯",L"钣金",L"板壁",L"法兰",L"折弯",L"折边",L"卷边",L"避让",L"展开",L"折回",L"压筋",L"下料",L"SMT_",L"特征组",L"编组",L"参数",L"关系式",L"零件族",L"UDF",L"REUSE_",L"圆角",L"倒角",L"抽壳",L"拔模",L"轮廓筋",L"加强筋",
        L"镜像",L"阵列",L"删除特征",L"草绘",L"HOLE",L"EDGE_",L"SOLID_SHELL",L"QA_",L"测量",L"壁厚",L"拔模角",L"连续性",L"质量属性",L"几何对比",L"对称性",L"特征依赖",L"设计规则",L"DV_",L"导入",L"重定义",L"特征顺序",L"加工信息表",L"打包",L"零件属性",L"批量导出"};
    return model_request_first(text,operations,_countof(operations))!=NULL;
}

static int model_request_current(const wchar_t *text){
    const wchar_t *current=model_request_first(text,model_existing_targets,_countof(model_existing_targets));
    const wchar_t *new_base=model_request_new_base(text);
    if(new_base&&(!current||new_base<current))return 0;
    return current||wcsstr(text,L"当前")||model_request_has_operations(text);
}
