#include "src/draw/draw_triangle.h"

// weird includes
#include "src/draw/draw3D.h"

/* The draw_Triangle#() functions are written under the assumption that the
   triangle is visible and non-degenerate. TODO: Explain more clearly what
   "visible" means.

   These functions are very closely related, but have certain differences:

         Light or NoLight
   Perpsective or Orthographic
     Bresenham or BoundingBox
*/

uint32_t g_rasterflags = 0;

#define TEXEL_PREC 5

static void compute_xspans(Window *win, zgl_Pixel v0, zgl_Pixel v1, zgl_Pixel v2, uint32_t *xspans, zgl_Pixit *_min_y, zgl_Pixit *_max_y);

draw_Triangle_fnc draw_Tri[4] = {
    [0] = draw_Triangle,
    [RASTERFLAG_NO_LIGHT] = draw_Triangle_NoLight,
    [RASTERFLAG_ORTHO] = draw_Triangle_Orth,
    [RASTERFLAG_NO_LIGHT | RASTERFLAG_ORTHO] = draw_Triangle_NoLight_Orth,
};

#define triangle_area_2D(a, b, c) ((fix64_t)((c).x - (a).x) * (fix64_t)((b).y - (a).y) - (fix64_t)((c).y - (a).y) * (fix64_t)((b).x - (a).x))

static uint32_t xspans[2160] = {0};

static uint64_t g_num_drawn = 0;
bool g_all_pixels_drawn = false;
void reset_rasterizer(void) {
    g_num_drawn = 0;
    g_all_pixels_drawn = false;
}

