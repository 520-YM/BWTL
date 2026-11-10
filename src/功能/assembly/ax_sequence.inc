
static ProError ax_view_sequence(ProMdl assembly,const PartCommand *cmd,PartText *out){
    typedef struct { ProMdlName name; ProOutline box; int id; } SequenceItem;
    SequenceItem items[PART_ARG_MAX]={0};
    int count=cmd->count-3;
    ProOutline range={{INFINITY,INFINITY,0},{-INFINITY,-INFINITY,0}};
    for(int i=0;i<count;i++){
        AxComponent owner={0};
        ProOutline local;
        ProMatrix pose;
        ProError error=ax_component_resolve(assembly,cmd->arg[i+3],&owner);
        if(!error)error=ProMdlMdlnameGet(owner.model,items[i].name);
        if(!error)error=ProSolidOutlineGet((ProSolid)owner.model,local);
        if(!error)error=ax_actual_pose(assembly,&owner.feature,pose);
        if(error)return error;
        items[i].id=owner.feature.id;
        for(int axis=0;axis<2;axis++){
            items[i].box[0][axis]=INFINITY;
            items[i].box[1][axis]=-INFINITY;
        }
        for(int corner=0;corner<8;corner++){
            Pro3dPnt point={local[corner&1][0],local[(corner>>1)&1][1],local[corner>>2][2]},world;
            if((error=ProPntTrfEval(point,pose,world)))return error;
            for(int axis=0;axis<2;axis++){
                if(!isfinite(world[axis]))return PRO_TK_BAD_CONTEXT;
                items[i].box[0][axis]=fmin(items[i].box[0][axis],world[axis]);
                items[i].box[1][axis]=fmax(items[i].box[1][axis],world[axis]);
                range[0][axis]=fmin(range[0][axis],world[axis]);
                range[1][axis]=fmax(range[1][axis],world[axis]);
            }
        }
    }
    double span=fmax(range[1][0]-range[0][0],range[1][1]-range[0][1]);
    if(!isfinite(span)||span<=0)return PRO_TK_BAD_CONTEXT;
    double scale=120/span;
    DeliveryOutput output={0};
    ProError error=dv_stage_begin(cmd->arg[2],&output);
    if(error)return error;
    ProPath path;
    if(swprintf_s(path,_countof(path),L"%ls\\sequence.svg",output.stage)<0)
        return dv_stage_finish(&output,PRO_TK_BAD_INPUTS,out);
    FILE *file=_wfopen(path,L"wt, ccs=UTF-8");
    if(!file)return dv_stage_finish(&output,PRO_TK_CANT_WRITE,out);
    int height=50+155*((count+2)/3);
    int ok=fwprintf(file,L"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 690 %d\" width=\"690\" height=\"%d\">\n"
        L"<rect width=\"100%%\" height=\"100%%\" fill=\"#ffffff\"/><text x=\"12\" y=\"23\" fill=\"#173247\">装配顺序 · XY 投影包络（蓝色为本步加入）</text>\n",height,height)>0;
    for(int i=0;ok&&i<count;i++){
        int x=10+230*(i%3),y=40+155*(i/3);
        ok=fwprintf(file,L"<g transform=\"translate(%d %d)\"><rect width=\"220\" height=\"145\" fill=\"#f4f7fa\"/>"
            L"<text x=\"5\" y=\"18\" font-size=\"12\">%d · ID:%d ",x,y,i+1,items[i].id)>0;
        for(const wchar_t *p=items[i].name;ok&&*p;p++)
            ok=(*p==L'&'?fputws(L"&amp;",file):*p==L'<'?fputws(L"&lt;",file):
                *p==L'>'?fputws(L"&gt;",file):fputwc(*p,file))!=WEOF;
        if(ok)ok=fputws(L"</text>\n",file)>=0;
        for(int j=0;ok&&j<=i;j++){
            Pro3dPnt *box=items[j].box;
            ok=fwprintf(file,L"<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"%ls\" fill-opacity=\"0.32\" stroke=\"%ls\"/>\n",
                50+(int)lround((box[0][0]-range[0][0])*scale),
                23+(int)lround((range[1][1]-box[1][1])*scale),
                (int)fmax(2,lround((box[1][0]-box[0][0])*scale)),
                (int)fmax(2,lround((box[1][1]-box[0][1])*scale)),
                j==i?L"#1976d2":L"#82949f",j==i?L"#135baa":L"#627984")>0;
        }
        if(ok)ok=fputws(L"</g>\n",file)>=0;
    }
    if(ok)ok=fputws(L"</svg>\n",file)>=0;
    if(fclose(file)!=0||!ok||!dv_file_nonempty(path))error=PRO_TK_CANT_WRITE;
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"已生成 %d 步装配顺序示意图：%ls\\sequence.svg。\n",count,output.target);
    return error;
}
