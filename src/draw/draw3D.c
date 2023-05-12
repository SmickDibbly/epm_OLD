#include "src/draw/draw.h"
#include "src/draw/draw_triangle.h"
#include "src/draw/draw3D.h"
#include "src/draw/text.h"
#include "src/draw/textures.h"
#include "src/draw/colors.h"
#include "src/draw/clip.h"
#include "src/draw/draw3D_BSP.h"

#include "src/world/world.h"
#include "src/world/selection.h"

#undef LOG_LABEL
#define LOG_LABEL "DRAW3D"

#define WIREFLAGS_DOTTED 0x01
#define WIREFLAGS_NO_VERTICES 0x02

FrustumSettings frset_persp = {
    .type = PROJ_PERSPECTIVE,
    .persp_half_fov = DEFAULT_HALF_FOV,
    .D = fixify(1),
    .F = fixify(1),
    .B = fixify(2048),
};

FrustumSettings frset_ortho = {
    .type = PROJ_ORTHOGRAPHIC,
    .ortho_W = fixify(512),
    .D = fixify(0),
    .F = -fixify(1024),
    .B = fixify(1024),
};

static Frustum g_frustum_persp;
static Frustum g_frustum_ortho;
static Frustum *g_p_frustum = &g_frustum_persp;

bool g_farclip = true;

#define area(a, b, c) ((fix64_t)((c).x - (a).x) * (fix64_t)((b).y - (a).y) - (fix64_t)((c).y - (a).y) * (fix64_t)((b).x - (a).x))

DrawSettings g_settings = {
    .i_draw = 3,
    .proj_type = PROJ_PERSPECTIVE,
    .lighting = true,
    .wireframe = false,
    .BSP_visualizer = false,
};

int const *const p_proj_type = &g_settings.proj_type;
bool const *const p_lighting = &g_settings.lighting;
bool const *const p_wireframe = &g_settings.wireframe;

bool no_bsp = false;

static bool (*fn_SegClip3D)
(fix64_t x0, fix64_t y0, fix64_t z0,
 fix64_t x1, fix64_t y1, fix64_t z1,
 Frustum *fr,
 Fix64Point *out_c0, Fix64Point *out_c1);

static void (*fn_TriClip3D)
(Fix64Point const *const v0,
 Fix64Point const *const v1,
 Fix64Point const *const v2,
 Frustum *fr,
 Fix64Point *out_poly, size_t *out_count, bool *out_new);

/*
void (*fn_visify_and_project)(Window const *const win, Fix64Point vertex, uint8_t *out_vis, DepthPixel *out_scr);
*/

#define VERTEX_BUFFER_SIZE 4096
static TransformedVertex g_vbuf[VERTEX_BUFFER_SIZE] = {0};

static epm_Result epm_ConstructFrustum(Frustum *fr, FrustumSettings *settings);
extern void epm_UpdateFrustumHeight(Window *win);

extern void transform_vertices
(Window const *const win, Frustum *const fr, int width,
 size_t const num_vertices, void const *const vertices,
 TransformedVertex *out_vbuf);

static void transform_Mesh
(Window const *const win, Frustum *const fr, int width, Mesh *mesh,
 TransformedVertex * out_vbuf);

extern void subdivide3D_and_draw_face(zgl_PixelArray *scr_p, Window *win, Face *face, TransformedVertex const *const vbuf);

extern void subdivide3D(Window const *const win, Fix64Point V0, Fix64Point V1, Fix64Point V2, size_t *out_num_subvs, Fix64Point *out_subvs, TransformedVertex *out_subvbuf, bool *out_new);

static void draw_bigbox
(zgl_PixelArray *scr_p, Window *win);

static void draw_faces
(zgl_PixelArray *scr_p, Window *win, FaceSet *fset, TransformedVertex const *const vbuf);

extern void draw_face
(zgl_PixelArray *scr_p, Window *win, Face *face, TransformedFace *tf);

extern void draw_wireframe_face
(zgl_PixelArray *scr_p, Window *win, TransformedWireFace *twf, zgl_Color color);

static void draw_wireframe_faces
(zgl_PixelArray *scr_p, Window *win, FaceSet *fset, zgl_Color color, TransformedVertex const *const vbuf);

static void draw_wireframe_edges
(zgl_PixelArray *scr_p, Window *win, Frustum *fr, EdgeSet *eset, zgl_Color color, TransformedVertex const *const vbuf, uint8_t flags);

static void draw_vertices(zgl_PixelArray *scr_p, Window *win, size_t num_vertices, TransformedVertex const *const vbuf);

static uint8_t (*fn_compute_viscode)(Fix64Point vertex);
static uint8_t compute_viscode_ortho(Fix64Point vertex);
static uint8_t compute_viscode_persp(Fix64Point vertex);

static void (*fn_eyespace_to_screen)(Window const *win, TransformedVertex *tv);
static void eyespace_to_screen_ortho(Window const *win, TransformedVertex *tv);
static void eyespace_to_screen_persp(Window const *win, TransformedVertex *tv);


/* -------------------------------------------------------------------------- */


epm_Result init_Draw3D(void) {
    epm_ConstructFrustum(&g_frustum_persp, &frset_persp);
    epm_ConstructFrustum(&g_frustum_ortho, &frset_ortho);
    epm_SetProjectionType(PROJ_PERSPECTIVE);
    
    return EPM_SUCCESS;
}

static epm_Result epm_ConstructFrustum(Frustum *fr, FrustumSettings *settings) {
    fr->type = settings->type;

    if (fr->type == PROJ_PERSPECTIVE) {
        fr->D = settings->D;
        if (fr->D == fixify(1))
            fr->W = fixify(1); // might as well be exact in this nice case
        else
            fr->W = (fix32_t)FIX_MUL(fr->D, tan18(settings->persp_half_fov));
        fr->B = settings->B;
        fr->F = settings->F;

        fr->xmin = -fr->W;
        fr->xmax = +fr->W - 1;
        fr->zmin = fr->F;
        fr->zmax = fr->B;
    }
    else if (fr->type == PROJ_ORTHOGRAPHIC) {
        fr->D = settings->D;
        fr->W = settings->ortho_W;
        fr->B = settings->B;
        fr->F = settings->F;
        
        fr->xmin = -fr->W;
        fr->xmax = +fr->W - 1;
        fr->zmin = fr->F;
        fr->zmax = fr->B;
    }
    
    return EPM_SUCCESS;
}

void epm_UpdateFrustumHeight(Window *win) {
    g_frustum_persp.H = (fix32_t)FIX_MULDIV(g_frustum_persp.W, win->mrect.h, win->mrect.w);
    g_frustum_persp.ymin = -g_frustum_persp.H;
    g_frustum_persp.ymax = +g_frustum_persp.H - 1;

    
    g_frustum_ortho.H = (fix32_t)FIX_MULDIV(g_frustum_ortho.W, win->mrect.h, win->mrect.w);
    g_frustum_persp.ymin = -g_frustum_ortho.H;
    g_frustum_persp.ymax = +g_frustum_ortho.H - 1;
}


