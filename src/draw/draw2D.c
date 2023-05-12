#include "src/misc/epm_includes.h"
#include "src/draw/window/window.h"
#include "src/draw/colors.h"
#include "src/draw/default_layout.h"
#include "src/world/world.h"
#include "src/world/geometry.h"
#include "src/world/selection.h"

#include "src/draw/draw2D.h"

/* TODO: Before looping through segs, compute which segs actually intersect the
   screenbox. */

#define DEFAULT_ZOOM 50
#define MAX_ZOOM (1LL<<34)
#define MIN_ZOOM (1LL<<16) // TODO: lower this

static uint32_t num_zooms = 94;
static ufix64_t zooms[94] = {0X10000, 0X12492, 0X14E5D, 0X17E21, 0X1B4B8, 0X1F31B, 0X23A68, 0X28BE4, 0X2E904, 0X35372, 0X3CD14, 0X45816, 0X4F6F4, 0X5AC84, 0X67C04, 0X76929, 0X8782E, 0X9ADEB, 0XB0FE8, 0XCA476, 0XE72D0, 0X108336, 0X12DF19, 0X159141, 0X18A601, 0X1C2B6E, 0X2031A2, 0X24CB02, 0X2A0C94, 0X300E60, 0X36EBDB, 0X3EC468, 0X47BBE4, 0X51FB4D, 0X5DB17C, 0X6B13FB, 0X7A5FFA, 0X8BDB66, 0X9FD62B, 0XB6AB9E, 0XD0C422, 0XEE9702, 0X110AC94, 0X137A0A9, 0X1642553, 0X1970615, 0X1D12B85, 0X2139F73, 0X25F91A8, 0X2B65D52, 0X3198F39, 0X38AECD3, 0X40C7C5E, 0X4A08E22, 0X549C702, 0X60B2C94, 0X6E832F2, 0X7E4CC82, 0X9057C02, 0XA4F6926, 0XBC8782B, 0XD7764C3, 0XF63E0DE, 0X1196B7D9, 0X1419F6AE, 0X16F919EB, 0X1A414231, 0X1E017038, 0X224AC964, 0X2730E629, 0X2CCA2B9C, 0X333031D6, 0X3A8038F4, 0X42DBAECD, 0X4C68C7C5, 0X57532D73, 0X63CCC63A, 0X720E9966, 0X8259D3E2, 0X94F8F226, 0XAA4114BD, 0XC293856A, 0XDE5F73E6, 0XFE23F22B, 0X122723955, 0X14BF04185, 0X17B5BB898, 0X1B18D6540, 0X1EF7D0600, 0X23645BDB6, 0X2872B21F4, 0X2E39F023B, 0X34D48028C, 0X3C609277B};


static View2D view2D[3] = {
    [PLN_TOP] = {
        .i_x = I_X,
        .i_y = I_Y,
        .center = {0,0},
        .i_zoom = DEFAULT_ZOOM
    },
    [PLN_SIDE] = {
        .i_x = I_Y,
        .i_y = I_Z,
        .center = {0,0},
        .i_zoom = DEFAULT_ZOOM
    },
    [PLN_FRONT] = {
        .i_x = I_X,
        .i_y = I_Z,
        .center = {0,0},
        .i_zoom = DEFAULT_ZOOM
    },
};

/** Returns the least multiple of @mult which is greater than or equal to @x.
 */
static int64_t round_up_pow2(int64_t x, int64_t log2_mult) {
    int64_t absx = ABS(x);
    int64_t remainder = absx & ((1<<log2_mult) - 1);
    if (remainder == 0) return x;
    if (x < 0) return -(absx - remainder);
    else return x + (1<<log2_mult) - remainder;
}

#define INSIDE 0
#define LEFT   1
#define RIGHT  2
#define BOTTOM 4
#define TOP    8