void draw_Triangle
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri) {
    if (g_all_pixels_drawn) return;
    
    Fix64Point_2D
        scr_v0 = tri->pixels[0]->XY,
        scr_v1 = tri->pixels[1]->XY,
        scr_v2 = tri->pixels[2]->XY;
    
    zgl_Pixit min_x, min_y, max_x, max_y;
    compute_xspans(win, pixel_from_mpixel(scr_v0), pixel_from_mpixel(scr_v1), pixel_from_mpixel(scr_v2), xspans, &min_y, &max_y);
    
    min_y = MAX(min_y, win->rect.y);
    max_y = MIN(max_y, win->rect.y + win->rect.h - 1);

    fix64_t V21_x = scr_v2.x - scr_v1.x;
    fix64_t V21_y = scr_v2.y - scr_v1.y;
    fix64_t V02_x = scr_v0.x - scr_v2.x;
    fix64_t V02_y = scr_v0.y - scr_v2.y;
    fix64_t V10_x = scr_v1.x - scr_v0.x;
    fix64_t V10_y = scr_v1.y - scr_v0.y;

    zgl_mPixel
        tx0 = *tri->texels[0],
        tx1 = *tri->texels[1],
        tx2 = *tri->texels[2];
    
    tx0.x >>= 16-TEXEL_PREC;
    tx0.y >>= 16-TEXEL_PREC;
    tx1.x >>= 16-TEXEL_PREC;
    tx1.y >>= 16-TEXEL_PREC;
    tx2.x >>= 16-TEXEL_PREC;
    tx2.y >>= 16-TEXEL_PREC;
    
    zgl_Color *tx_pixels = tri->texture->pixels;


    /* I declare minimal storage width for some values here in the hopes that
       the compiler can make efficient use of space. It doesn't seem to actually
       have a noticeable effect, however. 2023-04-25 */
    uint8_t tx_w_shift = 0;
    if (tri->texture->w == 256)
        tx_w_shift = 8;
    else if (tri->texture->w == 128)
        tx_w_shift = 7;
    else
        dibassert(false);
    
    uint16_t u_mod = (uint16_t)(tri->texture->w - 1);
    uint16_t v_mod = (uint16_t)(tri->texture->h - 1);
    uint64_t brightness = tri->brightness;
    uint64_t vbri0 = tri->vbri[0];
    uint64_t vbri1 = tri->vbri[1];
    uint64_t vbri2 = tri->vbri[2];
        
    zgl_Color *scr_curr;
    zgl_Color *scr_start = scr_p->pixels;
    uint16_t scr_w = (uint16_t)scr_p->w;
    
    fix64_t
        I0 = *tri->zinv[0],
        I1 = *tri->zinv[1],
        I2 = *tri->zinv[2];
    //1.32

    fix64_t
        d_W0 = FIX_MUL(I0, V21_y),
        d_W1 = FIX_MUL(I1, V02_y),
        d_W2 = FIX_MUL(I2, V10_y);
    // 27.16

    fix64_t
        d_bri     = d_W0*vbri0 + d_W1*vbri1 + d_W2*vbri2,
        d_numer_x = d_W0*tx0.x + d_W1*tx1.x + d_W2*tx2.x,
        d_numer_y = d_W0*tx0.y + d_W1*tx1.y + d_W2*tx2.y,
        d_denom   = d_W0       + d_W1       + d_W2;
    
    fix64_t C0, C1, C2; // barycentric coordinates of current pixel
    fix64_t W0, W1, W2; // Ci / Zi
    ufix64_t denom; // W0 + W1 + W2
    ufix64_t vbri; // fiodjfodifjodifjdoifjd
    ufix64_t numer_x; // W0*tx0.x + W1*tx1.x + W2*tx2.x
    ufix64_t numer_y; // W0*tx0.y + W1*tx1.y + W2*tx2.y
    ptrdiff_t scr_inc = min_y*scr_w; // just an attempt to avoid yet another
                                     // multiplication; not sure if worth it

    uint16_t tmp_num_drawn = 0;
    
    for (uint16_t y = (uint16_t)min_y; y <= max_y; y++) {
        min_x = xspans[y<<1], max_x = xspans[(y<<1) + 1] - 1;

        // TODO: The multiplication for W0, W1, W2 overflows when camera too
        // close to triangle. 2023-05-05
        
        C0 = (((fixify(min_x) - scr_v1.x)*V21_y) -
              ((fixify(    y) - scr_v1.y)*V21_x))>>16; // 22.32
        W0 = FIX_MUL(I0, C0); // 38.16
        
        C1 = (((fixify(min_x) - scr_v2.x)*V02_y) -
              ((fixify(    y) - scr_v2.y)*V02_x))>>16;
        W1 = FIX_MUL(I1, C1);

        C2 = (((fixify(min_x) - scr_v0.x)*V10_y) -
              ((fixify(    y) - scr_v0.y)*V10_x))>>16;
        W2 = FIX_MUL(I2, C2);

        vbri    = W0*vbri0 + W1*vbri1 + W2*vbri2;
        numer_x = W0*tx0.x + W1*tx1.x + W2*tx2.x;
        numer_y = W0*tx0.y + W1*tx1.y + W2*tx2.y;
        denom   = W0       + W1       + W2;
        
        scr_curr = scr_start + min_x + scr_inc;
        scr_inc += scr_w;

        for (uint16_t x = (uint16_t)min_x; x <= max_x;
             x++, scr_curr++, vbri += d_bri, numer_x += d_numer_x, numer_y += d_numer_y,
                 denom += d_denom) {
            if (*scr_curr & (1LL<<31)) { // already drawn
                continue;
            }

            /* These divisions cut the frame rate in half. 2023-04-25 */
            int16_t u = (int16_t)((numer_x>>TEXEL_PREC)/denom);
            int16_t v = (int16_t)((numer_y>>TEXEL_PREC)/denom);
            
            zgl_Color color = tx_pixels[(u & u_mod) + ((v & v_mod)<<tx_w_shift)];


            {
            // TODO: Interpolate brightness from vertex brightnesses
                brightness = vbri/denom;
                //                dibassert(brightness < 255);
            }
            
            color = ((((color & 0x00FF00FFu) * brightness) & 0xFF00FF00u) |
                     (((color & 0x0000FF00u) * brightness) & 0x00FF0000u)) >> 8;
            
            *scr_curr = color | (1LL<<31);
            tmp_num_drawn++;
        }
    }

    g_num_drawn += tmp_num_drawn;
    if (g_num_drawn >= (uint64_t)(win->rect.w * win->rect.h))
        g_all_pixels_drawn = true;
}

