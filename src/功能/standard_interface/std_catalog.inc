
#define STD_MAX_HOLES 12

typedef struct {
    wchar_t ref[48];
    double xyz[3],diameter;
} StdHole;

typedef struct {
    ProMdlName model;
    wchar_t revision[41],face[48],frame[48],material[OR_CELL];
    char stamp[240];
    double box[3],normal[3];
    int count;
    StdHole hole[STD_MAX_HOLES];
} StdInterface;

static int std_dir(wchar_t path[MAX_PATH],int create){
    return storage_method_directory(path,L"标准接口库",create);
}

static int std_path(const wchar_t *key,const wchar_t *rev,wchar_t path[MAX_PATH],int create){
    wchar_t folder[MAX_PATH];
    return or_safe_id(key)&&or_safe_id(rev)&&std_dir(folder,create)&&
        swprintf_s(path,MAX_PATH,L"%ls\\%ls.%ls.iface.tsv",folder,key,rev)>=0;
}


static int std_alias_path(const wchar_t *name,int create,wchar_t path[MAX_PATH]){
    wchar_t folder[MAX_PATH];
    return or_safe_id(name)&&std_dir(folder,create)&&swprintf_s(path,MAX_PATH,L"%ls\\%ls.alias.tsv",folder,name)>=0;
}

static int std_finish_new(FILE *file,const wchar_t *temp,const wchar_t *path){
    int good=!ferror(file);
    if(fclose(file))good=0;
    if(good&&MoveFileExW(temp,path,MOVEFILE_WRITE_THROUGH))return 1;
    DeleteFileW(temp);
    return 0;
}

static int std_read_alias(const wchar_t *name,wchar_t key[41],wchar_t revision[41]){
    wchar_t path[MAX_PATH],buffer[200]=L"";
    PartText detail={buffer,_countof(buffer),0};
    OrTable table={0};
    int good=std_alias_path(name,0,path)&&or_table_load(&table,path,4,&detail)&&table.size==1&&
        !wcscmp(table.rows[0].value[0],L"BWTL-STD-ALIAS-1")&&!_wcsicmp(table.rows[0].value[1],name)&&
        or_safe_id(table.rows[0].value[2])&&or_safe_id(table.rows[0].value[3]);
    if(good){
        wcscpy_s(key,41,table.rows[0].value[2]);
        wcscpy_s(revision,41,table.rows[0].value[3]);
    }
    or_free(&table);
    return good;
}