static uint32_t compute_outcode
(fix64_t x,    fix64_t y,
 fix64_t xmin, fix64_t xmax,
 fix64_t ymin, fix64_t ymax) {   
    uint32_t code = INSIDE;

    if      (x < xmin) code |= LEFT;
    else if (x > xmax) code |= RIGHT;
    
    if      (y < ymin) code |= BOTTOM;
    else if (y > ymax) code |= TOP;

    return code;
}

static bool clip_cohen_sutherland
(fix64_t x0,   fix64_t y0,
 fix64_t x1,   fix64_t y1,
 fix32_t *cx0, fix32_t *cy0,
 fix32_t *cx1, fix32_t *cy1,
 fix64_t xmin, fix64_t xmax,
 fix64_t ymin, fix64_t ymax) {
    // https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
    
    uint32_t outcode0 = compute_outcode(x0, y0, xmin, xmax, ymin, ymax);
    uint32_t outcode1 = compute_outcode(x1, y1, xmin, xmax, ymin, ymax);
    bool accept = false;
    
    while (true) {
        if ( ! (outcode0 | outcode1)) { // both endpoints inside; visible and no
                                        // clipping required
            accept = true;
            break;
        }
        else if (outcode0 & outcode1) { // both endpoints in SAME outside zone;
                                        // not visible
            break;
        }
        else {
            fix64_t x, y;

            // pick an endpoint which is outside (at least one exists)
            uint32_t outcode_out = (outcode1 > outcode0) ? outcode1 : outcode0;

            if (outcode_out & TOP) {
                x = x0 + FIX_DIV(FIX_MUL(x1-x0, ymax-y0), y1-y0);
                y = ymax;
            }
            else if (outcode_out & BOTTOM) {
                x = x0 + FIX_DIV(FIX_MUL(x1-x0, ymin-y0), y1-y0);
                y = ymin;
            }
            else if (outcode_out & RIGHT) {
                y = y0 + FIX_DIV(FIX_MUL(y1-y0, xmax-x0), x1-x0);
                x = xmax;
            }
            else if (outcode_out & LEFT) {
                y = y0 + FIX_DIV(FIX_MUL(y1-y0, xmin-x0), x1-x0);
                x = xmin;
            }
            else {
                dibassert(false);
            }

            
            if (outcode_out == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = compute_outcode(x0, y0, xmin, xmax, ymin, ymax);
            }
            else {
                x1 = x;
                y1 = y;
                outcode1 = compute_outcode(x1, y1, xmin, xmax, ymin, ymax);
            }
        }
    }

    *cx0 = (fix32_t)x0;
    *cy0 = (fix32_t)y0;
    *cx1 = (fix32_t)x1;
    *cy1 = (fix32_t)y1;
    
    return accept;
}

#undef INSIDE
#undef LEFT
#undef RIGHT
#undef BOTTOM
#undef TOP




static bool epm_ClipSegToRect_2D
(Fix64Seg_2D seg,
 zgl_mPixelRect rect,
 zgl_mPixelSeg *clipped_seg) {
    return clip_cohen_sutherland
        (seg.pt0.x, seg.pt0.y,
         seg.pt1.x, seg.pt1.y,
         &clipped_seg->pt0.x, &clipped_seg->pt0.y,
         &clipped_seg->pt1.x, &clipped_seg->pt1.y,
         rect.x, rect.x+rect.w-1,
         rect.y, rect.y+rect.h-1);
}


void set_zoom_level(View2D *v2d) {
    uint8_t gridres_world_shift; // a choice of grid "resolution". Always a power of two.

    // TODO: find min&max x&y

    v2d->worldbox.w = zooms[v2d->i_zoom];
    v2d->worldbox.h = FIX_MULDIV(v2d->worldbox.w,
                                 v2d->screenbox.h,
                                 v2d->screenbox.w);
    
    for (gridres_world_shift = 1; ; gridres_world_shift++) {
        v2d->num_vert_lines = (uint32_t)(v2d->worldbox.w >> gridres_world_shift);
        v2d->num_hori_lines = (uint32_t)(v2d->worldbox.h >> gridres_world_shift);

        if (v2d->num_vert_lines <= 16)
            break;
    }
    
    v2d->gridres_world_shift = gridres_world_shift;
    v2d->gridres_world = 1<<gridres_world_shift;
}

