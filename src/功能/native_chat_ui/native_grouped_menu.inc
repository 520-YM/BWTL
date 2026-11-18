/* A menu tree is borrowed for the life of the popup. Only a leaf returns a command. */
enum { NATIVE_GROUPED_FEATURES, NATIVE_GROUPED_IMITATION, NATIVE_GROUPED_COMPACT, NATIVE_GROUPED_STANDARD };
enum { GROUPED_LEAF, GROUPED_RAIL, GROUPED_TAB, GROUPED_OPEN, GROUPED_BACK, GROUPED_CLOSE, GROUPED_SIDE, GROUPED_CLOSE_SIDE, GROUPED_CATALOG_TOGGLE };

typedef struct {
    wchar_t label[192];
    HMENU submenu;
    UINT command;
    int disabled;
} GroupedMenuItem;

typedef struct {
    RECT rect;
    HMENU target;
    UINT command;
    int action,rail;
    wchar_t label[192];
} GroupedMenuHit;

typedef struct {
    GroupedMenuItem item;
    int heading,expanded,next;
} GroupedCatalogRow;

typedef struct {
    HWND window;
    HMENU root,branch,tab,showing[8];
    wchar_t section[192],tab_title[192],subpage[8][192];
    const wchar_t *title;
    HFONT bold;
    int mode,dpi,depth,scroll,limit,rail_scroll,rail_limit;
    int count,hot,focus,done,capture,restore_focus,body_top,body_bottom,rail_width;
    HMENU fasteners,side;
    RECT root_screen,work,side_bounds;
    GroupedCatalogRow *catalog;
    int *catalog_visible;
    RECT catalog_body;
    int catalog_count,catalog_visible_count,catalog_focus;
    int catalog_scroll,catalog_limit,catalog_drag,catalog_inline;
    UINT result;
    GroupedMenuHit hits[96];
} GroupedMenu;

static int grouped_read(HMENU menu,int position,GroupedMenuItem *out){
    MENUITEMINFOW info={sizeof(info)};
    memset(out,0,sizeof(*out));
    info.fMask=MIIM_FTYPE|MIIM_STRING|MIIM_ID|MIIM_SUBMENU|MIIM_STATE;
    info.dwTypeData=out->label;
    info.cch=_countof(out->label);
    if(!GetMenuItemInfoW(menu,(UINT)position,TRUE,&info)||(info.fType&MFT_SEPARATOR))return 0;
    out->submenu=info.hSubMenu;
    out->command=info.wID;
    out->disabled=(info.fState&(MFS_DISABLED|MFS_GRAYED))!=0;
    return out->label[0]!=L'\0';
}

static HMENU grouped_first_branch(HMENU root,wchar_t title[192]){
    int count=GetMenuItemCount(root);
    GroupedMenuItem item;
    title[0]=0;
    for(int i=0;i<count;i++)if(grouped_read(root,i,&item)&&item.submenu&&!item.disabled){
        wcscpy_s(title,192,item.label);
        return item.submenu;
    }
    return NULL;
}

static int grouped_px(const GroupedMenu *menu,int pixels){
    return MulDiv(pixels,menu->dpi,96);
}

static void grouped_fill(HDC dc,RECT area,COLORREF color){
    SetDCBrushColor(dc,color);
    FillRect(dc,&area,(HBRUSH)GetStockObject(DC_BRUSH));
}

