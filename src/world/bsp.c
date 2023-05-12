#include "src/world/bsp.h"
#include "zigil/zigil_mem.h"
//#include "src/draw/draw.h"
// In this project, the style-conforming identifier for an array is plural, such
// as things[] instead of thing[] (or *things instead of *thing in pointer
// form). The style-conforming identifier for an index into an array things[]
// (or *things) is i_thing.
//
// In the declaration of struct FaceSubset below, *i_faces is an array whose
// values are themselves indices into the array faces[]. Thus in *i_faces,
// the "i_face" part represents the values being indices, and the "s" represents
// the fact that it is an array.
//
// The style-conforming notation for an index into *i_faces is therefore
// i_i_face. In general, the number of "i_" prefixes represents how many layers
// deep the indices go. There should, however, be no reason for more than two
// levels.


// When a triangle is split during the BSP construction process, 1 or 2 new
// vertices are created, and 2 or 3 triangles are created. Any new vertex is on
// the line joining 2 vertices of the split triangle, and those 2 vertices are
// stored as "edge-parents" of the new vertex. Any new triangle also has the
// split triangle stored as a "face-parent". New vertices and new triangles also
// have the "progenitor face" and "progenitor" vertices, since triangles may be
// split multiple times during recursion.

// TODO: The leaves correspond to a convex partition of the world. Compute this
// partition: Starting from a leaf, how to find a minimum set of splitting
// planes that bound the convex subregion.

// TODO: At every recursive step, search for and take the best "free cut" if one
// exists.


typedef struct Maker_BSPTree Maker_BSPTree;
typedef struct Maker_BSPNode Maker_BSPNode;
typedef struct Maker_BSPFace Maker_BSPFace;


#define BSP_VERBOSITY
#ifdef BSP_VERBOSITY
#  define VERBOSITY
static void print_vertices(size_t num, WitPoint const *arr);
static void print_Faces(size_t num, Face const *arr);
static void print_Maker_BSPTree_diagram(Maker_BSPNode *node, size_t level);
static void print_Maker_BSPTree(Maker_BSPTree *tree);
static void print_Maker_BSPFaces(size_t num, Maker_BSPFace *arr);
static void print_BSPTree_diagram(BSPNode *node, size_t level);
static void print_BSPTree(BSPTree *tree);
static void print_BSPFaces(size_t num, BSPFace *bspfaces);
#else
#  define print_vertices(num, arr) (void)0
#  define print_Faces(num, arr) (void)0
#  define print_Maker_BSPTree_diagram(node, level) (void)0
#  define print_Maker_BSPTree(tree) (void)0
#  define print_Maker_BSPFaces(num, arr) (void)0
#  define print_BSPTree_diagram(node, level) (void)0
#  define print_BSPTree(tree) (void)0
#  define print_BSPFaces(num, bspfaces) (void)0
#endif

#include "verbosity.h"

#undef LOG_LABEL
#define LOG_LABEL "BSP"

static int num_alloc_nodes = 0;

struct Maker_BSPNode {
    Maker_BSPNode *parent;
    
    Maker_BSPNode *front;
    Maker_BSPNode *back;
    size_t i_bspface; // index to the BSP face itself

    WitPoint splitV; // point on splitting plane
    WitPoint splitN; // normal of splitting plane
};

struct Maker_BSPFace {
    size_t i_v0; // index into tree vertex array
    size_t i_v1; // index into tree vertex array
    size_t i_v2; // index into tree vertex array
    bool from_cut;
    size_t i_progenitor_face;
};

// NOTE: Do not confuse a BSPFace's parent face with a BSPNode's parent node.

typedef struct FaceSubset {
    size_t num_i_faces;
    size_t *i_faces; // an array of indices to bsptree faces array
    Maker_BSPNode *node;
} FaceSubset;

/* For each progenitor face, there is a corresponding struct for data about the
   subdivision of that face. */
typedef struct TriSubDiv {
    Face *face;

    size_t num_subV;
    size_t i_subV[64]; // indices into BSP vertex array

    size_t num_subE;
    struct {
        size_t i_subV0;
        size_t i_subV1;
    } i_subE[64];
    
    // for each vertex in the subdivision, record the edge-parents
    size_t num_subF;
    struct {
        size_t i_subV0;
        size_t i_subV1;
        size_t i_subV2;
    } i_subF[64];
} TriSubDiv;

struct Maker_BSPTree {
    size_t num_cuts;
    // TODO: Other stats.
    
    size_t num_nodes;
    Maker_BSPNode *root;
    
    size_t num_vertices;
    WitPoint vertices[MAX_STATIC_VERTICES];

    size_t num_faces;
    Maker_BSPFace bspfaces[MAX_STATIC_FACES];
};

typedef enum SpatialRelation {
    SR_IGNORE,
    SR_FRONT,
    SR_BACK,
    SR_COPLANAR,
    SR_CUT_TYPE1,
    SR_CUT_TYPE2F,
    SR_CUT_TYPE2B,
} SpatialRelation;

typedef struct SplittingData {
    size_t i_split;
    double balance;
    size_t num_cuts;
    size_t num_front_faces;
    size_t num_back_faces;

    SpatialRelation *relations;
    uint8_t *sides0, *sides1, *sides2;
} SplittingData;

static Face const * g_p_progenitor_faces;
static WitPoint const * g_p_progenitor_vertices;
static Maker_BSPTree g_maker_tree;
static BSPTree *g_p_final_tree;
static size_t g_i_node;
static size_t g_i_face;

#define BSPSEL_TRIVIAL 0
#define BSPSEL_MINCUT1_BALANCE2 1
#define BSPSEL_BALANCE1_MINCUT2 2
static int g_bspsel = 2;

extern epm_Result create_BSPTree(BSPTree *tree, int bspsel,
                                 size_t num_vertices, WitPoint const *vertices,
                                 size_t num_faces, Face const *faces);
static void BSP_recursion(FaceSubset subfaces);
static void construct_final_tree_depth_first(Maker_BSPNode *in_node, BSPNode *ex_parent, int side, uint16_t depth);

extern void destroy_BSPTree(BSPTree *p_tree);

extern void measure_BSPTree_recursion(BSPNode *node, int node_depth);

static void choose_splitting_plane(size_t num_i_faces, size_t *i_faces, SplittingData *data);
static void csd_mincut1_balance2(bool *early_exit, bool *replace, SplittingData *current, SplittingData *optimal);
static void csd_balance1_mincut2(bool *early_exit, bool *replace, SplittingData *current, SplittingData *optimal);
static void csd_trivial(bool *early_exit, bool *replace, SplittingData *current, SplittingData *optimal);
void (*compare_splitting_data)(bool *early_exit, bool *replace, SplittingData *current, SplittingData *optimal);
static void test_spatial_relation
(size_t i_face,
 WitPoint V0, WitPoint V1, WitPoint V2, WitPoint N,
 uint8_t *side0, uint8_t *side1, uint8_t *side2, SpatialRelation *sr);


static Maker_BSPNode *create_Maker_BSPNode();
static void delete_Maker_BSPTree(Maker_BSPTree *p_tree);
static void delete_Maker_BSPNode(Maker_BSPNode *p_node);

static bool find_intersection
(WitPoint lineV0, WitPoint lineV1,
 WitPoint planeV, WitPoint planeN,
 WitPoint *out_sect);

#if 0
static bool find_intersection2
(size_t i_v0, size_t i_v1,
 WitPoint V0,  WitPoint normal,
 WitPoint *out_sect);
#endif

static epm_Result compute_edges(BSPTree *tree) {
    size_t num_edges = 0;
    size_t i_fv0, i_fv1, i_fv2, i_ev0, i_ev1;
    bool duplicate;
    
    for (size_t i_face = 0; i_face < tree->num_bspfaces; i_face++) {
        i_fv0 = tree->bspfaces[i_face].face.i_v0;
        i_fv1 = tree->bspfaces[i_face].face.i_v1;
        i_fv2 = tree->bspfaces[i_face].face.i_v2;

        
        i_ev0 = MIN(i_fv0, i_fv1);
        i_ev1 = MAX(i_fv0, i_fv1);
        duplicate = false;
        for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
            if ((tree->edges[i_edge].i_v0 == i_ev0 && tree->edges[i_edge].i_v1 == i_ev1)) {
                duplicate = true;
                break; // from i_edge loop
            }
        }
        if ( ! duplicate) { // add to list
            if (num_edges == 100000)
                return EPM_ERROR;
            tree->edges[num_edges].i_v0 = i_ev0;
            tree->edges[num_edges].i_v1 = i_ev1;
            num_edges++;
        }


        i_ev0 = MIN(i_fv1, i_fv2);
        i_ev1 = MAX(i_fv1, i_fv2);
        duplicate = false;
        for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
            if ((tree->edges[i_edge].i_v0 == i_ev0 && tree->edges[i_edge].i_v1 == i_ev1)) {
                duplicate = true;
                break; // from i_edge loop
            }
        }
        if ( ! duplicate) { // add to list
            if (num_edges == 100000)
                return EPM_ERROR;
            tree->edges[num_edges].i_v0 = i_ev0;
            tree->edges[num_edges].i_v1 = i_ev1;
            num_edges++;
        }        


        i_ev0 = MIN(i_fv2, i_fv0);
        i_ev1 = MAX(i_fv2, i_fv0);
        duplicate = false;
        for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
            if ((tree->edges[i_edge].i_v0 == i_ev0 && tree->edges[i_edge].i_v1 == i_ev1)) {
                duplicate = true;
                break; // from i_edge loop
            }
        }
        if ( ! duplicate) { // add to list
            if (num_edges == 100000)
                return EPM_ERROR;
            tree->edges[num_edges].i_v0 = i_ev0;
            tree->edges[num_edges].i_v1 = i_ev1;
            num_edges++;
        }
    }

    vbs_printf("Found %zu edges from %zu faces.\n", num_edges, tree->num_bspfaces);

    tree->num_edges = num_edges;
    
    return EPM_SUCCESS;
}