static ProError std_measure(ProMdl model,const wchar_t *revision,const wchar_t *face,const wchar_t *reference,
    const wchar_t *list,StdInterface *entry,PartText *out){
    ProMdlType type;
    if(!model||!or_safe_id(revision)||!*list||wcslen(list)>=MAX_PATH||
       wcslen(face)>=_countof(entry->face)||wcslen(reference)>=_countof(entry->frame))return PRO_TK_BAD_INPUTS;
    ProError error=ProMdlTypeGet(model,&type);
    if(error||type!=PRO_MDL_PART)return error?error:PRO_TK_BAD_CONTEXT;
    ProBoolean modified;
    if((error=ProMdlModificationVerify(model,&modified)))return error;
    if(modified){part_text_add(out,L"请先保存当前零件，再登记接口及修订号。\n");return PRO_TK_BAD_CONTEXT;}
    AxComponent owner={0};
    owner.model=model;
    double mm;
    AxDeviceFrame frame;
    AxDevicePlane plane;
    ProOutline box;
    if((error=qa_mm_per_unit(model,&mm))||(error=ax_device_frame(&owner,reference,0,&frame))||
       (error=ax_device_face(&owner,face,&plane))||(error=ax_device_bounds(model,&frame,box))||
       (error=ProMdlMdlnameGet(model,entry->model))||(error=ax_study_verstamp(model,entry->stamp)))return error;
    wcscpy_s(entry->revision,_countof(entry->revision),revision);
    wcscpy_s(entry->face,_countof(entry->face),face);
    wcscpy_s(entry->frame,_countof(entry->frame),reference);
    ProMaterial material;
    if(!ProMaterialCurrentGet((ProSolid)model,&material)&&_wcsicmp(material.matl_name,L"PTC_SYSTEM_MTRL_PROPS"))
        wcscpy_s(entry->material,_countof(entry->material),material.matl_name);
    else wcscpy_s(entry->material,_countof(entry->material),L"未指定");
    if(wcspbrk(entry->material,L"\t\r\n"))return PRO_TK_BAD_CONTEXT;
    for(int j=0;j<3;j++){
        entry->box[j]=(box[1][j]-box[0][j])*mm;
        entry->normal[j]=ad_dot(plane.normal,frame.axis[j]);
    }
    wchar_t copy[MAX_PATH],*tail=NULL;
    if(list[0]==L','||list[wcslen(list)-1]==L','||wcsstr(list,L",,"))return PRO_TK_BAD_INPUTS;
    wcscpy_s(copy,_countof(copy),list);
    for(wchar_t *token=wcstok_s(copy,L",",&tail);token;token=wcstok_s(NULL,L",",&tail)){
        if(entry->count==STD_MAX_HOLES||wcslen(token)>=_countof(entry->hole[0].ref))return PRO_TK_BAD_INPUTS;
        AxHardwareSite site;
        error=ax_hw_site(&owner,token,face,mm,&site);
        if(error||!site.bore){
            part_text_add(out,L"%ls 无法读为穿过定位面的内孔。\n",token);
            return error?error:PRO_TK_INVALID_TYPE;
        }
        StdHole *hole=&entry->hole[entry->count];
        wcscpy_s(hole->ref,_countof(hole->ref),token);
        hole->diameter=2*site.cylinder.radius*mm;
        for(int j=0;j<3;j++)hole->xyz[j]=ax_project(site.contact,&frame,j)*mm;
        for(int j=0;j<entry->count;j++)if(ad_near(hole->xyz,entry->hole[j].xyz,0.001))return PRO_TK_E_FOUND;
        entry->count++;
    }
    return entry->count?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
}

static ProError std_record(ProMdl model,const wchar_t *key,const wchar_t *rev,const wchar_t *face,
    const wchar_t *frame,const wchar_t *holes,PartText *out){
    wchar_t path[MAX_PATH],temporary[MAX_PATH];
    wchar_t alias[MAX_PATH];
    if(!std_path(key,rev,path,1)||!std_alias_path(key,0,alias))return PRO_TK_BAD_INPUTS;
    if(GetFileAttributesW(alias)!=INVALID_FILE_ATTRIBUTES)return PRO_TK_E_FOUND;
    if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES){
        part_text_add(out,L"编号 %ls 修订 %ls 已登记，请使用新修订号。\n",key,rev);
        return PRO_TK_E_FOUND;
    }
    StdInterface entry={0};
    ProError error=std_measure(model,rev,face,frame,holes,&entry,out);
    if(error)return error;
    FILE *file=ax_study_atomic(path,temporary);
    if(!file)return PRO_TK_CANT_WRITE;
    fwprintf(file,L"BWTL-STD-INTERFACE-1\t%ls\t%ls\t%ls\t%ls\t%ls\t%hs\t%d\n",key,rev,entry.model,
        entry.material,entry.face,entry.stamp,entry.count);
    fwprintf(file,L"GEOMETRY\t%ls\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\n",entry.frame,
        entry.box[0],entry.box[1],entry.box[2],entry.normal[0],entry.normal[1],entry.normal[2]);
    for(int i=0;i<entry.count;i++){
        StdHole *h=&entry.hole[i];
        fwprintf(file,L"HOLE\t%ls\t%.17g\t%.17g\t%.17g\t%.17g\t-\t-\n",
            h->ref,h->xyz[0],h->xyz[1],h->xyz[2],h->diameter);
    }
    int good=!ferror(file);
    if(fclose(file))good=0;
    if(!good||!MoveFileExW(temporary,path,MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary);return PRO_TK_CANT_WRITE;}
    part_text_add(out,L"已登记 %ls / %ls 的原生定位面、坐标系和 %d 个实测孔。\n%ls\n",key,rev,entry.count,path);
    return PRO_TK_NO_ERROR;
}

typedef struct {wchar_t key[41];StdInterface geometry;} StdRecord;

