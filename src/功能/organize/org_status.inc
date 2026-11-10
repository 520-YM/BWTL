typedef struct {PartText *out;int failed,affected,suppressed,rows;} OrganizeStatus;

static ProError org_status_visit(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    OrganizeStatus *scan=data;
    ProFeatStatus state;
    unsigned flags;
    ProError error=org_read_status(feature,&state,&flags);
    if(error)return error;
    const wchar_t *kind=NULL;
    if(flags&PRO_FEAT_STAT_FAILED){scan->failed++;kind=L"失败";}
    else if(flags&(PRO_FEAT_STAT_CHILD_OF_FAILED|PRO_FEAT_STAT_CHILD_OF_EXT_FAILED)){
        scan->affected++;kind=L"受上游失败影响";
    }else if(state==PRO_FEAT_UNREGENERATED){scan->affected++;kind=L"未重生成";}
    else if(state==PRO_FEAT_SUPPRESSED||state==PRO_FEAT_FAMTAB_SUPPRESSED||state==PRO_FEAT_PROG_SUPPRESSED||
            state==PRO_FEAT_SIMP_REP_SUPPRESSED){
        scan->suppressed++;
        kind=state==PRO_FEAT_FAMTAB_SUPPRESSED?L"族表抑制":state==PRO_FEAT_PROG_SUPPRESSED?L"程序抑制":
            state==PRO_FEAT_SIMP_REP_SUPPRESSED?L"简化表示抑制":L"已抑制";
    }
    if(kind&&scan->rows++<80){
        ProName name;
        qa_feature_label(feature,name);
        part_text_add(scan->out,L"ID:%d｜%ls｜%ls\n",feature->id,name,kind);
    }
    return PRO_TK_NO_ERROR;
}

static ProError org_status(const OrganizeView *view,PartText *out){
    OrganizeStatus scan={out,0,0,0,0};
    ProError error=ProSolidFeatVisit((ProSolid)view->model,org_status_visit,NULL,&scan);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error)part_text_add(out,L"失败 %d 个，受影响或未重生成 %d 个，抑制 %d 个。%ls",
        scan.failed,scan.affected,scan.suppressed,scan.rows>80?L"已列出前 80 条。":L"");
    return error;
}
