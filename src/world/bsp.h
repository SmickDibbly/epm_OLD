#ifndef BSP_H
#define BSP_H

#include "src/misc/epm_includes.h"
#include "src/world/geometry.h"
#include "src/world/mesh.h"

// TODO: Allow the geometry-designer to suggest certain BSP splitting planes.

// Extra data tacked onto to a struct Face.
typedef struct BSPFace {
    Face face;
    
    Face const *progenitor_face;
    size_t i_progenitor_face;

    uint16_t bspflags;
    uint16_t depth;
    Face *twin;
} BSPFace;

// A BSP node should only contain pointers and metadata. Store the actual BSP
// face data in an array.
typedef struct BSPNode BSPNode;
struct BSPNode {
    BSPNode *parent; // TODO: Will this *ever* be used?
    BSPNode *front;
    BSPNode *back;

    BSPFace *bspface;
    size_t i_bspface;

    WitPoint splitV; // point on splitting plane
    WitPoint splitN; // normal of splitting plane
};


/* Note: As of 2023-03-30, we always have num_nodes == num_bspfaces, since by
   construction each node corresponds to exactly one face, and conversely. In
   the future I will likely not have such a one-to-one correspondence between
   faces and nodes (maybe leaf nodes will correspond to certain convex subsets
   of faces), hence the two separate variables to count each number. */
typedef struct BSPTree {
    size_t num_leaves;
    size_t num_cuts;
    
    size_t max_node_depth;
    double avg_node_depth;
    double avg_leaf_depth;
    double balance;
    
    size_t num_nodes;
    BSPNode *nodes;
    
    size_t num_vertices;
    WitPoint *vertices;
    uint8_t *vertex_intensities;

    size_t num_edges;
    Edge edges[10000];
    
    size_t num_bspfaces;
    BSPFace *bspfaces;

    /* The "progenitor" face structure that the BSP Tree is based on is kept on
       hand. Much of the data about the subdivided BSP faces can be inherited
       from the progenitor, such as normal vector, ambient/directional
       brightness, and texture (but not u,v mapping or vertices) */
    size_t num_progenitor_vertices;
    WitPoint const *progenitor_vertices;
    uint8_t const *progenitor_vertex_intensities;
    
    size_t num_progenitor_faces;
    Face const *progenitor_faces; // pointer to originally-given face array.
} BSPTree;

extern epm_Result create_BSPTree
(BSPTree *tree, int bspsel,
 size_t num_vertices, WitPoint const *vertices,
 size_t num_faces, Face const *faces);

extern void destroy_BSPTree(BSPTree *p_tree);

extern void measure_BSPTree(BSPTree *p_tree);

#endif /* BSP_H */
