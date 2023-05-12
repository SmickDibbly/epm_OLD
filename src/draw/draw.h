#ifndef DRAW_H
#define DRAW_H

#include "zigil/zigil.h"

extern zgl_Pixit scr_w;
extern zgl_Pixit scr_h;

typedef struct DepthPixel {
    Fix64Point_2D XY;
    fix64_t z;
} DepthPixel;

#endif /* DRAW_H */