void draw_Triangle_NoLight
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri) {
    Fix64Point_2D
        scr_v0 = tri->pixels[0]->XY,
        scr_v1 = tri->pixels[1]->XY,
        scr_v2 = tri->pixels[2]->XY;
    
    zgl_Pixit min_x, min_y, max_x, max_y;

    uint32_t xspans[2160] = {0};
    compute_xspans(win, pixel_from_mpixel(scr_v0), pixel_from_mpixel(scr_v1), pixel_from_mpixel(scr_v2), xspans, &min_y, &max_y);
    
    min_y = MAX(min_y, win->rect.y);
    max_y = MIN(max_y, win->rect.y + win->rect.h - 1);

    fix64_t V21_x = scr_v2.x - scr_v1.x;
    fix64_t V21_y = scr_v2.y - scr_v1.y;
    fix64_t V02_x = scr_v0.x - scr_v2.x;
    fix64_t V02_y = scr_v0.y - scr_v2.y;
    fix64_t V10_x = scr_v1.x - scr_v0.x;
    fix64_t V10_y = scr_v1.y - scr_v0.y;
    //11.16

    zgl_mPixel
        tx0 = *tri->texels[0],
        tx1 = *tri->texels[1],
        tx2 = *tri->texels[2];
    
    tx0.x >>= 16-TEXEL_PREC;
    tx0.y >>= 16-TEXEL_PREC;
    tx1.x >>= 16-TEXEL_PREC;
    tx1.y >>= 16-TEXEL_PREC;
    tx2.x >>= 16-TEXEL_PREC;
    tx2.y >>= 16-TEXEL_PREC;
    
    zgl_Color *tx_pixels = tri->texture->pixels;
    uint32_t tx_w_shift = 0;
    if (tri->texture->w == 256)
        tx_w_shift = 8;
    else if (tri->texture->w == 128)
        tx_w_shift = 7;
    uint32_t u_mod = tri->texture->w - 1; 
    uint32_t v_mod = tri->texture->h - 1;

    zgl_Color *scr_curr;
    zgl_Color *scr_start = scr_p->pixels;
    uint32_t scr_w = scr_p->w;
    
    fix64_t
        I0 = *tri->zinv[0],
        I1 = *tri->zinv[1],
        I2 = *tri->zinv[2];
    //1.32
    // NOTE: Zi are guaranteed to be > frustum near plane, which is 1<<16

    fix64_t
        d_W0 = FIX_MUL(I0, V21_y),
        d_W1 = FIX_MUL(I1, V02_y),
        d_W2 = FIX_MUL(I2, V10_y);
    // 27.16

    fix64_t
        d_numer_x = d_W0*tx0.x + d_W1*tx1.x + d_W2*tx2.x,
        d_numer_y = d_W0*tx0.y + d_W1*tx1.y + d_W2*tx2.y,
        d_denom   = d_W0 + d_W1 + d_W2;
    
    fix64_t C0, C1, C2; // barycentric coordinates of current pixel ?
    fix64_t W0, W1, W2; // Ci / Zi
    ufix64_t numer_x; // W0*tx0.x + W1*tx1.x + W2*tx2.x
    ufix64_t numer_y; // W0*tx0.y + W1*tx1.y + W2*tx2.y
    ufix64_t denom;   // W0 + W1 + W2
    ptrdiff_t scr_inc = min_y*scr_w; // just an attempt to avoid yet another
                                     // multiplication; not sure if worth it
    
    for (zgl_Pixit y = min_y; y <= max_y; y++) {
        min_x = xspans[y<<1], max_x = xspans[(y<<1) + 1] - 1;

        C0 = ((( fixify(min_x) - scr_v1.x)*V21_y) // 22.32
              - ((fixify(    y) - scr_v1.y)*V21_x))>>16; // 22.32
        W0 = FIX_MUL(I0, C0); // 38.16
        
        C1 = ((( fixify(min_x) - scr_v2.x)*V02_y)
              - ((fixify(    y) - scr_v2.y)*V02_x))>>16;
        W1 = FIX_MUL(I1, C1);

        C2 = ((( fixify(min_x) - scr_v0.x)*V10_y)
              - ((fixify(    y) - scr_v0.y)*V10_x))>>16;
        W2 = FIX_MUL(I2, C2);

        numer_x = W0*tx0.x + W1*tx1.x + W2*tx2.x;
        numer_y = W0*tx0.y + W1*tx1.y + W2*tx2.y;
        denom   = W0       + W1       + W2;
        
        scr_curr = scr_start + min_x + scr_inc;
        scr_inc += scr_w;
        
        for (zgl_Pixit x = min_x; x <= max_x;
             x++, scr_curr++, /*W0 += d_W0, W1 += d_W1, W2 += d_W2,*/
                 numer_x += d_numer_x, numer_y += d_numer_y, denom += d_denom) {
            if (*scr_curr & (1LL<<31)) { // already drawn
                continue;
            }
            g_num_drawn++;
            /*
            // TODO: I soooo wish I could remove this check.
            if (W0 < 0 || W1 < 0 || W2 < 0)
                continue;
            */

            *scr_curr = tx_pixels[(((numer_x>>TEXEL_PREC)/denom) & u_mod) +
                                  ((((numer_y>>TEXEL_PREC)/denom) & v_mod)<<tx_w_shift)] | (1LL<<31);

        }
    }
}

