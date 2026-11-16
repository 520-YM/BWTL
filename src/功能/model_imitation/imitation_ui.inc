static UINT imitation_pending;
#define IMITATION_BATCH_MAX 16
typedef struct {
    ProPath path;
    BY_HANDLE_FILE_INFORMATION info;
    wchar_t evidence[PART_TEXT_MAX];
} ImitationDelivery;
static ImitationDelivery imitation_last,imitation_batch[IMITATION_BATCH_MAX];
static int imitation_batch_count;

static void native_imitation_action(UINT action){
    PostMessageW(native_backend_window,BWTL_IMITATION_REQUEST,action,0);
}

static int imitation_file_info(const wchar_t *path,BY_HANDLE_FILE_INFORMATION *info){
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    int ok=GetFileInformationByHandle(file,info)&&
        !(info->dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
    CloseHandle(file);
    return ok;
}

static void imitation_queue_action(UINT action){
    if(action<IMITATION_CURRENT||action>IMITATION_MEMORY||imitation_pending||native_busy||smart_active||ai_process)return;
    imitation_pending=action;
    if(ProMacroLoad(L"~ Command `BWTLImitateModel`;")){
        imitation_pending=0;
        append_card(L"仿照模型",L"Creo 未接受仿照模型操作。");
    }
}

static ProError imitation_choose_filtered(wchar_t *path,DWORD capacity,const wchar_t *title,int multiple,
                                           const wchar_t *directory,const wchar_t *filter){
    OPENFILENAMEW choose={0};
    choose.lStructSize=sizeof(choose);
    choose.hwndOwner=native_window;
    choose.lpstrFilter=filter;
    choose.lpstrFile=path;
    choose.nMaxFile=capacity;
    choose.lpstrTitle=title;
    choose.lpstrInitialDir=directory;
    choose.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_DONTADDTORECENT;
    if(multiple)choose.Flags|=OFN_EXPLORER|OFN_ALLOWMULTISELECT;
    if(!GetOpenFileNameW(&choose))return CommDlgExtendedError()?PRO_TK_CANT_OPEN:PRO_TK_USER_ABORT;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_choose_at(wchar_t *path,DWORD capacity,const wchar_t *title,int multiple,
                                     const wchar_t *directory){
    return imitation_choose_filtered(path,capacity,title,multiple,directory,L"Creo 零件\0*.prt;*.prt.*\0\0");
}

static ProError imitation_choose(wchar_t *path,DWORD capacity,const wchar_t *title,int multiple){
    return imitation_choose_at(path,capacity,title,multiple,NULL);
}

static ProError imitation_load_source(wchar_t *path,ProMdl *source,int *opened){
    for(wchar_t *p=path;*p;p++)if(*p==L'/')*p=L'\\';
    const wchar_t *leaf=wcsrchr(path,L'\\');
    if(!leaf)return PRO_TK_BAD_INPUTS;
    leaf++;
    const wchar_t *suffix=wcschr(leaf,L'.');
    if(!suffix||_wcsnicmp(suffix,L".prt",4))return PRO_TK_BAD_INPUTS;
    if(suffix[4]){
        int rev;
        if(suffix[4]!=L'.'||!part_integer(suffix+5,&rev)||rev<1)return PRO_TK_BAD_INPUTS;
    }
    const DWORD attributes=GetFileAttributesW(path);
    if(attributes==INVALID_FILE_ATTRIBUTES){
        const DWORD reason=GetLastError();
        return reason==ERROR_FILE_NOT_FOUND||reason==ERROR_PATH_NOT_FOUND?PRO_TK_E_NOT_FOUND:PRO_TK_CANT_OPEN;
    }
    if((attributes&FILE_ATTRIBUTE_DIRECTORY)||!dv_file_nonempty(path))return PRO_TK_CANT_OPEN;
    ProMdlName name;
    if(wcsncpy_s(name,_countof(name),leaf,suffix-leaf))return PRO_TK_BAD_INPUTS;
    ProMdl existing;
    ProError error=ProMdlnameInit(name,PRO_MDL_PART,&existing);
    if(!error){
        ProPath origin;
        if(ProMdlOriginGet(existing,origin)||_wcsicmp(origin,path))return PRO_TK_E_IN_USE;
        *source=existing;
        return PRO_TK_NO_ERROR;
    }
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    error=ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,source);
    if(!error)*opened=1;
    return error;
}

static int imitation_library_is_directory(const wchar_t *path){
    DWORD attributes=GetFileAttributesW(path);
    return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)&&!(attributes&FILE_ATTRIBUTE_REPARSE_POINT);
}