static void grouped_text(GroupedMenu *menu,HDC dc,const wchar_t *label,RECT area,COLORREF color,int strong,UINT align){
    HGDIOBJ before=SelectObject(dc,strong&&menu->bold?(HGDIOBJ)menu->bold:
        native_font?(HGDIOBJ)native_font:GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(dc,color);
    DrawTextW(dc,label,-1,&area,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX|align);
    SelectObject(dc,before);
}

static void grouped_row(GroupedMenu *menu,HDC dc,RECT rect,RECT clip,const wchar_t *label,int action,
                        HMENU target,UINT command,int disabled,int accent,int arrow,int rail){
    RECT visible;
    if(!IntersectRect(&visible,&rect,&clip))return;
    int shown=menu->count;
    if(!disabled&&shown<(int)_countof(menu->hits)){
        GroupedMenuHit *entry=&menu->hits[menu->count++];
        entry->rect=visible;
        entry->target=target;
        entry->command=command;
        entry->action=action;
        entry->rail=rail;
        wcscpy_s(entry->label,_countof(entry->label),label);
    }else shown=-1;
    if(accent||shown>=0&&(shown==menu->hot||shown==menu->focus)){
        const COLORREF fill=accent==2?RGB(23,104,98):accent?RGB(216,238,234):RGB(229,242,241);
        grouped_fill(dc,rect,fill);
    }
    RECT text=rect;
    text.left+=grouped_px(menu,10);
    text.right-=grouped_px(menu,arrow?23:9);
    const COLORREF ink=disabled?RGB(119,135,143):accent==2?RGB(255,255,255):RGB(28,64,76);
    grouped_text(menu,dc,label,text,ink,accent!=0,DT_LEFT);
    if(arrow){
        text.left=rect.right-grouped_px(menu,16);
        text.right=rect.right-grouped_px(menu,5);
        grouped_text(menu,dc,arrow==2?L"⌄":L"›",text,ink,0,DT_CENTER);
    }
    if(shown>=0&&shown==menu->focus&&GetFocus()==menu->window){
        RECT dashed=visible;
        InflateRect(&dashed,-2,-2);
        if(dashed.right>dashed.left&&dashed.bottom>dashed.top)DrawFocusRect(dc,&dashed);
    }
}

static void grouped_menu_item(GroupedMenu *menu,HDC dc,RECT area,RECT clip,const GroupedMenuItem *item,int accent){
    grouped_row(menu,dc,area,clip,item->label,item->submenu?GROUPED_OPEN:GROUPED_LEAF,
        item->submenu,item->command,item->disabled,accent,!!item->submenu,0);
}

static int grouped_list_count(HMENU current){
    int count=0;
    GroupedMenuItem item;
    for(int i=0;i<GetMenuItemCount(current);i++)if(grouped_read(current,i,&item))count++;
    return count;
}

static int grouped_list(GroupedMenu *menu,HDC dc,RECT body,HMENU current,int x,int y,int right,int box){
    const int row=grouped_px(menu,35);
    for(int i=0;i<GetMenuItemCount(current);i++){
        GroupedMenuItem item;
        if(!grouped_read(current,i,&item))continue;
        RECT area={x,y,right,y+row};
        grouped_menu_item(menu,dc,area,body,&item,0);
        RECT divider={x+grouped_px(menu,8),y+row-1,right-grouped_px(menu,8),y+row};
        grouped_fill(dc,divider,RGB(230,238,240));
        y+=row;
    }
    if(box)y+=grouped_px(menu,4);
    return y;
}

static int grouped_card(GroupedMenu *menu,HDC dc,RECT body,GroupedMenuItem *item,int x,int y,int right){
    const int header=grouped_px(menu,34),row=grouped_px(menu,35);
    const int bottom=y+header+row*grouped_list_count(item->submenu)+grouped_px(menu,5);
    RECT frame={x,y,right,bottom};
    RECT area={x+1,y+1,right-1,y+header};
    grouped_fill(dc,frame,RGB(218,231,235));
    RECT inner={x+1,y+1,right-1,bottom-1};
    grouped_fill(dc,inner,RGB(255,255,255));
    grouped_row(menu,dc,area,body,item->label,GROUPED_OPEN,item->submenu,0,item->disabled,1,1,0);
    grouped_list(menu,dc,body,item->submenu,x+1,y+header,right-1,1);
    return bottom;
}

static void grouped_scrollbar(HDC dc,RECT area,int offset,int limit){
    if(limit<=0||area.bottom<=area.top)return;
    grouped_fill(dc,area,RGB(235,243,244));
    int height=area.bottom-area.top;
    int length=MulDiv(height,height,height+limit);
    if(length<12)length=height<12?height:12;
    int top=area.top+MulDiv(height-length,offset,limit);
    grouped_fill(dc,(RECT){area.left,top,area.right,top+length},RGB(111,155,163));
}

static int grouped_overview(GroupedMenu *menu,HDC dc,RECT body,int x,int y,int right){
    const int space=grouped_px(menu,9),row=grouped_px(menu,38);
    int first=GetMenuItemCount(menu->root),used=0;
    GroupedMenuItem item;
    for(int i=0;i<first;i++)if(grouped_read(menu->root,i,&item)&&item.submenu){first=i;break;}
    if(first){
        grouped_text(menu,dc,L"常用重建",(RECT){x,y,right,y+grouped_px(menu,29)},RGB(16,55,73),1,DT_LEFT);
        y+=grouped_px(menu,30);
    }
    const int columns=right-x>=grouped_px(menu,530)?2:1;
    const int width=(right-x-space*(columns-1))/columns;
    for(int i=0;i<first;i++)if(grouped_read(menu->root,i,&item)&&!item.submenu){
        const int left=x+(used%columns)*(width+space);
        grouped_menu_item(menu,dc,(RECT){left,y,left+width,y+row},body,&item,used?1:2);
        if(++used%columns==0)y+=row+space;
    }
    if(used%columns)y+=row+space;
    int block=0,bottom=y;
    for(int i=first;i<GetMenuItemCount(menu->root);i++)if(grouped_read(menu->root,i,&item)&&item.submenu){
        const int left=x+(block%columns)*(width+space);
        const int end=grouped_card(menu,dc,body,&item,left,y,left+width);
        if(end>bottom)bottom=end;
        if(++block%columns==0)y=bottom+space;
    }
    return (block%columns?bottom+space:y);
}

static int grouped_tabs(GroupedMenu *menu,HDC dc,RECT body,int left,int y,int right){
    int cursor=left,has_tabs=0;
    const int height=grouped_px(menu,33),gap=grouped_px(menu,5),pad=grouped_px(menu,22);
    HGDIOBJ last=SelectObject(dc,native_font?(HGDIOBJ)native_font:GetStockObject(DEFAULT_GUI_FONT));
    for(int i=0;i<GetMenuItemCount(menu->branch);i++){
        GroupedMenuItem item;
        if(!grouped_read(menu->branch,i,&item)||!item.submenu)continue;
        SIZE text={0};
        GetTextExtentPoint32W(dc,item.label,(int)wcslen(item.label),&text);
        int width=text.cx+pad;
        if(width>right-left)width=right-left;
        if(cursor>left&&cursor+width>right){cursor=left;y+=height+gap;}
        RECT tile={cursor,y,cursor+width,y+height};
        grouped_fill(dc,tile,RGB(244,248,249));
        grouped_row(menu,dc,tile,body,item.label,GROUPED_TAB,item.submenu,0,item.disabled,item.submenu==menu->tab,0,0);
        cursor+=width+gap;
        has_tabs=1;
    }
    SelectObject(dc,last);
    return has_tabs?y+height+gap:y;
}

static int grouped_standard_side(GroupedMenu *menu,HMENU branch,int top){
    const int width=menu->root_screen.right-menu->root_screen.left;
    const int height=menu->root_screen.bottom-menu->root_screen.top;
    const int available_width=menu->work.right-menu->work.left-width;
    const int available_height=menu->work.bottom-menu->root_screen.top;
    int side_width=branch?grouped_px(menu,300):0;
    int side_height=branch?grouped_px(menu,460):0;
    if(branch){
        if(available_width<grouped_px(menu,245))return 0;
        if(side_width>available_width)side_width=available_width;
        const int content=grouped_px(menu,55)+grouped_px(menu,30)*menu->catalog_count;
        if(side_height>content)side_height=content;
        if(side_height>available_height)side_height=available_height;
    }
    const int rounding=grouped_px(menu,10);
    int x=menu->root_screen.left;
    if(x+width+side_width>menu->work.right)x=menu->work.right-width-side_width;
    if(branch&&menu->root_screen.top+top+side_height>menu->work.bottom)
        top=menu->work.bottom-menu->root_screen.top-side_height;
    RECT side={width,top,width+side_width,top+side_height};
    const int full_height=branch&&side.bottom>height?side.bottom:height;
    HRGN shape=CreateRoundRectRgn(0,0,width+1,height+1,rounding,rounding);
    if(!shape)return 0;
    if(branch){
        HRGN extension=CreateRoundRectRgn(side.left-1,side.top,side.right+1,side.bottom+1,rounding,rounding);
        if(!extension||CombineRgn(shape,shape,extension,RGN_OR)==ERROR){
            if(extension)DeleteObject(extension);
            DeleteObject(shape);return 0;
        }
        DeleteObject(extension);
    }
    if(!SetWindowPos(menu->window,NULL,x,menu->root_screen.top,width+side_width,full_height,
           SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER)){
        DeleteObject(shape);return 0;
    }
    menu->side=branch;
    menu->side_bounds=side;
    if(!SetWindowRgn(menu->window,shape,FALSE))DeleteObject(shape);
    return 1;
}

static void grouped_border(HDC dc,RECT rect){
    SetDCPenColor(dc,RGB(173,196,203));
    HGDIOBJ brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
    HGDIOBJ pen=SelectObject(dc,GetStockObject(DC_PEN));
    Rectangle(dc,rect.left,rect.top,rect.right,rect.bottom);
    SelectObject(dc,brush);
    SelectObject(dc,pen);
}

static void grouped_catalog_rebuild(GroupedMenu *menu){
    menu->catalog_visible_count=0;
    for(int i=0;i<menu->catalog_count;){
        GroupedCatalogRow *entry=&menu->catalog[i];
        menu->catalog_visible[menu->catalog_visible_count++]=i;
        i=entry->heading&&!entry->expanded?entry->next:i+1;
    }
}

/* Index rows once. The borrowed menus remain valid until the popup closes. */
static void grouped_catalog_index(GroupedMenu *menu){
    int groups=GetMenuItemCount(menu->fasteners),capacity=groups;
    GroupedMenuItem group,item;
    for(int i=0;i<groups;i++)if(grouped_read(menu->fasteners,i,&group)&&group.submenu){
        int children=GetMenuItemCount(group.submenu);
        if(children>0)capacity+=children;
    }
    if(capacity<=0)return;
    menu->catalog=HeapAlloc(GetProcessHeap(),0,(SIZE_T)capacity*sizeof(*menu->catalog));
    menu->catalog_visible=HeapAlloc(GetProcessHeap(),0,(SIZE_T)capacity*sizeof(*menu->catalog_visible));
    if(!menu->catalog||!menu->catalog_visible)return;
    for(int i=0;i<groups;i++)if(grouped_read(menu->fasteners,i,&group)){
        GroupedCatalogRow *entry=&menu->catalog[menu->catalog_count++];
        *entry=(GroupedCatalogRow){group,!!group.submenu};
        if(group.submenu)
            for(int j=0;j<GetMenuItemCount(group.submenu);j++)
                if(grouped_read(group.submenu,j,&item)&&!item.submenu)
                    menu->catalog[menu->catalog_count++]=(GroupedCatalogRow){item,0};
        entry->next=menu->catalog_count;
    }
    grouped_catalog_rebuild(menu);
}

static int grouped_catalog_scroll_to(GroupedMenu *menu,int offset){
    if(offset<0)offset=0;
    if(offset>menu->catalog_limit)offset=menu->catalog_limit;
    if(offset==menu->catalog_scroll)return 0;
    menu->catalog_scroll=offset;
    menu->focus=menu->hot=-1;
    return 1;
}

static void grouped_paint_catalog(GroupedMenu *menu,HDC dc,RECT pane,int inline_page){
    const int pad=grouped_px(menu,12),header=grouped_px(menu,42),row=grouped_px(menu,30);
    grouped_fill(dc,pane,RGB(254,255,255));
    RECT title={pane.left+pad,pane.top,pane.right-grouped_px(menu,35),pane.top+header};
    RECT back={pane.right-grouped_px(menu,33),pane.top+4,pane.right-4,pane.top+header-4};
    if(inline_page){
        back.left=pane.left+pad;
        back.right=back.left+grouped_px(menu,69);
        title.left=back.right+pad;
        grouped_row(menu,dc,back,pane,L"‹ 返回",GROUPED_CLOSE_SIDE,NULL,0,0,0,0,0);
    }else grouped_row(menu,dc,back,pane,L"×",GROUPED_CLOSE_SIDE,NULL,0,0,0,0,0);
    grouped_text(menu,dc,L"紧固件",title,RGB(16,55,73),1,DT_LEFT);
    grouped_fill(dc,(RECT){pane.left+1,pane.top+header,pane.right-1,pane.top+header+1},RGB(208,224,229));

    RECT body={pane.left+pad/2,pane.top+header+pad/2,pane.right-pad-grouped_px(menu,6),pane.bottom-pad/2};
    menu->catalog_body=body;
    menu->catalog_limit=menu->catalog_visible_count*row-(body.bottom-body.top);
    if(menu->catalog_limit<0)menu->catalog_limit=0;
    grouped_catalog_scroll_to(menu,menu->catalog_scroll);
    menu->focus=-1;
    int first=menu->catalog_scroll/row;
    int y=body.top-(menu->catalog_scroll%row);
    int saved=SaveDC(dc);
    IntersectClipRect(dc,body.left,body.top,body.right,body.bottom);
    for(int i=first;i<menu->catalog_visible_count&&y<body.bottom;i++,y+=row){
        const GroupedCatalogRow *entry=&menu->catalog[menu->catalog_visible[i]];
        RECT area={body.left,y,body.right,y+row};
        if(i==menu->catalog_focus&&!entry->item.disabled)menu->focus=menu->count;
        if(entry->heading){
            area.bottom-=grouped_px(menu,2);
            grouped_row(menu,dc,area,body,entry->item.label,GROUPED_CATALOG_TOGGLE,
                entry->item.submenu,(UINT)i,entry->item.disabled,1,entry->expanded?2:1,0);
            continue;
        }
        area.left+=pad;
        grouped_menu_item(menu,dc,area,body,&entry->item,0);
        grouped_fill(dc,(RECT){area.left+pad,area.bottom-1,area.right-pad,area.bottom},RGB(230,238,240));
    }
    RestoreDC(dc,saved);
    grouped_scrollbar(dc,(RECT){body.right+grouped_px(menu,5),body.top,
        pane.right-grouped_px(menu,4),body.bottom},menu->catalog_scroll,menu->catalog_limit);
    grouped_border(dc,pane);
}

static void grouped_catalog_key(GroupedMenu *menu,WPARAM key){
    const int height=menu->catalog_body.bottom-menu->catalog_body.top,row=grouped_px(menu,30);
    const int page=height/row>0?height/row:1;
    const int backward=key==VK_UP||key==VK_PRIOR||key==VK_END||
        (key==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000));
    const int direction=backward?-1:1;
    const int last=menu->catalog_visible_count-1;
    int index=menu->catalog_focus+direction;
    if(menu->catalog_focus<0)
        index=menu->catalog_scroll/row+(backward?page-1:0);
    else if(key==VK_PRIOR||key==VK_NEXT)
        index=menu->catalog_focus+direction*page;
    if(key==VK_HOME||index<0)index=0;
    if(key==VK_END||index>last)index=last;
    while(index>=0&&index<=last&&menu->catalog[menu->catalog_visible[index]].item.disabled)
        index+=direction;
    if(index<0||index>last){
        index=index<0?0:last;
        while(index>=0&&index<=last&&menu->catalog[menu->catalog_visible[index]].item.disabled)
            index-=direction;
        if(index<0||index>last)return;
    }
    menu->catalog_focus=index;
    int offset=menu->catalog_scroll;
    if(index*row<offset)offset=index*row;
    else if((index+1)*row>offset+height)offset=(index+1)*row-height;
    grouped_catalog_scroll_to(menu,offset);
    InvalidateRect(menu->window,NULL,FALSE);
    UpdateWindow(menu->window);
}