void draw_Triangle_Orth
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri) {
    /* Essentially the same as draw_Triangle3() except that we can get away with
       linear interpolation of barycentric coordinates, saving some time (TODO:
       test this claim) */
    
    Fix64Point_2D
        scr_v0 = tri->pixels[0]->XY,
        scr_v1 = tri->pixels[1]->XY,
        scr_v2 = tri->pixels[2]->XY;
    
    zgl_Pixit min_x, min_y, max_x, max_y;

    uint32_t xspans[2160] = {0};    
    compute_xspans(win, pixel_from_mpixel(scr_v0), pixel_from_mpixel(scr_v1), pixel_from_mpixel(scr_v2), xspans, &min_y, &max_y);
    
    min_y = MAX(min_y, win->rect.y);
    max_y = MIN(max_y, win->rect.y + win->rect.h - 1);

    fix64_t V21_x = scr_v2.x - scr_v1.x;
    fix64_t V21_y = scr_v2.y - scr_v1.y;
    fix64_t V02_x = scr_v0.x - scr_v2.x;
    fix64_t V02_y = scr_v0.y - scr_v2.y;
    //fix64_t V10_x = scr_v1.x - scr_v0.x;
    fix64_t V10_y = scr_v1.y - scr_v0.y;
    //11.16

    zgl_mPixel
        tx0 = *tri->texels[0],
        tx1 = *tri->texels[1],
        tx2 = *tri->texels[2];

    tx0.x >>= 16-TEXEL_PREC;
    tx0.y >>= 16-TEXEL_PREC;
    tx1.x >>= 16-TEXEL_PREC;
    tx1.y >>= 16-TEXEL_PREC;
    tx2.x >>= 16-TEXEL_PREC;
    tx2.y >>= 16-TEXEL_PREC;
    
    zgl_Color *tx_pixels = tri->texture->pixels;
    uint32_t tx_w_shift = 0;
    if (tri->texture->w == 256)
        tx_w_shift = 8;
    else if (tri->texture->w == 128)
        tx_w_shift = 7;
    uint32_t u_mod = tri->texture->w - 1; 
    uint32_t v_mod = tri->texture->h - 1;
    fix32_t brightness = tri->brightness;
    
    zgl_Color *scr_curr;
    zgl_Color *scr_start = scr_p->pixels;
    uint32_t scr_w = scr_p->w;
    
    fix64_t
        d_C0 = V21_y,
        d_C1 = V02_y,
        d_C2 = V10_y;
    // 27.16
    
    fix64_t
        d_numer_x = d_C0*tx0.x + d_C1*tx1.x + d_C2*tx2.x,
        d_numer_y = d_C0*tx0.y + d_C1*tx1.y + d_C2*tx2.y;

    fix64_t area = triangle_area_2D(scr_v0, scr_v1, scr_v2)>>16;
    
    fix64_t C0, C1, C2;
    ufix64_t numer_x; // C0*tx0.x + C1*tx1.x + C2*tx2.x
    ufix64_t numer_y; // C0*tx0.y + C1*tx1.y + C2*tx2.y
    ptrdiff_t scr_inc = min_y*scr_w; // just an attempt to avoid yet another
                                     // multiplication; not sure if worth it

    for (zgl_Pixit y = min_y; y <= max_y; y++) {
        min_x = xspans[y<<1], max_x = xspans[(y<<1) + 1] - 1;

        C0 = (((fixify(min_x) - scr_v1.x)*V21_y) -
              ((fixify(    y) - scr_v1.y)*V21_x))>>16; // 22.16
        
        C1 = (((fixify(min_x) - scr_v2.x)*V02_y) -
              ((fixify(    y) - scr_v2.y)*V02_x))>>16;

        C2 = area - C0 - C1;
        
        numer_x = C0*tx0.x + C1*tx1.x + C2*tx2.x;
        numer_y = C0*tx0.y + C1*tx1.y + C2*tx2.y;
        
        scr_curr = scr_start + min_x + scr_inc;
        scr_inc += scr_w;
        
        for (zgl_Pixit x = min_x; x <= max_x;
             x++, scr_curr++, numer_x += d_numer_x, numer_y += d_numer_y) {
            if (*scr_curr & (1LL<<31)) { // already drawn
                continue;
            }
            g_num_drawn++;
            
            fix64_t u = (numer_x>>TEXEL_PREC)/area;
            fix64_t v = (numer_y>>TEXEL_PREC)/area;
            // TODO: I could move the divisions out of the loop at a loss of
            // precision. Investigate.
            
            zgl_Color color = tx_pixels[(u & u_mod) + ((v & v_mod)<<tx_w_shift)];
            color = ((((color & 0x00FF00FFu) * brightness) & 0xFF00FF00u) |
                     (((color & 0x0000FF00u) * brightness) & 0x00FF0000u)) >> 8;

            
            *scr_curr = color | (1LL<<31);
        }
    }
}

