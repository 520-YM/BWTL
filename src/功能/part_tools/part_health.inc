static ProError part_health(ProMdl model){ProError e=ProSolidFeatVisit((ProSolid)model,part_health_visit,NULL,NULL);return e==PRO_TK_E_NOT_FOUND?0:e;}
