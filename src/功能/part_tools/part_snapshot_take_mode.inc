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