static int grouped_catalog_toggle(GroupedMenu *menu,int index,int expanded){
    if(index<0||index>=menu->catalog_visible_count)return 0;
    GroupedCatalogRow *entry=&menu->catalog[menu->catalog_visible[index]];
    if(!entry->heading||entry->item.disabled)return 0;
    if(expanded<0)expanded=!entry->expanded;
    if(entry->expanded==expanded)return 0;
    entry->expanded=expanded;
    grouped_catalog_rebuild(menu);
    menu->catalog_focus=index;
    menu->focus=menu->hot=-1;
    InvalidateRect(menu->window,NULL,FALSE);
    UpdateWindow(menu->window);
    return 1;
}

static void grouped_catalog_point(GroupedMenu *menu,int y){
    RECT body=menu->catalog_body;
    if(body.bottom>body.top){
        menu->catalog_focus=-1;
        grouped_catalog_scroll_to(menu,MulDiv(y-body.top,menu->catalog_limit,body.bottom-body.top));
        InvalidateRect(menu->window,NULL,FALSE);
        UpdateWindow(menu->window);
    }
}

static void grouped_scroll(GroupedMenu *menu,int amount,int rail){
    int *value=rail?&menu->rail_scroll:&menu->scroll;
    const int bound=rail?menu->rail_limit:menu->limit;
    int next=*value+amount;
    if(next<0)next=0;
    if(next>bound)next=bound;
    if(next!=*value){
        if(menu->side)grouped_standard_side(menu,NULL,0);
        *value=next;
        menu->focus=menu->hot=-1;
        InvalidateRect(menu->window,NULL,FALSE);
        UpdateWindow(menu->window);
    }
}