/* Creating a BSPTree */
epm_Result create_BSPTree(BSPTree *tree, int bspsel,
                    size_t num_vertices, WitPoint const *vertices,
                    size_t num_faces, Face const *faces) {
    epm_Log(LT_INFO, "Creating BSP tree.");
    
    g_bspsel = bspsel;
    g_p_progenitor_vertices = vertices;
    g_p_progenitor_faces = faces;
    g_p_final_tree = tree;
    
    g_p_final_tree->num_progenitor_vertices = num_vertices;
    g_p_final_tree->progenitor_vertices = vertices;
    g_p_final_tree->num_progenitor_faces = num_faces;
    g_p_final_tree->progenitor_faces = faces;
    
    vbs_putchar('\n');
    vbs_printf("+----------------------------+\n"
               "| Starting the BSP algorithm |\n"
               "+----------------------------+\n");
    vbs_printf("Given: %zu vertices\n"
               "       %zu faces\n", num_vertices, num_faces);
    vbs_putchar('\n');
    vbs_printf(" Given Vertices\n"
               "+--------------+\n");
    print_vertices(num_vertices, vertices);
    vbs_putchar('\n');
    vbs_printf(" Given Faces\n"
               "+-----------+\n");
    print_Faces(num_faces, faces);
    vbs_putchar('\n');

    memset(&g_maker_tree, 0, sizeof(g_maker_tree));
    g_maker_tree.num_cuts = 0;
    g_maker_tree.root = NULL;
    
    // to start off, the BSP vertex array shall be identical to the given vertex
    // array. Additional vertices may be added as the algorithm proceeds.
    for (size_t i_vertices = 0; i_vertices < num_vertices; i_vertices++) {
        g_maker_tree.vertices[i_vertices] = vertices[i_vertices];
    }
    g_maker_tree.num_vertices = num_vertices;
    
    // similar to vertices thing above
    for (size_t i_face = 0; i_face < num_faces; i_face++) {
        g_maker_tree.bspfaces[i_face].i_v0 = faces[i_face].i_v0;
        g_maker_tree.bspfaces[i_face].i_v1 = faces[i_face].i_v1;
        g_maker_tree.bspfaces[i_face].i_v2 = faces[i_face].i_v2;
        g_maker_tree.bspfaces[i_face].from_cut = false;
        g_maker_tree.bspfaces[i_face].i_progenitor_face = i_face;
    }
    g_maker_tree.num_faces = num_faces;

    
    
    vbs_printf("Post-Copy: %zu BSP vertices\n"
                   "           %zu BSP faces\n",
                   g_maker_tree.num_vertices,
                   g_maker_tree.num_faces);
    vbs_putchar('\n');
    vbs_printf(" Post-Copy BSP Vertices\n"
                   "+----------------------+\n");
    print_vertices(g_maker_tree.num_vertices, g_maker_tree.vertices);
    vbs_putchar('\n');
    vbs_printf(" Post-Copy BSP Faces\n"
                   "+-------------------+\n");
    print_Maker_BSPFaces(g_maker_tree.num_faces, g_maker_tree.bspfaces);
    vbs_putchar('\n');



    // Start the recursive node-building algorithm. The first "subset"
    // represents EVERY face, and therefore i_faces[i_i_face] = i_i_face
    FaceSubset fsubset;
    fsubset.num_i_faces = num_faces;
    fsubset.i_faces = zgl_Malloc(fsubset.num_i_faces*sizeof(size_t));
    for (size_t i_i_face = 0; i_i_face < fsubset.num_i_faces; i_i_face++)
        fsubset.i_faces[i_i_face] = i_i_face;
    g_maker_tree.num_nodes = 0;
    g_maker_tree.root = fsubset.node = create_Maker_BSPNode();
    g_maker_tree.num_nodes++;
    fsubset.node->parent = NULL;

    switch (g_bspsel) {
    case BSPSEL_TRIVIAL:
        compare_splitting_data = csd_trivial;
        break;
    case BSPSEL_MINCUT1_BALANCE2:
        compare_splitting_data = csd_mincut1_balance2;
        break;
    case BSPSEL_BALANCE1_MINCUT2:
        compare_splitting_data = csd_balance1_mincut2;
        break;
    default:
        dibassert(false);
    }
    
    vbs_putchar('\n');
    vbs_printf("BSP Root Recursion Step Begin\n");
    BSP_recursion(fsubset);

    zgl_Free(fsubset.i_faces);
    fsubset.i_faces = NULL;

    print_Maker_BSPTree(&g_maker_tree);
    vbs_putchar('\n');
    
    vbs_printf(" Maker BSP Vertices\n"
               "+------------------+\n");
    print_vertices(g_maker_tree.num_vertices, g_maker_tree.vertices);
    vbs_putchar('\n');
    
    vbs_printf(" Maker BSP Faces\n"
               "+---------------+\n");
    print_Maker_BSPFaces(g_maker_tree.num_faces, g_maker_tree.bspfaces);
    vbs_putchar('\n');
    
    vbs_printf(" Maker BSP Tree\n"
               "+--------------+\n");
    print_Maker_BSPTree_diagram(g_maker_tree.root, 0);
    vbs_putchar('\n');
 
    g_p_final_tree->num_vertices = g_maker_tree.num_vertices;
    g_p_final_tree->vertices = zgl_Calloc(g_p_final_tree->num_vertices,
                                          sizeof(WitPoint));
    g_p_final_tree->vertex_intensities = zgl_Calloc(g_p_final_tree->num_vertices,
                                                    sizeof(uint8_t));
    memcpy(g_p_final_tree->vertices,
           g_maker_tree.vertices,
           g_p_final_tree->num_vertices*sizeof(*g_p_final_tree->vertices));
    
    g_p_final_tree->num_nodes    = g_maker_tree.num_nodes;
    g_p_final_tree->num_bspfaces = g_maker_tree.num_faces;
    g_p_final_tree->nodes        = zgl_Calloc(g_p_final_tree->num_nodes,
                                              sizeof(BSPNode));
    g_p_final_tree->bspfaces     = zgl_Calloc(g_p_final_tree->num_nodes,
                                              sizeof(BSPFace));
    g_p_final_tree->num_cuts   = g_maker_tree.num_cuts;
        
    g_i_node = 0;
    g_i_face = 0;
    construct_final_tree_depth_first(g_maker_tree.root, NULL, -1, 0);
    g_i_node = 0;
    g_i_face = 0;

    extern void compute_vertex_intensities(size_t num_vertices, WitPoint const *vertices, uint8_t *vertex_intensities);
    compute_vertex_intensities(g_p_final_tree->num_vertices,
                               g_p_final_tree->vertices,
                               g_p_final_tree->vertex_intensities);

    measure_BSPTree(g_p_final_tree);
    print_BSPTree(g_p_final_tree);
    vbs_putchar('\n');
    
    vbs_printf(" Final BSP Vertices\n"
               "+------------------+\n");
    print_vertices(g_p_final_tree->num_vertices, g_p_final_tree->vertices);
    vbs_putchar('\n');
    
    vbs_printf(" Final BSP Faces\n"
               "+---------------+\n");
    print_BSPFaces(g_p_final_tree->num_bspfaces, g_p_final_tree->bspfaces);
    vbs_putchar('\n');
    
    vbs_printf(" Final BSP Tree\n"
               "+--------------+\n");
    print_BSPTree_diagram(&g_p_final_tree->nodes[0], 0);
    vbs_putchar('\n');
    
    /* Free the internal tree */
    delete_Maker_BSPTree(&g_maker_tree);
    vbs_putchar('\n');

    compute_edges(g_p_final_tree);

    vbs_putchar('\n');
    
    return EPM_SUCCESS;
}

