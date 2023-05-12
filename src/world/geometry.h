#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "src/misc/epm_includes.h"
#include "src/draw/draw.h"
// The "wit", derived from "World unIT", is the basic unit of distance, much like
// the "tic" is the basic unit of time.

#define MAX_STATIC_VERTICES (4096)
#define MAX_STATIC_SEGS     (4096)
#define MAX_STATIC_FACES    (4096)
#define MAX_STATIC_TEXTURES (64)

typedef     fix32_t Wit;
typedef  Fix32Point WitPoint;
typedef    Fix32Seg WitSeg;
typedef Fix32Sphere WitSphere;
typedef   Fix32Rect WitRect;
typedef   Fix32Line WitLine;
typedef  Fix32Plane WitPlane;

typedef fix64_t Matrix4[4][4];
typedef fix64_t Vector4[4];

typedef struct Triangle {
    WitPoint v0, v1, v2;
} Triangle;

extern bool is_anticlockwise(struct Fix32Polygon_2D poly);

extern bool clip_seg_to_polygon
(Fix32Seg seg,
 Fix32Polygon_2D poly,
 Fix32Seg *clipped_seg);

extern bool clip_seg_to_rect
(Fix32Seg line,
 Fix32Rect rect,
 Fix32Seg *clipped_line);

extern bool cohen_sutherland
(fix32_t x0, fix32_t y0,
 fix32_t x1, fix32_t y1,
 fix32_t *cx0, fix32_t *cy0,
 fix32_t *cx1, fix32_t *cy1,
 fix32_t xmin, fix32_t xmax,
 fix32_t ymin, fix32_t ymax);

extern ufix32_t norm_Euclidean(Fix32Point u);
extern ufix32_t norm_L1(Fix32Point u);
extern ufix32_t norm_approx13(Fix32Point u);
extern ufix32_t norm_approx9(Fix32Point u);
extern ufix32_t norm_approx8(Fix32Point u);

extern ufix32_t norm2D_Euclidean(Fix32Point_2D u);
extern ufix32_t norm2D_L1(Fix32Point_2D u);
extern ufix32_t norm2D_Doom(Fix32Point_2D u);
extern ufix32_t norm2D_flipcode(Fix32Point_2D u);
extern ufix32_t norm2D_flipcode2(Fix32Point_2D u);

extern Fix32Point midpoint(Fix32Point pt0, Fix32Point pt1);


extern fix64_t dot(Fix32Point u, Fix32Point v);
extern fix32_t signdist(Fix32Point pt, Fix32Plane plane);

extern fix32_t dot2D(Fix32Point_2D u, Fix32Point_2D v);
extern fix32_t perp2D(Fix32Point_2D u, Fix32Point_2D v);
extern fix32_t signdist2D(Fix32Point_2D pt, Fix32Line_2D line);

extern Fix32Point_2D closest_point_2D(Fix32Point_2D point, Fix32Seg_2D seg);
extern bool plane_normal(Fix32Point *out_N, Fix32Point *P, Fix32Point *Q, Fix32Point *R);
extern fix64_t triangle_area_3D(Fix64Point P, Fix64Point Q, Fix64Point R);

extern Fix32Point_2D rotate_Fix32Point_2D(Fix32Point_2D pt, ang18_t ang);

extern bool point_in_tri(WitPoint pt, WitPoint V0, WitPoint V1, WitPoint V2);

#define SIDE_FRONT 0x01
#define SIDE_MID   0x02
#define SIDE_BACK  0x04
extern uint8_t sideof_tri(WitPoint P, Triangle tri);
extern uint8_t sideof_plane(WitPoint V, WitPoint planeV, WitPoint planeN);
/*
#if defined FIX32_GEOMETRY_AS_MACRO
# if defined FIX32_GEOMETRY_VIA_FIX64
#  define dot(u, v) ((fix32_t)(((((fix64_t)(u).x) * ((fix64_t)(v).x)) + (((fix64_t)(u).y) * ((fix64_t)(v).y)))>>FIX32_BITS))
#  define perp(u, v) ((fix32_t)(((((fix64_t)(u).x) * ((fix64_t)(v).y)) - (((fix64_t)(u).y) * ((fix64_t)(v).x)))>>FIX32_BITS))
# else // FIX32_GEOMETRY_VIA_FIX64 UNDEFINED
#  define dot(u, v)  (FIX32_MUL((u).x, (v).x) + FIX32_MUL((u).y, (v).y))
#  define perp(u, v) (FIX32_MUL((u).x, (v).y) - FIX32_MUL((u).y, (v).x))
# endif // FIX32_GEOMETRY_VIA_FIX64
#else // FIX32_GEOMETRY_AS_MACRO UNDEFINED
# if defined FIX32_GEOMETRY_VIA_FIX64
#  define dot(u, v)  _dot_FIX32_via_FIX64((u), (v))
#  define perp(u, v) _perp_FIX32_via_FIX64((u), (v))
# else // FIX32_GEOMETRY_VIA_FIX64 UNDEFINED
#  define  dot(u, v)  dot((u), (v))
#  define perp(u, v) perp((u), (v))
# endif
#endif
*/



