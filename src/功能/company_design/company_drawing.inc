#include <ProDrawing.h>
#include <ProPDF.h>
#include <ProPrint.h>

static int company_drawing_pdf_valid(const wchar_t *path){
    if(!dv_file_nonempty(path))return 0;
    FILE *file=_wfopen(path,L"rb");
    if(!file)return 0;
    char signature[5], tail[1025]={0};
    int valid=fread(signature,1,sizeof(signature),file)==sizeof(signature)&&
        !memcmp(signature,"%PDF-",sizeof(signature));
    if(valid&&!_fseeki64(file,0,SEEK_END)){
        __int64 size=_ftelli64(file);
        __int64 tail_size=size<1024?size:1024;
        valid=tail_size>5&&!_fseeki64(file,-tail_size,SEEK_END)&&
            fread(tail,1,(size_t)tail_size,file)==(size_t)tail_size;
        if(valid){
            valid=0;
            for(size_t i=0;i+5<=(size_t)tail_size;i++){
                if(!memcmp(tail+i,"%%EOF",5)){valid=1;break;}
            }
        }
    }else valid=0;
    fclose(file);
    return valid;
}

static ProError company_drawing_export(const wchar_t *expected_name,const wchar_t *out_base,PartText *out){
    static LONG serial;
    ProMdl drawing=NULL;
    ProMdlType type;
    ProMdlName name;
    ProPath directory,stage={0},temporary={0},target;
    ProPDFOptions options=NULL;
    int drawing_window, current_window, sheets;
    ProError error=ProMdlCurrentGet(&drawing);

    if(error||!drawing||ProMdlTypeGet(drawing,&type)||type!=PRO_MDL_DRAWING||
       ProMdlWindowGet(drawing,&drawing_window)||ProWindowCurrentGet(&current_window)||
       current_window!=drawing_window){
        part_text_add(out,L"请先在 Creo 中打开并激活要导出的原生工程图（.drw）。");
        return PRO_TK_BAD_CONTEXT;
    }
    error=ProMdlMdlnameGet(drawing,name);
    if(error)return error;
    if(expected_name&&_wcsicmp(expected_name,name)){
        part_text_add(out,L"当前工程图为 %ls，请先核对要导出的图纸。",name);
        return PRO_TK_BAD_CONTEXT;
    }
    if(!out_base||!dv_safe_leaf(out_base)){
        part_text_add(out,L"请提供一个不含后缀、由字母、数字、下划线或连字符组成的新文件名。");
        return PRO_TK_BAD_INPUTS;
    }
    error=ProDirectoryCurrentGet(directory);
    if(error||!storage_output_directory(directory)||
       !storage_directory_on_drive(directory,towupper(runtime_data[0]))){
        part_text_add(out,L"请先将 Creo 工作目录设为数据盘上的项目目录，再导出工程图。");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    if(swprintf_s(target,_countof(target),L"%ls\\%ls.pdf",directory,out_base)<0)
        return PRO_TK_BAD_INPUTS;
    if(GetFileAttributesW(target)!=INVALID_FILE_ATTRIBUTES){
        part_text_add(out,L"已存在 %ls，未覆盖。",target);
        return PRO_TK_E_FOUND;
    }
    if(GetLastError()!=ERROR_FILE_NOT_FOUND)return PRO_TK_CANT_WRITE;

    error=ProDrawingSheetsCount((ProDrawing)drawing,&sheets);
    if(error||sheets<1){
        part_text_add(out,L"未能读到有效工程图页面。");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    if(swprintf_s(stage,_countof(stage),L"%ls\\bwtl_pdf_%08lx_%llx_%08lx",directory,
        GetCurrentProcessId(),GetTickCount64(),InterlockedIncrement(&serial))<0||
       swprintf_s(temporary,_countof(temporary),L"%ls\\export.pdf",stage)<0||
       !CreateDirectoryW(stage,NULL))return PRO_TK_CANT_WRITE;


    error=ProPDFoptionsAlloc(&options);
    if(!error)error=ProPDFoptionsIntpropertySet(options,PRO_PDFOPT_EXPORT_MODE,PRO_PDF_2D_DRAWING);
    if(!error)error=ProPDFoptionsIntpropertySet(options,PRO_PDFOPT_SHEETS,PRINT_ALL_SHEETS);
    if(!error)error=ProPDFoptionsIntpropertySet(options,PRO_PDFOPT_PARAM_MODE,PRO_PDF_PARAMS_NONE);
    if(!error)error=ProPDFoptionsBoolpropertySet(options,PRO_PDFOPT_LAUNCH_VIEWER,PRO_B_FALSE);
    if(!error)error=ProPDFExport(drawing,temporary,options);
    if(options)ProPDFoptionsFree(options);
    if(!error&&!company_drawing_pdf_valid(temporary))error=PRO_TK_INCOMPLETE;
    if(!error&&!MoveFileExW(temporary,target,MOVEFILE_WRITE_THROUGH)){
        DWORD move_error=GetLastError();
        error=move_error==ERROR_ALREADY_EXISTS||move_error==ERROR_FILE_EXISTS?PRO_TK_E_FOUND:PRO_TK_CANT_WRITE;
    }

    if(error&&GetFileAttributesW(temporary)!=INVALID_FILE_ATTRIBUTES)DeleteFileW(temporary);
    if(!RemoveDirectoryW(stage))part_text_add(out,L"临时目录保留在 %ls。\n",stage);
    if(!error)
        part_text_add(out,L"已从当前工程图 %ls 导出全部 %d 张图纸，已核验 PDF 文件：%ls。",name,sheets,target);
    else if(error==PRO_TK_NO_LICENSE)
        part_text_add(out,L"当前 Creo 许可未提供 PDF 导出能力，未生成交付文件。");
    else
        part_text_add(out,L"工程图 PDF 导出未完成（Creo %d）。已有文件未被覆盖。",error);
    return error;
}