static void BSP_recursion(FaceSubset subset) {
    if (subset.num_i_faces == 1) { // leaf; no fronts or backs
        subset.node->i_bspface = subset.i_faces[0];
        subset.node->front = NULL;
        subset.node->back = NULL;
        vbs_printf("Leaf node.\n");
        return;
    }
    
    size_t num_i_faces = subset.num_i_faces;
    size_t *i_faces = subset.i_faces;
    Maker_BSPNode *node = subset.node;
    
    // +-------------------+
    // | Choosing Splitter |
    // +-------------------+
    
    SplittingData data;
    data.relations = zgl_Malloc(num_i_faces*sizeof(*data.relations));
    data.sides0 = zgl_Malloc(num_i_faces*sizeof(*data.sides0));
    data.sides1 = zgl_Malloc(num_i_faces*sizeof(*data.sides1));
    data.sides2 = zgl_Malloc(num_i_faces*sizeof(*data.sides2));
    
    vbs_printf("Choosing splitter face from %zu indices.\n", num_i_faces);
    //print_int_array_horizontal(subset.num_i_faces, i_faces);
    choose_splitting_plane(num_i_faces, i_faces, &data);
    vbs_printf("Chose face F[%zu] as the splitter.\n", data.i_split);

    // unpack splitting data
    size_t i_split = data.i_split;
    SpatialRelation *relations = data.relations;
    uint8_t *sides0 = data.sides0;
    uint8_t *sides1 = data.sides1;
    uint8_t *sides2 = data.sides2;

    node->i_bspface = i_split;

    Face const *og_face = &g_p_progenitor_faces[g_maker_tree.bspfaces[i_split].i_progenitor_face];
    WitPoint const
        * const splitV = &g_p_progenitor_vertices[og_face->i_v0],
        * const splitN = &og_face->normal;
    node->splitV = *splitV;
    node->splitN = *splitN;
    
    // +------------------------+
    // | Fill in front and back |
    // +------------------------+

    FaceSubset front, back;
    
    //size_t num_subdivisions = data.num_dubdiv;
    front.num_i_faces = data.num_front_faces;
    back.num_i_faces  = data.num_back_faces;
    
    vbs_printf("Starting with %zu faces, expecting %zu face subdivisions, yielding %zu front faces and %zu back faces.\n", num_i_faces-1, data.num_cuts, front.num_i_faces, back.num_i_faces);
    dibassert(front.num_i_faces + back.num_i_faces >= num_i_faces - 1);
    
    bool
        are_fronts = (front.num_i_faces > 0),
        are_backs  = (back.num_i_faces > 0);
    
    node->front = NULL;
    if (are_fronts) {
        // front node is created and has parent set now, but the rest is filled
        // during front recursive step
        node->front = front.node = create_Maker_BSPNode();
        g_maker_tree.num_nodes++;
        front.node->parent = node;
        front.i_faces = zgl_Malloc(front.num_i_faces*sizeof(size_t));
    }
    node->back = NULL;
    if (are_backs) {
        // back node is created and has parent set now, but the rest is filled
        // during back recursive step
        node->back = back.node = create_Maker_BSPNode();
        g_maker_tree.num_nodes++;
        back.node->parent = node;
        back.i_faces = zgl_Malloc(back.num_i_faces*sizeof(size_t));
    }

    // now we have created space to store both sublists of faces.
    size_t
        i_i_front = 0,
        i_i_back = 0;
    
    for (size_t i_i_face = 0; i_i_face < num_i_faces; i_i_face++) {
        size_t i_face = i_faces[i_i_face];
        uint8_t side0 = sides0[i_i_face];
        uint8_t side1 = sides1[i_i_face];
        uint8_t side2 = sides2[i_i_face];
        Maker_BSPFace *bspface = &g_maker_tree.bspfaces[i_face];
        
        switch (relations[i_i_face]) {
        case SR_IGNORE:
            break;
        case SR_COPLANAR:
            if (are_fronts) {
                Maker_BSPFace fnew_front;
                Maker_BSPFace *face = &g_maker_tree.bspfaces[i_face];

                fnew_front.i_v0 = face->i_v0;
                fnew_front.i_v1 = face->i_v1;
                fnew_front.i_v2 = face->i_v2;
                fnew_front.from_cut = bspface->from_cut;
                fnew_front.i_progenitor_face = face->i_progenitor_face;

                // store new faces in the array
                size_t i_fnew_front = i_face; // reuse storage
                g_maker_tree.bspfaces[i_fnew_front] = fnew_front;

                // add new face indices to front and back lists
                front.i_faces[i_i_front++] = i_fnew_front;
            }
            break;
        case SR_FRONT:
            if (are_fronts) {
                Maker_BSPFace fnew_front;
                Maker_BSPFace *face = &g_maker_tree.bspfaces[i_face];

                fnew_front.i_v0 = face->i_v0;
                fnew_front.i_v1 = face->i_v1;
                fnew_front.i_v2 = face->i_v2;
                fnew_front.from_cut = bspface->from_cut;
                fnew_front.i_progenitor_face = face->i_progenitor_face;

                // store new faces in the array
                size_t i_fnew_front = i_face; // reuse storage
                g_maker_tree.bspfaces[i_fnew_front] = fnew_front;

                // add new face indices to front and back lists
                front.i_faces[i_i_front++] = i_fnew_front;
            }
            break;
        case SR_BACK:
            if (are_backs) {
                Maker_BSPFace fnew_back;
                Maker_BSPFace *face = &g_maker_tree.bspfaces[i_face];

                fnew_back.i_v0 = face->i_v0;
                fnew_back.i_v1 = face->i_v1;
                fnew_back.i_v2 = face->i_v2;
                fnew_back.from_cut = bspface->from_cut;
                fnew_back.i_progenitor_face = face->i_progenitor_face;

                // store new faces in the array
                size_t i_fnew_back = i_face; // reuse storage
                g_maker_tree.bspfaces[i_fnew_back] = fnew_back;

                // add new face indices to front and back lists
                back.i_faces[i_i_back++] = i_fnew_back;
            }
            break;
        case SR_CUT_TYPE1:
            {
                // create one new vertex, then split face into two faces
                WitPoint vnew = {{0,0,0}};
                size_t i_vnew = 0;
                // update the number of vertices and get index for new vertex
                i_vnew = g_maker_tree.num_vertices++;
                Maker_BSPFace *face = &g_maker_tree.bspfaces[i_face];
                
                size_t i_fnew_front = 0;
                size_t i_fnew_back  = 0;
                Maker_BSPFace fnew_front;
                Maker_BSPFace fnew_back;

                if (side2 & SIDE_MID) {
                    // 0 -> 1 has intersection
                    //vbs_printf("%i, %i\n", side0, side1);
                    find_intersection(g_maker_tree.vertices[bspface->i_v0],
                                      g_maker_tree.vertices[bspface->i_v1],
                                      *splitV, *splitN, &vnew);
                    
                    if (side0 & SIDE_FRONT) {
                        // if 0 is front vertex:
                        //   front face: 2 0 N
                        //    back face: N 1 2
                        fnew_front.i_v0 = face->i_v0;
                        fnew_front.i_v1 = i_vnew;
                        fnew_front.i_v2 = face->i_v2;
                        fnew_back.i_v0 = i_vnew;
                        fnew_back.i_v1 = face->i_v1;
                        fnew_back.i_v2 = face->i_v2;
                    }
                    else if (side1 & SIDE_FRONT) {
                        // if 1 is front vertex:
                        //   front face: N 1 2
                        //    back face: 2 0 N
                        fnew_front.i_v0 = face->i_v1;
                        fnew_front.i_v1 = face->i_v2;
                        fnew_front.i_v2 = i_vnew;
                        fnew_back.i_v0 = i_vnew;
                        fnew_back.i_v1 = face->i_v2;
                        fnew_back.i_v2 = face->i_v0;
                    }
                    else {
                        dibassert(false);
                    }
                }
                else if (side0 & SIDE_MID) {
                    // 1 -> 2 has intersection
                    //vbs_printf("%i, %i\n", side1, side2);
                    find_intersection(g_maker_tree.vertices[bspface->i_v1],
                                      g_maker_tree.vertices[bspface->i_v2],
                                      *splitV, *splitN, &vnew);

                    if (side1 & SIDE_FRONT) {
                        // if 1 is front vertex:
                        //   front face: 0 1 N
                        //    back face: N 2 0
                        fnew_front.i_v0 = face->i_v1;
                        fnew_front.i_v1 = i_vnew;
                        fnew_front.i_v2 = face->i_v0;
                        fnew_back.i_v0 = i_vnew;
                        fnew_back.i_v1 = face->i_v2;
                        fnew_back.i_v2 = face->i_v0;
                    }
                    else if (side2 & SIDE_FRONT) {
                        // if 2 is front vertex:
                        //   front face: N 2 0
                        //    back face: 0 1 N
                        fnew_front.i_v0 = face->i_v2;
                        fnew_front.i_v1 = face->i_v0;
                        fnew_front.i_v2 = i_vnew;
                        fnew_back.i_v0 = i_vnew;
                        fnew_back.i_v1 = face->i_v0;
                        fnew_back.i_v2 = face->i_v1;
                    }
                    else {
                        dibassert(false);
                    }
                }
                else if (side1 & SIDE_MID) {
                    // 2 -> 0 has intersection
                    //vbs_printf("%i, %i\n", side2, side0);
                    find_intersection(g_maker_tree.vertices[bspface->i_v2],
                                      g_maker_tree.vertices[bspface->i_v0],
                                      *splitV, *splitN, &vnew);

                    if (side2 & SIDE_FRONT) {
                        // if 2 is front vertex:
                        //   front face: 1 2 N
                        //    back face: N 0 1
                        fnew_front.i_v0 = face->i_v2;
                        fnew_front.i_v1 = i_vnew;
                        fnew_front.i_v2 = face->i_v1;
                        fnew_back.i_v0 = i_vnew;
                        fnew_back.i_v1 = face->i_v0;
                        fnew_back.i_v2 = face->i_v1;
                    }
                    else if (side0 & SIDE_FRONT) {
                        // if 0 is front vertex:
                        //   front face: N 0 1
                        //    back face: 1 2 N
                        fnew_front.i_v0 = face->i_v0;
                        fnew_front.i_v1 = face->i_v1;
                        fnew_front.i_v2 = i_vnew;
                        fnew_back.i_v0 = i_vnew;
                        fnew_back.i_v1 = face->i_v1;
                        fnew_back.i_v2 = face->i_v2;
                    }
                    else {
                        dibassert(false);
                    }
                }
                else {
                    dibassert(false);
                }

                // finish filling out new faces
                fnew_front.from_cut = true;
                fnew_back.from_cut = true;
                fnew_front.i_progenitor_face = face->i_progenitor_face;
                fnew_back.i_progenitor_face = face->i_progenitor_face;

                g_maker_tree.num_cuts++;
                
                // store new vertices in the array
                g_maker_tree.vertices[i_vnew] = vnew;

                // store new faces in the array
                i_fnew_front = i_face; // reuse storage
                i_fnew_back  = g_maker_tree.num_faces++; // new storage
                g_maker_tree.bspfaces[i_fnew_front] = fnew_front;
                g_maker_tree.bspfaces[i_fnew_back] = fnew_back;

                // add new face indices to front and back lists
                front.i_faces[i_i_front++] = i_fnew_front;
                back.i_faces[i_i_back++] = i_fnew_back;
            }
            break;
        case SR_CUT_TYPE2F:
            {
                // create two new vertices, then split face into two pieces, and
                // then split the FRONT piece further into two faces.
                
                WitPoint vnew0 = {{0,0,0}};
                WitPoint vnew1 = {{0,0,0}};
                size_t i_vnew0 = 0;
                size_t i_vnew1 = 0;
                // update the number of vertices and get index for new vertex
                i_vnew0 = g_maker_tree.num_vertices++;
                i_vnew1 = g_maker_tree.num_vertices++;
                Maker_BSPFace *face = &g_maker_tree.bspfaces[i_face];

                Maker_BSPFace fnew_front0;
                Maker_BSPFace fnew_front1;
                Maker_BSPFace fnew_back;
                size_t i_fnew_front0 = 0;
                size_t i_fnew_front1 = 0;
                size_t i_fnew_back  = 0;

                if (side2 & SIDE_BACK) {
                    // 1 -> 2 and 2 -> 0 have intersection
                    //vbs_printf("%i, %i\n", side1, side2);
                    find_intersection(g_maker_tree.vertices[bspface->i_v1],
                                      g_maker_tree.vertices[bspface->i_v2],
                                      *splitV, *splitN, &vnew0);
                    //vbs_printf("%i, %i\n", side2, side0);
                    find_intersection(g_maker_tree.vertices[bspface->i_v2],
                                      g_maker_tree.vertices[bspface->i_v0],
                                      *splitV, *splitN, &vnew1);

                    fnew_front0.i_v0 = face->i_v0;
                    fnew_front0.i_v1 = face->i_v1;
                    fnew_front0.i_v2 = i_vnew1;
                    fnew_front1.i_v0 = face->i_v1;
                    fnew_front1.i_v1 = i_vnew0;
                    fnew_front1.i_v2 = i_vnew1;
                    fnew_back.i_v0 = i_vnew0;
                    fnew_back.i_v1 = face->i_v2;
                    fnew_back.i_v2 = i_vnew1;
                }
                else if (side1 & SIDE_BACK) {
                    // 0 -> 1 and 1 -> 2 have intersection
                    //vbs_printf("%i, %i\n", side0, side1);
                    find_intersection(g_maker_tree.vertices[bspface->i_v0],
                                      g_maker_tree.vertices[bspface->i_v1],
                                      *splitV, *splitN, &vnew0);
                    //vbs_printf("%i, %i\n", side1, side2);
                    find_intersection(g_maker_tree.vertices[bspface->i_v1],
                                      g_maker_tree.vertices[bspface->i_v2],
                                      *splitV, *splitN, &vnew1);
                    
                    fnew_front0.i_v0 = face->i_v2;
                    fnew_front0.i_v1 = face->i_v0;
                    fnew_front0.i_v2 = i_vnew1;
                    fnew_front1.i_v0 = face->i_v0;
                    fnew_front1.i_v1 = i_vnew0;
                    fnew_front1.i_v2 = i_vnew1;
                    fnew_back.i_v0 = i_vnew0;
                    fnew_back.i_v1 = face->i_v1;
                    fnew_back.i_v2 = i_vnew1;
                }
                else if (side0 & SIDE_BACK) {
                    // 2 -> 0 and 0 -> 1 have intersection
                    //vbs_printf("%i, %i\n", side2, side0);
                    find_intersection(g_maker_tree.vertices[bspface->i_v2],
                                      g_maker_tree.vertices[bspface->i_v0],
                                      *splitV, *splitN, &vnew0);
                    //vbs_printf("%i, %i\n", side0, side1);
                    find_intersection(g_maker_tree.vertices[bspface->i_v0],
                                      g_maker_tree.vertices[bspface->i_v1],
                                      *splitV, *splitN, &vnew1);
                    
                    fnew_front0.i_v0 = face->i_v1;
                    fnew_front0.i_v1 = face->i_v2;
                    fnew_front0.i_v2 = i_vnew1;
                    fnew_front1.i_v0 = face->i_v2;
                    fnew_front1.i_v1 = i_vnew0;
                    fnew_front1.i_v2 = i_vnew1;
                    fnew_back.i_v0 = i_vnew0;
                    fnew_back.i_v1 = face->i_v0;
                    fnew_back.i_v2 = i_vnew1;
                }
                else {
                    dibassert(false);
                }

                // finish filling out new faces
                fnew_front0.from_cut = true;
                fnew_front1.from_cut = true;
                fnew_back.from_cut = true;
                fnew_front0.i_progenitor_face = face->i_progenitor_face;
                fnew_front1.i_progenitor_face = face->i_progenitor_face;
                fnew_back.i_progenitor_face = face->i_progenitor_face;


                g_maker_tree.num_cuts++;
                // store new vertices in the array
                g_maker_tree.vertices[i_vnew0] = vnew0;
                g_maker_tree.vertices[i_vnew1] = vnew1;

                // store new faces in the array
                i_fnew_front0 = i_face; // reuse storage
                i_fnew_front1 = g_maker_tree.num_faces++; // new storage
                i_fnew_back   = g_maker_tree.num_faces++; // new storage
                g_maker_tree.bspfaces[i_fnew_front0] = fnew_front0;
                g_maker_tree.bspfaces[i_fnew_front1] = fnew_front1;
                g_maker_tree.bspfaces[i_fnew_back] = fnew_back;

                // add new face indices to front and back lists
                front.i_faces[i_i_front++] = i_fnew_front0;
                front.i_faces[i_i_front++] = i_fnew_front1;
                back.i_faces[i_i_back++] = i_fnew_back;
            }
            break;
        case SR_CUT_TYPE2B:
            {
                // create two new vertices, then split face into two pieces, and
                // then split the BACK piece further into two faces.
                WitPoint vnew0 = {{0,0,0}};
                WitPoint vnew1 = {{0,0,0}};
                size_t i_vnew0 = 0;
                size_t i_vnew1 = 0;
                // update the number of vertices and get index for new vertex
                i_vnew0 = g_maker_tree.num_vertices++;
                i_vnew1 = g_maker_tree.num_vertices++;
                Maker_BSPFace *face = &g_maker_tree.bspfaces[i_face];

                Maker_BSPFace fnew_front;
                Maker_BSPFace fnew_back0;
                Maker_BSPFace fnew_back1;
                size_t i_fnew_front = 0;
                size_t i_fnew_back0 = 0;
                size_t i_fnew_back1 = 0;
                
                if (side2 & SIDE_FRONT) {
                    // 0 -> 1 -> N1 -> 2 -> N0
                    //vbs_printf("%i, %i\n", side1, side2);
                    find_intersection(g_maker_tree.vertices[bspface->i_v1],
                                      g_maker_tree.vertices[bspface->i_v2],
                                      *splitV, *splitN, &vnew1);
                    //vbs_printf("%i, %i\n", side2, side0);
                    find_intersection(g_maker_tree.vertices[bspface->i_v2],
                                      g_maker_tree.vertices[bspface->i_v0],
                                      *splitV, *splitN, &vnew0);
                    
                    fnew_front.i_v0 = face->i_v2;
                    fnew_front.i_v1 = i_vnew0;
                    fnew_front.i_v2 = i_vnew1;
                    fnew_back0.i_v0 = i_vnew0;
                    fnew_back0.i_v1 = face->i_v1;
                    fnew_back0.i_v2 = i_vnew1;
                    fnew_back1.i_v0 = face->i_v0;
                    fnew_back1.i_v1 = face->i_v1;
                    fnew_back1.i_v2 = i_vnew0;
                }
                else if (side1 & SIDE_FRONT) {
                    //  and  have intersection
                    //vbs_printf("%i, %i\n", side0, side1);
                    find_intersection(g_maker_tree.vertices[bspface->i_v0],
                                      g_maker_tree.vertices[bspface->i_v1],
                                      *splitV, *splitN, &vnew1);
                    //vbs_printf("%i, %i\n", side1, side2);
                    find_intersection(g_maker_tree.vertices[bspface->i_v1],
                                      g_maker_tree.vertices[bspface->i_v2],
                                      *splitV, *splitN, &vnew0);
                    
                    fnew_front.i_v0 = face->i_v1;
                    fnew_front.i_v1 = i_vnew0;
                    fnew_front.i_v2 = i_vnew1;
                    fnew_back0.i_v0 = i_vnew0;
                    fnew_back0.i_v1 = face->i_v0;
                    fnew_back0.i_v2 = i_vnew1;
                    fnew_back1.i_v0 = face->i_v2;
                    fnew_back1.i_v1 = face->i_v0;
                    fnew_back1.i_v2 = i_vnew0;
                }
                else if (side0 & SIDE_FRONT) {
                    //  and  have intersection
                    //vbs_printf("%i, %i\n", side2, side0);
                    find_intersection(g_maker_tree.vertices[bspface->i_v2],
                                      g_maker_tree.vertices[bspface->i_v0],
                                      *splitV, *splitN, &vnew1);
                    //vbs_printf("%i, %i\n", side0, side1);
                    find_intersection(g_maker_tree.vertices[bspface->i_v0],
                                      g_maker_tree.vertices[bspface->i_v1],
                                      *splitV, *splitN, &vnew0);
                    
                    fnew_front.i_v0 = face->i_v0;
                    fnew_front.i_v1 = i_vnew0;
                    fnew_front.i_v2 = i_vnew1;
                    fnew_back0.i_v0 = i_vnew0;
                    fnew_back0.i_v1 = face->i_v2;
                    fnew_back0.i_v2 = i_vnew1;
                    fnew_back1.i_v0 = face->i_v1;
                    fnew_back1.i_v1 = face->i_v2;
                    fnew_back1.i_v2 = i_vnew0;
                }
                else {
                    dibassert(false);
                }

                // finish filling out new faces
                fnew_front.from_cut = true;
                fnew_back0.from_cut = true;
                fnew_back1.from_cut = true;
                fnew_front.i_progenitor_face = face->i_progenitor_face;
                fnew_back0.i_progenitor_face = face->i_progenitor_face;
                fnew_back1.i_progenitor_face = face->i_progenitor_face;

                
                g_maker_tree.num_cuts++;
                // store new vertices in the array
                g_maker_tree.vertices[i_vnew0] = vnew0;
                g_maker_tree.vertices[i_vnew1] = vnew1;

                // store new faces in the array
                i_fnew_front = i_face; // reuse storage
                i_fnew_back0 = g_maker_tree.num_faces++; // new storage
                i_fnew_back1 = g_maker_tree.num_faces++; // new storage
                g_maker_tree.bspfaces[i_fnew_front] = fnew_front;
                g_maker_tree.bspfaces[i_fnew_back0] = fnew_back0;
                g_maker_tree.bspfaces[i_fnew_back1] = fnew_back1;

                // add new face indices to front and back lists
                front.i_faces[i_i_front++] = i_fnew_front;
                back.i_faces[i_i_back++]   = i_fnew_back0;
                back.i_faces[i_i_back++]   = i_fnew_back1;
            }
            break; //break out of last switch case
        }
    }
    //vbs_printf("%zu, %zu\n", i_i_back, back.num_i_faces);
    dibassert(i_i_front == front.num_i_faces);
    dibassert(data.num_front_faces == front.num_i_faces);
    dibassert(i_i_back == back.num_i_faces);
    dibassert(data.num_back_faces == back.num_i_faces);
    i_i_front = 0;
    i_i_back = 0;
    

    zgl_Free(data.relations);
    zgl_Free(data.sides0);
    zgl_Free(data.sides1);
    zgl_Free(data.sides2);
    data.relations = NULL;
    data.sides0 = NULL;
    data.sides1 = NULL;
    data.sides2 = NULL;
    
    if (are_fronts) {
        vbs_printf("Front faces: %zu\n", front.num_i_faces);
        //print_int_array_horizontal(front.num_i_faces, front.i_faces);
    }
    else {
        vbs_printf("No fronts\n");
    }

    if (are_backs) {
        vbs_printf("Back faces: %zu\n", back.num_i_faces);
        //print_int_array_horizontal(back.num_i_faces, back.i_faces);
    }
    else {
        vbs_printf(("No backs\n"));
    }
    
    if (are_fronts) {
        vbs_putchar('\n');
        vbs_printf("BSP Front Recursion Step Begin\n");
        BSP_recursion(front);

        zgl_Free(front.i_faces);
        front.i_faces = NULL;
    } 
    if (are_backs) {
        vbs_putchar('\n');
        vbs_printf("BSP Back Recursion Step Begin\n");
        BSP_recursion(back);
	
        zgl_Free(back.i_faces);
        back.i_faces = NULL;
    }
}