void draw_Triangle_NoLight_Orth
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri) {
    /* Essentially the same as draw_Triangle3() except that we can get away with
       linear interpolation of barycentric coordinates, saving some time (TODO:
       test this claim) */
    
    Fix64Point_2D
        scr_v0 = tri->pixels[0]->XY,
        scr_v1 = tri->pixels[1]->XY,
        scr_v2 = tri->pixels[2]->XY;
    
    zgl_Pixit min_x, min_y, max_x, max_y;

    uint32_t xspans[2160] = {0};    
    compute_xspans(win, pixel_from_mpixel(scr_v0), pixel_from_mpixel(scr_v1), pixel_from_mpixel(scr_v2), xspans, &min_y, &max_y);
    
    min_y = MAX(min_y, win->rect.y);
    max_y = MIN(max_y, win->rect.y + win->rect.h - 1);

    fix64_t V21_x = scr_v2.x - scr_v1.x;
    fix64_t V21_y = scr_v2.y - scr_v1.y;
    fix64_t V02_x = scr_v0.x - scr_v2.x;
    fix64_t V02_y = scr_v0.y - scr_v2.y;
    //fix64_t V10_x = scr_v1.x - scr_v0.x;
    fix64_t V10_y = scr_v1.y - scr_v0.y;
    //11.16

    zgl_mPixel
        tx0 = *tri->texels[0],
        tx1 = *tri->texels[1],
        tx2 = *tri->texels[2];


    tx0.x >>= 16-TEXEL_PREC;
    tx0.y >>= 16-TEXEL_PREC;
    tx1.x >>= 16-TEXEL_PREC;
    tx1.y >>= 16-TEXEL_PREC;
    tx2.x >>= 16-TEXEL_PREC;
    tx2.y >>= 16-TEXEL_PREC;
    
    zgl_Color *tx_pixels = tri->texture->pixels;
    uint32_t tx_w_shift = 0;
    if (tri->texture->w == 256)
        tx_w_shift = 8;
    else if (tri->texture->w == 128)
        tx_w_shift = 7;
    uint32_t u_mod = tri->texture->w - 1; 
    uint32_t v_mod = tri->texture->h - 1;
    
    zgl_Color *scr_curr;
    zgl_Color *scr_start = scr_p->pixels;
    uint32_t scr_w = scr_p->w;
    
    fix64_t
        d_C0 = V21_y,
        d_C1 = V02_y,
        d_C2 = V10_y;
    // 27.16
    
    fix64_t
        d_numer_x = d_C0*tx0.x + d_C1*tx1.x + d_C2*tx2.x,
        d_numer_y = d_C0*tx0.y + d_C1*tx1.y + d_C2*tx2.y;

    fix64_t area = triangle_area_2D(scr_v0, scr_v1, scr_v2)>>16;
    
    fix64_t C0, C1, C2;
    ufix64_t numer_x; // C0*tx0.x + C1*tx1.x + C2*tx2.x
    ufix64_t numer_y; // C0*tx0.y + C1*tx1.y + C2*tx2.y
    ptrdiff_t scr_inc = min_y*scr_w; // just an attempt to avoid yet another
                                     // multiplication; not sure if worth it

    for (zgl_Pixit y = min_y; y <= max_y; y++) {
        min_x = xspans[y<<1], max_x = xspans[(y<<1) + 1] - 1;

        C0 = (((fixify(min_x) - scr_v1.x)*V21_y) -
              ((fixify(    y) - scr_v1.y)*V21_x))>>16; // 22.16
        
        C1 = (((fixify(min_x) - scr_v2.x)*V02_y) -
              ((fixify(    y) - scr_v2.y)*V02_x))>>16;

        C2 = area - C0 - C1;
        
        numer_x = C0*tx0.x + C1*tx1.x + C2*tx2.x;
        numer_y = C0*tx0.y + C1*tx1.y + C2*tx2.y;
        
        scr_curr = scr_start + min_x + scr_inc;
        scr_inc += scr_w;
        
        for (zgl_Pixit x = min_x; x <= max_x;
             x++, scr_curr++, numer_x += d_numer_x, numer_y += d_numer_y) {
            if (*scr_curr & (1LL<<31)) { // already drawn
                continue;
            }
            g_num_drawn++;
            
            fix64_t u = (numer_x>>TEXEL_PREC)/area;
            fix64_t v = (numer_y>>TEXEL_PREC)/area;
            // TODO: I could move the divisions out of the loop at a loss of
            // precision. Investigate.
            
            *scr_curr = tx_pixels[(u & u_mod) + ((v & v_mod)<<tx_w_shift)] | (1LL<<31);
        }
    }
}



