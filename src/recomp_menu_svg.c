/* Same compact rasterizer family used by SDL_image's SVG loader.
 * Only our build-time embedded assets are passed to this parser. */
#include "recomp_menu_icons.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
SDL_Surface *RecompRasterizeSvg(const unsigned char *data,size_t size,int height) {
    if(!data || !size || size>1024*1024 || height<1 || height>2048)return NULL;
    char *text=SDL_malloc(size+1);if(!text)return NULL;
    SDL_memcpy(text,data,size);text[size]=0;
    NSVGimage *svg=nsvgParse(text,"px",96.0f);SDL_free(text);
    if(!svg || !svg->shapes || svg->width<=0 || svg->height<=0) {nsvgDelete(svg);return NULL;}
    float scale=height/svg->height;
    int width=(int)SDL_ceilf(svg->width*scale);
    if(width<1 || width>2048) {nsvgDelete(svg);return NULL;}
    NSVGrasterizer *raster=nsvgCreateRasterizer();
    SDL_Surface *surface=raster?SDL_CreateSurface(width,height,SDL_PIXELFORMAT_RGBA32):NULL;
    if(surface)nsvgRasterize(raster,svg,0,0,scale,surface->pixels,width,height,surface->pitch);
    nsvgDeleteRasterizer(raster);nsvgDelete(svg);return surface;
}
