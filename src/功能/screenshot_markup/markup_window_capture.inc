/* PrintWindow renders Creo's chrome. Toolkit renders the OpenGL viewport. */
typedef struct {
    HWND window;
    RECT bounds;
    int width,height;
    LONG score;
} MarkupViewport;

static BOOL CALLBACK markup_find_viewport(HWND window,LPARAM data){
    MarkupViewport *view=(MarkupViewport *)data;
    wchar_t name[80];
    RECT bounds;
    if(!IsWindowVisible(window)||!GetClassNameW(window,name,_countof(name))||
       wcscmp(name,L"PGL_MOGL_WN_CLASS")||!GetWindowRect(window,&bounds))return TRUE;
    LONG score=labs(bounds.right-bounds.left-view->width)+labs(bounds.bottom-bounds.top-view->height);
    if(score<view->score){view->window=window;view->bounds=bounds;view->score=score;}
    return TRUE;
}

static void markup_copy_rectangle(HDC target,HDC source,const RECT *r){
    BitBlt(target,r->left,r->top,r->right-r->left,r->bottom-r->top,source,r->left,r->top,SRCCOPY);
}

static HBITMAP markup_full_window(HWND owner,MarkupProjection *map,RECT *viewport,int *width,int *height){
    RECT outer;
    if(!owner||IsIconic(owner)||!GetWindowRect(owner,&outer))return NULL;
    *width=outer.right-outer.left;
    *height=outer.bottom-outer.top;
    if(*width<100||*height<100||*width>8192||*height>8192)return NULL;
    MarkupViewport view={.width=(int)(map->viewport[1][0]-map->viewport[0][0]),
        .height=(int)(map->viewport[1][1]-map->viewport[0][1]),.score=LONG_MAX};
    EnumChildWindows(owner,markup_find_viewport,(LPARAM)&view);
    if(!view.window||view.score>8)return NULL;
    *viewport=view.bounds;
    OffsetRect(viewport,-outer.left,-outer.top);
    if(viewport->left<0||viewport->top<0||viewport->right>*width||viewport->bottom>*height)return NULL;

    HDC output=CreateCompatibleDC(NULL),source=CreateCompatibleDC(NULL);
    HBITMAP result=NULL;
    if(output&&source){
        BITMAPINFO info={0};
        info.bmiHeader=(BITMAPINFOHEADER){sizeof(BITMAPINFOHEADER),*width,-*height,1,32};
        void *pixels=NULL;
        result=CreateDIBSection(output,&info,DIB_RGB_COLORS,&pixels,NULL,0);
        if(result){
            HGDIOBJ old=SelectObject(output,result);
            HGDIOBJ old_source=SelectObject(source,map->image->bitmap);
            PatBlt(output,0,0,*width,*height,BLACKNESS);
            BOOL printed=PrintWindow(owner,output,2);
            HDC chrome=CreateCompatibleDC(output);
            HBITMAP overlay=chrome?CreateCompatibleBitmap(output,*width,*height):NULL;
            HGDIOBJ old_overlay=overlay?SelectObject(chrome,overlay):NULL;
            if(printed&&overlay)BitBlt(chrome,0,0,*width,*height,output,0,0,SRCCOPY);
            SetStretchBltMode(output,HALFTONE);
            SetBrushOrgEx(output,0,0,NULL);
            BOOL drawn=printed&&StretchBlt(output,viewport->left,viewport->top,
                viewport->right-viewport->left,viewport->bottom-viewport->top,source,0,0,
                map->image->width,map->image->height,SRCCOPY);
            /* Preserve the graphics toolbar, which is a sibling of the renderer branch. */
            if(drawn&&overlay){
                HWND child=view.window;
                for(HWND parent=GetParent(child);parent&&parent!=owner;child=parent,parent=GetParent(child)){
                    for(HWND sibling=GetWindow(child,GW_HWNDPREV);sibling;sibling=GetWindow(sibling,GW_HWNDPREV)){
                        RECT r,inside;
                        if(!IsWindowVisible(sibling)||!GetWindowRect(sibling,&r))continue;
                        OffsetRect(&r,-outer.left,-outer.top);
                        if(IntersectRect(&inside,viewport,&r)&&r.bottom-r.top<=80)
                            markup_copy_rectangle(output,chrome,&inside);
                    }
                }
            }
            if(overlay){SelectObject(chrome,old_overlay);DeleteObject(overlay);}
            if(chrome)DeleteDC(chrome);
            SelectObject(source,old_source);
            SelectObject(output,old);
            if(!drawn){DeleteObject(result);result=NULL;}
        }
    }
    if(source)DeleteDC(source);
    if(output)DeleteDC(output);
    return result;
}

static HWND markup_creo_window(void){
    HWND active=GetAncestor(GetActiveWindow(),GA_ROOT);
    HWND owner=native_window?GetWindow(native_window,GW_OWNER):NULL;
    HWND options[]={active,owner};
    for(int i=0;i<_countof(options);i++){
        HWND candidate=options[i];
        wchar_t title[256];
        DWORD pid=0;
        if(candidate)GetWindowThreadProcessId(candidate,&pid);
        if(pid==GetCurrentProcessId()&&GetWindowTextW(candidate,title,_countof(title))&&
           wcsstr(title,L"Creo Parametric"))return candidate;
    }
    return NULL;
}