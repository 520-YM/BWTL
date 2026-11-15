#include <ProWindows.h>
#include <ProView.h>

typedef struct {
    ProMdl model;
    ProWVerstamp stamp;
    ProPath directory;
    int window;
    unsigned long ticket;
    MarkupImage *image;
} MarkupCapture;
static MarkupCapture markup_capture;
static MarkupRequest markup_request;
static unsigned long markup_serial;
static int markup_pending;

static void markup_capture_clear(void){
    if(markup_capture.stamp)ProVerstampFree(&markup_capture.stamp);
    markup_image_free(markup_capture.image);
    memset(&markup_capture,0,sizeof(markup_capture));
}

static HBITMAP markup_read_raster(const wchar_t *path,int *width,int *height){
    IWICImagingFactory *factory=NULL;
    IWICBitmapDecoder *decoder=NULL;
    IWICBitmapFrameDecode *frame=NULL;
    IWICFormatConverter *converter=NULL;
    HBITMAP bitmap=NULL;
    UINT w=0,h=0;
    BYTE *pixels=NULL;
    HRESULT init=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    if(FAILED(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,
       &IID_IWICImagingFactory,(void **)&factory)))goto done;
    if(FAILED(IWICImagingFactory_CreateDecoderFromFilename(factory,path,NULL,GENERIC_READ,
       WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(IWICBitmapDecoder_GetFrame(decoder,0,&frame))||
       FAILED(IWICBitmapFrameDecode_GetSize(frame,&w,&h))||!w||!h||w>4096||h>4096)goto done;
    if(FAILED(IWICImagingFactory_CreateFormatConverter(factory,&converter))||
       FAILED(IWICFormatConverter_Initialize(converter,(IWICBitmapSource *)frame,&GUID_WICPixelFormat32bppBGRA,
          WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom)))goto done;
    BITMAPINFO info={0};
    info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=w;
    info.bmiHeader.biHeight=-(LONG)h;
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    bitmap=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);
    if(bitmap&&FAILED(IWICFormatConverter_CopyPixels(converter,NULL,w*4,w*h*4,pixels))){
        DeleteObject(bitmap);
        bitmap=NULL;
    }
    if(bitmap){*width=(int)w;*height=(int)h;}
done:
    if(converter)IWICFormatConverter_Release(converter);
    if(frame)IWICBitmapFrameDecode_Release(frame);
    if(decoder)IWICBitmapDecoder_Release(decoder);
    if(factory)IWICImagingFactory_Release(factory);
    if(SUCCEEDED(init))CoUninitialize();
    return bitmap;
}

typedef struct {
    MarkupImage *image;
    ProMdl model;
    int window;
    ProMatrix view,zoom;
    ProOutline viewport;
    ProVector toward_viewer;
    double span;
    const BYTE *pixels;
    int stride;
} MarkupProjection;

#include "markup_window_capture.inc"

static int markup_project(MarkupProjection *map,ProPoint3d model,Pro2dPnt result){
    ProPoint3d view,window;
    Pro2dPnt pixel;
    if(ProPntTrfEval(model,map->view,view)||ProPntTrfEval(view,map->zoom,window)||
       ProWindowCoordinatePixelGet(map->window,window,pixel))return 0;
    result[0]=(pixel[0]-map->viewport[0][0])*map->image->width/(map->viewport[1][0]-map->viewport[0][0]);
    result[1]=(map->viewport[1][1]-pixel[1])*map->image->height/(map->viewport[1][1]-map->viewport[0][1]);
    return isfinite(result[0])&&isfinite(result[1]);
}

typedef struct { ProDimension dimension; double diameter; int count,owner; } MarkupDriver;
static ProError markup_driver(ProDimension *dimension,ProError status,ProAppData data){
    MarkupDriver *driver=data;
    if(status)return status;
    ProDimensiontype type;
    ProBoolean driving,related,negative;
    double value;
    if(!ProDimensionTypeGet(dimension,&type)&&type==PRODIMTYPE_DIAMETER&&
       !ProDimensionValueGet(dimension,&value)&&part_close(value,driver->diameter)&&
       !ProDimensionIsDriving(dimension,&driving)&&driving&&
       !ProDimensionIsReldriven(dimension,&related)&&!related&&
       !ProDimensionIsRegenednegative(dimension,&negative)&&!negative){
        driver->dimension=*dimension;
        driver->count++;
    }
    return PRO_TK_NO_ERROR;
}

