#ifndef DRAW_TRIANGLE_H
#define DRAW_TRIANGLE_H

#include "src/draw/draw.h"
#include "src/draw/window/window.h"

// weird includes
#include "src/world/world.h"

typedef struct WorldTriangle {
    WitPoint vertices[3];
} WorldTriangle;

typedef struct ScreenTriangle {
    DepthPixel *pixels[3];
    fix64_t *zinv[3];
    zgl_PixelArray *texture;
    zgl_mPixel *texels[3];
    uint8_t vbri[3];
    fix32_t brightness;
} ScreenTriangle;

typedef struct TrianglePtr {
    WitPoint *vertices[3];
} TriangleI;

typedef struct Triangle2D {
    zgl_mPixel v0, v1, v2;
} Triangle2D;

extern void reset_depth_buffer(void);

typedef void (*draw_Triangle_fnc)(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri);

extern void draw_Triangle
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri);
extern void draw_Triangle_NoLight
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri);
extern void draw_Triangle_Orth
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri);
extern void draw_Triangle_BoundingBox
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri);

extern void draw_Triangle_NoLight_Orth
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri);
extern void draw_Triangle_NoLight_BoundingBox
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri);
extern void draw_Triangle_Orth_BoundingBox
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri);
extern void draw_Triangle_NoLight_Orth_BoundingBox
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri);

void draw_solid_triangle
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri, zgl_Color color);

extern bool g_all_pixels_drawn;
extern void reset_rasterizer(void);




#define RASTERFLAG_DEFAULT   0x00
#define RASTERFLAG_ORTHO     0x01
#define RASTERFLAG_NO_LIGHT  0x02
extern uint32_t g_rasterflags;

#define NUM_DRAW_TRIANGLE_FUNCTIONS 4
extern draw_Triangle_fnc draw_Tri[4];

#define draw_Tri(P_SCR, P_WIN, P_TRI) draw_Tri[g_rasterflags]((P_SCR), (P_WIN), (P_TRI))

#endif /* DRAW_TRIANGLE_H */
