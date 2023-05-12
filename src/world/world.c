#include "src/misc/epm_includes.h"
#include "src/system/dir.h"
#include "src/system/loop.h"
#include "src/draw/textures.h"
#include "src/world/world.h"
#include "src/world/bsp.h"

#include "src/entity/entity.h"

#define VERBOSITY
#include "verbosity.h"

EditorCamera const default_cam = {
    .pos = {.v = {fixify(0), fixify(0), fixify(0)}},
    .view_angle_h = 0,
    .view_angle_v = ANG18_PI2,
    .key_angular_motion = false,
    .key_angular_h = 0,
    .key_angular_v = 0,
    .mouse_angular_motion = false,
    .mouse_angular_h = 0,
    .mouse_angular_v = 0,
    .view_vec = {.v = {0, fixify(1), 0}},
    .view_vec_XY = {.x = 0, .y = fixify(1)},
    .key_motion = false,
    .mouse_motion = false, 
    .mouse_motion_vel = {.v = {0, 0, 0}},
    .vel = {.v = {0, 0, 0}},
    .collision_radius = CAMERA_COLLISION_RADIUS,
    .collision_height = CAMERA_COLLISION_HEIGHT,
    .collision_box_reach = CAMERA_COLLISION_BOX_REACH,    
};

EditorCamera cam = default_cam;
epm_EntityNode cam_node = {.entity = &cam, .onTic = onTic_cam};

Brush *frame;

Player player = {.pos = {.v = {0, 0, 256}}};
epm_EntityNode player_node = {.entity = &player, .onTic = onTic_player};

Transform tf;

BrushGeometry brushgeo = {0};
StaticGeometry staticgeo = {0};
BSPTree bsp = {0};
epm_World world = {0};

WitPoint onetime_vels[16] = {0};
int onetime_free = 0;
WitPoint cts_vels[16] = {0};
int cts_free = 0;

WitPoint directional_light_vector = {
    .v = {-fixify(1), 0, 0}
};
fix32_t directional_light_intensity = fixify(1)/2;
fix32_t ambient_light_intensity = fixify(1);

WitPoint bigbox_vertices[16] = {
    [0] = {{INT32_MIN, INT32_MIN, INT32_MIN}},
    [1] = {{INT32_MIN, INT32_MIN, INT32_MAX}},
    [2] = {{INT32_MIN, INT32_MAX, INT32_MAX}},
    [3] = {{INT32_MIN, INT32_MAX, INT32_MIN}},
    [4] = {{INT32_MAX, INT32_MIN, INT32_MIN}},
    [5] = {{INT32_MAX, INT32_MIN, INT32_MAX}},
    [6] = {{INT32_MAX, INT32_MAX, INT32_MAX}},
    [7] = {{INT32_MAX, INT32_MAX, INT32_MIN}},

    [8+0] = {{INT32_MIN, 0, 0}},
    [8+1] = {{INT32_MAX, 0, 0}},
    [8+2] = {{0, INT32_MIN, 0}},
    [8+3] = {{0, INT32_MAX, 0}},
    [8+4] = {{INT32_MIN, INT32_MIN, 0}},
    [8+5] = {{INT32_MIN, INT32_MAX, 0}},
    [8+6] = {{INT32_MAX, INT32_MIN, 0}},
    [8+7] = {{INT32_MAX, INT32_MAX, 0}},
};

Edge bigbox_edges[18] = {
    [0]  = {0, 1},
    [1]  = {1, 2},
    [2]  = {2, 3},
    [3]  = {3, 0},
    [4]  = {4, 5},
    [5]  = {5, 6},
    [6]  = {6, 7},
    [7]  = {7, 4},
    [8]  = {0, 4},
    [9]  = {1, 5},
    [10] = {2, 6},
    [11] = {3, 7},

    [12] = {8+0, 8+1},
    [13] = {8+2, 8+3},
    [14] = {8+4, 8+5},
    [15] = {8+5, 8+7},
    [16] = {8+7, 8+6},
    [17] = {8+6, 8+4},
};

EdgeSet view3D_bigbox = {
    .num_vertices = 16,
    .vertices = bigbox_vertices,
    .num_edges = 18,
    .edges = bigbox_edges,
    .wirecolor = 0x2596BE
};