/* A circular cut is editable only when its driving sketch dimension can be
   bound to this circle and no same-size circle in that sketch could share it. */
static int markup_cut_driver(ProFeature *feature,const ProCylinderdata *cylinder,MarkupDriver *driver){
    ProElement tree=NULL;
    ProSection section=NULL;
    ProFeature sketch=*feature;
    ProElement sketch_tree=NULL;
    int *entities=NULL,*section_dims=NULL,*solid_dims=NULL;
    int count=0,dim_count=0,circle_id=-1,matching_radius=0,found=0;
    if(ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree))goto cleanup;
    if(fd_section_from_tree(tree,&section)){
        ProElement reference_element;
        ProReference reference=NULL;
        ProSelection selection=NULL;
        ProModelitem item;
        int valid=!mc_tree_element(tree,(ProElemId[]){PRO_E_STD_SECTION,PRO_E_SEC_USE_SKETCH},2,&reference_element)&&
            !ProElementReferenceGet(reference_element,NULL,&reference)&&
            !ProReferenceToSelection(reference,&selection)&&
            !ProSelectionModelitemGet(selection,&item)&&item.type==PRO_FEATURE&&item.owner==feature->owner;
        if(selection)ProSelectionFree(&selection);
        if(reference)ProReferenceFree(reference);
        if(!valid||ProFeatureInit((ProSolid)feature->owner,item.id,&sketch))goto cleanup;
        int *children=NULL,children_count=0;
        valid=!ProFeatureChildrenGet(&sketch,&children,&children_count)&&children_count==1&&children[0]==feature->id;
        ProArrayFree((ProArray *)&children);
        if(!valid||ProFeatureElemtreeExtract(&sketch,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&sketch_tree)||
           fd_section_from_tree(sketch_tree,&section))goto cleanup;
    }
    ProMatrix frame;
    if(ProSectionLocationGet(section,frame)||fabs(ad_dot(frame[2],(double *)cylinder->e3))<0.99999||
       ProSectionEntityIdsGet(section,&entities,&count))goto cleanup;
    for(int i=0;i<count;i++){
        Pro2dEntdef *entity=NULL;
        if(ProSectionEntityGet(section,entities[i],&entity)||entity->type!=PRO_2D_CIRCLE)continue;
        Pro2dCircledef *circle=(Pro2dCircledef *)entity;
        if(!part_close(circle->radius,cylinder->radius))continue;
        matching_radius++;
        ProPoint3d point={circle->center[0],circle->center[1],0},world;
        ProVector relative;
        if(ProPntTrfEval(point,frame,world))goto cleanup;
        for(int j=0;j<3;j++)relative[j]=world[j]-cylinder->origin[j];
        double along=ad_dot(relative,(double *)cylinder->e3);
        for(int j=0;j<3;j++)relative[j]-=along*cylinder->e3[j];
        if(hypot(hypot(relative[0],relative[1]),relative[2])<=1e-5)circle_id=entities[i];
    }
    if(matching_radius!=1||circle_id<0||
       ProSectionDimensionIdsGet(section,&section_dims,&solid_dims,&dim_count))goto cleanup;
    for(int i=0;i<dim_count;i++){
        ProSecdimType type;
        if(ProSecdimTypeGet(section,section_dims[i],&type)||
           (type!=PRO_TK_DIM_DIA&&type!=PRO_TK_DIM_RAD))continue;
        int *refs=NULL,n=0;
        ProSectionPointType *points=NULL;
        int exact=!ProSecdimReferencesGet(section,section_dims[i],&refs,&points,&n)&&n==1&&refs[0]==circle_id;
        ProArrayFree((ProArray *)&refs);
        ProArrayFree((ProArray *)&points);
        if(!exact)continue;
        ProDimension dim;
        ProDimensiontype dimension_type;
        ProBoolean driving,related,negative;
        ProFeature owner;
        double value;
        if(ProModelitemInit(feature->owner,solid_dims[i],PRO_DIMENSION,&dim)||
           ProDimensionOwnerfeatureGet(&dim,&owner)||owner.id!=sketch.id||
           ProDimensionTypeGet(&dim,&dimension_type)||ProDimensionValueGet(&dim,&value)||
           ProDimensionIsDriving(&dim,&driving)||!driving||
           ProDimensionIsReldriven(&dim,&related)||related||
           ProDimensionIsRegenednegative(&dim,&negative)||negative||
           !((dimension_type==PRODIMTYPE_DIAMETER&&part_close(value,2*cylinder->radius))||
             (dimension_type==PRODIMTYPE_RADIUS&&part_close(value,cylinder->radius))))continue;
        driver->dimension=dim;
        driver->diameter=value;
        driver->owner=sketch.id;
        found++;
    }