static void grouped_paint(GroupedMenu *menu,HDC dc,RECT client){
    const int p=grouped_px(menu,14),row=grouped_px(menu,35);
    menu->count=0;
    if(menu->catalog_inline){
        grouped_paint_catalog(menu,dc,client,1);
        return;
    }
    GroupedMenuItem item;
    grouped_fill(dc,client,RGB(254,255,255));
    int header=grouped_px(menu,54),footer=0;
    if(menu->mode==NATIVE_GROUPED_IMITATION){
        for(int i=0,after=0;i<GetMenuItemCount(menu->root);i++)if(grouped_read(menu->root,i,&item)){
            if(item.submenu)after=1;
            else if(after){footer=grouped_px(menu,41);break;}
        }
    }
    if(menu->side){
        client.right=menu->root_screen.right-menu->root_screen.left;
        client.bottom=menu->root_screen.bottom-menu->root_screen.top;
    }
    menu->body_top=header+1;
    menu->body_bottom=client.bottom-footer-1;
    const int right=client.right;
    menu->rail_width=(menu->mode==NATIVE_GROUPED_COMPACT||menu->mode==NATIVE_GROUPED_STANDARD)?0:
        right<grouped_px(menu,720)?grouped_px(menu,149):grouped_px(menu,184);
    RECT body={menu->rail_width,menu->body_top,right,menu->body_bottom};
    RECT head={1,1,right-1,header};
    grouped_text(menu,dc,menu->title,(RECT){p,1,right-2*p,header},RGB(16,55,73),1,DT_LEFT);
    grouped_row(menu,dc,(RECT){right-grouped_px(menu,34),grouped_px(menu,9),right-4,header-grouped_px(menu,8)},
        head,L"×",GROUPED_CLOSE,NULL,0,0,0,0,0);
    if(menu->mode==NATIVE_GROUPED_FEATURES){
        for(int i=0;i<GetMenuItemCount(menu->root);i++)if(grouped_read(menu->root,i,&item)&&!item.submenu){
            RECT button={right-grouped_px(menu,155),grouped_px(menu,10),right-grouped_px(menu,42),header-grouped_px(menu,9)};
            grouped_fill(dc,button,RGB(234,242,245));
            grouped_menu_item(menu,dc,button,head,&item,0);
            break;
        }
    }
    grouped_fill(dc,(RECT){1,header,right-1,header+1},RGB(208,224,229));
    if(menu->rail_width){
        RECT rail={1,body.top,menu->rail_width,body.bottom};
        grouped_fill(dc,rail,RGB(240,247,248));
        grouped_fill(dc,(RECT){rail.right-1,rail.top,rail.right,rail.bottom},RGB(213,229,232));
        int y=rail.top+p-menu->rail_scroll;
        const int height=grouped_px(menu,44);
        int saved=SaveDC(dc);
        IntersectClipRect(dc,rail.left,rail.top,rail.right,rail.bottom);
        if(menu->mode==NATIVE_GROUPED_IMITATION){
            grouped_row(menu,dc,(RECT){p/2,y,rail.right-p/2,y+height-3},rail,L"全部操作",
                GROUPED_RAIL,NULL,0,0,!menu->branch,1,1);
            y+=height;
        }
        for(int i=0;i<GetMenuItemCount(menu->root);i++)if(grouped_read(menu->root,i,&item)&&item.submenu){
            grouped_row(menu,dc,(RECT){p/2,y,rail.right-p/2,y+height-3},rail,item.label,
                GROUPED_RAIL,item.submenu,0,item.disabled,item.submenu==menu->branch,1,1);
            y+=height;
        }
        RestoreDC(dc,saved);
        menu->rail_limit=y+menu->rail_scroll+p-rail.bottom;
        if(menu->rail_limit<0)menu->rail_limit=0;
        grouped_scrollbar(dc,(RECT){rail.right-5,rail.top+4,rail.right-2,rail.bottom-4},menu->rail_scroll,menu->rail_limit);
    }
    int saved=SaveDC(dc);
    IntersectClipRect(dc,body.left,body.top,body.right,body.bottom);
    int x=body.left+p,y=body.top+p-menu->scroll;
    const int end=right-p;
    if(menu->depth){
        grouped_row(menu,dc,(RECT){x,y,end,y+row},body,L"‹ 返回",GROUPED_BACK,NULL,0,0,1,0,0);
        y+=row+grouped_px(menu,5);
        grouped_text(menu,dc,menu->subpage[menu->depth-1],(RECT){x,y,end,y+row},RGB(16,55,73),1,DT_LEFT);
        y=grouped_list(menu,dc,body,menu->showing[menu->depth-1],x,y+row,end,0);
    }else if(menu->mode==NATIVE_GROUPED_IMITATION&&!menu->branch){
        y=grouped_overview(menu,dc,body,x,y,end);
    }else if(menu->mode==NATIVE_GROUPED_COMPACT||menu->mode==NATIVE_GROUPED_STANDARD){
        int used=0;
        for(int i=0;i<GetMenuItemCount(menu->root);i++)if(grouped_read(menu->root,i,&item)){
            if(item.submenu&&item.submenu==menu->fasteners){
                grouped_row(menu,dc,(RECT){x,y,end,y+row},body,item.label,GROUPED_SIDE,
                    item.submenu,0,item.disabled,1,1,0);
                y+=row+grouped_px(menu,5);
            }else if(item.submenu)y=grouped_card(menu,dc,body,&item,x,y,end)+grouped_px(menu,7);
            else{
                grouped_menu_item(menu,dc,(RECT){x,y,end,y+row},body,&item,used?0:2);
                y+=row+grouped_px(menu,5);
            }
            used++;
        }
    }else if(menu->branch){
        grouped_text(menu,dc,menu->section,(RECT){x,y,end,y+row},RGB(16,55,73),1,DT_LEFT);
        y+=row;
        if(menu->mode==NATIVE_GROUPED_FEATURES&&menu->tab)
            y=grouped_tabs(menu,dc,body,x,y,end)+grouped_px(menu,5);
        y=grouped_list(menu,dc,body,menu->tab?menu->tab:menu->branch,x,y,end,0);
    }
    RestoreDC(dc,saved);
    menu->limit=y+menu->scroll+p-body.bottom;
    if(menu->limit<0)menu->limit=0;
    grouped_scrollbar(dc,(RECT){right-8,body.top+6,right-4,body.bottom-5},menu->scroll,menu->limit);
    if(footer){
        RECT foot={1,body.bottom,right-1,client.bottom-1};
        grouped_fill(dc,foot,RGB(242,248,249));
        grouped_fill(dc,(RECT){1,body.bottom,right-1,body.bottom+1},RGB(211,227,231));
        int after=0,slot=0;
        for(int i=0;i<GetMenuItemCount(menu->root);i++)if(grouped_read(menu->root,i,&item)){
            if(item.submenu){after=1;continue;}
            if(!after)continue;
            int split=right/3;
            RECT area={slot?split:p,foot.top+3,slot?right-p:split,foot.bottom-3};
            grouped_menu_item(menu,dc,area,foot,&item,0);
            if(++slot==2)break;
        }
    }
    grouped_border(dc,client);
    if(menu->side)grouped_paint_catalog(menu,dc,menu->side_bounds,0);
}

