#ifndef CLIP_H
#define CLIP_H

/* Clipping algorithms optimized for specific viewing frustums (or intended to
   be optimized eventually if not already). */

#include "src/misc/epm_includes.h"
#include "src/draw/draw3D.h"
#include "zigil/zigil.h"

extern void TriClip2D
(Fix64Point_2D const *const v0,
 Fix64Point_2D const *const v1,
 Fix64Point_2D const *const v2,
 zgl_Pixit xmin, zgl_Pixit xmax, zgl_Pixit ymin, zgl_Pixit ymax,
 Fix64Point_2D *out_poly, size_t *out_count, bool *out_new);

extern void TriClip3D
(Fix64Point const *const v0,
 Fix64Point const *const v1,
 Fix64Point const *const v2,
 Frustum *fr,
 Fix64Point *out_poly, size_t *out_count, bool *out_new);

extern void TriClip3D_Orth
(Fix64Point const *const v0,
 Fix64Point const *const v1,
 Fix64Point const *const v2,
 Frustum *fr,
 Fix64Point *out_poly, size_t *out_count, bool *out_new);

extern bool SegClip3D
(fix64_t x0, fix64_t y0, fix64_t z0,
 fix64_t x1, fix64_t y1, fix64_t z1,
 Frustum *fr,
 Fix64Point *out_c0, Fix64Point *out_c1);

extern bool SegClip3D_Orth
(fix64_t x0, fix64_t y0, fix64_t z0,
 fix64_t x1, fix64_t y1, fix64_t z1,
 Frustum *fr,
 Fix64Point *out_c0, Fix64Point *out_c1);

#endif /* CLIP_H */
