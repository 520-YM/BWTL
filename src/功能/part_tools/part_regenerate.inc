static ProError part_regenerate(ProMdl model){ProError e=ProSolidRegenerate((ProSolid)model,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);return e?e:part_health(model);}
