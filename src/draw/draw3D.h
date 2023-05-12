#ifndef DRAW3D_H
#define DRAW3D_H

#include "src/misc/epm_includes.h"
#include "zigil/zigil.h"
#include "src/draw/draw_triangle.h"
//#include "src/draw/window/window.h"

typedef struct TransformedVertex {
    Fix64Point eye; // vertex in viewspacae
    DepthPixel pixel; // 
    fix64_t zinv;
    uint8_t vis;
} TransformedVertex;

typedef struct TransformedFace {
    TransformedVertex tv[3];
    uint8_t vbri[3];
} TransformedFace;

typedef struct TransformedWireFace {
    DepthPixel pixel[3];
    uint8_t vis[3];
    zgl_Color color;
} TransformedWireFace;

typedef struct DrawSettings {
    int i_draw;
    int proj_type;
    bool lighting;
    bool wireframe;
    bool BSP_visualizer;
} DrawSettings;

extern DrawSettings g_settings;

typedef struct Frustum {
    int type;
    
    // world coordinates (WC)
    fix32_t W; // depends only on FOV and d
    fix32_t H;

    Fix32Point VRP; // view reference point
    Fix32Point VPN; // view-plane normal
    Fix32Point VUP; // view up vector
    fix32_t    VPD; // view-plane distance

    // View Volume Specification
    // viewing-reference coordinates (VRC)
    Fix32Point PRP; // projection reference point
    fix32_t u_min, u_max, v_min, v_max;
    fix32_t F, B; // front and back distance
    fix32_t D; // distance from projection point to projection plane

    // 3D Viewport Specification
    fix32_t xmin, xmax, ymin, ymax, zmin, zmax;

    // normalized projection coordinates (NPC)
} Frustum;

extern void select_vertex(Window *win, zgl_Pixel pixel);
extern void select_face(Window *win, zgl_Pixel pixel);
extern void select_one_face(Window *win, zgl_Pixel pixel);
extern void select_brush(Window *win, zgl_Pixel pixel);
extern void draw3D(Window *win, zgl_PixelArray *scr_p);
extern WitPoint apply_transform(WitPoint in);
extern void subdivide3D_and_draw_face(zgl_PixelArray *scr_p, Window *win, Face *face, TransformedVertex const *const vbuf);
extern void subdivide3D(Window const *const win, Fix64Point V0, Fix64Point V1, Fix64Point V2, size_t *out_num_subvs, Fix64Point *out_subvs, TransformedVertex *out_subvbuf, bool *out_new);
extern void draw_face
(zgl_PixelArray *scr_p, Window *win, Face *face, TransformedFace *tf);
extern void draw_wireframe_face
(zgl_PixelArray *scr_p, Window *win, TransformedWireFace *twf, zgl_Color color);

/* -------------------------------------------------------------------------- */

// Visibility codes of a point in space, with respect to a cuboid viewing
// frustum. Can be stored in uint8_t. A viscode of 0 means the point is inside
// the frustum.
#define VIS_NEAR   0x01
#define VIS_FAR    0x02
#define VIS_LEFT   0x04
#define VIS_RIGHT  0x08
#define VIS_ABOVE  0x10
#define VIS_BELOW  0x20

#define DEFAULT_HALF_FOV (ANG18_PI4)
#define DEFAULT_DISTANCE_FROM_CAMERA (fixify32(8))

/* A minimal set of parameters to completely determine a viewing frustum. */
typedef struct FrustumSettings {
    int type;
    ang18_t persp_half_fov;
    fix32_t ortho_W;
    
    fix32_t D, F, B;
} FrustumSettings;

extern FrustumSettings frset_persp;
extern FrustumSettings frset_ortho;

/* -------------------------------------------------------------------------- */
// Render Settings

// outdated 2023-05-10
//extern int i_draw; // index to triangle rasterization function table
extern void epm_SetTriangleDrawFunction(int i_draw);
extern void epm_NextTriangleDrawFunction(void);
extern void epm_PrevTriangleDrawFunction(void);

extern bool no_bsp;

#define PROJ_PERSPECTIVE 0
#define PROJ_ORTHOGRAPHIC 1

extern int const *const p_proj_type;
extern void epm_SetProjectionType(int type);
extern void epm_ToggleProjectionType(void);

extern bool const *const p_lighting;
extern void epm_SetLighting(bool state);
extern void epm_ToggleLighting(void);

extern bool const *const p_wireframe;
extern void epm_SetWireframe(bool state);
extern void epm_ToggleWireframe(void);

#endif /* DRAW3D_H */