cleanup:
    ProArrayFree((ProArray *)&entities);
    ProArrayFree((ProArray *)&section_dims);
    ProArrayFree((ProArray *)&solid_dims);
    if(sketch_tree)ProFeatureElemtreeFree(&sketch,sketch_tree);
    if(tree)ProFeatureElemtreeFree(feature,tree);
    return found==1;
}

static int markup_visible_opening(MarkupProjection *map,const ProPoint3d center){
    ProRay ray;
    for(int i=0;i<3;i++){
        ray.start_point[i]=center[i]+map->span*map->toward_viewer[i];
        ray.dir_vector[i]=-map->toward_viewer[i];
    }
    QualityRay hits;
    if(qa_ray(map->model,&ray,1e-6,&hits))return 0;
    return !hits.count||hits.crossing[0].depth>=map->span-1e-4;
}

static int markup_rim_contrast(const MarkupProjection *map,const Pro2dPnt center,const Pro2dPnt rim){
    int distance=0;
    int x[2],y[2];
    const double offsets[]={0.75,1.22};
    for(int i=0;i<2;i++){
        x[i]=(int)lround(center[0]+offsets[i]*(rim[0]-center[0]));
        y[i]=(int)lround(center[1]+offsets[i]*(rim[1]-center[1]));
        if(x[i]<0||x[i]>=map->image->width||y[i]<0||y[i]>=map->image->height)return 0;
    }
    const BYTE *inside=map->pixels+y[0]*map->stride+x[0]*4;
    const BYTE *outside=map->pixels+y[1]*map->stride+x[1]*4;
    for(int i=0;i<3;i++)distance+=abs((int)inside[i]-outside[i]);
    return distance>=35;
}