static void draw3D_wireframe_mode(zgl_PixelArray *scr_p, Window *win) {
    // TODO: Separate modes for: brush geo, static geo, and BSP geo

    
    transform_vertices(win, g_p_frustum, 32,
                       world.bsp->num_vertices, world.bsp->vertices, g_vbuf);

    WitPoint BSPpos = {{tf.x, tf.y, tf.z}};
    if (g_settings.proj_type == PROJ_ORTHOGRAPHIC) {
        x_of(BSPpos) -= (fix32_t)FIX_MUL((g_p_frustum->D-g_p_frustum->F), x_of(cam.view_vec));
        y_of(BSPpos) -= (fix32_t)FIX_MUL((g_p_frustum->D-g_p_frustum->F), y_of(cam.view_vec));
        z_of(BSPpos) -= (fix32_t)FIX_MUL((g_p_frustum->D-g_p_frustum->F), z_of(cam.view_vec));
    }
    
    /*
    draw_wireframe_edges(scr_p, win, g_p_frustum,
                         &EdgeSet_from_Mesh(*world.bsp), 0x00FF00, g_vbuf, 0);
    */
    
    draw_BSPNode_wireframe(scr_p, win, world.bsp, &world.bsp->nodes[0], BSPpos, g_vbuf);
}

void draw3D(Window *win, zgl_PixelArray *scr_p) {
    reset_rasterizer();

    epm_UpdateFrustumHeight(win);
   
    /* Always draw bigbox, and always draw it first. */
    bool tmp = g_farclip;
    g_farclip = false;
    draw_bigbox(scr_p, win);
    g_farclip = tmp;

    if (world.loaded) {        
        if (g_settings.wireframe) {
            draw3D_wireframe_mode(scr_p, win);
        }
        else {
            if ( ! no_bsp) {
                transform_vertices(win, g_p_frustum, 32,
                                   world.bsp->num_vertices, world.bsp->vertices, g_vbuf);
                WitPoint BSPpos = {{tf.x, tf.y, tf.z}};
                if (g_settings.proj_type == PROJ_ORTHOGRAPHIC) {
                    x_of(BSPpos) -= (fix32_t)FIX_MUL((g_p_frustum->D-g_p_frustum->F), x_of(cam.view_vec));
                    y_of(BSPpos) -= (fix32_t)FIX_MUL((g_p_frustum->D-g_p_frustum->F), y_of(cam.view_vec));
                    z_of(BSPpos) -= (fix32_t)FIX_MUL((g_p_frustum->D-g_p_frustum->F), z_of(cam.view_vec));
                }

                draw_BSPTree(world.bsp, scr_p, win, BSPpos, g_vbuf);
            }
            else if (no_bsp) {
                transform_vertices(win, g_p_frustum, 32,
                                   staticgeo.num_vertices, staticgeo.vertices, g_vbuf);
                draw_faces(scr_p, win, &FaceSet_from_Mesh(staticgeo), g_vbuf);
            }
        }
    }


    // TEMP: Draw a short history of intersection points from mouse picking.
    transform_vertices(win, g_p_frustum, 32, MAX_SECTS, sect_ring, g_vbuf);
    for (size_t i_sect = 0; i_sect < MAX_SECTS; i_sect++) {
        zgl_mPixel tmp = {(fix32_t)g_vbuf[i_sect].pixel.XY.x,
                          (fix32_t)g_vbuf[i_sect].pixel.XY.y};
        zglDraw_mPixelDot(scr_p, &win->mrect, &tmp, 0xFFFFFF);
    }


    // Draw brush selection; always wireframe.
    for (BrushSelectionNode *node = brushsel.head; node; node = node->next) {
        Brush *brush = node->brush;
        transform_vertices(win, g_p_frustum, 32, brush->num_vertices, brush->vertices, g_vbuf);
        draw_wireframe_edges(scr_p, win, g_p_frustum, &EdgeSet_from_Mesh(*brush), color_selected_brush, g_vbuf, 0);
    }
    
    // draw brushframe; dotted wireframe.
    transform_vertices(win, g_p_frustum, 32, frame->num_vertices, frame->vertices, g_vbuf);
    draw_wireframe_edges(scr_p, win, g_p_frustum, &EdgeSet_from_Mesh(*frame), color_brushframe, g_vbuf, WIREFLAGS_DOTTED);

    // Draw brush selection point-of-reference: TODO: Make a cross shape or
    // larger dot.
    transform_vertices(win, g_p_frustum, 32, 1, &brushsel.POR, g_vbuf);
    zgl_mPixel tmp4 = {(fix32_t)g_vbuf[0].pixel.XY.x,
                       (fix32_t)g_vbuf[0].pixel.XY.y};
    zglDraw_mPixelDot(scr_p, &win->mrect, &tmp4, 0xFF0000);
}


static void draw_bigbox(zgl_PixelArray *scr_p, Window *win) {
    // an underlay of the world limits.
    transform_vertices(win, g_p_frustum, 32,
                       view3D_bigbox.num_vertices, view3D_bigbox.vertices, g_vbuf);
    draw_wireframe_edges(scr_p, win, g_p_frustum, &view3D_bigbox, 0x2596BE, g_vbuf, WIREFLAGS_NO_VERTICES);

    transform_vertices(win, g_p_frustum, 32,
                       view3D_grid.num_vertices, view3D_grid.vertices, g_vbuf);
    draw_wireframe_edges(scr_p, win, g_p_frustum, &view3D_grid,  0x235367, g_vbuf, WIREFLAGS_NO_VERTICES);
}


static void eyespace_to_screen_ortho(Window const *win, TransformedVertex *tv)
{
    Fix64Point eye = tv->eye;
    
    tv->vis = compute_viscode_ortho(eye);

    DepthPixel pix;
    pix.z = z_of(eye);
    pix.XY.x = win->mrect.x +
        (FIX_MUL((1L<<16) - FIX_DIV(x_of(eye), g_frustum_ortho.W), win->mrect.w)>>1);
    pix.XY.y = win->mrect.y +
        (FIX_MUL((1L<<16) - FIX_DIV(y_of(eye), g_frustum_ortho.H), win->mrect.h)>>1);   
    tv->pixel = pix;


    if (z_of(eye) != 0) {
        tv->zinv = (1LL<<48)/z_of(eye);
    }

}
static void eyespace_to_screen_persp(Window const *win, TransformedVertex *tv)
{
    Fix64Point eye = tv->eye;
    tv->vis = compute_viscode_persp(eye);

    DepthPixel pix;
    pix.z = z_of(eye);
    if ( ! (tv->vis & VIS_NEAR)) { // TODO: Clip to front plane.

        fix64_t x = FIX_MULDIV(g_frustum_persp.D, x_of(eye), z_of(eye));
        fix64_t y = FIX_MULDIV(g_frustum_persp.D, y_of(eye), z_of(eye));
        pix.XY.x = win->mrect.x + (FIX_MUL((1L<<16)-x, win->mrect.w)>>1);
        pix.XY.y = win->mrect.y + (FIX_MUL((1L<<16)-y, win->mrect.h)>>1);
    }
    tv->pixel = pix;

    if (z_of(eye) != 0) {
        tv->zinv = (1LL<<48)/z_of(eye);
    }
}

