static const wchar_t *model_request_first(const wchar_t *text,const wchar_t *const *terms,size_t count){
    const wchar_t *first=NULL;
    size_t i;
    for(i=0;i<count;i++){
        const wchar_t *match=wcsstr(text,terms[i]);
        if(match&&(!first||match<first))first=match;
    }
    return first;
}

static const wchar_t *const model_existing_targets[]={L"当前零件",L"当前模型",L"当前装配",L"这个零件",L"这个模型",L"这个装配",L"该零件",L"该装配",L"现有零件",L"现有模型",L"现有装配",L"已有零件",L"已有模型",
        L"原零件",L"原模型",L"在装配体内",L"装配里面",L"装配里",L"装配内",L"装配中",L"选中装配",L"选中的装配",L"不新建",L"不要新建",L"别新建",L"不另建",L"不要另建"};

/* Find a base-model creation clause before considering downstream features. */
static const wchar_t *model_request_new_base(const wchar_t *text){
    const wchar_t *verbs[]={L"新建",L"另建",L"创建",L"建立",L"画",L"做",L"建",L"生成"};
    const wchar_t *bases[]={L"钣金空零件",L"钣金零件",L"钣金件",L"钣金平面",L"钣金平板",L"钣金板",L"圆柱",L"长方体",L"正方体",L"立方体",L"方块",L"矩形板",L"板",L"圆筒",L"水杯",L"轴套",L"零件",L"装配",L"示例件",L"样件",L"试件"};
    const wchar_t *feature_suffixes[]={L"面",L"壁",L"族",L"孔",L"形孔",L"形的孔",L"形凸台",L"凸台",L"切口",L"切除",L"轴",L"特征",L"首壁",L"约束",L"参照",L"定位",L"参数",L"属性",L"草绘",L"加工信息表",L"上的",L"的"};
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

/* Limit automatic NEW routing to direct requests naming a whole model.
   Ambiguous clauses and reports continue through semantic routing. */
static const wchar_t *model_request_skip_words(const wchar_t *text,const wchar_t *const *words,size_t count){
    for(;;){
        while(iswspace(*text))text++;
        size_t i;
        for(i=0;i<count;i++){
            size_t length=wcslen(words[i]);
            if(!wcsncmp(text,words[i],length)){text+=length;break;}
        }
        if(i==count)return text;
    }
}

static const wchar_t *model_request_action_start(const wchar_t *text){
    static const wchar_t *const prefixes[]={L"另外",L"现在",L"直接",L"请你",L"请",L"麻烦你",L"麻烦",L"帮我",L"给我",L"替我"};
    const wchar_t *start=model_request_skip_words(text,prefixes,_countof(prefixes));
    if(start>text&&wcschr(L"，,",*start)&&*start)
        start=model_request_skip_words(start+1,prefixes,_countof(prefixes));
    return start;
}

static int model_request_explicit_new(const wchar_t *text){
    static const wchar_t *const verbs[]={L"新建",L"另建",L"创建",L"建立",L"生成",L"绘制",L"画",L"做",L"建"};
    static const wchar_t *const qualifiers[]={L"一个",L"一块",L"一张",L"一件",L"一套",L"个",L"块",L"张",L"件",L"套",L"独立",L"新的",L"空白",L"矩形"};
    static const wchar_t *const models[]={L"钣金空零件",L"钣金零件",L"钣金平面",L"钣金平板",L"钣金板",L"钣金件",L"平板",
        L"空装配体",L"空装配",L"总装配",L"装配体",L"装配",L"实体零件",L"空零件",L"零件",
        L"长方体",L"正方体",L"立方体",L"圆柱",L"圆筒",L"水杯",L"轴套",L"板"};
    static const wchar_t *const discussion[]={L"怎么",L"如何",L"为什么",L"能否",L"举例",L"例如",L"示例说明",L"要多久",L"有哪些",L"吗",L"？",L"?",
        L"不创建",L"不新建",L"不另建",L"不用创建",L"不用新建",L"不需要创建",L"不需要新建",L"不要新建",L"不要创建",L"不要另建",L"别新建",L"别创建",L"别另建",
        L"不要执行",L"别执行",L"不要建模",L"别建模",L"不要实际建模",L"别实际建模",L"不实际建模",L"只要方案",L"只讲",L"只分析",L"仅分析"};
    static const wchar_t *const feature[]={L"壁",L"首壁",L"孔",L"形孔",L"的",L"族",L"特征",L"切口",L"切除",L"草绘",L"草图",L"参数",L"属性",
        L"关系",L"图",L"约束",L"参照",L"定位",L"清单",L"加工",L"交付",L"模板",L"基准",L"教程",L"方法",L"步骤",L"方案",L"功能",L"报告",
        L"里",L"内",L"中",L"上",L"下",L"插入",L"装入",L"放入",L"放进"};
    static const wchar_t *const preserve[]={L"别动",L"不动",L"不要动",L"别改",L"不改",L"不要改",L"不修改",L"不要修改",L"保留"};
    if(model_request_first(text,discussion,_countof(discussion)))return 0;
    const wchar_t *start=model_request_action_start(text);
    size_t i;
    for(i=0;i<_countof(verbs);i++){
        size_t length=wcslen(verbs[i]);
        if(!wcsncmp(start,verbs[i],length)){start+=length;break;}
    }
    if(i==_countof(verbs))return 0;
    const wchar_t *noun=model_request_skip_words(start,qualifiers,_countof(qualifiers));
    for(i=0;i<_countof(models);i++){
        size_t length=wcslen(models[i]);
        if(!wcsncmp(noun,models[i],length)){noun+=length;break;}
    }
    if(i==_countof(models))return 0;
    while(iswspace(*noun))noun++;
    if(model_request_first(noun,feature,_countof(feature))==noun)return 0;
    for(const wchar_t *scan=text,*target;(target=model_request_first(scan,model_existing_targets,_countof(model_existing_targets)))!=NULL;scan=target+1){
        for(i=0;i<_countof(preserve);i++){
            size_t length=wcslen(preserve[i]);
            const wchar_t *before=target;
            while(before>text&&iswspace(before[-1]))before--;
            if((size_t)(before-text)>=length&&!wcsncmp(before-length,preserve[i],length))break;
        }
        if(i==_countof(preserve))return 0;
    }
    return 1;
}