static int std_read(const wchar_t *path,StdRecord *data){
    wchar_t buffer[240]=L"";
    PartText quiet={buffer,_countof(buffer),0};
    OrTable table={0};
    int good=or_table_load(&table,path,8,&quiet);
    if(!good){or_free(&table);return 0;}
    StdInterface *e=&data->geometry;
    OrRow *header=&table.rows[0];
    double count;
    good=!wcscmp(header->value[0],L"BWTL-STD-INTERFACE-1")&&or_safe_id(header->value[1])&&or_safe_id(header->value[2])&&
        wcslen(header->value[3])<_countof(e->model)&&wcslen(header->value[5])<_countof(e->face)&&
        or_number(header->value[7],&count)&&floor(count)==count&&count>=1&&count<=STD_MAX_HOLES&&table.size==2+(int)count&&
        WideCharToMultiByte(CP_UTF8,0,header->value[6],-1,e->stamp,_countof(e->stamp),NULL,NULL);
    if(good){
        OrRow *shape=&table.rows[1];
        good=!wcscmp(shape->value[0],L"GEOMETRY")&&wcslen(shape->value[1])<_countof(e->frame);
        for(int j=0;good&&j<3;j++)good=or_number(shape->value[2+j],&e->box[j])&&e->box[j]>0&&
            or_number(shape->value[5+j],&e->normal[j]);
        if(good)good=fabs(ad_dot(e->normal,e->normal)-1.0)<1e-4;
        wcscpy_s(data->key,_countof(data->key),header->value[1]);
        wcscpy_s(e->revision,_countof(e->revision),header->value[2]);
        wcscpy_s(e->model,_countof(e->model),header->value[3]);
        wcscpy_s(e->material,_countof(e->material),header->value[4]);
        wcscpy_s(e->face,_countof(e->face),header->value[5]);
        if(good)wcscpy_s(e->frame,_countof(e->frame),shape->value[1]);
        e->count=(int)count;
    }
    for(int i=0;good&&i<e->count;i++){
        OrRow *row=&table.rows[i+2];
        StdHole *hole=&e->hole[i];
        good=!wcscmp(row->value[0],L"HOLE")&&wcslen(row->value[1])<_countof(hole->ref)&&
            or_number(row->value[5],&hole->diameter)&&hole->diameter>0;
        for(int j=0;good&&j<3;j++)good=or_number(row->value[j+2],&hole->xyz[j]);
        if(good)wcscpy_s(hole->ref,_countof(hole->ref),row->value[1]);
    }
    or_free(&table);
    return good;
}

static int std_axis(const wchar_t *axis,int *index,int *sign){
    if(wcslen(axis)!=2||(axis[0]!=L'+'&&axis[0]!=L'-')||axis[1]<L'X'||axis[1]>L'Z')return 0;
    *index=axis[1]-L'X';*sign=axis[0]==L'+'?1:-1;
    return 1;
}

static int std_has_pitch(const StdInterface *e,double pitch,double tolerance){
    for(int i=1;i<e->count;i++)for(int j=0;j<i;j++){
        ProVector delta;
        ad_sub(e->hole[i].xyz,e->hole[j].xyz,delta);
        if(fabs(sqrt(ad_dot(delta,delta))-pitch)<=tolerance)return 1;
    }
    return 0;
}

static int std_matching(const StdInterface *a,const StdInterface *b,double tolerance){
    if(a->count!=b->count||ad_dot(a->normal,b->normal)<1-1e-5)return 0;
    unsigned used=0;
    for(int i=0;i<a->count;i++){
        int unique=-1;
        for(int j=0;j<b->count;j++){
            if((used&(1u<<j))||fabs(a->hole[i].diameter-b->hole[j].diameter)>tolerance||
                !ad_near(a->hole[i].xyz,b->hole[j].xyz,tolerance))continue;
            if(unique>=0)return 0;
            unique=j;
        }
        if(unique<0)return 0;
        used|=1u<<unique;
    }
    return 1;
}