static uint8_t compute_viscode_ortho(Fix64Point vertex) {
    uint8_t viscode = 0;
    
    if (x_of(vertex) < g_frustum_ortho.xmin) viscode |= VIS_RIGHT;
    if (x_of(vertex) > g_frustum_ortho.xmax) viscode |= VIS_LEFT;
    if (y_of(vertex) < g_frustum_ortho.ymin) viscode |= VIS_BELOW;
    if (y_of(vertex) > g_frustum_ortho.ymax) viscode |= VIS_ABOVE;
    if (z_of(vertex) < g_frustum_ortho.zmin) viscode |= VIS_NEAR;
    if (z_of(vertex) > g_frustum_ortho.zmax) viscode |= VIS_FAR;
    
    return viscode;
}

static uint8_t compute_viscode_persp(Fix64Point vertex) {
    uint8_t viscode = 0;
    if (x_of(vertex) < -z_of(vertex)) viscode |= VIS_RIGHT;
    if (x_of(vertex) > +z_of(vertex)) viscode |= VIS_LEFT;
    if (y_of(vertex) < -z_of(vertex)) viscode |= VIS_BELOW;
    if (y_of(vertex) > +z_of(vertex)) viscode |= VIS_ABOVE;
    if (z_of(vertex) < g_frustum_persp.zmin) viscode |= VIS_NEAR;
    if (g_farclip && z_of(vertex) > g_frustum_persp.zmax) viscode |= VIS_FAR;

    return viscode;
}

// world space to eyespace
WitPoint apply_transform(WitPoint in) {
    WitPoint out;
    
    // 1) translate
    fix64_t
        tmp_x = x_of(in) - tf.x,
        tmp_y = y_of(in) - tf.y,
        tmp_z = z_of(in) - tf.z;
        
    // 2) rotate
    fix64_t tmp = FIX_MUL(tmp_x, tf.hcos) + FIX_MUL(tmp_y, tf.hsin);
    x_of(out)  = (fix32_t)(-FIX_MUL(tmp_x, tf.hsin) + FIX_MUL(tmp_y, tf.hcos));
    y_of(out)  = (fix32_t)(-FIX_MUL(tmp, tf.vcos)   + FIX_MUL(tmp_z, tf.vsin));
    z_of(out)  = (fix32_t)(+FIX_MUL(tmp, tf.vsin)   + FIX_MUL(tmp_z, tf.vcos));

    if (g_p_frustum->type == PROJ_PERSPECTIVE) {
        // 3) scale x and y to normalized frustum, keep depth z
        x_of(out) = (LSHIFT32(x_of(out), 16))/g_frustum_persp.W;
        y_of(out) = (LSHIFT32(y_of(out), 16))/g_frustum_persp.H;
    }
    
    return out;
}
 
// TODO: rename: batch_transform_vertices()
extern void transform_vertices
(Window const * win, Frustum *const fr, int width, 
 size_t const num_vertices, void const * vertices,
 TransformedVertex * out_vbuf) {
    for (size_t i_v = 0; i_v < num_vertices; i_v++) {
        Fix64Point eye;

        // 1) translate
        Fix32Point v = ((Fix32Point *)vertices)[i_v];
        fix64_t
            tmp_x = x_of(v) - tf.x,
            tmp_y = y_of(v) - tf.y,
            tmp_z = z_of(v) - tf.z;
        
        // 2) rotate
        fix64_t tmp = FIX_MUL(tmp_x, tf.hcos) + FIX_MUL(tmp_y, tf.hsin);
        x_of(eye)  = (fix32_t)(-FIX_MUL(tmp_x, tf.hsin) + FIX_MUL(tmp_y, tf.hcos));
        y_of(eye)  = (fix32_t)(-FIX_MUL(tmp, tf.vcos)   + FIX_MUL(tmp_z, tf.vsin));
        z_of(eye)  = (fix32_t)(+FIX_MUL(tmp, tf.vsin)   + FIX_MUL(tmp_z, tf.vcos));

        if (g_p_frustum->type == PROJ_PERSPECTIVE) {
            // 3) scale x and y to normalized frustum, keep depth z
            x_of(eye) = (LSHIFT64(x_of(eye), 16))/g_frustum_persp.W;
            y_of(eye) = (LSHIFT64(y_of(eye), 16))/g_frustum_persp.H;
        }
        TransformedVertex *tv = out_vbuf + i_v;
        tv->eye = eye;
        fn_eyespace_to_screen(win, tv);
    }
}

static void transform_Mesh
(Window const *const win, Frustum *const fr, int width, Mesh *mesh,
 TransformedVertex * out_vbuf) {
    size_t num_vertices = mesh->num_vertices;
    WitPoint *vertices = mesh->vertices;
    WitPoint origin = mesh->origin;
    
    fix64_t
        vcos = tf.vcos,
        vsin = tf.vsin,
        hcos = tf.hcos,
        hsin = tf.hsin;
    fix64_t
        eye_x = tf.x,
        eye_y = tf.y,
        eye_z = tf.z;

    fix32_t W = fr->W;
    fix32_t H = fr->H = (fix32_t)FIX_MULDIV(W, win->mrect.h, win->mrect.w);
    fr->ymin = -fr->H;
    fr->ymax = +fr->H - 1;
    
    for (size_t i_v = 0; i_v < num_vertices; i_v++) {
        Fix64Point eye;

        // TODO: 1) Self-relative rotation.
        
        // 2) Self-relative & Cam-relative translation.
        fix64_t tmp_x, tmp_y, tmp_z;
        
        if (width == 32) {
            tmp_x = x_of(((Fix32Point *)vertices)[i_v]) - eye_x + x_of(origin);
            tmp_y = y_of(((Fix32Point *)vertices)[i_v]) - eye_y + y_of(origin);
            tmp_z = z_of(((Fix32Point *)vertices)[i_v]) - eye_z + z_of(origin);
        }
        else if (width == 64) {
            tmp_x = x_of(((Fix64Point *)vertices)[i_v]) - eye_x + x_of(origin);
            tmp_y = y_of(((Fix64Point *)vertices)[i_v]) - eye_y + y_of(origin);
            tmp_z = z_of(((Fix64Point *)vertices)[i_v]) - eye_z + z_of(origin);
        }
        else {
            return;
        }
        
        // 3) Cam-relative rotation.
        fix64_t tmp = FIX_MUL(tmp_x, hcos) + FIX_MUL(tmp_y, hsin);
        x_of(eye)  = -FIX_MUL(tmp_x, hsin) + FIX_MUL(tmp_y, hcos);
        y_of(eye)  = -FIX_MUL(tmp, vcos)   + FIX_MUL(tmp_z, vsin);
        z_of(eye)  = +FIX_MUL(tmp, vsin)   + FIX_MUL(tmp_z, vcos);

        // 4) Projection.
        TransformedVertex *tv = out_vbuf + i_v;
        if (fr->type == PROJ_PERSPECTIVE) {
            x_of(eye) = (LSHIFT64(x_of(eye), 16))/W;
            y_of(eye) = (LSHIFT64(y_of(eye), 16))/H;   
        }

        tv->eye = eye;
        fn_eyespace_to_screen(win, tv);
    }
}


