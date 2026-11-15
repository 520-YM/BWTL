#define MARKUP_OPEN (WM_APP+60)
#define MARKUP_APPLY (WM_APP+61)
#define MARKUP_READY (WM_APP+62)
#define MARKUP_RESULT (WM_APP+63)
#define MARKUP_PROGRESS (WM_APP+64)
#define MARKUP_CANCEL (WM_APP+65)
#define MARKUP_MAX_HOLES 256
#define MARKUP_MAX_OPENINGS 64
#define MARKUP_MAX_MARKS 8
#define MARKUP_MAX_EDITS (2*MARKUP_MAX_MARKS)

typedef struct {
    int feature,dimension,dimension_owner;
    double old_value,x,y,rx,ry;
    double diameter;
} MarkupHole;

typedef struct {
    int feature,owner,dimension[2];
    double size[2],x,y;
    double corners[4][2];
} MarkupOpening;

typedef struct {
    HBITMAP bitmap;
    int width,height,count,truncated,opening_count,openings_truncated;
    unsigned long ticket;
    wchar_t name[128];
    double mm_per_native;
    MarkupHole holes[MARKUP_MAX_HOLES];
    MarkupOpening openings[MARKUP_MAX_OPENINGS];
} MarkupImage;

typedef enum { MARKUP_CIRCLE, MARKUP_ARROW, MARKUP_BOX } MarkupTool;
typedef struct {
    MarkupTool tool;
    double x1,y1,x2,y2;
    int valid;
} MarkupMark;

typedef struct {
    unsigned long ticket;
    MarkupMark marks[MARKUP_MAX_MARKS];
    int count;
    wchar_t text[256];
} MarkupRequest;

typedef struct {
    unsigned long ticket;
    int success,retake;
    wchar_t text[2000];
} MarkupResult;

static int markup_area_contains(const MarkupMark *mark,double px,double py){
    double x=(mark->x1+mark->x2)*0.5, y=(mark->y1+mark->y2)*0.5;
    double rx=fabs(mark->x1-mark->x2)*0.5, ry=fabs(mark->y1-mark->y2)*0.5;
    if(mark->tool==MARKUP_CIRCLE)
        return rx>=3&&ry>=3&&hypot((x-px)/rx,(y-py)/ry)<=1;
    return fabs(x-px)<=rx&&fabs(y-py)<=ry;
}

static int markup_match(const MarkupImage *image,const MarkupMark *mark){
    if(!mark->valid||image->truncated)return -1;
    int found=-1;
    for(int i=0;i<image->count;i++){
        const MarkupHole *hole=&image->holes[i];
        int hit;
        if(mark->tool==MARKUP_ARROW)
            hit=hypot(mark->x2-hole->x,mark->y2-hole->y)<=fmax(7,fmax(hole->rx,hole->ry)+3);
        else hit=markup_area_contains(mark,hole->x,hole->y);
        if(!hit)continue;
        if(found>=0)return -2;
        found=i;
    }
    return found;
}

static int markup_match_opening(const MarkupImage *image,const MarkupMark *mark){
    if(!mark->valid||image->openings_truncated)return -1;
    int found=-1;
    for(int i=0;i<image->opening_count;i++){
        const MarkupOpening *opening=&image->openings[i];
        int hit=markup_area_contains(mark,opening->x,opening->y);
        if(mark->tool==MARKUP_ARROW){
            int positive=0,negative=0;
            for(int j=0;j<4;j++){
                const double *a=opening->corners[j],*b=opening->corners[(j+1)%4];
                double dx=b[0]-a[0],dy=b[1]-a[1];
                double cross=(dx*(mark->y2-a[1])-dy*(mark->x2-a[0]))/hypot(dx,dy);
                positive|=cross>5;
                negative|=cross<-5;
            }
            hit=!(positive&&negative);
        }
        if(hit){
            if(found>=0)return -2;
            found=i;
        }
    }
    return found;
}

static void markup_image_free(MarkupImage *image){
    if(!image)return;
    if(image->bitmap)DeleteObject(image->bitmap);
    free(image);
}

#include "markup_diameter.inc"