static int grouped_hit(const GroupedMenu *menu,int x,int y){
    POINT point={x,y};
    for(int i=0;i<menu->count;i++)if(PtInRect(&menu->hits[i].rect,point))return i;
    return -1;
}

static void grouped_choose(GroupedMenu *menu,int index){
    if(index<0||index>=menu->count)return;
    GroupedMenuHit hit=menu->hits[index];
    switch(hit.action){
    case GROUPED_LEAF:menu->result=hit.command;DestroyWindow(menu->window);return;
    case GROUPED_CLOSE:DestroyWindow(menu->window);return;
    case GROUPED_CATALOG_TOGGLE:grouped_catalog_toggle(menu,(int)hit.command,-1);return;
    case GROUPED_CLOSE_SIDE:
        if(menu->side&&!grouped_standard_side(menu,NULL,0))return;
        menu->catalog_inline=0;
        menu->focus=menu->hot=-1;
        InvalidateRect(menu->window,NULL,FALSE);
        UpdateWindow(menu->window);
        for(int i=0;i<menu->count;i++)if(menu->hits[i].action==GROUPED_SIDE){
            menu->focus=i;
            InvalidateRect(menu->window,NULL,FALSE);
            break;
        }
        return;
    case GROUPED_SIDE:
        if(menu->side){
            for(int i=0;i<menu->count;i++)if(menu->hits[i].action==GROUPED_CLOSE_SIDE){
                grouped_choose(menu,i);
                break;
            }
            return;
        }
        if(!menu->catalog_count)goto open_page;
        menu->catalog_scroll=0;
        menu->catalog_focus=-1;
        menu->focus=menu->hot=-1;
        menu->catalog_inline=!grouped_standard_side(menu,hit.target,hit.rect.top);
        InvalidateRect(menu->window,NULL,FALSE);
        UpdateWindow(menu->window);
        grouped_catalog_key(menu,VK_HOME);
        return;
    case GROUPED_RAIL:
        menu->branch=hit.target;
        wcscpy_s(menu->section,_countof(menu->section),hit.label);
        menu->tab=menu->mode==NATIVE_GROUPED_FEATURES?grouped_first_branch(menu->branch,menu->tab_title):NULL;
        menu->depth=0;
        break;
    case GROUPED_TAB:
        menu->tab=hit.target;
        wcscpy_s(menu->tab_title,_countof(menu->tab_title),hit.label);
        menu->depth=0;
        break;
    case GROUPED_OPEN:
open_page:
        if(menu->side&&!grouped_standard_side(menu,NULL,0))return;
        if(menu->depth>=(int)_countof(menu->showing))return;
        menu->showing[menu->depth]=hit.target;
        wcscpy_s(menu->subpage[menu->depth],_countof(menu->subpage[0]),hit.label);
        menu->depth++;
        break;
    case GROUPED_BACK:
        if(menu->depth)menu->depth--;
        break;
    }
    menu->scroll=0;
    menu->focus=menu->hot=-1;
    InvalidateRect(menu->window,NULL,FALSE);
    UpdateWindow(menu->window);
}

