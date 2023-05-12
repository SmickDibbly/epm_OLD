#ifndef BRUSH_H
#define BRUSH_H

#include "src/misc/epm_includes.h"
#include "src/world/geometry.h"
#include "zigil/zigil.h"
#include "src/draw/draw.h"
#include "src/world/mesh.h"

/* -------------------------------------------------------------------------- */
// Brush faces

#define FACEFLAG_SELECTED 0x01
typedef struct BrushQuadFace {
    uint32_t flags;
    
    QuadFace quad;
    
    Face *subface0;
    Face *subface1;
} BrushQuadFace;

typedef struct BrushPoly {
    uint32_t brushflags;

    Poly poly;

    Poly3 *subpolys; // .poly.num_i_v - 1 subpolys
} BrushPoly;

typedef struct BrushPoly4 {
    uint32_t brushflags;

    Poly4 poly4;

    Poly3 *subpoly0, *subpoly1;
} BrushPoly4;

typedef struct BrushPoly3 {
    uint32_t brushflags;

    Poly3 poly3;

    Poly3 *subpoly0;
} BrushPoly3;

#define UV_SCALE_TO_FIT 0x01
typedef struct BrushQuadFace2 {
    size_t i_v0, i_v1, i_v2, i_v3;

    uint8_t flags;
    WitPoint normal;
    fix32_t brightness;
    size_t i_tex;
    
    uint8_t uv_flags; // def 0
    ang18_t uv_ang; // def 0
    Fix32Point_2D uv_scale; // def (1.0, 1.0)
    Fix32Point_2D uv_offset; // def (0,0)
} BrushQuadFace2;
// When processing a BrushQuadFace into two triangle Faces, if uv_flags is 0
// then the UV mapping data above will be applied to the default UV mapping as
// follows:
// 1) Rotate by uv_ang about the origin (u,v)=(0,0)
// 2) Scale by uv_scale.x in the u dimension
// 3) Scale by uv_scale.y in the v dimension
// 4) Translate by uv_offset
//
// If uv_flags & UV_SCALE_TO_FIT is set, 

// TODO: "composite face"


/* -------------------------------------------------------------------------- */
// Generic Brush Container


typedef enum BrushType {
    BT_CUBOID,
    BT_CYLINDER,

    NUM_BT
} BrushType;

#define BRUSHFLAG_SELECTED 0x01
typedef struct Brush {
    uint32_t flags;

    char *name;
    
    int BSP;
    int CSG;
    WitPoint POR;
    zgl_Color wirecolor;

    size_t num_vertices;
    WitPoint *vertices;

    size_t num_edges;
    Edge *edges;
    
    size_t num_quads;
    BrushQuadFace *quads;
    
    BrushType type;
    void *brush;
} Brush;

void init_Brush(Brush *nonNULL_brush, WitPoint POR, int CSG);




/* -------------------------------------------------------------------------- */
// Specific Brush Geometry

typedef struct ComplexBrush {
    Brush *brush;
    
    size_t num_vertices;
    WitPoint *vertices;

    size_t num_edges;
    Edge *edges;
    
    size_t num_quads;
    BrushQuadFace *quads;
} ComplexBrush;


#define NUM_VERTICES_CUBOID 8
#define NUM_EDGES_CUBOID    18
#define NUM_QUADS_CUBOID    6
#define NUM_FACES_CUBOID    12
typedef struct CuboidBrush {
    Brush *container;
    
    size_t num_vertices;
    WitPoint vertices[NUM_VERTICES_CUBOID];

    size_t num_edges;
    Edge edges[NUM_EDGES_CUBOID];
    
    size_t num_quads;
    BrushQuadFace quads[NUM_QUADS_CUBOID];
} CuboidBrush;

#define Cuboid_of(BRUSH) ((CuboidBrush *)((BRUSH)->brush))

extern void init_CuboidBrush(CuboidBrush *nonNULL_cuboid, ufix32_t dx, ufix32_t dy, ufix32_t dz);
extern Brush *create_CuboidBrush(WitPoint origin, int CSG, ufix32_t dx, ufix32_t dy, ufix32_t dz);
extern Brush *new_CuboidBrush(void);


/*
#define NUM_VERTICES_CUBE 8
#define NUM_EDGES_CUBE    18
#define NUM_QUADS_CUBE    6
#define NUM_FACES_CUBE    12
typedef struct BoxBrushQuad {
    char *name;
    
    zgl_Color wirecolor;
    int CSG;
    
    WitPoint origin;
    ufix32_t dx, dy, dz;
    WitPoint point_of_reference;

    Mesh *mesh;

    bool TEMP_selected;
    
    size_t num_vertices;
    WitPoint vertices[NUM_VERTICES_CUBE];

    size_t num_edges;
    Edge edges[NUM_EDGES_CUBE];
    
    size_t num_quads;
    BrushQuadFace quads[NUM_QUADS_CUBE];
} BoxBrushQuad;
*/
/*
typedef struct BoxBrushQuad2 {
    zgl_Color wirecolor;
    int CSG;
    
    WitPoint origin;
    ufix32_t dx, dy, dz;

    Mesh *mesh;
    
    size_t num_vertices;
    WitPoint vertices[NUM_VERTICES_CUBE];

    size_t num_quads;
    BrushQuadFace2 quads[NUM_QUADS_CUBE];
} BoxBrushQuad2;

extern BoxBrushQuad *create_BoxBrushQuad(BoxBrushQuad *brush, WitPoint origin, ufix32_t dx, ufix32_t dy, ufix32_t dz, int CSG);

extern BoxBrushQuad2 *create_BoxBrushQuad2(BoxBrushQuad2 *brush, WitPoint origin, ufix32_t dx, ufix32_t dy, ufix32_t dz, int CSG);

extern void initialize_BoxBrushQuad(BoxBrushQuad *brush, int CSG);
*/

typedef struct CylinderBrush {
    zgl_Color wirecolor;
    int CSG;

    WitPoint origin;
    ufix32_t radius;
    
    size_t num_sides;

    size_t num_vertices;
    WitPoint *vertices;
    
    size_t num_edges;
    Edge *edges;

    size_t num_faces;
    Face *faces;
} CylinderBrush;



/* -------------------------------------------------------------------------- */
// The Brush Frame

extern Brush *frame;

extern void set_frame(BrushType type, /*parameters*/ Wit dx, Wit dy, Wit dz);


/* -------------------------------------------------------------------------- */

typedef struct BrushNode BrushNode;
struct BrushNode {
    BrushNode *next;
    BrushNode *prev;
    
    Brush *brush;
};
typedef struct BrushGeometry { //geometry as given by a level designer.
    BrushNode *head;
    BrushNode *tail;
} BrushGeometry;

extern epm_Result reset_BrushGeometry(void);
extern epm_Result epm_TriangulateBrushGeometry(void);

#endif /* BRUSH_H */