void draw_wireframe_face
(zgl_PixelArray *scr_p, Window *win, TransformedWireFace *twf, zgl_Color color) {
    if (area(twf->pixel[0].XY, twf->pixel[1].XY, twf->pixel[2].XY) <= 0) {
        return;
    }
    
    Fix64Point_2D scr_v0 = twf->pixel[0].XY;
    Fix64Point_2D scr_v1 = twf->pixel[1].XY;
    Fix64Point_2D scr_v2 = twf->pixel[2].XY;
    
#define FixPoint2D_64to32(POINT) ((Fix32Point_2D){(fix32_t)(POINT).x, (fix32_t)(POINT).y})
#define as_mPixelSeg(P1, P2) ((zgl_mPixelSeg){{(P1).x, (P1).y}, {(P2).x, (P2).y}})
    if ( ! (twf->vis[0] | twf->vis[1] | twf->vis[2])) { // no clipping
        zglDraw_mPixelSeg2(scr_p, &win->mrect,
                           &as_mPixelSeg(FixPoint2D_64to32(scr_v0),
                                         FixPoint2D_64to32(scr_v1)), color);
        zglDraw_mPixelSeg2(scr_p, &win->mrect,
                           &as_mPixelSeg(FixPoint2D_64to32(scr_v1),
                                         FixPoint2D_64to32(scr_v2)), color);
        zglDraw_mPixelSeg2(scr_p, &win->mrect,
                           &as_mPixelSeg(FixPoint2D_64to32(scr_v2),
                                         FixPoint2D_64to32(scr_v0)), color);
    }
    else { // clipping
        Fix64Point_2D subvs[32];
        bool new[32];
        size_t num_subvs;
        TriClip2D(&scr_v0, &scr_v1, &scr_v2,
                  win->mrect.x, win->mrect.x + win->mrect.w - 1,
                  win->mrect.y, win->mrect.y + win->mrect.h - 1,
                  subvs, &num_subvs, new);

        for (size_t i_subv = 0; i_subv < num_subvs; i_subv++) {
            zgl_mPixel tmp0;
            zgl_mPixel tmp1;
            tmp0.x = (fix32_t)subvs[i_subv].x;
            tmp0.y = (fix32_t)subvs[i_subv].y;

            tmp1.x = (fix32_t)subvs[(i_subv+1) % num_subvs].x;
            tmp1.y = (fix32_t)subvs[(i_subv+1) % num_subvs].y;

            zglDraw_mPixelSeg(scr_p, &win->mrect, tmp0, tmp1, color);
        }
    }
}