// TODO: For a multiply-subdivided triangle, collect all subvertices together with the barycentric coordinates and/or u,v coordinates. Update 2023-05-11: No idea what this TODO was supposed to mean.
static void compute_texel
( WitPoint _V0, WitPoint _V1, WitPoint _V2,
  Fix32Point_2D TV0, Fix32Point_2D TV1, Fix32Point_2D TV2,
  WitPoint _P0, WitPoint _P1, WitPoint _P2,
  Fix32Point_2D *TP0, Fix32Point_2D *TP1, Fix32Point_2D *TP2) {

    Fix64Point V0, V1, V2, P0, P1, P2;

    x_of(V0) = x_of(_V0);
    y_of(V0) = y_of(_V0);
    z_of(V0) = z_of(_V0);
    x_of(V1) = x_of(_V1);
    y_of(V1) = y_of(_V1);
    z_of(V1) = z_of(_V1);
    x_of(V2) = x_of(_V2);
    y_of(V2) = y_of(_V2);
    z_of(V2) = z_of(_V2);
    x_of(P0) = x_of(_P0);
    y_of(P0) = y_of(_P0);
    z_of(P0) = z_of(_P0);
    x_of(P1) = x_of(_P1);
    y_of(P1) = y_of(_P1);
    z_of(P1) = z_of(_P1);
    x_of(P2) = x_of(_P2);
    y_of(P2) = y_of(_P2);
    z_of(P2) = z_of(_P2);
    
    fix64_t area = triangle_area_3D(V0, V1, V2);
    dibassert(area != 0);
    // grab data from vertex buffers.        

    fix64_t B0, B1, B2;
        
    // texel 0
    B0 = (triangle_area_3D(P0, V1, V2)<<16)/area;
    B1 = (triangle_area_3D(P0, V0, V2)<<16)/area;
    B2 = (1<<16) - B0 - B1;
    TP0->x = (fix32_t)((B0*TV0.x + B1*TV1.x + B2*TV2.x)>>16);
    TP0->y = (fix32_t)((B0*TV0.y + B1*TV1.y + B2*TV2.y)>>16);

    // texel 1
    B0 = (triangle_area_3D(P1, V1, V2)<<16)/area;
    B1 = (triangle_area_3D(P1, V0, V2)<<16)/area;
    B2 = (1<<16) - B0 - B1;
    TP1->x = (fix32_t)((B0*TV0.x + B1*TV1.x + B2*TV2.x)>>16);
    TP1->y = (fix32_t)((B0*TV0.y + B1*TV1.y + B2*TV2.y)>>16);
                
    // texel 2
    B0 = (triangle_area_3D(P2, V1, V2)<<16)/area;
    B1 = (triangle_area_3D(P2, V0, V2)<<16)/area;
    B2 = (1<<16) - B0 - B1;
    TP2->x = (fix32_t)((B0*TV0.x + B1*TV1.x + B2*TV2.x)>>16);
    TP2->y = (fix32_t)((B0*TV0.y + B1*TV1.y + B2*TV2.y)>>16);
}