WitPoint grid_vertices[24] = {
    {{3*(FIX32_MAX/4), FIX32_MIN, 0}},
    {{3*(FIX32_MAX/4), FIX32_MAX, 0}},
    {{FIX32_MAX/2, FIX32_MIN, 0}},
    {{FIX32_MAX/2, FIX32_MAX, 0}},
    {{FIX32_MAX/4, FIX32_MIN, 0}},
    {{FIX32_MAX/4, FIX32_MAX, 0}},
    {{FIX32_MIN/4, FIX32_MIN, 0}},
    {{FIX32_MIN/4, FIX32_MAX, 0}},
    {{FIX32_MIN/2, FIX32_MIN, 0}},
    {{FIX32_MIN/2, FIX32_MAX, 0}},
    {{3*(FIX32_MIN/4), FIX32_MIN, 0}},
    {{3*(FIX32_MIN/4), FIX32_MAX, 0}},

    {{FIX32_MIN, 3*(FIX32_MAX/4), 0}},
    {{FIX32_MAX, 3*(FIX32_MAX/4), 0}},
    {{FIX32_MIN, FIX32_MAX/2, 0}},
    {{FIX32_MAX, FIX32_MAX/2, 0}},
    {{FIX32_MIN, FIX32_MAX/4, 0}},
    {{FIX32_MAX, FIX32_MAX/4, 0}},
    {{FIX32_MIN, FIX32_MIN/4, 0}},
    {{FIX32_MAX, FIX32_MIN/4, 0}},
    {{FIX32_MIN, FIX32_MIN/2, 0}},
    {{FIX32_MAX, FIX32_MIN/2, 0}},
    {{FIX32_MIN, 3*(FIX32_MIN/4), 0}},
    {{FIX32_MAX, 3*(FIX32_MIN/4), 0}},    
};

Edge grid_edges[12] = {
    [0] =  {0, 1},
    [1] =  {2, 3},
    [2] =  {4, 5},
    [3] =  {6, 7},

    [4] =  {8, 9},
    [5] =  {10, 11},
    [6] =  {12, 13},
    [7] =  {14, 15},

    [8] =  {16, 17},
    [9] =  {18, 19},
    [10] = {20, 21},
    [11] = {22, 23},    
};

EdgeSet view3D_grid = {
    .num_vertices = 24,
    .vertices = grid_vertices,
    .num_edges = 12,
    .edges = grid_edges,
    .wirecolor = 0x235367
};

epm_Result epm_Tic(void) {
    for (epm_EntityNode *node = world.entity_head.next; node; node = node->next) {
        if (node->onTic) node->onTic(node->entity);
    }
    
    return EPM_CONTINUE;
}

extern void compute_face_brightnesses(size_t num_faces, Face *faces);
extern void compute_face_normals(WitPoint *vertices, size_t num_faces, Face *faces);

void compute_vertex_intensities(size_t num_vertices, WitPoint const *vertices, uint8_t *vertex_intensities) {
    // TEMP: Compute brightness based on a single point light source at (0,0,0)
    // with max brightness. Inverse square law.
    WitPoint source_point = {{fixify(256),fixify(256),0}};
    uint8_t source_brightness = 0xFF;
    
    for (size_t i_v = 0; i_v < num_vertices; i_v++) {
        WitPoint v = vertices[i_v];
        WitPoint diff = {{
                x_of(source_point)-x_of(v),
                y_of(source_point)-y_of(v),
                z_of(source_point)-z_of(v)}};

        fix64_t D2 = norm_Euclidean(diff)>>8;
        /*
        fix64_t D2 = ((fix64_t)x_of(diff)*(fix64_t)x_of(diff) +
                      (fix64_t)y_of(diff)*(fix64_t)y_of(diff) +
                      (fix64_t)z_of(diff)*(fix64_t)z_of(diff))>>18;
        */
        printf("D2 = %s\n", fmt_fix_x(D2, 16));
        printf("SRC = %s\n", fmt_fix_x(((fix64_t)source_brightness)<<16, 16));

        D2 = MAX((1<<16), (D2));
        fix64_t tmp = (((fix64_t)source_brightness)<<16)/D2;
        printf("%lu\n", tmp);
        dibassert(tmp>>16 < 255);
        vertex_intensities[i_v] = (uint8_t)(tmp);
        putchar('\n');
    }
}


