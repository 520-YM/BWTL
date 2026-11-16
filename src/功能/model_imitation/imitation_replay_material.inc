static int imitation_explicit_material(const ProMaterial *material)
{
    return material->matl_name[0]&&_wcsicmp(material->matl_name,L"PTC_SYSTEM_MTRL_PROPS");
}

static ProError imitation_material_body(ProMdl model,ProSolidBody *body)
{
    ProSolidBody *bodies=NULL;
    int count=0,active=0;
    ProError error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        ProBoolean construction;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(error||state!=PRO_BODY_STATE_ACTIVE)continue;
        error=ProSolidBodyIsConstruction(&bodies[i],&construction);
        if(!error&&!construction){
            *body=bodies[i];
            active++;
        }
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    return error?error:active==1?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError imitation_material_mass(ProMdl source,ProMdl target)
{
    ProMassProperty a,b;
    ProUnitsystem system_a,system_b;
    ProUnititem units_a={0},units_b={0};
    ProUnitConversion conversion={0};
    ProSolidBody original,created;
    ProError error=imitation_material_body(source,&original);
    if(!error)error=imitation_material_body(target,&created);
    if(!error)error=ProSolidBodyMassPropertyGet(&original,NULL,&a);
    if(!error)error=ProSolidBodyMassPropertyGet(&created,NULL,&b);
    if(!error)error=ProMdlPrincipalunitsystemGet(source,&system_a);
    if(!error)error=ProMdlPrincipalunitsystemGet(target,&system_b);
    if(!error)error=ProUnitsystemUnitGet(&system_a,PRO_UNITTYPE_MASS,&units_a);
    if(!error)error=ProUnitsystemUnitGet(&system_b,PRO_UNITTYPE_MASS,&units_b);
    if(!error)error=ProUnitConversionCalculate(&units_a,&units_b,&conversion);
    if(error)return error;
    const double expected=a.mass*conversion.scale+conversion.offset;
    return isfinite(expected)&&isfinite(b.mass)&&
        fabs(expected-b.mass)<=fmax(1e-12,fabs(expected)*1e-6)?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError imitation_material_import(ProMaterial *source,ProMdl target,const ProPath stage,
                                         ProMaterial *local)
{
    const wchar_t *name=source->matl_name;
    if(wcspbrk(name,L"\\/:*?\"<>|")||!wcscmp(name,L".")||!wcscmp(name,L".."))return PRO_TK_BAD_INPUTS;
    ProPath file;
    if(swprintf_s(file,_countof(file),L"%ls\\%ls.mtl",stage,name)<0)return PRO_TK_BAD_INPUTS;
    if(GetFileAttributesW(file)!=INVALID_FILE_ATTRIBUTES)return PRO_TK_E_IN_USE;
    ProError error=ProMaterialfileWrite(source,file);
    if(!error&&GetFileAttributesW(file)==INVALID_FILE_ATTRIBUTES)error=PRO_TK_CANT_WRITE;
    if(!error)error=ProMaterialfileRead((ProSolid)target,source->matl_name);
    if(!error){
        local->part=(ProSolid)target;
        wcscpy_s(local->matl_name,_countof(local->matl_name),name);
    }
    return error;
}

static ProError imitation_material_read(ProMdl model,ProMaterial *part,ProMaterial *body)
{
    ProSolidBody single;
    ProError error=imitation_material_body(model,&single);
    if(!error)error=ProMaterialCurrentGet((ProSolid)model,part);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error)error=ProSolidBodyMaterialGet(&single,body);
    return error;
}

static ProError imitation_material_replay(ProMdl source,ProMdl target,ProMdlsubtype subtype,
                                         const ProPath stage,PartText *out)
{
    if(subtype!=PROMDLSTYPE_PART_SOLID&&subtype!=PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_NO_ERROR;
    ProSolidBody body;
    ProError error=imitation_material_body(target,&body);
    if(error==PRO_TK_BAD_CONTEXT)return PRO_TK_NO_ERROR;
    ProMaterial old_part={0},old_body={0},new_part={0},new_body={0};
    if(!error)error=imitation_material_read(source,&old_part,&old_body);
    const int has_part=imitation_explicit_material(&old_part),has_body=imitation_explicit_material(&old_body);
    if(!error&&has_part)error=imitation_material_import(&old_part,target,stage,&new_part);
    if(!error&&has_part)error=ProMaterialCurrentSet(&new_part);
    if(!error&&has_body){
        if(has_part&&!_wcsicmp(old_part.matl_name,old_body.matl_name))new_body=new_part;
        else error=imitation_material_import(&old_body,target,stage,&new_body);
        if(!error)error=ProSolidBodyMaterialSet(&body,&new_body);
    }
    if(!error&&subtype==PROMDLSTYPE_PART_SHEETMETAL&&!has_part&&!has_body){
        ProMaterial system={L"PTC_SYSTEM_MTRL_PROPS",(ProSolid)target};
        ProMaterial current_part={0},current_body={0};
        error=imitation_material_read(target,&current_part,&current_body);
        if(!error&&imitation_explicit_material(&current_part))error=ProMaterialCurrentSet(&system);
        trace("Restore source unassigned sheet material",error);
        if(!error&&imitation_explicit_material(&current_body))error=ProSolidBodyMaterialSet(&body,&system);
        trace("Restore source unassigned sheet body material",error);
    }
    if(!error&&(has_part||has_body))error=part_regenerate(target);
    if(!error&&has_part)part_text_add(out,L"零件材料 %ls。\n",new_part.matl_name);
    if(!error&&has_body)part_text_add(out,L"实体材料 %ls。\n",new_body.matl_name);
    return error;
}

static ProError imitation_material_verify(ProMdl source,ProMdl target,ProMdlsubtype subtype)
{
    if(subtype!=PROMDLSTYPE_PART_SOLID&&subtype!=PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_NO_ERROR;
    ProMaterial old_part={0},old_body={0},new_part={0},new_body={0};
    ProError error=imitation_material_read(source,&old_part,&old_body);
    if(error==PRO_TK_BAD_CONTEXT)return PRO_TK_NO_ERROR;
    if(!error&&!imitation_explicit_material(&old_part)&&!imitation_explicit_material(&old_body)){
        if(subtype!=PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_NO_ERROR;
        error=imitation_material_read(target,&new_part,&new_body);
        if(!error&&(imitation_explicit_material(&new_part)||imitation_explicit_material(&new_body)))
            error=PRO_TK_INCOMPLETE;
        return error;
    }
    if(!error)error=imitation_material_read(target,&new_part,&new_body);
    if(!error&&imitation_explicit_material(&old_part)&&_wcsicmp(old_part.matl_name,new_part.matl_name))
        error=PRO_TK_INCOMPLETE;
    if(!error&&imitation_explicit_material(&old_body)&&_wcsicmp(old_body.matl_name,new_body.matl_name))
        error=PRO_TK_INCOMPLETE;
    return error?error:imitation_material_mass(source,target);
}

static ProError imitation_bend_parameter(ProMdl model,const wchar_t *name,ProParamvalue *value){
    ProModelitem item;
    ProParameter parameter;
    ProError error=ProMdlToModelitem(model,&item);
    if(!error)error=ProParameterInit(&item,(wchar_t *)name,&parameter);
    if(!error)error=ProParameterValueWithUnitsGet(&parameter,value,NULL);
    return error;
}

static ProError imitation_bend_factor_replay(ProMdl source,ProMdl result,const wchar_t *name,ProParamvalueType type){
    ProParamvalue original={0},created={0};
    ProError error=imitation_bend_parameter(source,name,&original);
    ProModelitem item;
    ProParameter parameter;
    if(!error)error=ProMdlToModelitem(result,&item);
    if(!error)error=ProParameterInit(&item,(wchar_t *)name,&parameter);
    if(!error)error=ProParameterValueWithUnitsGet(&parameter,&created,NULL);
    if(error)return error;
    if(original.type!=type||created.type!=type)return PRO_TK_UNSUPPORTED;
    if(type==PRO_PARAM_STRING&&!_wcsicmp(original.value.s_val,created.value.s_val))
        return PRO_TK_NO_ERROR;
    if(type==PRO_PARAM_DOUBLE&&fabs(original.value.d_val-created.value.d_val)<=1e-9)
        return PRO_TK_NO_ERROR;
    return ProParameterValueWithUnitsSet(&parameter,&original,NULL);
}
static ProError imitation_bend_table_replay(ProMdl source,ProMdl result,ProMdlsubtype subtype,PartText *out){
    if(subtype!=PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_NO_ERROR;
    const wchar_t *name=L"SMT_PART_BEND_TABLE_NAME";
    ProParamvalue original={0},created={0};
    ProError error=imitation_bend_parameter(source,name,&original);
    if(error==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
    if(!error)error=imitation_bend_parameter(result,name,&created);
    if(error)return error;
    if(original.type!=PRO_PARAM_STRING||created.type!=PRO_PARAM_STRING)return PRO_TK_UNSUPPORTED;
    if(original.value.s_val[0])return PRO_TK_NO_ERROR;
    if(created.value.s_val[0])error=ProSmtPartBendtableRemove((ProPart)result);
    if(!error)error=imitation_bend_factor_replay(source,result,L"SMT_PART_BEND_ALLOW_FACTOR_TYPE",PRO_PARAM_STRING);
    if(!error)error=imitation_bend_factor_replay(source,result,L"SMT_PART_BEND_ALLOWANCE_FACTOR",PRO_PARAM_DOUBLE);
    if(!error)error=part_regenerate(result);
    if(!error)part_text_add(out,L"已沿用原件的折弯余量系数。\n");
    return error;
}
