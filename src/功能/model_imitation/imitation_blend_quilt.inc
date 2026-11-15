/* Creo 10 can raise an access violation in ProSurfaceAreaEval on a
   boundary-blend quilt just reopened from disk. Derive the trimmed area
   from the native tessellation, and still perform the boundary/seam checks. */
static int imitation_blend_face(ProMdl model,ProSurface surface)
{
    ProGeomitem item;
    ProFeature feature;
    ProFeattype type;
    ProLine subtype=L"";
    return !ProSurfaceToGeomitem((ProSolid)model,surface,&item)&&
        !ProGeomitemFeatureGet(&item,&feature)&&
        !ProFeatureTypeGet(&feature,&type)&&type==PRO_FEAT_DATUM_SURF&&
        !ProFeatureSubtypeGet(&feature,subtype)&&
        (!_wcsicmp(subtype,L"Blend")||!wcscmp(subtype,L"混合")||
         !_wcsicmp(subtype,L"Boundary Blend")||!wcscmp(subtype,L"边界混合"));
}

static ProError imitation_blend_face_area(ProSurface surface,double mm,double *area)
{
    ProSurfaceTessellationInput input=NULL;
    ProTessellation tessellation=NULL;
    Pro3dPnt *vertices=NULL;
    ProTriangle *triangles=NULL;
    int points=0,facets=0;
    *area=0;
    ProError error=ProSurfacetessellationinputAlloc(&input);
    if(!error)error=ProSurfacetessellationinputChordheightSet(input,0.0001/mm);
    if(!error)error=ProSurfaceTessellationGet(surface,input,&tessellation);
    if(!error)error=ProTessellationVerticesGet(tessellation,&vertices);
    if(!error)error=ProArraySizeGet((ProArray)vertices,&points);
    if(!error)error=ProTessellationFacetsGet(tessellation,&triangles);
    if(!error)error=ProArraySizeGet((ProArray)triangles,&facets);
    if(!error&&(!facets||points<3))error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<facets;i++){
        const int *index=triangles[i];
        if(index[0]<0||index[1]<0||index[2]<0||index[0]>=points||index[1]>=points||index[2]>=points){
            error=PRO_TK_INCOMPLETE;
            break;
        }
        ProVector u,v,cross;
        ad_sub(vertices[index[1]],vertices[index[0]],u);
        ad_sub(vertices[index[2]],vertices[index[0]],v);
        ad_cross(u,v,cross);
        *area+=sqrt(ad_dot(cross,cross))/2;
    }
    if(vertices)ProArrayFree((ProArray *)&vertices);
    if(triangles)ProArrayFree((ProArray *)&triangles);
    if(tessellation)ProTessellationFree(tessellation);
    if(input)ProSurfacetessellationinputFree(input);
    return error?error:*area>0&&isfinite(*area)?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