static LRESULT CALLBACK grouped_proc(HWND window,UINT msg,WPARAM w,LPARAM l){
    GroupedMenu *menu=(GroupedMenu *)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(msg==WM_NCCREATE){
        menu=((CREATESTRUCTW *)l)->lpCreateParams;
        menu->window=window;
        SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)menu);
        return TRUE;
    }
    if(!menu)return DefWindowProcW(window,msg,w,l);
    switch(msg){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
        PAINTSTRUCT paint;
        RECT rect;
        HDC dc=BeginPaint(window,&paint);
        GetClientRect(window,&rect);
        HDC canvas=CreateCompatibleDC(dc);
        HBITMAP bitmap=canvas?CreateCompatibleBitmap(dc,rect.right,rect.bottom):NULL;
        if(bitmap){
            HGDIOBJ before=SelectObject(canvas,bitmap);
            SetBkMode(canvas,TRANSPARENT);
            grouped_paint(menu,canvas,rect);
            BitBlt(dc,0,0,rect.right,rect.bottom,canvas,0,0,SRCCOPY);
            SelectObject(canvas,before);
            DeleteObject(bitmap);
        }else{
            SetBkMode(dc,TRANSPARENT);
            grouped_paint(menu,dc,rect);
        }
        if(canvas)DeleteDC(canvas);
        EndPaint(window,&paint);
        return 0;
    }
    case WM_PRINTCLIENT:{
        RECT rect;
        GetClientRect(window,&rect);
        SetBkMode((HDC)w,TRANSPARENT);
        grouped_paint(menu,(HDC)w,rect);
        return 0;
    }
    case WM_MOUSEMOVE:{
        if(menu->catalog_drag){
            grouped_catalog_point(menu,(short)HIWORD(l));
            return 0;
        }
        int next=grouped_hit(menu,(short)LOWORD(l),(short)HIWORD(l));
        if(next!=menu->hot){menu->hot=next;InvalidateRect(window,NULL,FALSE);}
        return 0;
    }
    case WM_LBUTTONDOWN:{
        RECT rect;POINT point={(short)LOWORD(l),(short)HIWORD(l)};
        GetClientRect(window,&rect);
        RECT catalog=menu->catalog_inline?rect:menu->side_bounds;
        if((menu->side||menu->catalog_inline)&&PtInRect(&catalog,point)){
            if(menu->catalog_limit&&point.x>=menu->catalog_body.right&&
               point.y>=menu->catalog_body.top&&point.y<menu->catalog_body.bottom){
                menu->catalog_drag=1;
                grouped_catalog_point(menu,point.y);
            }
            return 0;
        }
        if(menu->side){
            rect.right=menu->root_screen.right-menu->root_screen.left;
            rect.bottom=menu->root_screen.bottom-menu->root_screen.top;
        }
        if(!PtInRect(&rect,point)){DestroyWindow(window);return 0;}
        if(point.x>=rect.right-grouped_px(menu,12)&&point.y>=menu->body_top&&point.y<menu->body_bottom){
            int wanted=MulDiv(point.y-menu->body_top,menu->limit,menu->body_bottom-menu->body_top);
            grouped_scroll(menu,wanted-menu->scroll,0);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if(menu->catalog_drag)menu->catalog_drag=0;
        else grouped_choose(menu,grouped_hit(menu,(short)LOWORD(l),(short)HIWORD(l)));
        return 0;
    case WM_MOUSEWHEEL:{
        POINT point={(short)LOWORD(l),(short)HIWORD(l)};
        ScreenToClient(window,&point);
        if(menu->catalog_inline||(menu->side&&PtInRect(&menu->side_bounds,point))){
            int shift=-(short)HIWORD(w)*grouped_px(menu,90)/WHEEL_DELTA;
            if(grouped_catalog_scroll_to(menu,menu->catalog_scroll+shift)){
                menu->catalog_focus=-1;
                InvalidateRect(window,NULL,FALSE);
                UpdateWindow(window);
            }
            return 0;
        }
        grouped_scroll(menu,-(short)HIWORD(w)*grouped_px(menu,105)/WHEEL_DELTA,
            menu->rail_width&&point.x<menu->rail_width);
        return 0;
    }
    case WM_KEYDOWN:{
        if((menu->side||menu->catalog_inline)&&(w==VK_ESCAPE||w==VK_LEFT||w==VK_RIGHT||
           w==VK_UP||w==VK_DOWN||w==VK_HOME||w==VK_END||w==VK_NEXT||w==VK_PRIOR||w==VK_TAB)){
            if(w==VK_LEFT||w==VK_RIGHT){
                int index=menu->focus;
                if(index>=0&&index<menu->count&&menu->hits[index].action==GROUPED_CATALOG_TOGGLE&&
                   grouped_catalog_toggle(menu,(int)menu->hits[index].command,w==VK_RIGHT))return 0;
                if(w==VK_RIGHT)return 0;
            }
            if(w==VK_ESCAPE||w==VK_LEFT){
                for(int i=0;i<menu->count;i++)if(menu->hits[i].action==GROUPED_CLOSE_SIDE){
                    grouped_choose(menu,i);
                    break;
                }
            }else grouped_catalog_key(menu,w);
        }else if(w==VK_ESCAPE){
            if(menu->depth){
                menu->depth--;
                menu->scroll=0;menu->focus=menu->hot=-1;
                InvalidateRect(window,NULL,FALSE);
                UpdateWindow(window);
            }else DestroyWindow(window);
        }else if(w==VK_RETURN||w==VK_SPACE||
                  (w==VK_RIGHT&&menu->focus>=0&&menu->focus<menu->count&&menu->hits[menu->focus].action==GROUPED_SIDE)){
            grouped_choose(menu,menu->focus);
        }else if(w==VK_NEXT||w==VK_PRIOR){
            int rail=menu->focus>=0&&menu->focus<menu->count&&menu->hits[menu->focus].rail;
            grouped_scroll(menu,(w==VK_NEXT?1:-1)*(menu->body_bottom-menu->body_top-grouped_px(menu,36)),rail);
        }else if(menu->count&&(w==VK_TAB||w==VK_UP||w==VK_DOWN||w==VK_HOME||w==VK_END)){
            int previous=w==VK_UP||(w==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000));
            const int first=0;
            const int count=menu->count-first;
            if(w==VK_HOME)menu->focus=first;
            else if(w==VK_END)menu->focus=menu->count-1;
            else if(menu->focus<first)menu->focus=previous?menu->count-1:first;
            else menu->focus=first+(menu->focus-first+(previous?count-1:1))%count;
            InvalidateRect(window,NULL,FALSE);
        }
        return 0;
    }
    case WM_ACTIVATE:
        if(LOWORD(w)==WA_INACTIVE)PostMessageW(window,WM_CLOSE,0,0);
        return 0;
    case WM_CAPTURECHANGED:
        if(menu->capture&&!menu->done)PostMessageW(window,WM_CLOSE,0,0);
        return 0;
    case WM_CLOSE:DestroyWindow(window);return 0;
    case WM_DESTROY:
        menu->restore_focus=GetActiveWindow()==window;
        menu->done=1;
        if(GetCapture()==window)ReleaseCapture();
        return 0;
    }
    return DefWindowProcW(window,msg,w,l);
}