#define NODESIDE_FRONT 0
#define NODESIDE_BACK 1
#define NODESIDE_ROOT -1

#define BSP_FRONT_MASK 0x3
static void construct_final_tree_depth_first(Maker_BSPNode *maker_node, BSPNode *final_parent, int side, uint16_t depth) {
    if (maker_node == NULL)
        return;
    Maker_BSPFace *maker_face = g_maker_tree.bspfaces + maker_node->i_bspface;
    
    BSPNode *final_node = g_p_final_tree->nodes + g_i_node;
    BSPFace *final_face = g_p_final_tree->bspfaces + g_i_face;
    final_node->parent = final_parent;
    if (side == NODESIDE_FRONT) {
        final_parent->front = final_node;
        final_face->bspflags = 0 | BSP_FRONT_MASK;
    }
    else if (side == NODESIDE_BACK) {
        final_parent->back = final_node;
        final_face->bspflags = 0;
    }
    final_node->i_bspface = g_i_face;
    final_node->bspface = final_face;
    final_node->splitV = maker_node->splitV;
    final_node->splitN = maker_node->splitN;
    g_i_node++;
    g_i_face++;
    
    Face const *og_face = &g_p_progenitor_faces[maker_face->i_progenitor_face];
    final_face->progenitor_face = og_face;
    final_face->i_progenitor_face = maker_face->i_progenitor_face;
    final_face->face.i_v0 = maker_face->i_v0;
    final_face->face.i_v1 = maker_face->i_v1;
    final_face->face.i_v2 = maker_face->i_v2;
    final_face->face.flags = 0;
    final_face->face.normal = og_face->normal;
    final_face->face.i_tex = og_face->i_tex;
    final_face->depth = depth;
        
    if (maker_face->from_cut) {
        compute_texel
        (g_p_progenitor_vertices[og_face->i_v0],
         g_p_progenitor_vertices[og_face->i_v1],
         g_p_progenitor_vertices[og_face->i_v2],
         og_face->tv0,
         og_face->tv1,
         og_face->tv2,
         g_p_final_tree->vertices[final_face->face.i_v0],
         g_p_final_tree->vertices[final_face->face.i_v1],
         g_p_final_tree->vertices[final_face->face.i_v2],
         &final_face->face.tv0,
         &final_face->face.tv1,
         &final_face->face.tv2);
    }
    else {
        final_face->face.tv0 = og_face->tv0;
        final_face->face.tv1 = og_face->tv1;
        final_face->face.tv2 = og_face->tv2;
    }
    
    final_face->face.brightness = og_face->brightness;
    // TODO: Is it really worth it to locally store all the inherited face data?

    construct_final_tree_depth_first(maker_node->front, final_node,
                                     NODESIDE_FRONT, depth+1);
    construct_final_tree_depth_first(maker_node->back, final_node,
                                     NODESIDE_BACK, depth+1);
}

