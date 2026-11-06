#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "../src/native_attachments.h"
int main(void){
    wchar_t temp[MAX_PATH],path[MAX_PATH],error[256];BITMAPFILEHEADER file={0};BITMAPINFOHEADER info={0};
    DWORD pixels[4]={0xffff0000,0xff00ff00,0xff0000ff,0xffffffff};FILE *f;int i;NativeImage images[4];
    GetTempPathW(MAX_PATH,temp);swprintf_s(native_attachment_root,MAX_PATH,L"%lsBWTL-attachment-test-%lu",temp,GetCurrentProcessId());CreateDirectoryW(native_attachment_root,NULL);
    swprintf_s(path,MAX_PATH,L"%ls\\source.bmp",native_attachment_root);file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+sizeof(pixels);
    info.biSize=sizeof(info);info.biWidth=2;info.biHeight=2;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
    f=_wfopen(path,L"wb");assert(f);fwrite(&file,1,sizeof(file),f);fwrite(&info,1,sizeof(info),f);fwrite(pixels,1,sizeof(pixels),f);fclose(f);
    for(i=0;i<4;i++)assert(native_image_import(path,error,256));assert(native_image_count==4);assert(!native_image_import(path,error,256));
    assert(native_images_snapshot(images)==4);for(i=0;i<4;i++){BYTE signature[8];f=_wfopen(images[i].path,L"rb");assert(f);assert(fread(signature,1,8,f)==8);fclose(f);assert(signature[0]==137&&signature[1]=='P'&&signature[2]=='N'&&signature[3]=='G');assert(!images[i].thumbnail);}
    native_images_clear();assert(native_image_count==0);assert(!native_image_import(L"C:\\missing-bwtl-test-image.png",error,256));
    f=_wfopen(path,L"wb");assert(f);fputs("this is not an image",f);fclose(f);assert(!native_image_import(path,error,256));
    puts("ATTACHMENTS PASS: actual WIC decode, PNG normalization, thumbnails, 4-file bound, invalid/missing image rejection, retained archived files.");return 0;
}
