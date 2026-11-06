#ifndef BWTL_NATIVE_ATTACHMENTS_H
#define BWTL_NATIVE_ATTACHMENTS_H
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <wincodec.h>
#include <commdlg.h>
#include <shellapi.h>
#define NATIVE_IMAGE_MAX 4
typedef struct {wchar_t path[MAX_PATH],name[100];HBITMAP thumbnail;} NativeImage;
static NativeImage native_images[NATIVE_IMAGE_MAX];
static int native_image_count;
static SRWLOCK native_image_lock=SRWLOCK_INIT;
static wchar_t native_attachment_root[MAX_PATH];
static LONG native_image_serial;
static int native_image_import(const wchar_t *path,wchar_t *error,size_t cap){
    IWICImagingFactory *factory=NULL;IWICBitmapDecoder *decoder=NULL;IWICBitmapFrameDecode *frame=NULL;
    IWICFormatConverter *converter=NULL;IWICBitmapScaler *scaler=NULL;IWICBitmapEncoder *encoder=NULL;
    IWICStream *stream=NULL;IWICBitmapFrameEncode *output=NULL;IPropertyBag2 *props=NULL;
    UINT width=0,height=0,tw,th;HRESULT hr;int result=0,co=0;wchar_t dest[MAX_PATH];
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;SYSTEMTIME now;DWORD bytes_hi=0;HANDLE file;
    HBITMAP thumbnail=NULL;BYTE *bits=NULL;BITMAPINFO bi={0};HDC dc;NativeImage image={0};
    error[0]=0;if(!native_attachment_root[0]){wcscpy_s(error,cap,L"附件目录尚未就绪。");return 0;}
    AcquireSRWLockShared(&native_image_lock);result=native_image_count<NATIVE_IMAGE_MAX;ReleaseSRWLockShared(&native_image_lock);
    if(!result){wcscpy_s(error,cap,L"每次最多附4张图片，请先清除附件。");return 0;}result=0;
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){wcscpy_s(error,cap,L"图片文件不可读取。");return 0;}
    {DWORD size=GetFileSize(file,&bytes_hi);CloseHandle(file);if(bytes_hi||size>20*1024*1024||!size){wcscpy_s(error,cap,L"单张图片需为1字节至20MB。");return 0;}}
    hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);co=SUCCEEDED(hr);
    hr=CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&factory);if(FAILED(hr))goto done;
    if(FAILED(hr=IWICImagingFactory_CreateDecoderFromFilename(factory,path,NULL,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||
       FAILED(hr=IWICBitmapDecoder_GetFrame(decoder,0,&frame))||FAILED(hr=IWICBitmapFrameDecode_GetSize(frame,&width,&height)))goto done;
    if(!width||!height||width>16000||height>16000||(ULONGLONG)width*height>40000000){hr=E_INVALIDARG;goto done;}
    if(FAILED(hr=IWICImagingFactory_CreateFormatConverter(factory,&converter))||
       FAILED(hr=IWICFormatConverter_Initialize(converter,(IWICBitmapSource *)frame,&GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom)))goto done;
    CreateDirectoryW(native_attachment_root,NULL);GetLocalTime(&now);
    if(swprintf_s(dest,MAX_PATH,L"%ls\\image-%04u%02u%02u-%02u%02u%02u-%lu-%llu-%ld.png",native_attachment_root,now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,GetCurrentProcessId(),GetTickCount64(),InterlockedIncrement(&native_image_serial))<0){hr=E_INVALIDARG;goto done;}
    if(FAILED(hr=IWICImagingFactory_CreateStream(factory,&stream))||FAILED(hr=IWICStream_InitializeFromFilename(stream,dest,GENERIC_WRITE))||
       FAILED(hr=IWICImagingFactory_CreateEncoder(factory,&GUID_ContainerFormatPng,NULL,&encoder))||
       FAILED(hr=IWICBitmapEncoder_Initialize(encoder,(IStream *)stream,WICBitmapEncoderNoCache))||
       FAILED(hr=IWICBitmapEncoder_CreateNewFrame(encoder,&output,&props))||FAILED(hr=IWICBitmapFrameEncode_Initialize(output,props))||
       FAILED(hr=IWICBitmapFrameEncode_SetSize(output,width,height))||FAILED(hr=IWICBitmapFrameEncode_SetPixelFormat(output,&format))||
       FAILED(hr=IWICBitmapFrameEncode_WriteSource(output,(IWICBitmapSource *)converter,NULL))||
       FAILED(hr=IWICBitmapFrameEncode_Commit(output))||FAILED(hr=IWICBitmapEncoder_Commit(encoder)))goto done;
    {WIN32_FILE_ATTRIBUTE_DATA data;if(!GetFileAttributesExW(dest,GetFileExInfoStandard,&data)||data.nFileSizeHigh||data.nFileSizeLow>20*1024*1024){hr=E_INVALIDARG;goto done;}}
    tw=96;th=(UINT)((double)height*tw/width);if(th>60){th=60;tw=(UINT)((double)width*th/height);}if(!tw)tw=1;if(!th)th=1;
    if(FAILED(hr=IWICImagingFactory_CreateBitmapScaler(factory,&scaler))||
       FAILED(hr=IWICBitmapScaler_Initialize(scaler,(IWICBitmapSource *)converter,tw,th,WICBitmapInterpolationModeFant)))goto done;
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=tw;bi.bmiHeader.biHeight=-(LONG)th;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    dc=GetDC(NULL);thumbnail=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void **)&bits,NULL,0);ReleaseDC(NULL,dc);if(!thumbnail){hr=E_OUTOFMEMORY;goto done;}
    if(FAILED(hr=IWICBitmapScaler_CopyPixels(scaler,NULL,tw*4,tw*th*4,bits)))goto done;
    wcscpy_s(image.path,MAX_PATH,dest);{const wchar_t *name=wcsrchr(path,L'\\');wcsncpy_s(image.name,100,name?name+1:path,_TRUNCATE);}image.thumbnail=thumbnail;
    AcquireSRWLockExclusive(&native_image_lock);
    if(native_image_count<NATIVE_IMAGE_MAX){native_images[native_image_count++]=image;thumbnail=NULL;result=1;}
    ReleaseSRWLockExclusive(&native_image_lock);