void destroy_BSPTree(BSPTree *p_tree) {
    if (p_tree->vertices)
        memset(p_tree->vertices, 0, p_tree->num_vertices*sizeof(*p_tree->vertices));
    if (p_tree->nodes)
        memset(p_tree->nodes, 0, p_tree->num_nodes*sizeof(*p_tree->nodes));
    if (p_tree->bspfaces)
        memset(p_tree->bspfaces, 0, p_tree->num_bspfaces*sizeof(*p_tree->bspfaces));
    
    zgl_Free(p_tree->vertices);
    zgl_Free(p_tree->nodes);
    zgl_Free(p_tree->bspfaces);

    p_tree->vertices = NULL;
    p_tree->nodes = NULL;
    p_tree->bspfaces = NULL;
}



















/* Choosing a splitting plane */

// A splitting plane is less desirable if it:
// - creates too many subdivisions;
// - is severely unbalanced;
// - subdivides a non-nearly-degenerate face into a nearly-degenerate face;
// - subdivides a face into a near-zero-area face;
// - has one side entirely inaccessible during normal gameplay


/** Returns TRUE if current_data is optimal and should replace optimal_data */
static void csd_mincut1_balance2(bool *early_exit, bool *replace, SplittingData *current, SplittingData *optimal) {
    // TODO: find minimum cut. Among all the splitting planes with the
    // minimum number of cut, choose the most balanced.
    
    *replace = false;
    
    if (current->num_cuts == optimal->num_cuts) {
        if (optimal->balance < current->balance) {
            *replace = true;
        }
    }
    else if (current->num_cuts < optimal->num_cuts) {
        *replace = true;
    }

    *early_exit = false;
}

static void csd_balance1_mincut2(bool *early_exit, bool *replace, SplittingData *current, SplittingData *optimal) {
    *replace = false;
    
    if (ABS(optimal->balance - current->balance) < 0.01) {
        if (current->num_cuts == optimal->num_cuts) {
            *replace = true;
        }
    }
    else if (optimal->balance < current->balance) {
        *replace = true;
    }

    *early_exit = false;
}

static void csd_trivial(bool *early_exit, bool *replace, SplittingData *current, SplittingData *optimal) {
    (void)current;
    (void)optimal;
    
    *replace = true;
    *early_exit = true;
}

static void choose_splitting_plane(size_t num_i_faces, size_t *i_faces, SplittingData *out_data) {
    SplittingData data__A;
    SplittingData data__B;

    dibassert(num_i_faces > 0);
    data__A.relations = zgl_Malloc(num_i_faces*sizeof(*data__A.relations));
    data__A.sides0 = zgl_Malloc(num_i_faces*sizeof(*data__A.sides0));
    data__A.sides1 = zgl_Malloc(num_i_faces*sizeof(*data__A.sides1));
    data__A.sides2 = zgl_Malloc(num_i_faces*sizeof(*data__A.sides2));
    
    data__B.relations = zgl_Malloc(num_i_faces*sizeof(*data__B.relations));
    data__B.sides0 = zgl_Malloc(num_i_faces*sizeof(*data__B.sides0));
    data__B.sides1 = zgl_Malloc(num_i_faces*sizeof(*data__B.sides1));
    data__B.sides2 = zgl_Malloc(num_i_faces*sizeof(*data__B.sides2));

    SplittingData *optimal_data = &data__A;
    SplittingData *current_data = &data__B;
    SplittingData *temp_swap;
    
    for (size_t i_i_splitface = 0; i_i_splitface < num_i_faces; i_i_splitface++) {
        size_t i_curr_split = i_faces[i_i_splitface];
            
        WitPoint const
            * const split_V0 = &g_p_progenitor_vertices[g_p_progenitor_faces[g_maker_tree.bspfaces[i_curr_split].i_progenitor_face].i_v0],
            * const split_V1 = &g_p_progenitor_vertices[g_p_progenitor_faces[g_maker_tree.bspfaces[i_curr_split].i_progenitor_face].i_v1],
            * const split_V2 = &g_p_progenitor_vertices[g_p_progenitor_faces[g_maker_tree.bspfaces[i_curr_split].i_progenitor_face].i_v2],
            * const split_N = &g_p_progenitor_faces[g_maker_tree.bspfaces[i_curr_split].i_progenitor_face].normal;
    
        size_t num_front_faces = 0, num_back_faces = 0;
        size_t num_cuts = 0;

        // Test & tally the spatial relationship between the splitter and all faces.
        
        for (size_t i_i_face = 0; i_i_face < num_i_faces; i_i_face++) {
            size_t i_face = i_faces[i_i_face];
            if (i_face == i_curr_split) {
                current_data->relations[i_i_face] = SR_IGNORE;
                continue;
            }
        
            test_spatial_relation(i_face,
                                  *split_V0, *split_V1, *split_V2, *split_N,
                                  current_data->sides0 + i_i_face,
                                  current_data->sides1 + i_i_face,
                                  current_data->sides2 + i_i_face,
                                  current_data->relations + i_i_face);
        
            switch (current_data->relations[i_i_face]) {
            case SR_IGNORE:
                dibassert(false);
                break;
            case SR_FRONT:
                num_front_faces++;
                break;
            case SR_BACK:
                num_back_faces++;
                break;
            case SR_COPLANAR:
                // For now, coplanar faces will be treated as if in front.
                num_front_faces++;
                break;
            case SR_CUT_TYPE1:
                num_cuts++;
                num_front_faces++;
                num_back_faces++;
                break;
            case SR_CUT_TYPE2F:
                num_cuts++;
                num_front_faces += 2;
                num_back_faces++;
                break;
            case SR_CUT_TYPE2B:
                num_cuts++;
                num_front_faces++;
                num_back_faces += 2;
                break;
            default:
                dibassert(false);
            }
        }

        double balance;
        if (num_front_faces == 0) {
            balance = 0;
        }
        else if (num_back_faces == 0) {
            balance = 0;
        }
        else {
            if (num_front_faces <= num_back_faces) {
                balance = (double)num_front_faces / (double)num_back_faces;
            }
            else {
                balance = (double)num_back_faces / (double)num_front_faces;
            }
        }

        current_data->i_split = i_curr_split;
        current_data->balance = balance;
        current_data->num_cuts = num_cuts;
        current_data->num_front_faces = num_front_faces;
        current_data->num_back_faces = num_back_faces;


        if (i_i_splitface == 0) { // trivially accept first face as optimal, to
                                  // seed the subsequent iterations.
            optimal_data->i_split = current_data->i_split;
            optimal_data->balance = current_data->balance;
            optimal_data->num_cuts = current_data->num_cuts;
            optimal_data->num_front_faces = current_data->num_front_faces;
            optimal_data->num_back_faces = current_data->num_back_faces;
            memcpy(optimal_data->relations, current_data->relations,
                   num_i_faces*sizeof(*optimal_data->relations));
            memcpy(optimal_data->sides0, current_data->sides0,
                   num_i_faces*sizeof(*optimal_data->sides0));
            memcpy(optimal_data->sides1, current_data->sides1,
                   num_i_faces*sizeof(*optimal_data->sides1));
            memcpy(optimal_data->sides2, current_data->sides2,
                   num_i_faces*sizeof(*optimal_data->sides2));
        }

        bool early_exit, replace;
        compare_splitting_data(&early_exit, &replace, current_data, optimal_data);

        if (replace) {
            temp_swap = current_data;
            current_data = optimal_data;
            optimal_data = temp_swap;
        }
        
        if (early_exit) {
            break;
        }

        continue;
    } /* end of splitter candidates loop */


    out_data->i_split = optimal_data->i_split;
    out_data->balance = optimal_data->balance;
    out_data->num_cuts = optimal_data->num_cuts;
    out_data->num_front_faces = optimal_data->num_front_faces;
    out_data->num_back_faces = optimal_data->num_back_faces;
    memcpy(out_data->relations, optimal_data->relations,
           num_i_faces*sizeof(*out_data->relations));
    memcpy(out_data->sides0, optimal_data->sides0,
           num_i_faces*sizeof(*out_data->sides0));
    memcpy(out_data->sides1, optimal_data->sides1,
           num_i_faces*sizeof(*out_data->sides1));
    memcpy(out_data->sides2, optimal_data->sides2,
           num_i_faces*sizeof(*out_data->sides2));

    zgl_Free(data__A.relations);
    zgl_Free(data__A.sides0);
    zgl_Free(data__A.sides1);
    zgl_Free(data__A.sides2);
    
    zgl_Free(data__B.relations);
    zgl_Free(data__B.sides0);
    zgl_Free(data__B.sides1);
    zgl_Free(data__B.sides2);
}






