#ifdef BWTL_REUSE_DIAGNOSTIC_TEST
static int ux_test_group_fail=0;
#endif
static ProError ux_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
 int kind=ux_kind(cmd->arg[0]),bodies=0;FdTxn tx={0};ProError e;double volume=0;ProMdlsubtype subtype;
 if(reuse_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
 if(kind==UX_INFO)return ux_info(model,report,capacity);if(kind==UX_UDF_INFO||kind==UX_UDF_SAVE)return ux_udf(model,cmd,NULL,report,capacity);
 if(ux_recovery_uncertain||mc_recovery_uncertain)return PRO_TK_BAD_CONTEXT;
 if(kind==UX_PARAM)return ux_param(model,cmd,report,capacity);if(kind==UX_RELATION)return ux_relation(model,cmd,report,capacity);if(kind==UX_FAMILY)return ux_family(model,cmd,report,capacity);
 if((e=ProMdlSubtypeGet(model,&subtype)))return e;if(subtype==PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_BAD_CONTEXT;
 if(kind==UX_SKIP)return ux_skip(model,cmd,report,capacity);
 if((e=fd_begin(&tx,model,cmd->arg[2],kind==UX_POINT_SKETCH?PRO_FEAT_CURVE:PRO_FEAT_GROUP_HEAD)))goto done;
 if((e=sd_body_count(model,&bodies))||(kind!=UX_POINT_SKETCH&&(e=ux_solid(model,&volume))))goto done;
 switch(kind){case UX_MIRROR:e=ux_mirror(&tx,cmd);break;case UX_GRID:case UX_CURVE:case UX_POINTS:e=ux_pattern(&tx,cmd,kind);break;case UX_POINT_SKETCH:e=ux_point_sketch(&tx,cmd);break;case UX_GROUP:e=ux_group(&tx,cmd);break;case UX_UDF_PLACE:e=ux_udf(model,cmd,&tx,report,capacity);break;default:e=PRO_TK_BAD_INPUTS;}
 if(!e)e=part_regenerate(model);
 if(!e&&kind!=UX_POINT_SKETCH){e=ux_solid(model,&volume);if(!e&&kind==UX_GROUP&&!part_close(volume,tx.volume))e=PRO_TK_GENERAL_ERROR;if(!e&&kind!=UX_GROUP&&kind!=UX_UDF_PLACE&&part_close(volume,tx.volume))e=PRO_TK_GENERAL_ERROR;}
#ifdef BWTL_REUSE_DIAGNOSTIC_TEST
 if(!e&&kind==UX_GROUP&&ux_test_group_fail)e=PRO_TK_GENERAL_ERROR;
#endif
 if(!e&&!report[0])swprintf_s(report,capacity,L"已完成%ls：%ls，原生特征、重生成与实体校验通过，未保存。",cmd->arg[0],cmd->arg[2]);
done:if(e&&kind==UX_GROUP&&tx.feature.owner==model&&tx.feature.id>0){ProGroup group=tx.feature;ProFeattype type;if(!ProFeatureTypeGet(&tx.feature,&type)&&type==PRO_FEAT_GROUP_HEAD&&(ProGroupUngroup(&group)||part_regenerate(model))){tx.rollback_ok=0;tx.before_count=0;/* Never delete a failed group header together with original members. */}}am_finish(&tx,e,bodies,report,capacity);if(e&&!tx.rollback_ok)ux_recovery_uncertain=1;return e;
}