#define wit_from_mPixit(screen_dist, scr_ref, wit_ref) \
    FIX_MULDIV((screen_dist), (wit_ref), (scr_ref))

#define screendist_from_worlddist(world_dist, scr_ref, wit_ref) \
    (ufix32_t)FIX_MULDIV((world_dist), (scr_ref), (wit_ref))


void zoom_level_up(Plane tsf) {
    view2D[tsf].i_zoom = MIN(num_zooms-1, view2D[tsf].i_zoom + 1);
    set_zoom_level(view2D + tsf);
}

void zoom_level_down(Plane tsf) {
    view2D[tsf].i_zoom = view2D[tsf].i_zoom == 0 ? 0 : view2D[tsf].i_zoom - 1;
    set_zoom_level(view2D + tsf);
}

void scroll(Window *win, int dx, int dy, Plane tsf) {
    View2D *v2d = &view2D[tsf];
    
    v2d->center.x += wit_from_mPixit(-LSHIFT32(dx, 16),
                                     fixify(win->rect.w),
                                     zooms[v2d->i_zoom]);
        
    v2d->center.y -= wit_from_mPixit(-LSHIFT32(dy, 16),
                                     fixify(win->rect.h),
                                     FIX_MULDIV(zooms[v2d->i_zoom],
                                                win->rect.h,
                                                win->rect.w));

    v2d->center.x = MIN(v2d->center.x, INT32_MAX);
    v2d->center.x = MAX(v2d->center.x, INT32_MIN);
    v2d->center.y = MIN(v2d->center.y, INT32_MAX);
    v2d->center.y = MAX(v2d->center.y, INT32_MIN);
}


static int screenpoint_from_worldpoint_64
(const Fix64Point_2D *world_pt, zgl_mPixel *mapscr_pt, View2D *v2d) {
    mapscr_pt->x = (fix32_t)(v2d->screenbox.x +
                    v2d->screenbox.w/2 +
                    FIX_MULDIV(world_pt->x - v2d->center.x,
                                 v2d->screenbox.w,
                                 v2d->worldbox.w));
    
    mapscr_pt->y = (fix32_t)(v2d->screenbox.y +
                    v2d->screenbox.h/2 -
                    FIX_MULDIV(world_pt->y - v2d->center.y,
                                 v2d->screenbox.w,
                                 v2d->worldbox.w));
    
    return 0;
}

static int screenpoint_from_worldpoint
(const WitPoint *world_pt, Fix64Point_2D *mapscr_pt, View2D *v2d) {
    mapscr_pt->x = (v2d->screenbox.x +
                    v2d->screenbox.w/2 +
                    FIX_MULDIV(world_pt->v[v2d->i_x] - v2d->center.x,
                                 v2d->screenbox.w,
                                 v2d->worldbox.w));
    
    mapscr_pt->y = (v2d->screenbox.y +
                    v2d->screenbox.h/2 -
                    FIX_MULDIV(world_pt->v[v2d->i_y] - v2d->center.y,
                                 v2d->screenbox.w,
                                 v2d->worldbox.w));
    
    return 0;
}

static bool potentially_visible_seg(Fix64Seg_2D seg, zgl_mPixelRect rect) {
    return !(MAX(seg.pt0.x, seg.pt1.x) < rect.x ||
             MIN(seg.pt0.x, seg.pt1.x) > rect.x+rect.w ||
             MAX(seg.pt0.y, seg.pt1.y) < rect.y ||
             MIN(seg.pt0.y, seg.pt1.y) > rect.y+rect.h);
}

/*
static bool in_worldrect_tsf(WitPoint pt, WitRect rect, View2D *v2d) {
    return ((pt.v[v2d->i_x] >= rect.v[v2d->i_x]) &&
            (pt.v[v2d->i_y] >= rect.v[v2d->i_y]) &&
            (pt.v[v2d->i_x] < rect.v[v2d->i_x]+rect.dv[v2d->i_x]) &&
            (pt.v[v2d->i_y] < rect.v[v2d->i_y]+rect.dv[v2d->i_y]));
}
*/

