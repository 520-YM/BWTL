
#define AX_GROUP_HOLES 12
#define AX_GROUP_VISITS 20000

typedef struct {
    wchar_t token[48];
    AxHardwareSite geometry;
} AxGroupHole;

static double ax_group_distance(const AxGroupHole *a,const AxGroupHole *b,double mm){
    ProVector delta;
    ad_sub(a->geometry.contact,b->geometry.contact,delta);
    return sqrt(ad_dot(delta,delta))*mm;
}

static double ax_group_turn(const AxGroupHole *a,const AxGroupHole *b,const AxGroupHole *c,double mm){
    ProVector u,v,cross;
    ad_sub(b->geometry.contact,a->geometry.contact,u);
    ad_sub(c->geometry.contact,a->geometry.contact,v);
    ad_cross(u,v,cross);
    return ad_dot(cross,a->geometry.plane.direction)*mm*mm;
}

static ProError ax_group_read(ProMdl assembly,const wchar_t *owner,const wchar_t *face,const wchar_t *list,
    double mm,AxGroupHole holes[AX_GROUP_HOLES],int *count,PartText *out){
    AxComponent component={0};
    ProError error=ax_hw_occurrence(assembly,owner,mm,&component);
    if(error)return error;
    wchar_t copy[MAX_PATH],*remaining=NULL;
    if(wcslen(list)>=_countof(copy)||!*list||list[0]==L','||list[wcslen(list)-1]==L','||wcsstr(list,L",,"))
        return PRO_TK_BAD_INPUTS;
    wcscpy_s(copy,_countof(copy),list);
    *count=0;
    for(wchar_t *token=wcstok_s(copy,L",",&remaining);token;token=wcstok_s(NULL,L",",&remaining)){
        if(*count==AX_GROUP_HOLES||wcslen(token)>=_countof(holes->token))return PRO_TK_OUT_OF_RANGE;
        AxGroupHole *hole=&holes[*count];
        error=ax_hw_site(&component,token,face,mm,&hole->geometry);
        if(!error)error=ax_hw_world(&component,&hole->geometry);
        if(error||!hole->geometry.bore){
            part_text_add(out,L"%ls 上的 %ls 无法读成经过指定配合面的内孔。\n",owner,token);
            return error?error:PRO_TK_INVALID_TYPE;
        }
        for(int i=0;i<*count;i++)if(ax_group_distance(&holes[i],hole,mm)<0.001){
            part_text_add(out,L"%ls 和 %ls 指向同一轴心，请各孔只选一个内圆柱面。\n",holes[i].token,token);
            return PRO_TK_E_FOUND;
        }
        wcscpy_s(hole->token,_countof(hole->token),token);
        (*count)++;
    }
    return *count?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
}

typedef struct {
    const AxGroupHole *source,*target;
    int source_count,target_count,anchor,visits,solutions;
    int attempt[AX_GROUP_HOLES],answer[AX_GROUP_HOLES];
    double mm,tolerance;
} AxGroupSearch;

static int ax_group_accept(AxGroupSearch *s,int i,int j){
    if(fabs(s->source[i].geometry.cylinder.radius-s->target[j].geometry.cylinder.radius)*2*s->mm>s->tolerance)
        return 0;
    for(int k=0;k<i;k++){
        double from=ax_group_distance(&s->source[i],&s->source[k],s->mm);
        double to=ax_group_distance(&s->target[j],&s->target[s->attempt[k]],s->mm);
        if(fabs(from-to)>s->tolerance)return 0;
        for(int l=0;l<k;l++){
            double a=ax_group_turn(&s->source[l],&s->source[k],&s->source[i],s->mm);
            double b=ax_group_turn(&s->target[s->attempt[l]],&s->target[s->attempt[k]],&s->target[j],s->mm);
            double scale=ax_group_distance(&s->source[k],&s->source[l],s->mm)+from;
            if(fabs(a)>s->tolerance*scale&&a*b<=0)return 0;
        }
    }
    return 1;
}

static void ax_group_search(AxGroupSearch *s,int i,unsigned used){
    if(s->solutions>1||++s->visits>AX_GROUP_VISITS)return;
    if(i==s->source_count){
        if(!s->solutions)memcpy(s->answer,s->attempt,sizeof(s->answer));
        s->solutions++;
        return;
    }
    for(int j=0;j<s->target_count;j++){
        if(!i&&s->anchor>=0&&j!=s->anchor)continue;
        if((used&(1u<<j))||!ax_group_accept(s,i,j))continue;
        s->attempt[i]=j;
        ax_group_search(s,i+1,used|(1u<<j));
    }
}