void draw_face(zgl_PixelArray *scr_p, Window *win, Face *face, TransformedFace *tf) {
    DepthPixel
        scr_V0 = tf->tv[0].pixel,
        scr_V1 = tf->tv[1].pixel,
        scr_V2 = tf->tv[2].pixel;
    
    if (area(scr_V0.XY, scr_V1.XY, scr_V2.XY) <= 0) {
        return;
    }

    if ( ! (tf->tv[0].vis | tf->tv[1].vis | tf->tv[2].vis)) { // no clipping needed
        ScreenTriangle screen_tri = (ScreenTriangle){
            .pixels = {&scr_V0, &scr_V1, &scr_V2},
            .zinv[0] = &tf->tv[0].zinv,
            .zinv[1] = &tf->tv[1].zinv,
            .zinv[2] = &tf->tv[2].zinv,
            .vbri[0] = tf->vbri[0],
            .vbri[1] = tf->vbri[1],
            .vbri[2] = tf->vbri[2],
            .brightness = face->brightness,
            .texture = textures[face->i_tex].pixarr,
            .texels = {(zgl_mPixel *)&face->tv0,
                       (zgl_mPixel *)&face->tv1,
                       (zgl_mPixel *)&face->tv2}};
        
        if (face->flags & FC_SELECTED) {
            screen_tri.brightness = 255;
        }

        draw_Tri[g_rasterflags](scr_p, win, &screen_tri);
    }
    else { // clipping needed
        Fix64Point_2D subvs[32];
        bool new[32];
        size_t num_subvs;
        TriClip2D(&scr_V0.XY, &scr_V1.XY, &scr_V2.XY,
                  win->mrect.x, win->mrect.x + win->mrect.w - 1,
                  win->mrect.y, win->mrect.y + win->mrect.h - 1,
                  subvs, &num_subvs, new);

        // need signed
        ptrdiff_t const i_0[] = {0, -1, -1, -2, -2, -3, -3, -4, -4, -5, -5};
        ptrdiff_t const i_1[] = {1,  0,  2, -1,  3, -2,  4, -3,  5, -4,  6};
        ptrdiff_t const i_2[] = {2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7};
        
        #define area(a, b, c) ((fix64_t)((c).x - (a).x) * (fix64_t)((b).y - (a).y) - (fix64_t)((c).y - (a).y) * (fix64_t)((b).x - (a).x))

        fix64_t area = ABS(area(scr_V0.XY, scr_V1.XY, scr_V2.XY)>>16);
        fix64_t B0, B1, B2;

        dibassert(area != 0);

        // number of subdivided triangles is 2 less than number of vertices
        for (size_t i_subface = 0; i_subface + 2 < num_subvs; i_subface++) {
            size_t
                i_subv0 = (num_subvs + i_0[i_subface]) % num_subvs,
                i_subv1 = (num_subvs + i_1[i_subface]) % num_subvs,
                i_subv2 = (num_subvs + i_2[i_subface]) % num_subvs;
            
            zgl_mPixel subTV0, subTV1, subTV2;
            DepthPixel subV0, subV1, subV2;
            subV0.XY = subvs[i_subv0];
            subV1.XY = subvs[i_subv1];
            subV2.XY = subvs[i_subv2];
            
            ScreenTriangle subtri;
            subtri.texture = textures[face->i_tex].pixarr;
            subtri.zinv[0] = &tf->tv[0].zinv;
            subtri.zinv[1] = &tf->tv[1].zinv;
            subtri.zinv[2] = &tf->tv[2].zinv;
            subtri.brightness = face->brightness;
            
            fix64_t Zinv;
            fix64_t
                Z0 = scr_V0.z,
                Z1 = scr_V1.z,
                Z2 = scr_V2.z;
            Fix32Point_2D
                TV0 = face->tv0,
                TV1 = face->tv1,
                TV2 = face->tv2;
            uint64_t
                vbri0 = tf->vbri[0],//bsp.vertex_intensities[face->i_v0],
                vbri1 = tf->vbri[1],//bsp.vertex_intensities[face->i_v1],
                vbri2 = tf->vbri[2];//bsp.vertex_intensities[face->i_v2];

            vbri0 <<= 16;
            vbri1 <<= 16;
            vbri2 <<= 16;
            
            // depth & texel 0
            B0 = ABS(area(subV0.XY, scr_V1.XY, scr_V2.XY))/area;
            B1 = ABS(area(subV0.XY, scr_V0.XY, scr_V2.XY))/area;
            B2 = (1<<16) - B0 - B1;
            Zinv = ((B0<<16)/Z0 + (B1<<16)/Z1 + (B2<<16)/Z2);
            subV0.z  = ((B0+B1+B2)<<16)/Zinv;
            subTV0.x = (fix32_t)((B0*FIX_DIV(TV0.x, Z0) +
                                  B1*FIX_DIV(TV1.x, Z1) +
                                  B2*FIX_DIV(TV2.x, Z2))/Zinv);
            subTV0.y = (fix32_t)((B0*FIX_DIV(TV0.y, Z0) +
                                  B1*FIX_DIV(TV1.y, Z1) +
                                  B2*FIX_DIV(TV2.y, Z2))/Zinv);
            subtri.vbri[0] = (uint8_t)((B0*FIX_DIV(vbri0, Z0) +
                                        B1*FIX_DIV(vbri1, Z1) +
                                        B2*FIX_DIV(vbri2, Z2))/Zinv);

            // depth & texel 1
            B0 = ABS(area(subV1.XY, scr_V1.XY, scr_V2.XY))/area;
            B1 = ABS(area(subV1.XY, scr_V0.XY, scr_V2.XY))/area;
            B2 = (1<<16) - B0 - B1;
            Zinv = ((B0<<16)/Z0 + (B1<<16)/Z1 + (B2<<16)/Z2);
            subV1.z  = ((B0+B1+B2)<<16)/Zinv;
            subTV1.x = (fix32_t)((B0*FIX_DIV(TV0.x, Z0) +
                                  B1*FIX_DIV(TV1.x, Z1) +
                                  B2*FIX_DIV(TV2.x, Z2))/Zinv);
            subTV1.y = (fix32_t)((B0*FIX_DIV(TV0.y, Z0) +
                                  B1*FIX_DIV(TV1.y, Z1) +
                                  B2*FIX_DIV(TV2.y, Z2))/Zinv);
            subtri.vbri[1] = (uint8_t)((B0*FIX_DIV(vbri0, Z0) +
                                        B1*FIX_DIV(vbri1, Z1) +
                                        B2*FIX_DIV(vbri2, Z2))/Zinv);
            // depth & texel 2
            B0 = ABS(area(subV2.XY, scr_V1.XY, scr_V2.XY))/area;
            B1 = ABS(area(subV2.XY, scr_V0.XY, scr_V2.XY))/area;
            B2 = (1<<16) - B0 - B1;
            Zinv = ((B0<<16)/Z0 + (B1<<16)/Z1 + (B2<<16)/Z2);
            subV2.z  = ((B0+B1+B2)<<16)/Zinv;
            subTV2.x = (fix32_t)((B0*FIX_DIV(TV0.x, Z0) +
                                  B1*FIX_DIV(TV1.x, Z1) +
                                  B2*FIX_DIV(TV2.x, Z2))/Zinv);
            subTV2.y = (fix32_t)((B0*FIX_DIV(TV0.y, Z0) +
                                  B1*FIX_DIV(TV1.y, Z1) +
                                  B2*FIX_DIV(TV2.y, Z2))/Zinv);
            subtri.vbri[2] = (uint8_t)((B0*FIX_DIV(vbri0, Z0) +
                                        B1*FIX_DIV(vbri1, Z1) +
                                        B2*FIX_DIV(vbri2, Z2))/Zinv);
            
            subtri.texels[0] = &subTV0;
            subtri.texels[1] = &subTV1;
            subtri.texels[2] = &subTV2;
            subtri.pixels[0] = &subV0;
            subtri.pixels[1] = &subV1;
            subtri.pixels[2] = &subV2;
            
            draw_Tri[g_rasterflags](scr_p, win, &subtri);
        }
    }
}


#define area2(a, b, c) ((fix64_t)(x_of(c) - x_of(a)) * (fix64_t)(y_of(b) - y_of(a)) - (fix64_t)(y_of(c) - y_of(a)) * (fix64_t)(x_of(b) - x_of(a)))