/*
int draw_grid_player(zgl_PixelArray *scr_p, View2D *v2d) {
    zgl_mPixelRect screen_box = v2d->screenbox;
    
    if (!in_worldrect_tsf(cam.pos, world_box)) return 0;
    
    zgl_mPixel pixel;
    screenpoint_from_worldpoint(cam.pos, &pixel, screen_box, world_box, tsf);
    ufix32_t player_map_radius = screendist_from_worlddist(cam.collision_radius, screen_box.w, world_box.dv[vx]);
    ufix32_t player_map_box_reach = screendist_from_worlddist(cam.collision_box_reach, screen_box.w, world_box.dv[vx]);
    
    zglDraw_mPixelCircle(scr_p, screen_box, (zgl_mPixelCircle){pixel, player_map_radius}, wirecolors.player_collision_circle);

    zgl_mPixelSeg collision_box;

    collision_box = (zgl_mPixelSeg){(zgl_mPixel){pixel.x-player_map_box_reach, pixel.y-player_map_box_reach}, (zgl_mPixel){pixel.x+player_map_box_reach-1, pixel.y-player_map_box_reach}};
    zglDraw_mPixelSeg(scr_p, screen_box, collision_box, wirecolors.player_collision_box);

    collision_box = (zgl_mPixelSeg){(zgl_mPixel){pixel.x-player_map_box_reach, pixel.y-player_map_box_reach}, (zgl_mPixel){pixel.x-player_map_box_reach, pixel.y+player_map_box_reach-1}};
    zglDraw_mPixelSeg(scr_p, screen_box, collision_box, wirecolors.player_collision_box);

    collision_box = (zgl_mPixelSeg){(zgl_mPixel){pixel.x-player_map_box_reach, pixel.y+player_map_box_reach-1}, (zgl_mPixel){pixel.x+player_map_box_reach-1, pixel.y+player_map_box_reach-1}};
    zglDraw_mPixelSeg(scr_p, screen_box, collision_box, wirecolors.player_collision_box);

    collision_box = (zgl_mPixelSeg){(zgl_mPixel){pixel.x+player_map_box_reach-1, pixel.y-player_map_box_reach}, (zgl_mPixel){pixel.x+player_map_box_reach-1, pixel.y+player_map_box_reach-1}};
    zglDraw_mPixelSeg(scr_p, screen_box, collision_box, wirecolors.player_collision_box);

    zglDraw_mPixelDot(scr_p, screen_box, pixel, wirecolors.player);



        
    zgl_mPixelSeg view_vec;
    WitPoint wpt = {{
            x_of(cam.pos) + x_of(cam.view_vec)*32,
            y_of(cam.pos) + y_of(cam.view_vec)*32,
            z_of(cam.pos) + z_of(cam.view_vec)*32,
        }};
    screenpoint_from_worldpoint(cam.pos, &view_vec.pt0,
                                screen_box, world_box, tsf);
    screenpoint_from_worldpoint(wpt, &view_vec.pt1,
                                screen_box, world_box, tsf);

    zglDraw_mPixelSeg(scr_p, screen_box, view_vec, wirecolors.player);
    
    return 0;
}
*/