done:
    if(thumbnail)DeleteObject(thumbnail);if(output)IWICBitmapFrameEncode_Release(output);if(props)IPropertyBag2_Release(props);
    if(encoder)IWICBitmapEncoder_Release(encoder);if(stream)IWICStream_Release(stream);if(scaler)IWICBitmapScaler_Release(scaler);
    if(converter)IWICFormatConverter_Release(converter);if(frame)IWICBitmapFrameDecode_Release(frame);if(decoder)IWICBitmapDecoder_Release(decoder);if(factory)IWICImagingFactory_Release(factory);if(co)CoUninitialize();
    if(!result)swprintf_s(error,cap,L"无法读取该图片（0x%08lx）。支持PNG、JPEG、BMP等本机可解码格式，最大4000万像素。",(unsigned long)hr);
    return result;
}
static void native_images_clear(void){
    int i;AcquireSRWLockExclusive(&native_image_lock);for(i=0;i<native_image_count;i++)if(native_images[i].thumbnail)DeleteObject(native_images[i].thumbnail);
    memset(native_images,0,sizeof(native_images));native_image_count=0;ReleaseSRWLockExclusive(&native_image_lock);
}
static int native_images_snapshot(NativeImage out[NATIVE_IMAGE_MAX]){
    int i,n;AcquireSRWLockShared(&native_image_lock);n=native_image_count;
    for(i=0;i<n;i++){out[i]=native_images[i];out[i].thumbnail=NULL;}ReleaseSRWLockShared(&native_image_lock);return n;
}
static int native_images_clipboard(HWND owner,wchar_t *error,size_t cap){
    int result=0;HANDLE handle;wchar_t temp[MAX_PATH],path[MAX_PATH];error[0]=0;
    if(!OpenClipboard(owner))return 0;
    if((handle=GetClipboardData(CF_HDROP))!=NULL){
        UINT count=DragQueryFileW((HDROP)handle,0xffffffff,NULL,0),i;wchar_t files[4][MAX_PATH];count=count>4?4:count;
        for(i=0;i<count;i++)DragQueryFileW((HDROP)handle,i,files[i],MAX_PATH);CloseClipboard();
        for(i=0;i<count;i++)if(native_image_import(files[i],error,cap))result=1;return result;
    }
    if((handle=GetClipboardData(CF_DIB))!=NULL){
        SIZE_T size=GlobalSize(handle);BYTE *data=GlobalLock(handle);BITMAPFILEHEADER head={0};BITMAPINFOHEADER *info=(BITMAPINFOHEADER *)data;HANDLE file;DWORD wrote;
        if(!data||size<sizeof(BITMAPINFOHEADER)||size>20*1024*1024||info->biSize<40||info->biSize>size){if(data)GlobalUnlock(handle);CloseClipboard();return 0;}
        head.bfType=0x4d42;head.bfSize=(DWORD)(sizeof(head)+size);head.bfOffBits=sizeof(head)+info->biSize;
        if(info->biBitCount<=8)head.bfOffBits+=(info->biClrUsed?info->biClrUsed:(1u<<info->biBitCount))*4;
        if(info->biCompression==BI_BITFIELDS&&info->biSize==40)head.bfOffBits+=12;
        GetTempPathW(MAX_PATH,temp);swprintf_s(path,MAX_PATH,L"%lsBWTL-clipboard-%lu-%llu.bmp",temp,GetCurrentProcessId(),GetTickCount64());
        file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,NULL);
        if(file!=INVALID_HANDLE_VALUE){result=WriteFile(file,&head,sizeof(head),&wrote,NULL)&&wrote==sizeof(head)&&WriteFile(file,data,(DWORD)size,&wrote,NULL)&&wrote==size;CloseHandle(file);}
        GlobalUnlock(handle);CloseClipboard();if(result){result=native_image_import(path,error,cap);DeleteFileW(path);}return result;
    }
    CloseClipboard();return 0;
}
#endif