static ProError std_search(const wchar_t *axis,const double limit[3],double pitch,double tolerance,
    const StdRecord *against,PartText *out){
    wchar_t folder[MAX_PATH],pattern[MAX_PATH];
    int index=0,sign=0;
    if(!against&&(!std_axis(axis,&index,&sign)||!isfinite(pitch)||pitch<=0||
        limit[0]<=0||limit[1]<=0||limit[2]<=0))return PRO_TK_BAD_INPUTS;
    if(!std_dir(folder,0)||swprintf_s(pattern,_countof(pattern),L"%ls\\*.iface.tsv",folder)<0)return PRO_TK_E_NOT_FOUND;
    WIN32_FIND_DATAW found;
    HANDLE data=FindFirstFileW(pattern,&found);
    if(data==INVALID_HANDLE_VALUE)return PRO_TK_E_NOT_FOUND;
    int matches=0,examined=0;
    do{
        if(found.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
        wchar_t path[MAX_PATH];
        if(swprintf_s(path,_countof(path),L"%ls\\%ls",folder,found.cFileName)<0)continue;
        StdRecord entry={0};
        if(!std_read(path,&entry))continue;
        if(++examined>OR_ROWS){part_text_add(out,L"超过 %d 份接口记录，结果已截断。\n",OR_ROWS);break;}
        if(against){
            if((!_wcsicmp(entry.key,against->key)&&!_wcsicmp(entry.geometry.revision,against->geometry.revision))||
                !std_matching(&against->geometry,&entry.geometry,tolerance))continue;
        }else{
            StdInterface *e=&entry.geometry;
            if(e->normal[index]*sign<0.9999||e->box[0]>limit[0]+tolerance||e->box[1]>limit[1]+tolerance||
                e->box[2]>limit[2]+tolerance||!std_has_pitch(e,pitch,tolerance))continue;
        }
        matches++;
        StdInterface *e=&entry.geometry;
        part_text_add(out,L"%ls / %ls，模型 %ls，材料 %ls，%d 孔，包络 %.6g × %.6g × %.6g mm。\n",
            entry.key,e->revision,e->model,e->material,e->count,e->box[0],e->box[1],e->box[2]);
    }while(FindNextFileW(data,&found));
    FindClose(data);
    part_text_add(out,L"检查 %d 份接口，匹配 %d 份。\n",examined,matches);
    return matches?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static ProError std_lookup(const wchar_t *key,const wchar_t *rev,StdRecord *e){
    wchar_t path[MAX_PATH];
    if(std_path(key,rev,path,0)&&std_read(path,e))return PRO_TK_NO_ERROR;
    wchar_t original[41],version[41];
    return std_read_alias(key,original,version)&&!_wcsicmp(version,rev)&&
        std_path(original,version,path,0)&&std_read(path,e)?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static ProError std_compare(const wchar_t *key,const wchar_t *rev,const wchar_t *other,const wchar_t *version,
    double tolerance,PartText *out){
    StdRecord a={0},b={0};
    ProError error=std_lookup(key,rev,&a);
    if(!error)error=std_lookup(other,version,&b);
    if(error)return error;
    StdInterface *x=&a.geometry,*y=&b.geometry;
    part_text_add(out,L"%ls %ls → %ls %ls，材料 %ls → %ls，孔数 %d → %d。\n",
        key,rev,other,version,x->material,y->material,x->count,y->count);
    part_text_add(out,L"包络差值 X %.6g / Y %.6g / Z %.6g mm，配合面夹角 %.6g°。\n",
        y->box[0]-x->box[0],y->box[1]-x->box[1],y->box[2]-x->box[2],
        acos(fmin(1,fmax(-1,ad_dot(x->normal,y->normal))))*180.0/3.14159265358979323846);
    for(int i=0;i<x->count;i++){
        const StdHole *old=&x->hole[i];
        int nearest=0;
        double distance=INFINITY;
        for(int j=0;j<y->count;j++){
            ProVector d;
            ad_sub(old->xyz,y->hole[j].xyz,d);
            double mm=sqrt(ad_dot(d,d));
            if(mm<distance){nearest=j;distance=mm;}
        }
        part_text_add(out,L"%ls 最近候选 %ls，位置差 %.6g mm，孔径 %.6g → %.6g mm。\n",
            old->ref,y->hole[nearest].ref,distance,old->diameter,y->hole[nearest].diameter);
    }
    part_text_add(out,std_matching(x,y,tolerance)?L"定位接口在给定公差内等同。建议在目标装配复测包络及净空。\n":
        L"孔径、站位或定位方向含超差值。安装前需复测对应参照。\n");
    return PRO_TK_NO_ERROR;
}