static epm_Result draw_grid_lines(zgl_PixelArray *scr_p, View2D *v2d) {
    zgl_mPixelRect screen_box = v2d->screenbox;
    ufix32_t gridres_screen;
    
    gridres_screen = screendist_from_worlddist(v2d->gridres_world, screen_box.w, v2d->worldbox.w);
    dibassert(gridres_screen > 0);
    
    Fix64Point_2D base_world;
    zgl_mPixel base_screen;
    base_world.x = round_up_pow2(v2d->worldbox.x, v2d->gridres_world_shift);
    base_world.y = round_up_pow2(v2d->worldbox.y, v2d->gridres_world_shift);
    screenpoint_from_worldpoint_64(&base_world, &base_screen, v2d);
    
    zgl_mPixel offset_s;
    offset_s.x = screendist_from_worlddist(base_world.x - v2d->worldbox.x, screen_box.w, v2d->worldbox.w);
    offset_s.y = screendist_from_worlddist(base_world.y - v2d->worldbox.y, screen_box.w, v2d->worldbox.w);
    dibassert(offset_s.x >= 0 && offset_s.y >= 0);

    // Draw horizontal grid lines.   
    zgl_mPixelSeg seg; // the current seg to be drawn
    
    seg.pt0.x = screen_box.x;
    seg.pt0.y = screen_box.y + screen_box.h - offset_s.y;
    seg.pt1.x = screen_box.x + screen_box.w;
    seg.pt1.y = seg.pt0.y;
    zglDraw_mPixelSeg2(scr_p, &screen_box, &seg, world2D_colors.grid_primary_wire);
    for (uint32_t i = 1; i <= v2d->num_hori_lines; i++) {
        seg.pt0.y -= gridres_screen; // start at bottommost line, work up
        seg.pt1.y = seg.pt0.y;
        zglDraw_mPixelSeg2(scr_p, &screen_box, &seg, world2D_colors.grid_primary_wire);
    }

    // Draw vertical grid lines.
    seg.pt0.x = screen_box.x + offset_s.x;
    seg.pt0.y = screen_box.y;
    seg.pt1.x = seg.pt0.x;
    seg.pt1.y = screen_box.y + screen_box.w;   
    zglDraw_mPixelSeg2(scr_p, &screen_box, &seg, world2D_colors.grid_primary_wire);
    for (uint32_t i = 1; i <= v2d->num_vert_lines; i++) {
        seg.pt0.x += gridres_screen;
        seg.pt1.x = seg.pt0.x;
        zglDraw_mPixelSeg2(scr_p, &screen_box, &seg, world2D_colors.grid_primary_wire);
    }

    // Draw origin (axis) lines, which are thicker than others
    Fix64Point_2D origin_w = {0,0};
    zgl_mPixel origin_s;
    screenpoint_from_worldpoint_64(&origin_w, &origin_s, v2d);
    
    zglDraw_mPixelSeg
        (scr_p, &screen_box,
         (zgl_mPixel){origin_s.x-(1<<16), screen_box.y},
         (zgl_mPixel){origin_s.x-(1<<16), screen_box.y + screen_box.w},
         world2D_colors.grid_axis_edge_wire);
    
    zglDraw_mPixelSeg
        (scr_p, &screen_box,
         (zgl_mPixel){origin_s.x, screen_box.y},
         (zgl_mPixel){origin_s.x, screen_box.y + screen_box.w},
         world2D_colors.grid_axis_center_wire);
    
    zglDraw_mPixelSeg
        (scr_p, &screen_box,
         (zgl_mPixel){origin_s.x+(1<<16), screen_box.y},
         (zgl_mPixel){origin_s.x+(1<<16), screen_box.y + screen_box.w},
         world2D_colors.grid_axis_edge_wire);

    zglDraw_mPixelSeg
        (scr_p, &screen_box,
         (zgl_mPixel){screen_box.x, origin_s.y-(1<<16)},
         (zgl_mPixel){screen_box.x + screen_box.w, origin_s.y-(1<<16)},
         world2D_colors.grid_axis_edge_wire);
    
    zglDraw_mPixelSeg
        (scr_p, &screen_box,
         (zgl_mPixel){screen_box.x, origin_s.y},
         (zgl_mPixel){screen_box.x + screen_box.w, origin_s.y},
         world2D_colors.grid_axis_center_wire);
    
    zglDraw_mPixelSeg
        (scr_p, &screen_box,
         (zgl_mPixel){screen_box.x, origin_s.y+(1<<16)},
         (zgl_mPixel){screen_box.x + screen_box.w, origin_s.y+(1<<16)},
         world2D_colors.grid_axis_edge_wire);

    //temporary, draw grid base point
    zglDraw_mPixelDot(scr_p, &screen_box,
                      &(zgl_mPixel){screen_box.x + offset_s.x,
                                    screen_box.y + screen_box.h - offset_s.y},
                      0x0FF0F0);

    return EPM_SUCCESS;
}

