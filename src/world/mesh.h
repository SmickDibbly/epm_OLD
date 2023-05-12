#ifndef MESH_H
#define MESH_H

#include "zigil/zigil.h"
#include "src/misc/epm_includes.h"
#include "src/world/geometry.h"

#define MAX_MESH_VERTICES 4096
#define MAX_MESH_EDGES 4096
#define MAX_MESH_FACES 4096
typedef struct Mesh {
    zgl_Color wirecolor;

    WitPoint origin;
    
    fix32_t scale;
    
    ang18_t ang_h;
    ang18_t ang_v;
    WitRect AABB;
    
    size_t num_vertices;
    WitPoint *vertices;
    WitPoint *vertex_normals;

    size_t num_edges;
    Edge *edges;

    size_t num_faces;
    Face *faces;
} Mesh;

typedef struct EdgeMesh {
    zgl_Color wirecolor;
    
    size_t num_vertices;
    WitPoint *vertices;
    
    size_t num_edges;
    Edge *edges;
} EdgeMesh;

#define FaceSet_from_Mesh(MESH)                 \
    ((FaceSet){                                 \
        .num_vertices = (MESH).num_vertices,    \
        .vertices = (MESH).vertices,            \
        .num_faces = (MESH).num_faces,          \
        .faces = (MESH).faces                   \
    })

#define EdgeSet_from_Mesh(MESH)                 \
    ((EdgeSet){                                 \
        .num_vertices = (MESH).num_vertices,    \
        .vertices = (MESH).vertices,            \
        .num_edges = (MESH).num_edges,          \
        .edges = (MESH).edges                   \
    })


extern void load_Mesh_dibj_0(Mesh *mesh, char *filename);
extern epm_Result load_Mesh_dibj_1(Mesh *mesh, char *filename);
extern void load_Mesh_obj(Mesh *mesh, char *filename);
extern void write_Mesh_dibj_1(Mesh const *mesh, char const *filename);

extern void compute_face_normals(WitPoint *vertices, size_t num_faces, Face *faces);
extern void compute_face_brightnesses(size_t num_faces, Face *faces);

extern int count_edges_from_faces(size_t num_faces, Face *faces);
extern epm_Result compute_edges_from_faces
(size_t num_edges, Edge *edges, size_t num_faces, Face *faces);

#endif /* MESH_H */