/**
   Draw mesh as wireframe, but use edge-based culling and clipping.
*/
static void draw_wireframe_edges(zgl_PixelArray *scr_p, Window *win, Frustum *fr, EdgeSet *eset, zgl_Color color, TransformedVertex const *const vbuf, uint8_t wireflags) {
    int (*fn_drawseg)(zgl_PixelArray *, const zgl_mPixelRect *,
                      const zgl_mPixel, const zgl_mPixel, const zgl_Color);
    fn_drawseg =
        wireflags & WIREFLAGS_DOTTED ?
        zglDraw_mPixelSeg_Dotted :
        zglDraw_mPixelSeg;

    size_t num_edges = eset->num_edges;
    Edge *edges = eset->edges;
    
    uint8_t vis0, vis1;
    Fix64Point p0, p1;
    Fix64Point c0, c1;
    bool accept;
    fix64_t x, y;
    
    DepthPixel scr0, scr1;

    for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
        size_t i_v0 = edges[i_edge].i_v0;
        size_t i_v1 = edges[i_edge].i_v1;
        
        p0 = vbuf[i_v0].eye;
        p1 = vbuf[i_v1].eye;

        vis0 = vbuf[i_v0].vis;
        vis1 = vbuf[i_v1].vis;
        
        // TODO: This rejects some lines that need to be clipped!
        if (vis0 & vis1) { // trivial reject
            continue;
        }

        if ( ! (vis0 | vis1)) {
            fn_drawseg(scr_p, &win->mrect,
                       (zgl_mPixel){(fix32_t)vbuf[i_v0].pixel.XY.x, (fix32_t)vbuf[i_v0].pixel.XY.y},
                       (zgl_mPixel){(fix32_t)vbuf[i_v1].pixel.XY.x, (fix32_t)vbuf[i_v1].pixel.XY.y},
                       color);            
        }
        else {
            accept = fn_SegClip3D(x_of(p0), y_of(p0), z_of(p0),
                                  x_of(p1), y_of(p1), z_of(p1),
                                  fr, &c0, &c1);

            if ( ! accept) { // reject
                continue; // this might never occur. TODO
            }

            if (g_settings.proj_type == PROJ_PERSPECTIVE) {
                x = FIX_MULDIV(fr->D, x_of(c0), z_of(c0));
                y = FIX_MULDIV(fr->D, y_of(c0), z_of(c0));
                scr0.XY.x = win->mrect.x + (FIX_MUL((1L<<16)-x, win->mrect.w)>>1);
                scr0.XY.y = win->mrect.y + (FIX_MUL((1L<<16)-y, win->mrect.h)>>1);

                x = FIX_MULDIV(fr->D, x_of(c1), z_of(c1));
                y = FIX_MULDIV(fr->D, y_of(c1), z_of(c1));
                scr1.XY.x = win->mrect.x + (FIX_MUL((1L<<16)-x, win->mrect.w)>>1);
                scr1.XY.y = win->mrect.y + (FIX_MUL((1L<<16)-y, win->mrect.h)>>1);


                fn_drawseg(scr_p, &win->mrect,
                           (zgl_mPixel){(fix32_t)scr0.XY.x, (fix32_t)scr0.XY.y},
                           (zgl_mPixel){(fix32_t)scr1.XY.x, (fix32_t)scr1.XY.y},
                           color);    
            }
            else if (g_settings.proj_type == PROJ_ORTHOGRAPHIC) {
                scr0.XY.x = win->mrect.x + (FIX_MUL((1L<<16) - FIX_DIV(x_of(c0), g_p_frustum->W), win->mrect.w)>>1);
                scr0.XY.y = win->mrect.y + (FIX_MUL((1L<<16) - FIX_DIV(y_of(c0), g_p_frustum->H), win->mrect.h)>>1);

                scr1.XY.x = win->mrect.x + (FIX_MUL((1L<<16) - FIX_DIV(x_of(c1), g_p_frustum->W), win->mrect.w)>>1);
                scr1.XY.y = win->mrect.y + (FIX_MUL((1L<<16) - FIX_DIV(y_of(c1), g_p_frustum->H), win->mrect.h)>>1);

                fn_drawseg(scr_p, &win->mrect,
                           (zgl_mPixel){(fix32_t)scr0.XY.x, (fix32_t)scr0.XY.y},
                           (zgl_mPixel){(fix32_t)scr1.XY.x, (fix32_t)scr1.XY.y},
                           color);
            }
        }
    }

    if ( ! (wireflags & WIREFLAGS_NO_VERTICES)) {
        size_t num_v = eset->num_vertices;
        for (size_t i_v = 0; i_v < num_v; i_v++) {
            zglDraw_mPixelDot(scr_p, &win->mrect, &(zgl_mPixel){(fix32_t)vbuf[i_v].pixel.XY.x, (fix32_t)vbuf[i_v].pixel.XY.y}, color);
        }    
    }
}


void subdivide3D(Window const *const win, Fix64Point V0, Fix64Point V1, Fix64Point V2, size_t *out_num_subvs, Fix64Point *out_subvs, TransformedVertex *out_subvbuf, bool *out_new) {
    size_t num_subvs;
    fn_TriClip3D(&V0, &V1, &V2, g_p_frustum, out_subvs, &num_subvs, out_new);
    
    Fix64Point eye;
    for (size_t i_subv = 0; i_subv < num_subvs; i_subv++) {
        eye = out_subvs[i_subv];

        TransformedVertex *tv = out_subvbuf + i_subv;
        tv->eye = eye;
        fn_eyespace_to_screen(win, tv);
        //        fn_visify_and_project(win, eye, &tv->vis, &tv->pixel);
        
        if (z_of(eye) != 0) {
            tv->zinv = (1LL<<48)/z_of(eye);
        }
    }
    
    *out_num_subvs = num_subvs;
}

/**
   Draw mesh as wireframe, but use face-based culling and clipping. Visualizes
   triangle subdivisions.
*/
static void draw_wireframe_faces(zgl_PixelArray *scr_p, Window *win, FaceSet *fset, zgl_Color color, TransformedVertex const *const vbuf) {
    size_t num_faces = fset->num_faces;
    Face *faces = fset->faces;

    for (size_t i_face = 0; i_face < num_faces; i_face++) {
        Face *face = faces + i_face;

        if (face->flags & FC_DEGEN) {
            continue;
        }
        
        size_t
            i_v0 = face->i_v0,
            i_v1 = face->i_v1,
            i_v2 = face->i_v2;

        // grab data from vertex buffer
        TransformedWireFace twface;
        twface.vis[0] = vbuf[i_v0].vis;
        twface.vis[1] = vbuf[i_v1].vis;
        twface.vis[2] = vbuf[i_v2].vis;
            
        if (twface.vis[0] & twface.vis[1] & twface.vis[2]) { // trivial reject
            continue;
        }
        
        if ( ! ((twface.vis[0]) | (twface.vis[1]) | (twface.vis[2]))) {
            twface.pixel[0] = vbuf[i_v0].pixel;
            twface.pixel[1] = vbuf[i_v1].pixel;
            twface.pixel[2] = vbuf[i_v2].pixel;
            twface.color = color;

            // vertex number labels if selected
            if (face->flags & FC_SELECTED) {
                draw_BMPFont_string
                    (scr_p, NULL, "0",
                     (fix32_t)(twface.pixel[0].XY.x>>16),
                     (fix32_t)(twface.pixel[0].XY.y>>16), FC_MONOGRAM1, 0xFFFFFF);
                draw_BMPFont_string
                    (scr_p, NULL, "1",
                     (fix32_t)(twface.pixel[1].XY.x>>16),
                     (fix32_t)(twface.pixel[1].XY.y>>16), FC_MONOGRAM1, 0xFFFFFF);
                draw_BMPFont_string
                    (scr_p, NULL, "2",
                     (fix32_t)(twface.pixel[2].XY.x>>16),
                     (fix32_t)(twface.pixel[2].XY.y>>16), FC_MONOGRAM1, 0xFFFFFF);
            }
            
            draw_wireframe_face(scr_p, win, &twface, color);
        }
        else {
            size_t num_vertices;
            Fix64Point poly[32];
            TransformedVertex tpoly[32];
            bool new[32];
            subdivide3D(win, vbuf[i_v0].eye, vbuf[i_v1].eye, vbuf[i_v2].eye,
                        &num_vertices, poly, tpoly, new);

            // zig-zag subdivision
            ptrdiff_t const i_0[]  = { 0,-1,-1,-2,-2,-3,-3,-4,-4,-5,-5};
            ptrdiff_t const i_1[]  = { 1, 0, 2,-1, 3,-2, 4,-3, 5,-4, 6};
            ptrdiff_t const i_2[]  = { 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7};

            size_t i_subv0, i_subv1, i_subv2;
            TransformedWireFace twf;
            if (num_vertices == 0) {
                continue;
            }
            else if (num_vertices == 3) {
                twf = (TransformedWireFace){
                    {tpoly[0].pixel, tpoly[1].pixel, tpoly[2].pixel},
                    {tpoly[0].vis,   tpoly[1].vis,   tpoly[2].vis},
                    color,
                };
                
                draw_wireframe_face(scr_p, win, &twf, color);
            }
            else { // num_vertices >= 4
                for (size_t i_subface = 0; i_subface + 2 + 1 < num_vertices; i_subface++) {
                    i_subv0 = (num_vertices + i_0[i_subface]) % num_vertices;
                    i_subv1 = (num_vertices + i_1[i_subface]) % num_vertices;
                    i_subv2 = (num_vertices + i_2[i_subface]) % num_vertices;

                    twf = (TransformedWireFace){
                        {tpoly[i_subv0].pixel, tpoly[i_subv1].pixel, tpoly[i_subv2].pixel},
                        {tpoly[i_subv0].vis,   tpoly[i_subv1].vis,   tpoly[i_subv2].vis},
                        color,
                    };
                
                    draw_wireframe_face(scr_p, win, &twf, color);
                }
                
                // handle last one separately for correct wire coloring.
                i_subv0 = (num_vertices + i_0[num_vertices-3]) % num_vertices;
                i_subv1 = (num_vertices + i_1[num_vertices-3]) % num_vertices;
                i_subv2 = (num_vertices + i_2[num_vertices-3]) % num_vertices;
                if (num_vertices & 1) {
                    twf = (TransformedWireFace){
                        {tpoly[i_subv0].pixel, tpoly[i_subv1].pixel, tpoly[i_subv2].pixel},
                        {tpoly[i_subv0].vis,   tpoly[i_subv1].vis,   tpoly[i_subv2].vis},
                        color,
                    };
                }
                else {
                    twf = (TransformedWireFace){
                        {tpoly[i_subv0].pixel, tpoly[i_subv1].pixel, tpoly[i_subv2].pixel},
                        {tpoly[i_subv0].vis,   tpoly[i_subv1].vis,   tpoly[i_subv2].vis},
                        color,
                    };
                }
                
                draw_wireframe_face(scr_p, win, &twf, color);
            }

            if (face->flags & FC_SELECTED) {
                char num[3+1];
                for (size_t i_vertex = 0; i_vertex < num_vertices; i_vertex++) {
                    sprintf(num, "%zu", i_vertex);
                    draw_BMPFont_string(scr_p, NULL, num,
                                        (fix32_t)(tpoly[i_vertex].pixel.XY.x>>16),
                                        (fix32_t)(tpoly[i_vertex].pixel.XY.y>>16),
                                        FC_MONOGRAM1, 0xFFFFFF);
                }
            }
        }
    }
    
    return;
}