static epm_Result draw_grid_brushgeo(zgl_PixelArray *scr_p, View2D *v2d) {
    zgl_mPixelRect screenbox = v2d->screenbox;
    Fix64Seg_2D trans_MPS;
    zgl_mPixelSeg clipped_MPS;

    for (BrushNode *node = brushgeo.head; node; node = node->next) {
        Brush *brush = node->brush;

        for (uint32_t i_e = 0; i_e < brush->num_edges; i_e++) {
            screenpoint_from_worldpoint(&brush->vertices[brush->edges[i_e].i_v0],
                                        &trans_MPS.pt0, v2d);
            screenpoint_from_worldpoint(&brush->vertices[brush->edges[i_e].i_v1],
                                        &trans_MPS.pt1, v2d);

            if ( ! potentially_visible_seg(trans_MPS, screenbox)) continue;
        
            epm_ClipSegToRect_2D(trans_MPS, screenbox, &clipped_MPS);
            zglDraw_mPixelSeg2(scr_p, &screenbox, &clipped_MPS, color_brush);

            zgl_mPixel middle;

            screenpoint_from_worldpoint_64(&v2d->center, &middle, v2d);

            /*
            middle = closest_point_2D(middle, clipped_MPS);
            if (zgl_in_mPixelRect(middle, screenbox))
                zglDraw_mPixelDot(scr_p, &screenbox, &middle, 0xFFFF00);
            */
            
            for (uint32_t i_v = 0; i_v < brush->num_vertices; i_v++) {
                Fix64Point_2D tmp1;
                zgl_mPixel tmp2;

                screenpoint_from_worldpoint(&brush->vertices[i_v], &tmp1, v2d);
                tmp2.x = (fix32_t)tmp1.x;
                tmp2.y = (fix32_t)tmp1.y;
                if (zgl_in_mPixelRect(tmp2, screenbox))
                    zglDraw_mPixelDot(scr_p, &screenbox, &tmp2, color_brush);
            }
        }
    }
    
    return 0;
}

static epm_Result draw_grid_staticgeo(zgl_PixelArray *scr_p, View2D *v2d) {
    zgl_mPixelRect screenbox = v2d->screenbox;
    Fix64Seg_2D trans_MPS;
    zgl_mPixelSeg clipped_MPS;
    
    for (uint32_t i = 0; i < staticgeo.num_edges; i++) {

        screenpoint_from_worldpoint(&staticgeo.vertices[staticgeo.edges[i].i_v0],
                                    &trans_MPS.pt0, v2d);
        screenpoint_from_worldpoint(&staticgeo.vertices[staticgeo.edges[i].i_v1],
                                    &trans_MPS.pt1, v2d);

        if ( ! potentially_visible_seg(trans_MPS, screenbox)) continue;
        
        epm_ClipSegToRect_2D(trans_MPS, screenbox, &clipped_MPS);
        zglDraw_mPixelSeg2(scr_p, &screenbox, &clipped_MPS, world2D_colors.default_wire);

        zgl_mPixel middle;

        screenpoint_from_worldpoint_64(&v2d->center, &middle, v2d);

        middle = closest_point_2D(middle, clipped_MPS);
        if (zgl_in_mPixelRect(middle, screenbox))
            zglDraw_mPixelDot(scr_p, &screenbox, &middle, 0xFFFF00);
    }

    for (uint32_t i = 0; i < staticgeo.num_vertices; i++) {
        Fix64Point_2D tmp1;
        zgl_mPixel tmp2;

        screenpoint_from_worldpoint(&staticgeo.vertices[i], &tmp1, v2d);
        tmp2.x = (fix32_t)tmp1.x;
        tmp2.y = (fix32_t)tmp1.y;
        if (zgl_in_mPixelRect(tmp2, screenbox))
            zglDraw_mPixelDot(scr_p, &screenbox, &tmp2, world2D_colors.default_wire);
    }
    
    return 0;
}

