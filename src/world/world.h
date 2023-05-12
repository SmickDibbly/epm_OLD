#ifndef WORLD_H
#define WORLD_H

#include "src/world/brush.h"
#include "src/misc/epm_includes.h"
#include "src/world/geometry.h"
#include "src/entity/editor_camera.h"
#include "src/entity/player.h"
#include "src/world/mesh.h"
#include "src/world/bsp.h"
#include "src/entity/entity.h"

//                        -2^31
#define WORLD_MIN   -2147483648
//                     2^31 - 1
#define WORLD_MAX   +2147483647
//                         2^32
#define WORLD_SIZE  +4294967296

typedef struct Transform {
    Wit x, y, z;
    WitPoint dir;
    fix32_t vcos, vsin, hcos, hsin;
} Transform;

#define BSP_DEFAULT 0
#define BSP_NON_CUTTER 1
#define BSP_NON_CUTTEE 2

#define CSG_UNKNOWN 0
#define CSG_SUBTRACTIVE 1
#define CSG_ADDITIVE 2
#define CSG_SPECIAL 3

typedef struct StaticGeometry {
    size_t num_vertices;
    WitPoint vertices[MAX_STATIC_VERTICES];
    uint8_t vertex_intensities[MAX_STATIC_VERTICES];
    
    size_t num_edges;
    Edge edges[MAX_STATIC_SEGS];

    size_t num_faces;
    Face faces[MAX_STATIC_FACES];
    
    Brush *progenitor_brush[MAX_STATIC_FACES];
    
} StaticGeometry;
epm_Result reset_StaticGeometry(void);

typedef struct epm_World {
    bool loaded;
    BrushGeometry *brushgeo;
    StaticGeometry *staticgeo;
    BSPTree *bsp;
    epm_EntityNode entity_head;
} epm_World;

extern epm_Result read_world(StaticGeometry *geo, char *filename);
extern void write_world(StaticGeometry const *geo, char const *filename);

extern epm_Result epm_ReadWorldFile(epm_World *world, char const *filename);
extern epm_Result epm_WriteWorldFile(epm_World *world, char const *filename);

extern epm_Result epm_LoadWorld(char *worldname);
extern epm_Result epm_UnloadWorld(void);

extern Transform tf;

extern WitPoint directional_light_vector;
extern fix32_t directional_light_intensity;
extern fix32_t ambient_light_intensity;

/*
typedef struct MeshNode MeshNode;
struct MeshNode {
    MeshNode *next;
    Mesh *mesh;
};
extern MeshNode root_mesh;
*/

//extern Fix32Point grid3D[16];
extern EdgeSet view3D_bigbox;
extern EdgeSet view3D_grid;


extern EditorCamera cam;
extern Player player;

extern BrushGeometry brushgeo;
extern StaticGeometry staticgeo;
extern BSPTree bsp;

extern epm_World world;

#endif /* WORLD_H */
