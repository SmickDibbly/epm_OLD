#ifndef DRAW3D_BSP_H
#define DRAW3D_BSP_H

#include "src/misc/epm_includes.h"
#include "src/draw/draw.h"
#include "src/draw/draw3D.h"

#define MAX_SECTS 16
extern WitPoint sect_ring[MAX_SECTS];

extern BSPFace *BSPFace_below
(Window *win,
 zgl_Pixel pixel,
 Frustum *fr);

extern void draw_BSPTree
(BSPTree *tree,
 zgl_PixelArray *scr_p,
 Window *win,
 WitPoint campos,
 TransformedVertex *vbuf);

extern void draw_face_BSP_visualizer
(zgl_PixelArray *scr_p,
 Window *win,
 Face *face,
 TransformedFace *tf);

extern void draw_BSPNode_wireframe(zgl_PixelArray *scr_p, Window *win, BSPTree *tree, BSPNode *node, WitPoint campos, TransformedVertex *vbuf);

extern WitPoint *vertex_below(Window *win, zgl_Pixel pixel, Frustum *fr);

#endif /* DRAW3D_BSP_H */