static uint8_t BSP_sideof(WitPoint V, WitPoint planeV, WitPoint planeN) {
    WitPoint point_to_plane;
    x_of(point_to_plane) = x_of(planeV) - x_of(V);
    y_of(point_to_plane) = y_of(planeV) - y_of(V);
    z_of(point_to_plane) = z_of(planeV) - z_of(V);
    
    fix64_t S =
        (fix64_t)x_of(point_to_plane)*(fix64_t)x_of(planeN) +
        (fix64_t)y_of(point_to_plane)*(fix64_t)y_of(planeN) +
        (fix64_t)z_of(point_to_plane)*(fix64_t)z_of(planeN);

    if (S == 0)
        return SIDE_MID;
    else if (S < 0)
        return SIDE_FRONT;
    else
        return SIDE_BACK;
}



static void test_spatial_relation
(size_t i_bspface, WitPoint planeV0, WitPoint planeV1, WitPoint planeV2,
 WitPoint planeN, uint8_t *side0, uint8_t *side1, uint8_t *side2,
 SpatialRelation *sr) {
    WitPoint pt0, pt1, pt2;

    Maker_BSPFace const *bspface = &g_maker_tree.bspfaces[i_bspface];
    pt0 = g_maker_tree.vertices[bspface->i_v0];
    pt1 = g_maker_tree.vertices[bspface->i_v1];
    pt2 = g_maker_tree.vertices[bspface->i_v2];

    /*
    Triangle tri = {
        .v0 = planeV0,
        .v1 = planeV1,
        .v2 = planeV2,
    };
    uint8_t f0 = *side0 = sideof(pt0, tri);
    uint8_t f1 = *side1 = sideof(pt1, tri);
    uint8_t f2 = *side2 = sideof(pt2, tri);
    */
    
    uint8_t f0 = *side0 = BSP_sideof(pt0, planeV0, planeN);
    uint8_t f1 = *side1 = BSP_sideof(pt1, planeV0, planeN);
    uint8_t f2 = *side2 = BSP_sideof(pt2, planeV0, planeN);

    //vbs_printf("(f0, f1, f2): (%i, %i, %i)\n", f0, f1, f2);
    
    if ((f0&f1&f2)&SIDE_MID)
        // every point the same relation, and that relation is MID
        *sr = SR_COPLANAR;
    // now at least one non-MID
    else if ( ! ((f0|f1|f2)&SIDE_BACK))
        // one or two points MID, the other FRONT
        // equiv., all three are not BACK
        *sr = SR_FRONT;
    // now at least one BACK
    else if ( ! ((f0|f1|f2)&SIDE_FRONT))
        // one or two points MID, the other BACK
        // equiv., all three are not FRONT
        *sr = SR_BACK;
    // now at least one FRONT and at least one BACK
    else if ((f0|f1|f2)&SIDE_MID)
        *sr = SR_CUT_TYPE1;
    // now either two FRONT and one BACK, or two BACK and one FRONT 
    else if ((f0&SIDE_FRONT)+(f1&SIDE_FRONT)+(f2&SIDE_FRONT) > SIDE_FRONT)
        *sr = SR_CUT_TYPE2F;
    else if ((f0&SIDE_BACK)+(f1&SIDE_BACK)+(f2&SIDE_BACK) > SIDE_BACK)
        *sr = SR_CUT_TYPE2B;
    else
        dibassert(false);
}


static Maker_BSPNode *create_Maker_BSPNode() {
    num_alloc_nodes++;
    Maker_BSPNode *node = zgl_Malloc(sizeof(*node));

    node->parent = NULL;
    node->front = NULL;
    node->back = NULL;
    node->i_bspface = 0;

    return node;
}

static void delete_Maker_BSPNode(Maker_BSPNode *p_node) {
    //vbs_printf("Deleting BSP node.\n");
    num_alloc_nodes--;
    
    if (p_node->front != NULL) {
        delete_Maker_BSPNode(p_node->front);
    }

    if (p_node->back != NULL) {
        delete_Maker_BSPNode(p_node->back);
    }

    zgl_Free(p_node);
    p_node = NULL;
}

static void delete_Maker_BSPTree(Maker_BSPTree *p_tree) {
    //vbs_printf("Deleting BSP tree.\n");
    
    if (p_tree->root != NULL) {
        delete_Maker_BSPNode(p_tree->root);
    }

    memset(p_tree, 0, sizeof (*p_tree));
}





/* Structure-printing */

/*
static void print_int_array_horizontal(size_t num, size_t *arr) {
    vbs_printf("[ ");
    for (size_t i = 0; i < num; i++) {
        vbs_printf("%zu ", arr[i]);
    }

    vbs_printf("]\n");
}
*/

#ifdef BSP_VERBOSITY

static void print_vertices(size_t num, WitPoint const *arr) {
    for (size_t i = 0; i < num; i++) {
        vbs_printf("[%zu] = %s\n", i, fmt_Fix32Point(arr[i]));
    }
}

static void print_Faces(size_t num, Face const *arr) {
    for (size_t i = 0; i < num; i++) {
        vbs_printf("[%zu] = (%zu, %zu, %zu)\n", i, arr[i].i_v0, arr[i].i_v1, arr[i].i_v2);
    }
}


static void print_Maker_BSPTree_diagram(Maker_BSPNode *node, size_t level) {
    if (node == NULL)
        return;
    
    for (size_t i = 0; i < level; i++)
	    vbs_printf(i == level - 1 ? "|-" : "  ");
    vbs_printf("%zu\n", node->i_bspface);
	
    print_Maker_BSPTree_diagram(node->front, level + 1);
    print_Maker_BSPTree_diagram( node->back, level + 1);
}

static void print_Maker_BSPTree(Maker_BSPTree *tree) {
    vbs_printf(" Maker BSP Summary \n");
    vbs_printf("+-----------------+\n");
    vbs_printf("# Vertices: %zu\n", tree->num_vertices);
    vbs_printf("# BSPFaces: %zu\n", tree->num_faces);
    vbs_printf("    # Cuts: %zu\n", tree->num_cuts);
}

static void print_Maker_BSPFaces(size_t num, Maker_BSPFace *arr) {
    for (size_t i_face = 0; i_face < num; i_face++) {
        vbs_printf("F[%zu] = V[%zu] -> V[%zu] -> V[%zu];  OGF[%zu]\n", i_face, arr[i_face].i_v0, arr[i_face].i_v1, arr[i_face].i_v2, arr[i_face].i_progenitor_face);
    }
}


static void print_BSPTree_diagram(BSPNode *node, size_t level) {
    if (node == NULL)
        return;
    
    for (size_t i = 0; i < level; i++)
	    vbs_printf(i == level - 1 ? "|-" : "  ");
    vbs_printf("F[%zu]\n", node->i_bspface);
	
    print_BSPTree_diagram(node->front, level + 1);
    print_BSPTree_diagram(node->back,  level + 1);
}

static void print_BSPTree(BSPTree *tree) {
    if (tree == NULL) return;
    
    vbs_printf(" BSP Summary\n");
    vbs_printf("+-----------+\n");
    vbs_printf("# Progenitor Vertices: %zu\n", tree->num_progenitor_vertices);
    vbs_printf("   # Progenitor Faces: %zu\n", tree->num_progenitor_faces);
    vbs_printf("           # Vertices: %zu\n", tree->num_vertices);
    vbs_printf("              # Nodes: %zu\n", tree->num_nodes);
    vbs_printf("              # Faces: %zu\n", tree->num_bspfaces);
    vbs_printf("               # Cuts: %zu\n", tree->num_cuts);
}

static void print_BSPFaces(size_t num, BSPFace *arr) {
    for (size_t i_bspface = 0; i_bspface < num; i_bspface++) {
        vbs_printf("F[%zu] = V[%zu] -> V[%zu] -> V[%zu];  OGF[%zu]\n", i_bspface, arr[i_bspface].face.i_v0, arr[i_bspface].face.i_v1, arr[i_bspface].face.i_v2, arr[i_bspface].i_progenitor_face);
    }
}
#endif

/* Geometry functions. TODO: Put in common EPM geometry */

#define FIX32_dbl(x) ((double)(x)/(1LL<<32))