static ProError ax_group_compare(ProMdl assembly,const wchar_t *from,const wchar_t *from_face,const wchar_t *sources,
    const wchar_t *to,const wchar_t *to_face,const wchar_t *targets,double tolerance,int anchor,PartText *out){
    AxGroupHole source[AX_GROUP_HOLES],target[AX_GROUP_HOLES];
    AxGroupSearch search={.source=source,.target=target,.anchor=anchor,.tolerance=tolerance};
    if(!assembly||!isfinite(tolerance)||tolerance<0.001||tolerance>5)return PRO_TK_BAD_INPUTS;
    ProError error=qa_mm_per_unit(assembly,&search.mm);
    if(!error)error=ax_group_read(assembly,from,from_face,sources,search.mm,source,&search.source_count,out);
    if(!error)error=ax_group_read(assembly,to,to_face,targets,search.mm,target,&search.target_count,out);
    if(error)return error;
    if(search.source_count<2||search.target_count<search.source_count||anchor>=search.target_count){
        part_text_add(out,L"源孔组至少两个孔，新侧孔数不能少于源侧，固定孔序号不能超范围。\n");
        return PRO_TK_BAD_INPUTS;
    }
    ax_group_search(&search,0,0);
    if(search.solutions!=1||search.visits>AX_GROUP_VISITS){
        part_text_add(out,search.visits>AX_GROUP_VISITS?L"候选组合太多，缩小范围或指定首孔。\n":
            search.solutions?L"存在多个成组匹配，指定首孔或补充不对称孔。\n":L"孔组的孔径、孔距或配合面朝向不吻合。\n");
        return search.solutions?PRO_TK_E_FOUND:PRO_TK_E_NOT_FOUND;
    }
    part_text_add(out,L"实测得到唯一的 %d 孔对应，公差 %.6g mm。\n",search.source_count,tolerance);
    for(int i=0;i<search.source_count;i++){
        AxGroupHole *a=&source[i],*b=&target[search.answer[i]];
        part_text_add(out,L"%ls → %ls，直径 %.6g → %.6g mm，距首孔 %.6g → %.6g mm。\n",
            a->token,b->token,a->geometry.cylinder.radius*2*search.mm,b->geometry.cylinder.radius*2*search.mm,
            ax_group_distance(&source[0],a,search.mm),ax_group_distance(&target[search.answer[0]],b,search.mm));
    }
    return PRO_TK_NO_ERROR;
}


static ProError ax_group_stations(ProMdl assembly,const wchar_t *owner,const wchar_t *face,const wchar_t *part,
    const wchar_t *shaft,const wchar_t *seat,const wchar_t *listed,int install,PartText *out){
    if(!assembly||!ax_study_key(part))return PRO_TK_BAD_INPUTS;
    double mm;
    int count=0,occupied[AX_GROUP_HOLES]={0};
    AxGroupHole holes[AX_GROUP_HOLES];
    AxHardwareSite sites[AX_GROUP_HOLES];
    ProError error=qa_mm_per_unit(assembly,&mm);
    if(!error)error=ax_group_read(assembly,owner,face,listed,mm,holes,&count,out);
    if(error)return error;
    for(int i=0;i<count;i++)sites[i]=holes[i].geometry;
    error=ax_hw_count_placements(assembly,part,shaft,sites,count,mm,occupied);
    if(error)return error;

    PartCommand cmd={.count=7};
    wcscpy_s(cmd.arg[2],PART_TOKEN_MAX,owner);
    wcscpy_s(cmd.arg[3],PART_TOKEN_MAX,face);
    wcscpy_s(cmd.arg[4],PART_TOKEN_MAX,part);
    wcscpy_s(cmd.arg[5],PART_TOKEN_MAX,shaft);
    wcscpy_s(cmd.arg[6],PART_TOKEN_MAX,seat);
    int duplicate=0;
    for(int i=0;i<count;i++){
        duplicate+=occupied[i]>1;
        if(!occupied[i])wcscpy_s(cmd.arg[cmd.count++],PART_TOKEN_MAX,holes[i].token);
    }
    const int changed=install&&cmd.count>7&&!duplicate;
    if(changed){
        ax_recovery_uncertain=0;
        error=ax_hw_place_holes(assembly,&cmd,AX_HOLE_ARRAY,mm,out);
        if(error)return error;
        memset(occupied,0,sizeof(occupied));
        error=ax_hw_count_placements(assembly,part,shaft,sites,count,mm,occupied);
        if(error)return error;
    }
    int missing=0; duplicate=0;
    for(int i=0;i<count;i++){
        missing+=occupied[i]==0;
        duplicate+=occupied[i]>1;
        double nearest=INFINITY;
        for(int j=0;j<count;j++)if(i!=j)nearest=fmin(nearest,ax_group_distance(&holes[i],&holes[j],mm));
        part_text_add(out,L"站位 %d %ls，孔径 %.6g mm，%ls（%d 个），",i+1,holes[i].token,
            2*sites[i].cylinder.radius*mm,!occupied[i]?L"空位":occupied[i]>1?L"重复":L"已装",occupied[i]);
        if(isfinite(nearest))part_text_add(out,L"最近站位 %.6g mm。\n",nearest);
        else part_text_add(out,L"单站位。\n");
    }
    part_text_add(out,L"读回 %d 个实际孔位，空位 %d，重复 %d。%ls\n",count,missing,duplicate,
        install&&duplicate?L"存在重复实例，保持装配原样。":changed?L"本次新增待保存。":L"");
    return install&&(missing||duplicate)?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