#define LEFT 0
#define RIGHT 1

static void low_bresenham
(Window *win,
 zgl_Pixit x0,
 zgl_Pixit y0,
 zgl_Pixit x1,
 zgl_Pixit y1,
 uint32_t side,
 uint32_t *xspans) {
    int32_t
        min_x = win->rect.x,
        max_x = win->rect.x + win->rect.w - 1,
        min_y = win->rect.y,
        max_y = win->rect.y + win->rect.h - 1; // TODO: no magic
    
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    int32_t yi = 1;
    
    if (dy < 0) {
        yi = -1;
        dy = -dy;
    }

    int32_t D = (LSHIFT32(dy, 1)) - dx;
    
    zgl_Pixit x = x0, y = y0;
    bool y_was_incremented = true;
    
    uint32_t *xspans_p = xspans + (2*y0);
    if (side == RIGHT) xspans_p++;

    for (; x <= x1; x++) {
        if (y_was_incremented) {
            if (y >= min_y && max_y >= y) {
                *xspans_p = MIN(MAX(x, min_x), max_x);
            }
            xspans_p += LSHIFT32(yi, 1);
            y_was_incremented = false;
	    }
        
        if (D > 0) {
            y = y + yi;
            D = D + (LSHIFT32((dy - dx), 1));
            y_was_incremented = true;
        }
        else {
            D = D + (LSHIFT32(dy, 1));
        }
    }

}



static void high_bresenham
(Window *win,
 zgl_Pixit x0,
 zgl_Pixit y0,
 zgl_Pixit x1,
 zgl_Pixit y1,
 uint32_t side,
 uint32_t *xspans) {
    int32_t
        min_x = win->rect.x,
        max_x = win->rect.x + win->rect.w - 1,
        min_y = win->rect.y,
        max_y = win->rect.y + win->rect.h - 1; // TODO: no magic

    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    int32_t xi = 1;
    
    if (dx < 0) {
        xi = -1;
        dx = -dx;
    }

    int32_t D = (LSHIFT32(dx, 1)) - dy;
    
    uint32_t *xspans_p = xspans + (2*y0);
    if (side == RIGHT) xspans_p++;
    
    zgl_Pixit x = x0, y = y0;
    for ( ; y <= y1; y++) {
        if (y >= min_y && max_y >= y) {
            *xspans_p = MIN(MAX(x, min_x), max_x);
        }
        xspans_p += 2;
	
        if (D > 0) {
            x = x + xi;
            D = D + (LSHIFT32((dx - dy), 1));
        }
        else {
            D = D + (LSHIFT32(dx, 1));
        }
    }
}


void compute_xspans_line(Window *win, zgl_Pixel L, zgl_Pixel R, uint32_t *xspans, uint32_t side) {
    if (ABS(L.y - R.y) < ABS(L.x - R.x)) {
        if (L.x > R.x) {
            low_bresenham(win, R.x, R.y, L.x, L.y, side, xspans);
        }
        else {
            low_bresenham(win, L.x, L.y, R.x, R.y, side, xspans);
        }
    }
    else {
        if (L.y > R.y) {
            high_bresenham(win, R.x, R.y, L.x, L.y, side, xspans);
        }
        else {
            high_bresenham(win, L.x, L.y, R.x, R.y, side, xspans);
        }
    }
}