static ProError markup_surface(ProGeomitem *item,ProError status,ProAppData data){
    MarkupProjection *map=data;
    if(status)return status;
    ProFeature feature;
    ProFeattype type;
    ProFeatStatus state;
    ProPatternStatus pattern;
    ProSurface surface;
    ProSrftype shape;
    if(ProGeomitemFeatureGet(item,&feature)||ProFeatureTypeGet(&feature,&type)||
       (type!=PRO_FEAT_HOLE&&type!=PRO_FEAT_CUT&&type!=PRO_FEAT_CUT_SMT)||
       ProFeatureStatusGet(&feature,&state)||state!=PRO_FEAT_ACTIVE||
       ProFeaturePatternStatusGet(&feature,&pattern)||
       ProGeomitemToSurface(item,&surface)||ProSurfaceTypeGet(surface,&shape)||shape!=PRO_SRF_CYL)
        return PRO_TK_NO_ERROR;
    ProGeomitemdata *raw=NULL;
    if(ProSurfaceDataGet(surface,&raw))return PRO_TK_NO_ERROR;
    ProSurfacedata *geometry=raw->data.p_surface_data;
    ProCylinderdata cylinder=geometry->srf_shape.cylinder;
    ProUvParam uv={(geometry->uv_min[0]+geometry->uv_max[0])*0.5,(geometry->uv_min[1]+geometry->uv_max[1])*0.5};
    ProVector p3,normal,deriv1[2],deriv2[3],radial;
    int inner=!ProSurfaceXyzdataEval(surface,uv,p3,deriv1,deriv2,normal);
    if(inner){
        for(int i=0;i<3;i++)radial[i]=p3[i]-cylinder.origin[i]-uv[1]*cylinder.e3[i];
        inner=ad_dot(radial,normal)<0;
    }
    if(!inner){ProGeomitemdataFree(&raw);return PRO_TK_NO_ERROR;}
    double facing=ad_dot(cylinder.e3,map->toward_viewer);
    double end=facing>=0?geometry->uv_max[1]:geometry->uv_min[1];
    ProPoint3d center;
    for(int i=0;i<3;i++)center[i]=cylinder.origin[i]+end*cylinder.e3[i];
    ProGeomitemdataFree(&raw);
    if(fabs(facing)<0.45||!markup_visible_opening(map,center))return PRO_TK_NO_ERROR;
    MarkupDriver driver={.diameter=2*cylinder.radius,.owner=feature.id};
    int editable=pattern==PRO_PATTERN_NONE&&
        (type==PRO_FEAT_HOLE?(!ProFeatureDimensionVisit(&feature,markup_driver,NULL,&driver)&&driver.count==1):
         markup_cut_driver(&feature,&cylinder,&driver));
    Pro2dPnt middle,p;
    if(!markup_project(map,center,middle))return PRO_TK_NO_ERROR;
    MarkupHole hole={feature.id,editable?driver.dimension.id:-1,driver.owner,driver.diameter,middle[0],middle[1],0,0,2*cylinder.radius};
    int visible_samples=0;
    for(int i=0;i<8;i++){
        const double angle=i*3.141592653589793/4;
        ProPoint3d rim;
        for(int j=0;j<3;j++)rim[j]=center[j]+cylinder.radius*(cos(angle)*cylinder.e1[j]+sin(angle)*cylinder.e2[j]);
        if(!markup_project(map,rim,p))return PRO_TK_NO_ERROR;
        visible_samples+=markup_rim_contrast(map,middle,p);
        hole.rx=fmax(hole.rx,fabs(p[0]-hole.x));
        hole.ry=fmax(hole.ry,fabs(p[1]-hole.y));
    }
    MarkupImage *image=map->image;
    if(visible_samples<3||hole.rx<2||hole.ry<2||hole.x-hole.rx<0||hole.x+hole.rx>=image->width||
       hole.y-hole.ry<0||hole.y+hole.ry>=image->height)return PRO_TK_NO_ERROR;
    for(int i=0;i<image->count;i++){
        const MarkupHole *old=&image->holes[i];
        if(old->feature==hole.feature&&old->dimension==hole.dimension&&hypot(old->x-hole.x,old->y-hole.y)<1)return PRO_TK_NO_ERROR;
    }
    if(image->count==MARKUP_MAX_HOLES)image->truncated=1;
    else image->holes[image->count++]=hole;
    return PRO_TK_NO_ERROR;
}

static ProError markup_visit_surface(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    MarkupProjection *map=data;
    ProGeomitem item;
    if(ProSurfaceToGeomitem((ProSolid)map->model,surface,&item))return PRO_TK_NO_ERROR;
    return markup_surface(&item,0,data);
}

#include "markup_opening.inc"

static ProError markup_check_capture(const MarkupCapture *capture){
    ProMdl model=NULL;
    ProPath directory;
    ProWVerstamp stamp=NULL;
    int window;
    if(!capture->stamp||ProMdlCurrentGet(&model)||model!=capture->model||
       ProWindowCurrentGet(&window)||window!=capture->window||
       ProDirectoryCurrentGet(directory)||_wcsicmp(directory,capture->directory)||
       ProMdlVerstampGet(model,&stamp))return PRO_TK_BAD_CONTEXT;
    int same=ProVerstampEqual(capture->stamp,stamp);
    ProVerstampFree(&stamp);
    return same?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static double markup_mm_scale(ProMdl model){
    ProUnitsystem units;
    ProUnititem native,mm;
    ProUnitConversion conversion;
    if(ProMdlPrincipalunitsystemGet(model,&units)||ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&native)||
       ProUnitInit(model,L"mm",&mm)||ProUnitConversionCalculate(&native,&mm,&conversion)||
       !isfinite(conversion.scale)||conversion.scale<=0||conversion.offset!=0)return 0;
    return conversion.scale;
}

