#include "src/world/brush.h"
#include "src/world/world.h"
#include "src/draw/textures.h"
#include "src/draw/colors.h"

//#define VERBOSITY
#include "verbosity.h"

#undef LOG_LABEL
#define LOG_LABEL "BRUSH"

static CuboidBrush const TEMPLATE_CuboidBrush = {
    .container = NULL,
    
    .num_vertices = NUM_VERTICES_CUBOID,
    .vertices = {{{0}}}, // filled in on creation

    .num_edges = NUM_EDGES_CUBOID,
    .edges = {
        [0] = {0, 1},
        [1] = {1, 2},
        [2] = {2, 3},
        [3] = {3, 0},
        [4] = {7, 6},
        [5] = {6, 5},
        [6] = {5, 4},
        [7] = {4, 7},
        [8] = {0, 4},
        [9] = {5, 1},
        [10] = {2, 6},
        [11] = {7, 3},
    },
    
    .num_quads = NUM_QUADS_CUBOID,
    .quads = {
        [0] = {0, {0, 1, 2, 3,
                    .normal={{ 0x00000000,  0x00000000,  0x00010000}},
                    .tv0={          0, (256<<16)-1},
                    .tv1={(256<<16)-1, (256<<16)-1},
                    .tv2={(256<<16)-1,           0},
                    .tv3={          0,           0},
            }, NULL, NULL},
        [1] = {0, {7, 6, 5, 4,
                    .normal={{ 0x00000000,  0x00000000, -0x00010000}},
                    .tv0={          0, (256<<16)-1},
                    .tv1={(256<<16)-1, (256<<16)-1},
                    .tv2={(256<<16)-1,           0},
                    .tv3={          0,           0},
            }, NULL, NULL},
        [2] = {0, {1, 0, 4, 5,
                    .normal={{ 0x00000000,  0x00010000,  0x00000000}},
                    .tv0={          0, (256<<16)-1},
                    .tv1={(256<<16)-1, (256<<16)-1},
                    .tv2={(256<<16)-1,           0},
                    .tv3={          0,           0},
            }, NULL, NULL},
        [3] = {0, {3, 2, 6, 7,
                    .normal={{ 0x00000000, -0x00010000,  0x00000000}},
                    .tv0={          0, (256<<16)-1},
                    .tv1={(256<<16)-1, (256<<16)-1},
                    .tv2={(256<<16)-1,           0},
                    .tv3={          0,           0},
            }, NULL, NULL},
        [4] = {0, {0, 3, 7, 4,
                    .normal={{ 0x00010000,  0x00000000,  0x00000000}},
                    .tv0={          0, (256<<16)-1},
                    .tv1={(256<<16)-1, (256<<16)-1},
                    .tv2={(256<<16)-1,           0},
                    .tv3={          0,           0},
            }, NULL, NULL},
        [5] = {0, {2, 1, 5, 6,
                    .normal={{-0x00010000,  0x00000000,  0x00000000}},
                    .tv0={          0, (256<<16)-1},
                    .tv1={(256<<16)-1, (256<<16)-1},
                    .tv2={(256<<16)-1,           0},
                    .tv3={          0,           0},
            }, NULL, NULL},
    },
};


void init_CuboidBrush(CuboidBrush *cuboid, ufix32_t dx, ufix32_t dy, ufix32_t dz) {
    Brush *container = cuboid->container;    
    *cuboid = TEMPLATE_CuboidBrush;
    cuboid->container = container;

    container->num_vertices = cuboid->num_vertices;
    container->vertices = cuboid->vertices;
    container->num_edges = cuboid->num_edges;
    container->edges = cuboid->edges;
    container->num_quads = cuboid->num_quads;
    container->quads = cuboid->quads;
    
    WitPoint *v = cuboid->vertices;
    Wit
        x = x_of(cuboid->container->POR),
        y = y_of(cuboid->container->POR),
        z = z_of(cuboid->container->POR);
    
    v[0] = (WitPoint){{x   , y   , z   }};
    v[1] = (WitPoint){{x+dx, y   , z   }};
    v[2] = (WitPoint){{x+dx, y+dy, z   }};
    v[3] = (WitPoint){{x   , y+dy, z   }};
    v[4] = (WitPoint){{x   , y   , z+dz}};
    v[5] = (WitPoint){{x+dx, y   , z+dz}};
    v[6] = (WitPoint){{x+dx, y+dy, z+dz}};
    v[7] = (WitPoint){{x   , y+dy, z+dz}};

    for (size_t i_quad = 0; i_quad < cuboid->num_quads; i_quad++) {
        QuadFace *quad = &cuboid->quads[i_quad].quad;
        quad->i_tex = 0; // default texture
        quad->tv0 = (Fix32Point_2D){0, (textures[quad->i_tex].h << 16) - 1};
        quad->tv1 = (Fix32Point_2D){(textures[quad->i_tex].w << 16) - 1,
                                    (textures[quad->i_tex].h << 16) - 1};
        quad->tv2 = (Fix32Point_2D){(textures[quad->i_tex].w << 16) - 1, 0};
        quad->tv3 = (Fix32Point_2D){0, 0};
    }

    for (size_t i_quad = 0; i_quad < cuboid->num_quads; i_quad++) {
        QuadFace *quad = &cuboid->quads[i_quad].quad;
        scale_quad_texels_to_world
            (v[quad->i_v0], v[quad->i_v1],
             v[quad->i_v2], v[quad->i_v3],
             &quad->tv0, &quad->tv1,
             &quad->tv2, &quad->tv3,
             &textures[quad->i_tex]);
    }
}