void subdivide3D_and_draw_face(zgl_PixelArray *scr_p, Window *win, Face *face, TransformedVertex const *const vbuf) {
    // If face is partially outside frustum, we do 3D clipping &
    // subdivision against all frustum planes. An alternative would be
    // to only 3D clip & subdivide against near plane, then do 2D
    // clipping in screen space.

    Fix64Point V0 = vbuf[face->i_v0].eye;
    Fix64Point V1 = vbuf[face->i_v1].eye;
    Fix64Point V2 = vbuf[face->i_v2].eye;

    size_t num_vertices;
    Fix64Point poly[32];
    TransformedVertex tpoly[32];
    bool new[32];
    subdivide3D(win, V0, V1, V2, &num_vertices, poly, tpoly, new);
    
    fix64_t area = triangle_area_3D(V0, V1, V2);
    dibassert(area != 0);

    fix64_t B0, B1, B2;

    // zig-zag subdivision
    ptrdiff_t const i_0[] = {0, -1, -1, -2, -2, -3, -3, -4, -4, -5, -5};
    ptrdiff_t const i_1[] = {1,  0,  2, -1,  3, -2,  4, -3,  5, -4,  6};
    ptrdiff_t const i_2[] = {2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7};
    
    for (size_t i_subface = 0; i_subface + 2 < num_vertices; i_subface++) {
        size_t
            i_subv0 = (num_vertices + i_0[i_subface]) % num_vertices,
            i_subv1 = (num_vertices + i_1[i_subface]) % num_vertices,
            i_subv2 = (num_vertices + i_2[i_subface]) % num_vertices;
                
        // construct subface structure
        Face subface;
        subface.i_v0 = i_subv0;
        subface.i_v1 = i_subv1;
        subface.i_v2 = i_subv2;
        subface.flags = face->flags; // TODO: Possible to get degenerate subface?
        subface.normal = face->normal;
        subface.brightness = face->brightness;
        subface.i_tex = face->i_tex;
                
        // grab data from vertex buffers.
        TransformedFace subtface;
        subtface.tv[0] = tpoly[i_subv0];
        subtface.tv[1] = tpoly[i_subv1];
        subtface.tv[2] = tpoly[i_subv2];
        
        Fix32Point_2D
            TV0 = face->tv0,
            TV1 = face->tv1,
            TV2 = face->tv2;
        uint64_t
            vbri0 = bsp.vertex_intensities[face->i_v0],
            vbri1 = bsp.vertex_intensities[face->i_v1],
            vbri2 = bsp.vertex_intensities[face->i_v2];

        // texel 0
        B0 = (triangle_area_3D(poly[i_subv0], V1, V2)<<16)/area;
        B1 = (triangle_area_3D(poly[i_subv0], V0, V2)<<16)/area;
        B2 = (1<<16) - B0 - B1;
        subface.tv0.x = (fix32_t)((B0*TV0.x + B1*TV1.x + B2*TV2.x)>>16);
        subface.tv0.y = (fix32_t)((B0*TV0.y + B1*TV1.y + B2*TV2.y)>>16);
        subtface.vbri[0] = (uint8_t)((B0*vbri0 + B1*vbri1 + B2*vbri2)>>16);
        
        // texel 1
        B0 = (triangle_area_3D(poly[i_subv1], V1, V2)<<16)/area;
        B1 = (triangle_area_3D(poly[i_subv1], V0, V2)<<16)/area;
        B2 = (1<<16) - B0 - B1;
        subface.tv1.x = (fix32_t)((B0*TV0.x + B1*TV1.x + B2*TV2.x)>>16);
        subface.tv1.y = (fix32_t)((B0*TV0.y + B1*TV1.y + B2*TV2.y)>>16);
        subtface.vbri[1] = (uint8_t)((B0*vbri0 + B1*vbri1 + B2*vbri2)>>16);
        
        // texel 2
        B0 = (triangle_area_3D(poly[i_subv2], V1, V2)<<16)/area;
        B1 = (triangle_area_3D(poly[i_subv2], V0, V2)<<16)/area;
        B2 = (1<<16) - B0 - B1;
        subface.tv2.x = (fix32_t)((B0*TV0.x + B1*TV1.x + B2*TV2.x)>>16);
        subface.tv2.y = (fix32_t)((B0*TV0.y + B1*TV1.y + B2*TV2.y)>>16);
        subtface.vbri[2] = (uint8_t)((B0*vbri0 + B1*vbri1 + B2*vbri2)>>16);
        
        if (g_settings.BSP_visualizer){
            draw_face_BSP_visualizer(scr_p, win, &subface, &subtface);
        }
        else {
            draw_face(scr_p, win, &subface, &subtface);
        }
        
    }

}

