static int imitation_explicit_material(const ProMaterial *material)
{
    return material->matl_name[0]&&_wcsicmp(material->matl_name,L"PTC_SYSTEM_MTRL_PROPS");
}

static ProError imitation_material_mass(ProMdl source,ProMdl target)
{
    ProMassProperty a,b;
    ProUnitsystem system_a,system_b;
    ProUnititem units_a={0},units_b={0};
    ProUnitConversion conversion={0};
    ProError error=ProSolidMassPropertyGet((ProSolid)source,NULL,&a);
    if(!error)error=ProSolidMassPropertyGet((ProSolid)target,NULL,&b);
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
    if(!error&&!imitation_explicit_material(&old_part)&&!imitation_explicit_material(&old_body))return PRO_TK_NO_ERROR;
    if(!error)error=imitation_material_read(target,&new_part,&new_body);
    if(!error&&imitation_explicit_material(&old_part)&&_wcsicmp(old_part.matl_name,new_part.matl_name))
        error=PRO_TK_INCOMPLETE;
    if(!error&&imitation_explicit_material(&old_body)&&_wcsicmp(old_body.matl_name,new_body.matl_name))
        error=PRO_TK_INCOMPLETE;
    return error?error:imitation_material_mass(source,target);
}