static epm_Result draw_grid_bspgeo(zgl_PixelArray *scr_p, View2D *v2d) {
    zgl_mPixelRect screenbox = v2d->screenbox;
    Fix64Seg_2D trans_MPS;
    zgl_mPixelSeg clipped_MPS;
    
    for (uint32_t i = 0; i < bsp.num_edges; i++) {

        screenpoint_from_worldpoint(&bsp.vertices[bsp.edges[i].i_v0],
                                    &trans_MPS.pt0, v2d);
        screenpoint_from_worldpoint(&bsp.vertices[bsp.edges[i].i_v1],
                                    &trans_MPS.pt1, v2d);

        if ( ! potentially_visible_seg(trans_MPS, screenbox)) continue;
        
        epm_ClipSegToRect_2D(trans_MPS, screenbox, &clipped_MPS);
        zglDraw_mPixelSeg2(scr_p, &screenbox, &clipped_MPS, world2D_colors.default_wire);

        zgl_mPixel middle;

        screenpoint_from_worldpoint_64(&v2d->center, &middle, v2d);

        middle = closest_point_2D(middle, clipped_MPS);
        if (zgl_in_mPixelRect(middle, screenbox))
            zglDraw_mPixelDot(scr_p, &screenbox, &middle, 0xFFFF00);
    }

    for (uint32_t i = 0; i < bsp.num_vertices; i++) {
        Fix64Point_2D tmp1;
        zgl_mPixel tmp2;

        screenpoint_from_worldpoint(&bsp.vertices[i], &tmp1, v2d);
        tmp2.x = (fix32_t)tmp1.x;
        tmp2.y = (fix32_t)tmp1.y;
        if (zgl_in_mPixelRect(tmp2, screenbox))
            zglDraw_mPixelDot(scr_p, &screenbox, &tmp2, world2D_colors.default_wire);
    }
    
    return 0;
    
}

static void draw_wireframe_2D(zgl_PixelArray *scr_p, View2D *v2d, EdgeSet *eset, zgl_Color color, uint8_t flags) {
    size_t num_vertices = eset->num_vertices;
    WitPoint *vertices = eset->vertices;
    size_t num_edges = eset->num_edges;
    Edge *edges = eset->edges;
    
    Fix64Seg_2D trans_MPS;
    zgl_mPixelSeg clipped_MPS;
    
    for (uint32_t i = 0; i < num_edges; i++) {
        screenpoint_from_worldpoint(vertices + edges[i].i_v0,
                                    &trans_MPS.pt0, v2d);
        screenpoint_from_worldpoint(vertices + edges[i].i_v1,
                                    &trans_MPS.pt1, v2d);

        if ( ! potentially_visible_seg(trans_MPS, v2d->screenbox)) continue;

        epm_ClipSegToRect_2D(trans_MPS, v2d->screenbox, &clipped_MPS);
        if ( ! flags) {
            zglDraw_mPixelSeg(scr_p, &v2d->screenbox, clipped_MPS.pt0, clipped_MPS.pt1, color);    
        }
        else {
            zglDraw_mPixelSeg_Dotted(scr_p, &v2d->screenbox, clipped_MPS.pt0, clipped_MPS.pt1, color);
        }
        
    }
    
    for (uint32_t i = 0; i < num_vertices; i++) {
        screenpoint_from_worldpoint(vertices + i, &trans_MPS.pt0, v2d);
        zglDraw_mPixelDot(scr_p, &v2d->screenbox, &clipped_MPS.pt0, color);
    }
}