void compute_xspans(Window *win, zgl_Pixel v0, zgl_Pixel v1, zgl_Pixel v2, uint32_t *xspans, zgl_Pixit *min_y, zgl_Pixit *max_y) {
    zgl_Pixel low, mid, hi;

    low = v0;
    mid = v1;
    hi = v2;
    
    if (low.y > mid.y) {
        mid = v0;
        low = v1;
    }
    if (mid.y > hi.y)
    {
        hi = mid;
        mid = v2;
        if (low.y > mid.y)
        {
            mid = low;
            low = v2;
        }
    }
    
    *min_y = low.y;
    *max_y = hi.y;

    int  hi_to_mid_side,
         hi_to_low_side,
        mid_to_low_side;
    
    if (low.y == mid.y) {
        // flat bottom; just two lines to do
        if (low.x < mid.x) {
            hi_to_low_side = LEFT;
            hi_to_mid_side = RIGHT;
        }
        else {
            hi_to_low_side = RIGHT;
            hi_to_mid_side = LEFT;

        }
        compute_xspans_line(win, hi, mid, xspans, hi_to_mid_side);
        compute_xspans_line(win, hi, low, xspans, hi_to_low_side);
    }
    else if (hi.y == mid.y) {
        // flat top; just two lines to do
        if (hi.x < mid.x) {
             hi_to_low_side = LEFT;
            mid_to_low_side = RIGHT;
        }
        else {
             hi_to_low_side = RIGHT;
            mid_to_low_side = LEFT;
        }
        compute_xspans_line(win,  hi, low, xspans,  hi_to_low_side);
        compute_xspans_line(win, mid, low, xspans, mid_to_low_side);
    }
    else {
        // neither top nor bottom are flat; three lines to do.

        // is mid point above or below the line joining max and min?
        // if above, two lefts.
        // else, two rights

        if ((mid.y-hi.y)*(low.x-hi.x) > (low.y-hi.y)*(mid.x-hi.x)) {
             hi_to_mid_side = RIGHT;
            mid_to_low_side = RIGHT;
             hi_to_low_side = LEFT;
        }
        else {
             hi_to_mid_side = LEFT;
            mid_to_low_side = LEFT;
             hi_to_low_side = RIGHT;
        }
        
        compute_xspans_line(win,  hi, mid, xspans,  hi_to_mid_side);
        compute_xspans_line(win, mid, low, xspans, mid_to_low_side);
        compute_xspans_line(win,  hi, low, xspans,  hi_to_low_side);
    }
}
#undef LEFT
#undef RIGHT




void draw_solid_triangle
(zgl_PixelArray *scr_p, Window *win, ScreenTriangle *tri, zgl_Color color) {
    if (g_all_pixels_drawn) return;
    
    Fix64Point_2D
        scr_v0 = tri->pixels[0]->XY,
        scr_v1 = tri->pixels[1]->XY,
        scr_v2 = tri->pixels[2]->XY;
    
    zgl_Pixit min_x, min_y, max_x, max_y;
    compute_xspans(win, pixel_from_mpixel(scr_v0), pixel_from_mpixel(scr_v1), pixel_from_mpixel(scr_v2), xspans, &min_y, &max_y);
    
    min_y = MAX(min_y, win->rect.y);
    max_y = MIN(max_y, win->rect.y + win->rect.h - 1);

    zgl_Color *scr_curr;
    zgl_Color *scr_start = scr_p->pixels;
    uint16_t scr_w = (uint16_t)scr_p->w;
    
    ptrdiff_t scr_inc = min_y*scr_w; // just an attempt to avoid yet another
                                     // multiplication; not sure if worth it

    uint16_t tmp_num_drawn = 0;
    
    for (uint16_t y = (uint16_t)min_y; y <= max_y; y++) {
        min_x = xspans[y<<1], max_x = xspans[(y<<1) + 1] - 1;
        
        scr_curr = scr_start + min_x + scr_inc;
        scr_inc += scr_w;

        for (uint16_t x = (uint16_t)min_x; x <= max_x; x++, scr_curr++) {
            if (*scr_curr & (1LL<<31)) { // already drawn
                continue;
            }
            
            *scr_curr = color | (1LL<<31);
            tmp_num_drawn++;
        }
    }

    g_num_drawn += tmp_num_drawn;
    if (g_num_drawn >= (uint64_t)(win->rect.w * win->rect.h))
        g_all_pixels_drawn = true;
}