/*
extern int normalize
(Fix32Point vec,
 Fix32Point *normalized);
extern int normalize_secret_float
(Fix32Point vec,
 Fix32Point *normalized);
*/

/*
extern int set_length
(Fix32Point vec,
 fix32_t length,
 Fix32Point *normalized);
extern int set_length_secret_flaot
(Fix32Point vec,
 fix32_t length,
 Fix32Point *normalized);
*/

/*
extern fix32_t x_intercept
(Fix32Seg seg);

extern fix32_t y_intercept
(Fix32Seg seg);

extern fix32_t z_intercept
(Fix32Seg seg);
*/

/*
extern Fix32LineExact seg_to_line_exact
(Fix32Seg seg,
 Fix32LineExact *line);
*/

/*
extern Fix32Line seg_to_line
(Fix32Seg const *seg,
 Fix32Line *line);
*/

/*
extern Fix32Point midpoint
(Fix32Point pt0,
 Fix32Point pt1,
 Fix32Point *mid);
*/

/*
extern int32_t is_front
(Fix32Point pt,
 Fix32Seg seg);
*/

/*
extern Fix32Point compute_line_normal
(Fix32Seg seg,
 Fix32Point *normal);

extern fix32_t compute_line_constant
(Fix32Point origin,
 Fix32Point normal,
 fix32_t *constant);
*/

/*
extern Fix32Point closest_point
(Fix32Point point, 
 Fix32Seg seg);
*/


/* Intersection functions */

extern bool overlap_Rect_2D(const Fix32Rect_2D *r1, const Fix32Rect_2D *r2);
extern bool overlap_Rect(const Fix32Rect *r1, const Fix32Rect *r2);
extern void Mat4Mul(Vector4 *out_vec, Matrix4 mat, Vector4 vec);

#define FC_DEGEN 0x01

typedef struct Face Face;
typedef struct QuadFace QuadFace;

typedef struct Poly Poly;
typedef struct Poly4 Poly4;
typedef struct Poly3 Poly3;

typedef struct EditorPolyData {
    uint8_t edflags;
    void *brushface; // back pointer to close the Brush-Tri-BSP cycle    
} EditorPolyData;

struct Poly {
    uint8_t polyflags;
    
    size_t num_v;
    size_t *i_v;

    WitPoint normal;
    uint8_t brightness;
    
    size_t i_tex;
    zgl_mPixel *tx;

    EditorPolyData ed;
};

struct Poly3 {
    uint8_t polyflags;
    
    size_t i_v0, i_v1, i_v2;
    
    WitPoint normal;
    uint8_t brightness;
    
    size_t i_tex;
    zgl_mPixel tx0, tx1, tx2;

    EditorPolyData ed;
};

struct Poly4 {
    uint8_t polyflags;
    
    size_t i_v0, i_v1, i_v2, i_v3;
    
    WitPoint normal;
    uint8_t brightness;

    size_t i_tex;
    zgl_mPixel tx0, tx1, tx2, tx3;

    EditorPolyData ed;
};


#define FC_SELECTED 0x02
struct Face {
    size_t i_v0, i_v1, i_v2;
    
    uint8_t flags;
    WitPoint normal;
    uint8_t brightness;
    size_t i_tex;
    
    Fix32Point_2D tv0, tv1, tv2;

    void *brushface;
};



struct QuadFace {
    size_t i_v0, i_v1, i_v2, i_v3;
    
    uint8_t flags;
    WitPoint normal;
    uint8_t brightness;
    size_t i_tex;

    Fix32Point_2D tv0, tv1, tv2, tv3;
    void *brushface;
};

typedef struct Edge {
    size_t i_v0, i_v1;
} Edge;

typedef struct EdgeSet {
    size_t num_vertices;
    WitPoint *vertices;
    
    size_t num_edges;
    Edge *edges;

    zgl_Color wirecolor;
} EdgeSet;

typedef struct FaceSet {
    size_t num_vertices;
    WitPoint *vertices;
    
    size_t num_faces;
    Face *faces;

    zgl_Color wirecolor;
} FaceSet;

#endif /* GEOMETRY_H */
