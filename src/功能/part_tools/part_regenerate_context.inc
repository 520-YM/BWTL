static ProError part_regenerate_context(ProMdl model,const PartSnapshot *snapshot){ProError e=part_regenerate(model);if(!e&&snapshot->in_assembly)e=part_regenerate(snapshot->window_model);return e;}