static UINT native_grouped_menu(HWND button,HMENU root,const wchar_t *title,int mode){
    RECT anchor,owner;
    if(!root||!GetWindowRect(button,&anchor))return 0;
    const UINT flags=TPM_RETURNCMD|TPM_NONOTIFY|TPM_LEFTALIGN;
    if(!IsWindowVisible(button)||!GetWindowRect(native_window,&owner))
        return TrackPopupMenuEx(root,flags,anchor.left,anchor.bottom,native_window,NULL);
    MONITORINFO screen={sizeof(screen)};
    if(!GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&screen))
        return TrackPopupMenuEx(root,flags,anchor.left,anchor.bottom,native_window,NULL);
    GroupedMenu menu={0};
    menu.title=title;
    menu.root=root;
    menu.mode=mode;
    menu.dpi=native_dpi>0?native_dpi:96;
    menu.focus=menu.hot=-1;
    if(mode==NATIVE_GROUPED_STANDARD)menu.fasteners=GetSubMenu(root,1);
    if(mode==NATIVE_GROUPED_FEATURES){
        menu.branch=grouped_first_branch(root,menu.section);
        if(menu.branch)menu.tab=grouped_first_branch(menu.branch,menu.tab_title);
    }
    RECT work=screen.rcWork;
    const int compact=mode==NATIVE_GROUPED_COMPACT||mode==NATIVE_GROUPED_STANDARD;
    int width=grouped_px(&menu,compact?346:938);
    int owner_width=owner.right-owner.left;
    if(width>owner_width)width=owner_width;
    if(width>work.right-work.left)width=work.right-work.left;
    int height=grouped_px(&menu,compact?370:596);
    if(height>work.bottom-work.top)height=work.bottom-work.top;
    if(width<grouped_px(&menu,260)||height<grouped_px(&menu,220))
        return TrackPopupMenuEx(root,flags,anchor.left,anchor.bottom,native_window,NULL);
    int left=anchor.left,top=anchor.bottom;
    if(left+width>owner.right)left=owner.right-width;
    if(left<work.left)left=work.left;
    if(left+width>work.right)left=work.right-width;
    if(top+height>work.bottom&&anchor.top-height>=work.top)top=anchor.top-height;
    if(top+height>work.bottom)top=work.bottom-height;
    if(top<work.top)top=work.top;
    menu.root_screen=(RECT){left,top,left+width,top+height};
    menu.work=work;
    WNDCLASSW wc={0};
    wc.style=CS_DROPSHADOW;
    wc.lpfnWndProc=grouped_proc;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)grouped_proc,&wc.hInstance))
        return TrackPopupMenuEx(root,flags,anchor.left,anchor.bottom,native_window,NULL);
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.lpszClassName=L"BWTLGroupedMenu";
    ATOM registered=RegisterClassW(&wc);
    if(!registered&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
        return TrackPopupMenuEx(root,flags,anchor.left,anchor.bottom,native_window,NULL);
    LOGFONTW font={0};
    if(native_font&&GetObjectW(native_font,sizeof(font),&font)==sizeof(font)){
        font.lfWeight=FW_SEMIBOLD;
        menu.bold=CreateFontIndirectW(&font);
    }
    HWND parent=native_window;
    HWND prior=GetFocus();
    HWND pop=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,title,WS_POPUP|WS_CLIPSIBLINGS,
        left,top,width,height,parent,NULL,wc.hInstance,&menu);
    if(!pop){
        if(menu.bold)DeleteObject(menu.bold);
        if(registered)UnregisterClassW(wc.lpszClassName,wc.hInstance);
        return TrackPopupMenuEx(root,flags,anchor.left,anchor.bottom,parent,NULL);
    }
    if(menu.fasteners)grouped_catalog_index(&menu);
    HRGN shape=CreateRoundRectRgn(0,0,width+1,height+1,grouped_px(&menu,10),grouped_px(&menu,10));
    if(shape&&!SetWindowRgn(pop,shape,FALSE))DeleteObject(shape);
    int enabled=IsWindowEnabled(parent);
    if(enabled)EnableWindow(parent,FALSE);
    ShowWindow(pop,SW_SHOWNORMAL);
    SetFocus(pop);
    SetCapture(pop);
    menu.capture=GetCapture()==pop;
    UpdateWindow(pop);
    MSG msg={0};
    BOOL get=1;
    while(!menu.done&&(get=GetMessageW(&msg,NULL,0,0))>0){
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if(IsWindow(pop))DestroyWindow(pop);
    if(IsWindow(parent)){
        if(enabled)EnableWindow(parent,TRUE);
        if(menu.restore_focus||GetActiveWindow()==parent){
            SetActiveWindow(parent);
            if(IsWindow(prior))SetFocus(prior);
        }
    }
    if(get==0)PostQuitMessage((int)msg.wParam);
    if(menu.catalog_visible)HeapFree(GetProcessHeap(),0,menu.catalog_visible);
    if(menu.catalog)HeapFree(GetProcessHeap(),0,menu.catalog);
    if(menu.bold)DeleteObject(menu.bold);
    if(registered)UnregisterClassW(wc.lpszClassName,wc.hInstance);
    return menu.result;
}