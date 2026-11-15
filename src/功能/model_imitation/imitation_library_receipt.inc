static ProError imitation_library_write_receipt(const wchar_t *file,const wchar_t *receipt)
{
    const int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,receipt,-1,NULL,0,NULL,NULL);
    if(bytes<=1)return PRO_TK_BAD_INPUTS;
    char *encoded=malloc((size_t)bytes);
    if(!encoded)return PRO_TK_OUT_OF_MEMORY;
    ProError error=PRO_TK_BAD_INPUTS;
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,receipt,-1,encoded,bytes,NULL,NULL)==bytes){
        HANDLE output=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(output==INVALID_HANDLE_VALUE){
            const DWORD reason=GetLastError();
            error=reason==ERROR_FILE_EXISTS||reason==ERROR_ALREADY_EXISTS?PRO_TK_E_FOUND:PRO_TK_CANT_WRITE;
        }else{
            DWORD written=0;
            const BOOL ok=WriteFile(output,encoded,(DWORD)(bytes-1),&written,NULL)&&
                written==(DWORD)(bytes-1)&&FlushFileBuffers(output);
            CloseHandle(output);
            error=ok?PRO_TK_NO_ERROR:PRO_TK_CANT_WRITE;
            if(error)DeleteFileW(file);
        }
    }
    free(encoded);
    return error;
}