static int imitation_library_folder(const wchar_t *path){
    if(!CreateDirectoryW(path,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    return imitation_library_is_directory(path);
}

static ProError imitation_library_paths(ProPath root,ProPath standards,ProPath library){
    ProPath parent;
    if(wcscpy_s(parent,_countof(parent),runtime_data))return PRO_TK_BAD_CONTEXT;
    wchar_t *end=wcsrchr(parent,L'\\');
    if(!end||_wcsicmp(end+1,L"data"))return PRO_TK_BAD_CONTEXT;
    *end=0;
    if(swprintf_s(root,PRO_PATH_SIZE,L"%ls\\公司资料",parent)<0||
       swprintf_s(standards,PRO_PATH_SIZE,L"%ls\\公司标准",root)<0||
       swprintf_s(library,PRO_PATH_SIZE,L"%ls\\公司标准件",standards)<0)return PRO_TK_BAD_INPUTS;
    return PRO_TK_NO_ERROR;
}

#include "imitation_library_receipt.inc"
#include "imitation_standard_size.inc"

static ProError imitation_save_library(const ImitationDelivery *delivery,PartText *out){
    const wchar_t *path=delivery->path;
    const BY_HANDLE_FILE_INFORMATION *original=&delivery->info;
    if(!path[0]){
        part_text_add(out,L"本次会话还没有可入库的仿照结果。");
        return PRO_TK_BAD_CONTEXT;
    }
    HANDLE source=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(source==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
    BY_HANDLE_FILE_INFORMATION now;
    ProError error=PRO_TK_BAD_CONTEXT;
    if(!GetFileInformationByHandle(source,&now)||
       (now.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       now.dwVolumeSerialNumber!=original->dwVolumeSerialNumber||
       now.nFileIndexHigh!=original->nFileIndexHigh||now.nFileIndexLow!=original->nFileIndexLow||
       now.nFileSizeHigh!=original->nFileSizeHigh||now.nFileSizeLow!=original->nFileSizeLow||
       CompareFileTime(&now.ftLastWriteTime,&original->ftLastWriteTime)){
        part_text_add(out,L"工作目录中的仿照文件已变化，请重新运行验收后入库。");
        goto done;
    }
    ProPath root,standards,library,destination,existing,receipt;
    error=imitation_library_paths(root,standards,library);
    if(error)goto done;
    error=PRO_TK_BAD_CONTEXT;
    const wchar_t *leaf=wcsrchr(path,L'\\');
    if(!leaf)goto done;
    leaf++;
    const wchar_t *dot=wcschr(leaf,L'.');
    ProMdlName name;
    if(!dot||wcsncpy_s(name,_countof(name),leaf,dot-leaf))goto done;
    if(swprintf_s(destination,_countof(destination),L"%ls\\%ls",library,leaf)<0||
       !imitation_library_folder(root)||!imitation_library_folder(standards)||!imitation_library_folder(library)){
        error=PRO_TK_CANT_WRITE;
        goto done;
    }
    if(!dv_native_revision(library,name,existing)||
       (swprintf_s(existing,_countof(existing),L"%ls\\%ls.prt",library,name)>0&&
        GetFileAttributesW(existing)!=INVALID_FILE_ATTRIBUTES)){
        part_text_add(out,L"公司标准件目录已有同名模型，保留现有文件。");
        error=PRO_TK_E_FOUND;
        goto done;
    }
    if(swprintf_s(receipt,_countof(receipt),L"%ls.验收.txt",destination)<0){
        error=PRO_TK_BAD_INPUTS;
        goto done;
    }
    error=imitation_library_write_receipt(receipt,delivery->evidence);
    if(error)goto done;
    if(!CopyFileW(path,destination,TRUE)){
        error=PRO_TK_CANT_WRITE;
        if(!DeleteFileW(receipt))part_text_add(out,L"未能清理本次新建的验收记录。\n%ls\n",receipt);
        goto done;
    }
    error=PRO_TK_NO_ERROR;
    part_text_add(out,L"已存入公司标准件及验收记录。\n%ls\n%ls\n",destination,receipt);
done:
    CloseHandle(source);
    return error;
}

static int imitation_record(const wchar_t *path,ImitationDelivery *delivery,ProMdl source,
                            const ImitationOutcome *outcome,const PartText *receipt){
    BY_HANDLE_FILE_INFORMATION info;
    if(!imitation_file_info(path,&info)||wcslen(path)>=_countof(delivery->path))return 0;
    ProMdlName original=L"";
    ProMdlMdlnameGet(source,original);
    delivery->info=info;
    wcscpy_s(delivery->path,_countof(delivery->path),path);
    delivery->evidence[0]=0;
    PartText evidence={delivery->evidence,_countof(delivery->evidence),0};
    part_text_add(&evidence,L"BWTL 仿照验收\n源模型 %ls\n尺寸试改 %ls\n",
        original,outcome->edit_verified?L"已通过并恢复原值":L"本次未执行");
    if(receipt->truncated)part_text_add(&evidence,L"重建回执达到长度上限。\n");
    part_text_add(&evidence,L"\n%ls",receipt->text);
    return 1;
}

static void imitation_batch_failure(const wchar_t *label,const wchar_t *path,ProMdl source,
                                    const ImitationOutcome *outcome,ProError error,PartText *out){
    part_text_add(out,L"%ls 未通过，",label);
    if(outcome->replay.step){
        part_text_add(out,L"第 %d 步 %ls",outcome->replay.step,outcome->replay.operation);
        if(outcome->replay.label[0])part_text_add(out,L" %ls",outcome->replay.label);
        part_text_add(out,L"（源特征 %d）",outcome->replay.feature);
    }else if(outcome->replay.feature){
        part_text_add(out,L"源特征 %ls（ID %d）%ls",
            outcome->replay.label[0]?outcome->replay.label:L"未命名",
            outcome->replay.feature,outcome->replay.operation);
    }else if(outcome->stage)part_text_add(out,L"%ls",outcome->stage);
    else if(!source&&error==PRO_TK_E_NOT_FOUND)
        part_text_add(out,L"源文件缺失或 Creo 无法定位：%ls",path[0]?path:label);
    else if(!source)part_text_add(out,L"打开源文件");
    else part_text_add(out,L"记录交付文件");
    part_text_add(out,L"，Creo %d。\n",error);
    replay_report_reference_failure(&outcome->replay.reference,out);
    if(source&&outcome->dependency_block)imitation_dependency_names(source,out);
}

typedef struct {
    const wchar_t *directory,*first;
    int count;
} ImitationFileSelection;

static ProError imitation_file_selection(const wchar_t *files,ImitationFileSelection *selection){
    if(!files[0])return PRO_TK_BAD_INPUTS;
    const wchar_t *first=files+wcslen(files)+1;
    if(!*first){
        selection->first=files;
        selection->count=1;
        return PRO_TK_NO_ERROR;
    }
    selection->directory=files;
    selection->first=first;
    for(const wchar_t *p=first;*p;p+=wcslen(p)+1)selection->count++;
    return selection->count>IMITATION_BATCH_MAX?PRO_TK_OUT_OF_RANGE:PRO_TK_NO_ERROR;
}

static ProError imitation_selection_path(const ImitationFileSelection *selection,const wchar_t *leaf,ProPath path){
    if(selection->directory)
        return swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",selection->directory,leaf)<0?PRO_TK_BAD_INPUTS:PRO_TK_NO_ERROR;
    return wcscpy_s(path,PRO_PATH_SIZE,leaf)?PRO_TK_BAD_INPUTS:PRO_TK_NO_ERROR;
}

static const wchar_t *imitation_selection_label(const wchar_t *path){
    const wchar_t *leaf=wcsrchr(path,L'\\');
    if(!leaf)leaf=wcsrchr(path,L'/');
    return leaf?leaf+1:path;
}

static ProError imitation_batch_files(const wchar_t *files,PartText *out){
    ImitationFileSelection selection={0};
    ProError error=imitation_file_selection(files,&selection);
    if(error)return error;
    const int count=selection.count;
    int warned=0;
    imitation_batch_count=0;
    const wchar_t *leaf=selection.first;
    for(int i=0;i<count;i++,leaf+=wcslen(leaf)+1){
        ProPath path=L"",result=L"";
        ProMdl source=NULL;
        int opened=0;
        error=imitation_selection_path(&selection,leaf,path);
        if(!error)error=imitation_load_source(path,&source,&opened);
        wchar_t details[PART_TEXT_MAX]=L"";
        PartText receipt={details,_countof(details),0};
        ImitationOutcome outcome={0};
        if(!error){
            if(!warned)warned=imitation_warn_wall(source);
            error=imitation_run_mode_impl(source,result,0,1,&outcome,&receipt);
        }
        const wchar_t *label=imitation_selection_label(leaf);
        if(!error&&imitation_record(result,&imitation_batch[imitation_batch_count],source,&outcome,&receipt)){
            imitation_batch_count++;
            part_text_add(out,L"%ls → %ls\n",label,result);
        }else{
            if(!error){
                error=PRO_TK_CANT_OPEN;
                outcome.stage=L"记录交付文件";
            }
            imitation_batch_failure(label,path,source,&outcome,error,out);
        }
        if(opened&&source)ProMdlErase(source);
        if(dv_recovery_uncertain||sh_recovery_uncertain){
            part_text_add(out,L"Creo 需要核对恢复状态，已结束批次。\n");
            break;
        }
    }
    part_text_add(out,L"批量通过 %d / %d 个。\n",imitation_batch_count,count);
    return imitation_batch_count==count?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError imitation_batch_run(PartText *out){
    wchar_t files[8192]=L"";
    ProError error=imitation_choose(files,_countof(files),L"选择要按原特征树仿照的零件（最多 16 个）",1);
    return error?error:imitation_batch_files(files,out);
}

#include "imitation_batch_preview.inc"
#include "imitation_definition_report.inc"
#include "imitation_dimension_report.inc"

static ProError imitation_diagnose(ProMdl model,PartText *out){
    ImitationSourceInfo info={0};
    ImitationShapeMetrics shape;
    ProMdlName name;
    ProMdlsubtype subtype;
    ProError error=ProMdlMdlnameGet(model,name);
    if(!error)error=ProMdlSubtypeGet(model,&subtype);
    if(error)return error;
    part_text_add(out,L"%ls，%ls。\n",name,subtype==PROMDLSTYPE_PART_SHEETMETAL?L"钣金零件":L"当前模型");
    error=imitation_read_source(model,&info);
    if(error)part_text_add(out,L"原生特征状态或尺寸读取返回 Creo %d。\n",error);
    else part_text_add(out,L"特征 %d 项，驱动尺寸 %d 个，导入特征 %d 项。\n",info.features,info.dimensions,info.imports);
    ProMdlType type;
    if(ProMdlTypeGet(model,&type)||type!=PRO_MDL_PART)return PRO_TK_BAD_CONTEXT;
    imitation_reference_report(model,out);
    ProError family=replay_family_read(model,NULL,out);
    if(family)part_text_add(out,L"族表有效值读取返回 Creo %d。\n",family);
    imitation_dimension_report(model,out);
    replay_preview(model,out);
    if(subtype==PROMDLSTYPE_PART_SHEETMETAL){
        double mm,t;
        ProError gauge=fab_thickness(model,&mm,&t);
        if(gauge)part_text_add(out,L"原生板厚读取返回 Creo %d。\n",gauge);
        else part_text_add(out,L"板厚 %.8g mm。\n",t);
    }
    ProError geometry=imitation_shape_read(model,&shape);
    if(geometry){
        imitation_shape_error(&shape,geometry,out);
        imitation_definition_reference_report(model,out);
        return geometry;
    }
    part_text_add(out,L"实体 %d 个，可量测面组 %d 个，空面组 %d 个，壁构造面 %d 张。实体面积 %.10g mm²，体积 %.10g mm³。\n",
        shape.bodies,shape.quilts,shape.empty_quilts,shape.wall_refs,shape.area,shape.volume);
    if(shape.recovered_mass)part_text_add(out,L"实体面积和体积已通过逐实体读取恢复。\n");
    imitation_definition_reference_report(model,out);
    return error;
}

static ProError imitation_compare(ProMdl source,ProMdl result,PartText *out){
    ImitationSourceInfo a={0},b={0};
    ProError error=imitation_read_source(source,&a);
    if(!error)error=imitation_read_source(result,&b);
    if(error)return error;
    ProError tree=replay_verify_order(source,result,out);
    part_text_add(out,L"全模型特征 %d / %d 项，驱动尺寸 %d / %d 个。\n",
        a.features,b.features,a.dimensions,b.dimensions);
    ProError sheet=imitation_sheet_compare(source,result,out);
    if(sheet)part_text_add(out,L"零件类型、板厚或折弯定义有差异（Creo %d）。\n",sheet);
    ImitationShapeMetrics shape;
    error=imitation_shape_read(source,&shape);
    if(error)imitation_shape_error(&shape,error,out);
    else error=imitation_shape_verify_impl(source,result,&shape,0,1,out);
    return error?error:sheet?sheet:tree;
}

#include "imitation_coverage.inc"
#include "imitation_flat_check.inc"

static int imitation_ui_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    UINT action=imitation_pending;
    imitation_pending=0;
    if(!action||native_busy||smart_active||ai_process)return 0;
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(action==IMITATION_COVERAGE){
        imitation_coverage_report(&out);
        append_card(L"仿照模型",report);
        return 0;
    }
    if((action==IMITATION_CURRENT||action==IMITATION_FILE||action==IMITATION_STRICT||action==IMITATION_BATCH||
        action==IMITATION_COMPARE_FLAT||action==IMITATION_NEUTRAL||action==IMITATION_MANUAL)
        &&!storage_prepare_output()){
        append_card(L"仿照模型",storage_output_message);
        return 0;
    }
    native_set_busy(3);
    ProError error;
    if(action==IMITATION_ASSEMBLY_LEARN){
        ProMdl active=NULL;
        ProMdlCurrentGet(&active);
        error=ax_learning_save(active,&out);
    }else if(action==IMITATION_MEMORY){imitation_memory_report(&out);error=PRO_TK_NO_ERROR;}
    else if(action==IMITATION_LEARN)error=method_save_last(&out);
    else if(action==IMITATION_LIBRARY)error=imitation_save_library(&imitation_last,&out);
    else if(action==IMITATION_OPEN_STANDARD)error=imitation_open_standard_dialog(&out);
    else if(action==IMITATION_BATCH)error=imitation_batch_run(&out);
    else if(action==IMITATION_BATCH_PREVIEW)error=imitation_batch_preview_run(&out);
    else if(action==IMITATION_BATCH_LIBRARY){
        int success=0;
        for(int i=0;i<imitation_batch_count;i++){
            ProError item=imitation_save_library(&imitation_batch[i],&out);
            if(!item)success++;
            else part_text_add(&out,L"%ls 入库未完成（Creo %d）。\n",imitation_batch[i].path,item);
        }
        part_text_add(&out,L"批量入库 %d / %d 个。\n",success,imitation_batch_count);
        error=success&&success==imitation_batch_count?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
    }
    else{
        ProMdl source=NULL,comparison=NULL;
        int opened=0;
        ProPath path=L"",result=L"";
        if(action==IMITATION_FILE){
            error=imitation_choose(path,_countof(path),L"选择要仿照的 Creo 零件",0);
            if(!error)error=imitation_load_source(path,&source,&opened);
        }else if(action==IMITATION_NEUTRAL){
            error=imitation_choose_filtered(path,_countof(path),L"选择 STEP 或 IGES 几何",0,NULL,
                L"STEP 与 IGES\0*.stp;*.step;*.igs;*.iges\0\0");
            if(!error)error=imitation_neutral_load(path,&source,&out);
            if(!error)opened=1;
        }else error=ProMdlCurrentGet(&source);
        if(!error&&source){
            if(action==IMITATION_DIAGNOSE)error=imitation_diagnose(source,&out);
            else if(action==IMITATION_COMPARE||action==IMITATION_COMPARE_FLAT){
                error=imitation_choose(path,_countof(path),L"选择与当前模型比对的零件",0);
                if(!error)error=imitation_load_source(path,&comparison,&opened);
                if(!error)error=action==IMITATION_COMPARE_FLAT?imitation_flat_compare(source,comparison,&out):
                    imitation_compare(source,comparison,&out);
            }else{
                imitation_warn_wall(source);
                ImitationOutcome outcome={.manual=action==IMITATION_MANUAL};
                error=imitation_run_mode_impl(source,result,1,action==IMITATION_STRICT||action==IMITATION_NEUTRAL,&outcome,&out);
                if(!error&&result[0])imitation_record(result,&imitation_last,source,&outcome,&out);
            }
        }else if(!error)error=PRO_TK_E_NOT_FOUND;
        else if(error==PRO_TK_E_IN_USE)
            part_text_add(&out,L"Creo 会话已加载同名零件。可打开该零件后使用「仿照当前模型」。\n");
        if(opened)ProMdlErase(comparison?comparison:source);
    }
    native_set_busy(0);
    if(error==PRO_TK_USER_ABORT)return 0;
    if(error)part_text_add(&out,L"操作未完成（Creo %d）。\n",error);
    const wchar_t *role=action==IMITATION_ASSEMBLY_LEARN?L"装配学习":action==IMITATION_MEMORY?L"学习记忆":L"仿照模型";
    append_card(role,report);
    if(action!=IMITATION_MEMORY)save_turn(role,report);
    return 0;
}