Brush *new_CuboidBrush(void) {
    void *data = zgl_Malloc(sizeof(Brush) + sizeof(CuboidBrush));
    Brush *brush = (Brush *)data;
    brush->brush = (void *)((char *)data + sizeof(Brush));
    Cuboid_of(brush)->container = brush;

    return brush;
}

Brush *create_CuboidBrush
(WitPoint origin, int CSG, // <--- common brush parameters
 ufix32_t dx, ufix32_t dy, ufix32_t dz // <--- cuboid brush parameters
 ) {
    Brush *brush = new_CuboidBrush();
    init_Brush(brush, origin, CSG);

    brush->type = BT_CUBOID;
    init_CuboidBrush(Cuboid_of(brush), dx, dy, dz);
    return brush;
}



void triangulate_CuboidBrush(CuboidBrush *cuboid, StaticGeometry *geo) {
    for (size_t i_v = 0; i_v < cuboid->num_vertices; i_v++) {
        // TODO: Check if vertex is already in list, to save space. I can't do
        // this quickly later since vertices don't know what faces they are part
        // of.
        geo->vertices[geo->num_vertices + i_v] = cuboid->vertices[i_v];
    }
    
    for (size_t i_e = 0; i_e < cuboid->num_edges; i_e++) {
        geo->edges[geo->num_edges + i_e] = cuboid->edges[i_e];
    }
    
    for (size_t i_q = 0; i_q < cuboid->num_quads; i_q++) {
    // TODO: "prune" (ie. skip) brush faces that are contained within an earlier
    // brush face.
        
        BrushQuadFace *brushquad = &cuboid->quads[i_q];
        scale_quad_texels_to_world
            (cuboid->vertices[brushquad->quad.i_v0],
             cuboid->vertices[brushquad->quad.i_v1],
             cuboid->vertices[brushquad->quad.i_v2],
             cuboid->vertices[brushquad->quad.i_v3],
             &brushquad->quad.tv0, &brushquad->quad.tv1,
             &brushquad->quad.tv2, &brushquad->quad.tv3,
             &textures[brushquad->quad.i_tex]);

        size_t i_face0 = geo->num_faces + 2*i_q;
        size_t i_face1 = geo->num_faces + 2*i_q + 1;
        geo->progenitor_brush[i_face0] = cuboid->container;
        geo->progenitor_brush[i_face1] = cuboid->container;
        Face *face0 = &geo->faces[i_face0];
        Face *face1 = &geo->faces[i_face1];

        // inherited properties: .i_tex, .normal, .flags
        
        brushquad->subface0 = face0;
        brushquad->subface1 = face1;
        face0->brushface = (void *)brushquad;
        face1->brushface = (void *)brushquad;
        
        QuadFace *quad = &brushquad->quad;
        face0->i_tex = quad->i_tex;
        face1->i_tex = quad->i_tex;

        face0->flags = quad->flags;
        face1->flags = quad->flags;

        if (cuboid->container->CSG == CSG_SUBTRACTIVE) {
            face0->normal = quad->normal;
            face1->normal = quad->normal;

            face0->i_v0 = geo->num_vertices + quad->i_v0;
            face0->i_v1 = geo->num_vertices + quad->i_v1;
            face0->i_v2 = geo->num_vertices + quad->i_v2;
            face1->i_v0 = geo->num_vertices + quad->i_v2;
            face1->i_v1 = geo->num_vertices + quad->i_v3;
            face1->i_v2 = geo->num_vertices + quad->i_v0;

            face0->tv0 = quad->tv0;
            face0->tv1 = quad->tv1;
            face0->tv2 = quad->tv2;
            face1->tv0 = quad->tv2;
            face1->tv1 = quad->tv3;
            face1->tv2 = quad->tv0;            
        }
        else if (cuboid->container->CSG == CSG_ADDITIVE) {
            // The brush itself is oriented subtractively, so additive
            // triangulation requires negating the normals and reordering the
            // vertex indices and texels.
            face0->normal = (WitPoint){{
                    -x_of(quad->normal),
                    -y_of(quad->normal),
                    -z_of(quad->normal)}};
            face1->normal = (WitPoint){{
                    -x_of(quad->normal),
                    -y_of(quad->normal),
                    -z_of(quad->normal)}};

            face0->i_v0 = geo->num_vertices + quad->i_v2;
            face0->i_v1 = geo->num_vertices + quad->i_v1;
            face0->i_v2 = geo->num_vertices + quad->i_v0;
            face1->i_v0 = geo->num_vertices + quad->i_v0;
            face1->i_v1 = geo->num_vertices + quad->i_v3;
            face1->i_v2 = geo->num_vertices + quad->i_v2;

            face0->tv0 = quad->tv3;
            face0->tv1 = quad->tv0;
            face0->tv2 = quad->tv1;
            face1->tv0 = quad->tv1;
            face1->tv1 = quad->tv2;
            face1->tv2 = quad->tv3;
        }
        else {
            dibassert(false);
        }
    }

    compute_face_brightnesses(2 * cuboid->num_quads, geo->faces + geo->num_faces);
    
    geo->num_vertices += cuboid->num_vertices;
    geo->num_edges    += cuboid->num_edges;
    geo->num_faces    += 2 * cuboid->num_quads;
}