static ProError markup_take(MarkupCapture *capture){
    ProMdlType type;
    ProError error=ProMdlCurrentGet(&capture->model);
    if(!error)error=ProMdlTypeGet(capture->model,&type);
    if(error||type!=PRO_MDL_PART)return PRO_TK_INVALID_TYPE;
    if(!storage_directory_on_drive(runtime_data,L'E'))
        return PRO_TK_CANT_WRITE;
    if((error=ProDirectoryCurrentGet(capture->directory))||
       (error=ProWindowCurrentGet(&capture->window))||
       (error=ProMdlVerstampGet(capture->model,&capture->stamp)))return error;
    MarkupImage *image=calloc(1,sizeof(*image));
    if(!image)return PRO_TK_OUT_OF_MEMORY;
    capture->image=image;
    image->mm_per_native=markup_mm_scale(capture->model);
    image->ticket=capture->ticket=++markup_serial;
    ProMdlName name;
    if(!ProMdlMdlnameGet(capture->model,name))wcsncpy_s(image->name,_countof(image->name),name,_TRUNCATE);
    MarkupProjection map={.image=image,.model=capture->model,.window=capture->window};
    if((error=ProViewMatrixGet(map.model,NULL,map.view))||
       (error=ProWindowCurrentMatrixGet(map.zoom))||
       (error=ProWindowPixelOutlineGet(map.window,map.viewport)))return error;
    double w=map.viewport[1][0]-map.viewport[0][0],h=map.viewport[1][1]-map.viewport[0][1];
    if(w<100||h<100)return PRO_TK_BAD_CONTEXT;
    Pro3dPnt bounds[2];
    if((error=ProSolidOutlineGet((ProSolid)map.model,bounds)))return error;
    map.span=4*hypot(hypot(bounds[1][0]-bounds[0][0],bounds[1][1]-bounds[0][1]),bounds[1][2]-bounds[0][2])+1;
    for(int i=0;i<3;i++)map.toward_viewer[i]=map.view[i][2];
    if(!ad_unit(map.toward_viewer))return PRO_TK_BAD_CONTEXT;
    ProPath path;
    if(swprintf_s(path,_countof(path),L"%ls\\markup-%lu-%lu.tif",runtime_data,GetCurrentProcessId(),image->ticket)<0)
        return PRO_TK_CANT_WRITE;
    const double scale=fmin(1,1400/fmax(w,h));
    error=ProRasterFileWrite(map.window,PRORASTERDEPTH_24,w*scale/100,h*scale/100,
        PRORASTERDPI_100,PRORASTERTYPE_TIFF,path);
    if(!error)image->bitmap=markup_read_raster(path,&image->width,&image->height);
    DeleteFileW(path);
    if(error||!image->bitmap)return error?error:PRO_TK_CANT_OPEN;
    DIBSECTION bitmap;
    if(GetObjectW(image->bitmap,sizeof(bitmap),&bitmap)!=sizeof(bitmap)||!bitmap.dsBm.bmBits)
        return PRO_TK_CANT_OPEN;
    map.pixels=bitmap.dsBm.bmBits;
    map.stride=bitmap.dsBm.bmWidthBytes;
    error=ProSolidSurfaceVisit((ProSolid)map.model,markup_visit_surface,NULL,&map);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    error=ProSolidFeatVisit((ProSolid)map.model,markup_opening_feature,NULL,&map);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    error=markup_check_capture(capture);
    if(error)return error;
    RECT view;
    int width,height;
    HBITMAP full=markup_full_window(markup_creo_window(),&map,&view,&width,&height);
    if(!full)return PRO_TK_CANT_OPEN;
    double sx=(double)(view.right-view.left)/image->width;
    double sy=(double)(view.bottom-view.top)/image->height;
    for(int i=0;i<image->count;i++){
        MarkupHole *hole=&image->holes[i];
        hole->x=view.left+hole->x*sx;
        hole->y=view.top+hole->y*sy;
        hole->rx*=sx;
        hole->ry*=sy;
    }
    for(int i=0;i<image->opening_count;i++){
        MarkupOpening *opening=&image->openings[i];
        opening->x=view.left+opening->x*sx;
        opening->y=view.top+opening->y*sy;
        for(int j=0;j<4;j++){
            opening->corners[j][0]=view.left+opening->corners[j][0]*sx;
            opening->corners[j][1]=view.top+opening->corners[j][1]*sy;
        }
    }
    DeleteObject(image->bitmap);
    image->bitmap=full;
    image->width=width;
    image->height=height;
    return PRO_TK_NO_ERROR;
}