static bool find_intersection
(WitPoint lineV0, WitPoint lineV1,
 WitPoint planeV, WitPoint planeN,
 WitPoint *out_sect) {
    // if both endpoints are the same, we should not be here in the first place
    dibassert( ! (x_of(lineV0) == x_of(lineV1) &&
                  y_of(lineV0) == y_of(lineV1) &&
                  z_of(lineV0) == z_of(lineV1)));
    
    vbs_printf("planeV: (%lf, %lf, %lf)\n",
               FIX_dbl(x_of(planeV)),
               FIX_dbl(y_of(planeV)),
               FIX_dbl(z_of(planeV)));
    vbs_printf("planeN: (%lf, %lf, %lf)\n",
               FIX_dbl(x_of(planeN)),
               FIX_dbl(y_of(planeN)),
               FIX_dbl(z_of(planeN)));

    vbs_printf("lineV0: (%lf, %lf, %lf)\n",
               FIX_dbl(x_of(lineV0)),
               FIX_dbl(y_of(lineV0)),
               FIX_dbl(z_of(lineV0)));
    vbs_printf("lineV1: (%lf, %lf, %lf)\n",
               FIX_dbl(x_of(lineV1)),
               FIX_dbl(y_of(lineV1)),
               FIX_dbl(z_of(lineV1)));
    
    WitPoint lineDir;
    x_of(lineDir) = x_of(lineV1) - x_of(lineV0);
    y_of(lineDir) = y_of(lineV1) - y_of(lineV0);
    z_of(lineDir) = z_of(lineV1) - z_of(lineV0);
    vbs_printf("lineDir: (%lf, %lf, %lf)\n",
               FIX_dbl(x_of(lineDir)),
               FIX_dbl(y_of(lineDir)),
               FIX_dbl(z_of(lineDir)));
    
    WitPoint line_to_plane;
    x_of(line_to_plane) = x_of(planeV) - x_of(lineV0);
    y_of(line_to_plane) = y_of(planeV) - y_of(lineV0);
    z_of(line_to_plane) = z_of(planeV) - z_of(lineV0);
    vbs_printf("l_to_p: (%lf, %lf, %lf)\n",
               FIX_dbl(x_of(line_to_plane)),
               FIX_dbl(y_of(line_to_plane)),
               FIX_dbl(z_of(line_to_plane)));

    // numer is essentially the "sideof".

    // This is a temporary violation my own self-imposed constraint of not using
    // floating point types. But I simply haven't been able to make this
    // function work in fixed point... YET. Precision is *extremely* important
    // here.
    double numer = FIX32_dbl((fix64_t)x_of(line_to_plane)*(fix64_t)x_of(planeN) +
                             (fix64_t)y_of(line_to_plane)*(fix64_t)y_of(planeN) +
                             (fix64_t)z_of(line_to_plane)*(fix64_t)z_of(planeN));
    double denom = FIX32_dbl((fix64_t)x_of(lineDir)*(fix64_t)x_of(planeN) +
                             (fix64_t)y_of(lineDir)*(fix64_t)y_of(planeN) +
                             (fix64_t)z_of(lineDir)*(fix64_t)z_of(planeN));
    
    vbs_printf("(numer, denom): (%lf, %lf)\n", numer, denom);
    dibassert(denom != 0.0);
    
    double T = numer/denom;
    vbs_printf("     T: %lf\n", T);

    dibassert(T > -0.0009);
    dibassert(T < 1.0009);

    if (ABS(T-1.0) < 0.001)
        T = 1.0;
    else if (ABS(T) < 0.001)
        T = 0.0;
    
    x_of(*out_sect) = x_of(lineV0) + (fix32_t)FIX_MUL(dbl_FIX(T), x_of(lineDir));
    y_of(*out_sect) = y_of(lineV0) + (fix32_t)FIX_MUL(dbl_FIX(T), y_of(lineDir));
    z_of(*out_sect) = z_of(lineV0) + (fix32_t)FIX_MUL(dbl_FIX(T), z_of(lineDir));
    
    vbs_putchar('\n');
        
    return true;
}

#if 0
static bool find_intersection2
(size_t i_v0, size_t i_v1,
 WitPoint V0,  WitPoint normal,
 WitPoint *out_sect) {
    vbs_printf("    V0: (%8X, %8X, %8X)\n", x_of(V0), y_of(V0), z_of(V0));
    vbs_printf("Normal: (%8X, %8X, %8X)\n", x_of(normal), y_of(normal), z_of(normal));
    
    WitPoint
        L0 = internal_tree.vertices[i_v0],
        L1 = internal_tree.vertices[i_v1];

    vbs_printf("Segment %zu -> %zu:\n", i_v0, i_v1);
    vbs_printf("    L0: (%8X, %8X, %8X)\n", x_of(L0), y_of(L0), z_of(L0));
    vbs_printf("    L1: (%8X, %8X, %8X)\n", x_of(L1), y_of(L1), z_of(L1));

    WitPoint dL;
    x_of(dL) = x_of(L1) - x_of(L0);
    y_of(dL) = y_of(L1) - y_of(L0);
    z_of(dL) = z_of(L1) - z_of(L0);
    vbs_printf("    dL: (%8X, %8X, %8X)\n", x_of(dL), y_of(dL), z_of(dL));
    
    WitPoint P;
    x_of(P) = x_of(V0) - x_of(L0);
    y_of(P) = y_of(V0) - y_of(L0);
    z_of(P) = z_of(V0) - z_of(L0);
    vbs_printf("     P: (%8X, %8X, %8X)\n", x_of(P), y_of(P), z_of(P));

    fix64_t N = ((fix64_t)x_of(P)*(fix64_t)x_of(normal) +
                 (fix64_t)y_of(P)*(fix64_t)y_of(normal) +
                 (fix64_t)z_of(P)*(fix64_t)z_of(normal));
    fix64_t D = ((fix64_t)x_of(dL)*(fix64_t)x_of(normal) +
                 (fix64_t)y_of(dL)*(fix64_t)y_of(normal) +
                 (fix64_t)z_of(dL)*(fix64_t)z_of(normal));
    
    vbs_printf("(N, D): (%16lX, %16lX)\n", N, D);
    dibassert(D != 0);
    
    fix64_t T = (N<<16)/D;
    vbs_printf("     T: %lX\n", T);
    
    dibassert(T >= 0);
    dibassert(T <= 1<<16);
    
    x_of(*out_sect) = x_of(L0) + FIX_MUL(T, x_of(dL));
    y_of(*out_sect) = y_of(L0) + FIX_MUL(T, y_of(dL));
    z_of(*out_sect) = z_of(L0) + FIX_MUL(T, z_of(dL));
    
    vbs_putchar('\n');
    
    return true;
}
#endif

static int g_max_node_depth;
static int g_num_leaves;
static int g_total_node_depth;
static int g_total_leaf_depth;
#include <math.h>
void measure_BSPTree(BSPTree *p_tree) {
    if (p_tree == NULL) return;

    g_max_node_depth = 0;
    g_num_leaves = 0;
    g_total_node_depth = 0;
    g_total_leaf_depth = 0;

    measure_BSPTree_recursion(&p_tree->nodes[0], 0);
    
    p_tree->max_node_depth = g_max_node_depth;
    p_tree->num_leaves = g_num_leaves;
    p_tree->avg_node_depth = (double)g_total_node_depth / (double)p_tree->num_nodes;
    p_tree->avg_leaf_depth = (double)g_total_leaf_depth / (double)g_num_leaves;
    p_tree->balance = p_tree->avg_leaf_depth / log2((double)p_tree->num_nodes);
    
    snprintf(bigbuf, BIGBUF_LEN,
             "      BSP Profile      \n"
             "+---------------------+\n"
             "# Progenitor Vertices | %zu\n"
             "           # Vertices | %zu\n"
             "                      |    \n"
             "   # Progenitor Faces | %zu\n"
             "              # Faces | %zu\n"
             "               # Cuts | %zu\n"
             "                      |    \n"
             "              # Nodes | %zu\n"
             "             # Leaves | %zu\n"
             "            Max Depth | %zu\n"
             "       Avg Node Depth | %lf\n"
             "       Avg Leaf Depth | %lf\n"
             "              Balance | %lf\n",
             p_tree->num_progenitor_vertices,
             p_tree->num_vertices,
             p_tree->num_progenitor_faces,
             p_tree->num_bspfaces,
             p_tree->num_cuts,
             p_tree->num_nodes,
             p_tree->num_leaves,
             p_tree->max_node_depth,
             p_tree->avg_node_depth,
             p_tree->avg_leaf_depth,
             p_tree->balance);

    puts(bigbuf);
}

void measure_BSPTree_recursion(BSPNode *node, int node_depth) {
    if (node == NULL) return;
    
    dibassert(node->bspface->depth == node_depth);
    g_max_node_depth = MAX(node_depth, g_max_node_depth);
    g_total_node_depth += node_depth;
    if (node->front == NULL && node->back == NULL) {
        g_num_leaves++;
        g_total_leaf_depth += node_depth;
    }

    measure_BSPTree_recursion(node->front, node_depth+1);
    measure_BSPTree_recursion(node->back, node_depth+1);
}

static void CMDH_measure_BSPTree(int argc, char **argv, char *output_str) {
    extern BSPTree bsp;
    measure_BSPTree(&bsp);
    
    strcpy(output_str, bigbuf);
}

#include "src/input/command.h"
epm_Command const CMD_measure_BSPTree = {
    .name = "measure_BSPTree",
    .argc_min = 1,
    .argc_max = 1,
    .handler = CMDH_measure_BSPTree,
};