static void draw_bigbox_2D(zgl_PixelArray *scr_p, View2D *v2d, EdgeSet *eset) {
    /* Very similar to draw_wireframe_2D, but I may want to handle the bigbox differently in the future. */
    size_t num_vertices = eset->num_vertices;
    WitPoint *vertices = eset->vertices;
    size_t num_edges = eset->num_edges;
    Edge *edges = eset->edges;
    
    Fix64Seg_2D trans_MPS;
    zgl_mPixelSeg clipped_MPS;
    
    for (uint32_t i = 0; i < num_edges; i++) {
        screenpoint_from_worldpoint(vertices + edges[i].i_v0,
                                    &trans_MPS.pt0, v2d);
        screenpoint_from_worldpoint(vertices + edges[i].i_v1,
                                    &trans_MPS.pt1, v2d);

        if ( ! potentially_visible_seg(trans_MPS, v2d->screenbox)) continue;

        epm_ClipSegToRect_2D(trans_MPS, v2d->screenbox, &clipped_MPS);
        zglDraw_mPixelSeg2(scr_p, &v2d->screenbox, &clipped_MPS, 0x2596BE);
    }
    
    for (uint32_t i = 0; i < num_vertices; i++) {
        screenpoint_from_worldpoint(vertices + i, &trans_MPS.pt0, v2d);
        clipped_MPS.pt0.x = (fix32_t)trans_MPS.pt0.x;
        clipped_MPS.pt0.y = (fix32_t)trans_MPS.pt0.y;
        zglDraw_mPixelDot(scr_p, &v2d->screenbox, &clipped_MPS.pt0, 0x2596BE);
    }
}


void draw_View2D(Window *win, zgl_PixelArray *scr_p) {
    Plane tsf = *(Plane *)(win->data);
    View2D *v2d = &view2D[tsf];

    /* Generate new zoom table.
    static bool generated = true;
    if (!generated) {
        int i = 0;
        for (ufix64_t zoom = MIN_ZOOM; zoom <= MAX_ZOOM;) {
            zooms[i++] = zoom;
            num_zooms++;
            printf("%#lX, ", zoom);
            zoom = (zoom<<3)/7;
        }
        printf("\n%i zoom levels\n", num_zooms);
        generated = true;
    }
    */

    v2d->screenbox = win->mrect;
    set_zoom_level(v2d);
    v2d->worldbox.x = (v2d->center.x - v2d->worldbox.w/2);
    v2d->worldbox.y = (v2d->center.y - v2d->worldbox.h/2);

    // Grid background.
    zgl_GrayRect(scr_p,
                 intify(v2d->screenbox.x),
                 intify(v2d->screenbox.y),
                 intify(v2d->screenbox.w),
                 intify(v2d->screenbox.h),
                 0x2F);
    draw_grid_lines(scr_p, v2d);

    // Draw the Big Box
    draw_bigbox_2D(scr_p, v2d, &view3D_bigbox);

    // Draw World Geometry (in either brush, triangulated, or BSP form)
    draw_grid_brushgeo(scr_p, v2d);
    //draw_grid_staticgeo(scr_p, v2d);
    //draw_grid_bspgeo(scr_p, v2d);

    // Draw brush selection.
    for (BrushSelectionNode *node = brushsel.head; node; node = node->next) {
        Brush *brush = (Brush *)node->brush;

        draw_wireframe_2D(scr_p, v2d, &EdgeSet_from_Mesh(*brush), color_selected_brush, 0);
    }
    
    // Draw framebrush
    draw_wireframe_2D(scr_p, v2d, &EdgeSet_from_Mesh(*frame), color_brushframe, 1);
        
    // Draw brush selection point-of-reference
    Fix64Seg_2D trans_MPS;
    zgl_mPixelSeg clipped_MPS;

    screenpoint_from_worldpoint(&brushsel.POR, &trans_MPS.pt0, v2d);
    clipped_MPS.pt0.x = (fix32_t)trans_MPS.pt0.x;
    clipped_MPS.pt0.y = (fix32_t)trans_MPS.pt0.y;
    zglDraw_mPixelDot(scr_p, &v2d->screenbox, &clipped_MPS.pt0, 0xFABCDE);
}