static void draw_vertices(zgl_PixelArray *scr_p, Window *win, size_t num_vertices, TransformedVertex const *const vbuf) {
    for (size_t i_v = 0; i_v < num_vertices; i_v++) {
        zgl_mPixel p = {(fix32_t)vbuf[i_v].pixel.XY.x, (fix32_t)vbuf[i_v].pixel.XY.y};
        uint8_t vis0 = vbuf[i_v].vis;
        
        // TODO: This rejects some lines that need to be clipped!
        if (vis0) { // trivial reject
            continue;
        }
        else {
            zglDraw_mPixelDot(scr_p, &win->mrect, &p, 0xFF00FF);
        }

    }
}

static void draw_faces(zgl_PixelArray *scr_p, Window *win, FaceSet *fset, TransformedVertex const *const vbuf) {
    size_t num_faces = fset->num_faces;
    Face *faces = fset->faces;
    
    for (size_t i_face = 0; i_face < num_faces; i_face++) {
        Face *face = faces + i_face;

        if (face->flags & FC_DEGEN) {
            continue;
        }
        
        size_t
            i_v0 = face->i_v0,
            i_v1 = face->i_v1,
            i_v2 = face->i_v2;

        // grab data from vertex buffer
        TransformedFace tface;
        tface.tv[0].vis = vbuf[i_v0].vis;
        tface.tv[1].vis = vbuf[i_v1].vis;
        tface.tv[2].vis = vbuf[i_v2].vis;
        
        if (tface.tv[0].vis & tface.tv[1].vis & tface.tv[2].vis) {
            // trivial reject
            continue;
        }
        
        if ( ! ((tface.tv[0].vis) | (tface.tv[1].vis) | (tface.tv[2].vis))) {
            tface.tv[0] = vbuf[i_v0];
            tface.tv[1] = vbuf[i_v1];
            tface.tv[2] = vbuf[i_v2];
            draw_face(scr_p, win, face, &tface);
        }
        else {
            subdivide3D_and_draw_face(scr_p, win, face, vbuf);
        } 
    }
    
    return;    
}


/* -------------------------------------------------------------------------- */

#define roundup_pow2_32(v)                      \
    v--;                                        \
    v |= v >> 1;                                \
    v |= v >> 2;                                \
    v |= v >> 4;                                \
    v |= v >> 8;                                \
    v |= v >> 16;                               \
    v++                                         \

#define roundup_pow2_64(v)                      \
    v--;                                        \
    v |= v >> 1;                                \
    v |= v >> 2;                                \
    v |= v >> 4;                                \
    v |= v >> 8;                                \
    v |= v >> 16;                               \
    v |= v >> 32;                               \
    v++                                         \





void select_vertex(Window *win, zgl_Pixel pixel) {
    vertex_below(win, pixel, g_p_frustum);
}

void select_one_face(Window *win, zgl_Pixel pixel) {
    BSPFace *bspface = BSPFace_below(win, pixel, g_p_frustum);
    if (bspface) {
        bool selected = ((BrushQuadFace *)bspface->progenitor_face->brushface)->flags & FACEFLAG_SELECTED;
        sel_clear(&sel_face);
        clear_brush_selection(&brushsel);
        if ( ! selected) {
            sel_toggle_face(bspface->progenitor_face->brushface);
        }
    }
}

void select_face(Window *win, zgl_Pixel pixel) {
    BSPFace *bspface = BSPFace_below(win, pixel, g_p_frustum);
    if (bspface) {
        sel_toggle_face(bspface->progenitor_face->brushface);
    }
}

void select_brush(Window *win, zgl_Pixel pixel) {
    BSPFace *bspface = BSPFace_below(win, pixel, g_p_frustum);
    if (bspface) {
        toggle_selected_brush(&brushsel, staticgeo.progenitor_brush[bspface->i_progenitor_face]);
    }
}

void epm_SetTriangleDrawFunction(int _i_draw) {
    g_settings.i_draw = _i_draw;
    _epm_Log("DRAW3D", LT_INFO, "Changed triangle draw function: %i\n", g_settings.i_draw);
}
void epm_NextTriangleDrawFunction(void) {
    g_settings.i_draw = (g_settings.i_draw + 1) & 7;
    _epm_Log("DRAW3D", LT_INFO, "Changed triangle draw function: %i\n", g_settings.i_draw);
}
void epm_PrevTriangleDrawFunction(void) {
    g_settings.i_draw = (g_settings.i_draw + 7) & 7;
    _epm_Log("DRAW3D", LT_INFO, "Changed triangle draw function: %i\n", g_settings.i_draw);
}

void epm_SetProjectionType(int type) {
    if (type != PROJ_ORTHOGRAPHIC && type != PROJ_PERSPECTIVE) return;

    g_settings.proj_type = type;

    if (type == PROJ_PERSPECTIVE) {
        g_rasterflags &= ~RASTERFLAG_ORTHO;
        
        g_p_frustum = &g_frustum_persp;
        fn_SegClip3D = SegClip3D;
        fn_TriClip3D = TriClip3D;
        fn_compute_viscode = compute_viscode_persp;
        fn_eyespace_to_screen = eyespace_to_screen_persp;
    }
    else if (type == PROJ_ORTHOGRAPHIC) {
        g_rasterflags |= RASTERFLAG_ORTHO;
        
        g_p_frustum = &g_frustum_ortho;
        fn_SegClip3D = SegClip3D_Orth;
        fn_TriClip3D = TriClip3D_Orth;
        fn_compute_viscode = compute_viscode_ortho;
        fn_eyespace_to_screen = eyespace_to_screen_ortho;
    }
    else {
        dibassert(false);
    }

}

void epm_ToggleProjectionType(void) {
    g_settings.proj_type = g_settings.proj_type == PROJ_ORTHOGRAPHIC ? PROJ_PERSPECTIVE : PROJ_ORTHOGRAPHIC;
}

void epm_SetLighting(bool state) {
    g_settings.lighting = state;
    if ( ! g_settings.lighting)
        g_rasterflags |= RASTERFLAG_NO_LIGHT;
    else
        g_rasterflags &= ~RASTERFLAG_NO_LIGHT;
}

void epm_ToggleLighting(void) {
    g_settings.lighting = !g_settings.lighting;
    if ( ! g_settings.lighting)
        g_rasterflags |= RASTERFLAG_NO_LIGHT;
    else
        g_rasterflags &= ~RASTERFLAG_NO_LIGHT;
}

void epm_SetWireframe(bool state) {
    g_settings.wireframe = state;
}

void epm_ToggleWireframe(void) {
    g_settings.wireframe = !g_settings.wireframe;
}