epm_Result epm_InitWorld(void) {
    world.loaded = false;
    world.brushgeo = &brushgeo;
    world.staticgeo = &staticgeo;
    world.bsp = &bsp;

    world.entity_head.next = &cam_node;
    cam_node.next = &player_node;
    player_node.next = NULL;
    
    frame = create_CuboidBrush((WitPoint){{-fixify(128), -fixify(128), -fixify(128)}}, CSG_SUBTRACTIVE,
                               fixify(256), fixify(256), fixify(256));

    return EPM_SUCCESS;
}

epm_Result epm_TermWorld(void) {
    epm_UnloadWorld();

    return EPM_SUCCESS;
}

epm_Result epm_LoadWorld(char *worldname) {
    if (world.loaded) epm_UnloadWorld();

    if (EPM_SUCCESS != epm_ReadWorldFile(&world, worldname)) {
        epm_UnloadWorld();
        return EPM_FAILURE;
    }

    if (EPM_SUCCESS != epm_TriangulateBrushGeometry()) {
        epm_UnloadWorld();
        return EPM_FAILURE;
    }
    
    if (EPM_SUCCESS !=
        create_BSPTree(world.bsp, 1,
                       staticgeo.num_vertices, staticgeo.vertices,
                       staticgeo.num_faces, staticgeo.faces)) {
        epm_UnloadWorld();
        return EPM_FAILURE;
    }

    // temporary
    world.entity_head.next = &cam_node;
    cam_node.next = &player_node;
    player_node.next = NULL;
    
    world.loaded = true;
    
    return EPM_SUCCESS;
}

epm_Result epm_UnloadWorld(void) {
    destroy_BSPTree(world.bsp);

    reset_StaticGeometry();

    reset_BrushGeometry();
    
    world.loaded = false;
    
    return EPM_SUCCESS;
}

epm_Result reset_BrushGeometry(void) {
    if (brushgeo.head == NULL) {
        dibassert(brushgeo.tail == NULL);
        return EPM_SUCCESS;
    }
    
    BrushNode *node_next;
    for (BrushNode *node = brushgeo.head; node; node = node_next) {
         node_next = node->next;

         dibassert(node->brush);
         zgl_Free(node->brush);
         zgl_Free(node);
    }

    brushgeo.head = NULL;
    brushgeo.tail = NULL;
    
    return EPM_SUCCESS;
}

epm_Result reset_StaticGeometry(void) {
    memset(&staticgeo, 0, sizeof staticgeo);
    
    return EPM_SUCCESS;    
}


#include "src/input/command.h"

static void CMDH_loadworld(int argc, char **argv, char *output_str) {
    (void)output_str;
    
    epm_LoadWorld(argv[1]);
}

epm_Command const CMD_loadworld = {
    .name = "loadworld",
    .argc_min = 2,
    .argc_max = 2,
    .handler = CMDH_loadworld,
};

static void CMDH_unloadworld(int argc, char **argv, char *output_str) {
    (void)argc, (void)argv, (void)output_str;
    
    epm_UnloadWorld();
}

epm_Command const CMD_unloadworld = {
    .name = "unloadworld",
    .argc_min = 1,
    .argc_max = 1,
    .handler = CMDH_unloadworld,
};


epm_Result epm_SaveWorld(char const *filename) {
    epm_WriteWorldFile(&world, filename);

    return EPM_SUCCESS;
}

static void CMDH_saveworld(int argc, char **argv, char *output_str) {
    (void)argc, (void)argv, (void)output_str;

    if (argc == 1)
        epm_SaveWorld("tmp"); // TODO: Save over current file.
    else
        epm_SaveWorld(argv[1]); // TODO: Save over current file.
}

epm_Command const CMD_saveworld = {
    .name = "saveworld",
    .argc_min = 1,
    .argc_max = 2,
    .handler = CMDH_saveworld,
};




static void CMDH_rebuild(int argc, char **argv, char *output_str) {
    if ( ! world.loaded) {
        sprintf(output_str, "Can't rebuild an empty world.");
        return;
    }

    //destroy_BSPTree(world.bsp);
    //reset_StaticGeometry();
    //reset_BrushGeometry();
    
    epm_TriangulateBrushGeometry();
    
    if (EPM_SUCCESS !=
        create_BSPTree(world.bsp, 1,
                       staticgeo.num_vertices, staticgeo.vertices,
                       staticgeo.num_faces, staticgeo.faces)) {
        epm_UnloadWorld();
        return;
    }

    world.loaded = true;

}

epm_Command const CMD_rebuild = {
    .name = "rebuild",
    .argc_min = 1,
    .argc_max = 1,
    .handler = CMDH_rebuild,
};
