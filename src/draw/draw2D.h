#ifndef DRAW2D_H
#define DRAW2D_H

#include "src/misc/epm_includes.h"
#include "src/draw/draw.h"
#include "src/draw/window/window.h"

typedef enum Plane {
    PLN_TOP = 0,
    PLN_SIDE = 1,
    PLN_FRONT = 2,
} Plane;

typedef struct View2D {
    uint32_t i_x, i_y;
    Fix64Point_2D center;
    Fix64Rect_2D worldbox;
    zgl_mPixelRect screenbox;
    uint32_t i_zoom;
    fix64_t gridres_world;
    uint8_t gridres_world_shift;
    uint32_t num_vert_lines;
    uint32_t num_hori_lines;
} View2D;

extern void set_zoom_level(View2D *v2d);
extern void zoom_level_up(Plane tsf);
extern void zoom_level_down(Plane tsf);
extern void draw_View2D(Window *win, zgl_PixelArray *scr_p);
extern void scroll(Window *win, int dx, int dy, Plane tsf);

#endif /* DRAW2D_H */
