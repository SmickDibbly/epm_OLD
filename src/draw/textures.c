#include "src/draw/textures.h"

// weird includes
#include "src/world/geometry.h"

#undef LOG_LABEL
#define LOG_LABEL "TEXTURES"

size_t g_num_textures = 0;
epm_Texture textures[MAX_TEXTURES] = {0};
static zgl_PixelArray *texture_pix[MAX_TEXTURES] = {NULL};

static void texture_name_from_filename(char const *filename, char *tex_name);

static char g_nametmp[MAX_TEXTURE_NAME_LEN + 1] = {'\0'};

epm_Texture *load_Texture(char const *filename) {
    zgl_PixelArray *pix = texture_pix[g_num_textures];
    epm_Texture *tex = &textures[g_num_textures];

    texture_name_from_filename(filename, g_nametmp);
    
    pix = zgl_ReadBMP(filename);
    if ( ! pix) {
        epm_Log(LT_WARN, "Texture named \"%s\" not found from file.\n", g_nametmp);
        return NULL;
    }

    if (pix->w == 0 || pix->h == 0) {
        epm_Log(LT_WARN, "Texture named \"%s\" has 0 width and/or 0 height; cannot use.\n", g_nametmp);
        return NULL;
    }

    if ((pix->w & (pix->w - 1)) != 0 ||
        (pix->h & (pix->h - 1)) != 0 ) {
        epm_Log(LT_WARN, "Texture named \"%s\" has either width or height that is not a power of 2; cannot use.\n", g_nametmp);
        return NULL;
    }
    
    snprintf(tex->name, MAX_TEXTURE_NAME_LEN + 1, "%s", g_nametmp);
    tex->pixarr = pix;
    dibassert(pix->w <= 1024);
    dibassert(pix->h <= 1024);
    tex->w = (uint16_t)pix->w;
    tex->h = (uint16_t)pix->h;
    g_num_textures++;

    memset(g_nametmp, 0, MAX_TEXTURE_NAME_LEN+1);
    
    return tex;
}

epm_Result unload_Texture(epm_Texture *tex) {
    tex->name[0] = '\0';
    zgl_DestroyPixelArray(tex->pixarr);
    tex->w = 0;
    tex->h = 0;

    return EPM_SUCCESS;
}

epm_Result texture_index_from_name(char const *name, size_t *out_i_tex) {
    for (size_t i_tex = 0; i_tex < g_num_textures; i_tex++) {
        if (0 == strcmp(name, textures[i_tex].name)) {
            *out_i_tex = i_tex;
            return EPM_SUCCESS;
        }
    }

    return EPM_FAILURE;
}

static char g_nametmp0[MAX_TEXTURE_NAME_LEN + 1] = {'\0'};
static void texture_name_from_filename(char const *filename, char *tex_name) {
    bool reading = false;
    size_t i = 0;

    size_t len = strlen(filename);
    dibassert(len < INT_MAX);
    
    for (int i_ch = (int)len - 1; i_ch >= 0; i_ch--) {
        if (( ! reading) && (filename[i_ch] == '.')) {
            reading = true;
            continue;
        }
        if (reading && (filename[i_ch] == '/')) {
            break;
        }
        if (reading) {
            g_nametmp0[i] = filename[i_ch];
            i++;
        }
    }

    len = strlen(g_nametmp0);
    dibassert(len < INT_MAX);
    
    for (int i_ch0 = (int)len - 1, i_ch1 = 0; i_ch0 >= 0; i_ch0--, i_ch1++) {
        tex_name[i_ch1] = g_nametmp0[i_ch0];
    }

    memset(g_nametmp0, 0, MAX_TEXTURE_NAME_LEN+1);
}

epm_Result get_texture_by_name(char const *name, size_t *out_i_tex) {
    for (size_t i_tex = 0; i_tex < g_num_textures; i_tex++) {
        if (0 == strcmp(name, textures[i_tex].name)) {
            *out_i_tex = i_tex;
            return EPM_SUCCESS;
        }
    }

    // texture not found in memory; try to load it

    char filename[MAX_TEXTURE_NAME_LEN + 1] = {'\0'};
    snprintf(filename, MAX_TEXTURE_NAME_LEN + 1, "../assets/textures/%s.bmp", name);
    
    if (NULL == load_Texture(filename)) {
        return EPM_FAILURE;
    }

    return EPM_SUCCESS;
}


void scale_texels_to_world(Fix32Point V0, Fix32Point V1, Fix32Point V2, Fix32Point_2D *TV0, Fix32Point_2D *TV1, Fix32Point_2D *TV2, epm_Texture *tex) {
    (void)tex;
    
    Fix32Point dir10 = {{
            x_of(V0) - x_of(V1),
            y_of(V0) - y_of(V1),
            z_of(V0) - z_of(V1)}};
    Fix32Point dir12 = {{
            x_of(V2) - x_of(V1),
            y_of(V2) - y_of(V1),
            z_of(V2) - z_of(V1)}};
    
    ufix32_t norm10 = norm_Euclidean(dir10);
    ufix32_t norm12 = norm_Euclidean(dir12);

    Fix32Point_2D Tdir10 = {TV0->x - TV1->x,
                            TV0->y - TV1->y};
    Fix32Point_2D Tdir12 = {TV2->x - TV1->x,
                            TV2->y - TV1->y};

    ufix32_t Tnorm10 = norm2D_Euclidean(Tdir10);
    ufix32_t Tnorm12 = norm2D_Euclidean(Tdir12);

    //printf("norm10: %s\n", fmt_ufix_d(norm10, 16, 4));
    //printf("norm12: %s\n", fmt_ufix_d(norm12, 16, 4));
    //printf("Tnorm10: %s\n", fmt_ufix_d(Tnorm10, 16, 4));
    //printf("Tnorm02: %s\n", fmt_ufix_d(Tnorm12, 16, 4));
    
    // map one world unit to one texture pixit
    TV0->x = (fix32_t)(TV1->x + FIX_MULDIV(norm10, Tdir10.x, Tnorm10));
    TV0->y = (fix32_t)(TV1->y + FIX_MULDIV(norm10, Tdir10.y, Tnorm10));

    TV2->x = (fix32_t)(TV1->x + FIX_MULDIV(norm12, Tdir12.x, Tnorm12));
    TV2->y = (fix32_t)(TV1->y + FIX_MULDIV(norm12, Tdir12.y, Tnorm12));

    // ensure u,v coordinates are non-negative
    while (TV0->x < 0 || TV1->x < 0 || TV2->x < 0) {
        TV0->x += (fix32_t)((tex->w)<<16);
        TV1->x += (fix32_t)((tex->w)<<16);
        TV2->x += (fix32_t)((tex->w)<<16);
    }
    while (TV0->y < 0 || TV1->y < 0 || TV2->y < 0) {
        TV0->y += (fix32_t)((tex->h)<<16);
        TV1->y += (fix32_t)((tex->h)<<16);
        TV2->y += (fix32_t)((tex->h)<<16);
    }
    
    //printf("TV0: (%s, %s)\n", fmt_fix_d(TV0->x, 16, 4), fmt_fix_d(TV0->y, 16, 4));
    //printf("TV1: (%s, %s)\n", fmt_fix_d(TV1->x, 16, 4), fmt_fix_d(TV1->y, 16, 4));
    //printf("TV2: (%s, %s)\n", fmt_fix_d(TV2->x, 16, 4), fmt_fix_d(TV2->y, 16, 4));
    //putchar('\n');
}


void scale_quad_texels_to_world(Fix32Point V0, Fix32Point V1, Fix32Point V2, Fix32Point V3, Fix32Point_2D *TV0, Fix32Point_2D *TV1, Fix32Point_2D *TV2, Fix32Point_2D *TV3, epm_Texture *tex) {
    (void)V1;
    //printf("TV0: (%s, %s)\n", fmt_fix_d(TV0->x, 16, 4), fmt_fix_d(TV0->y, 16, 4));
    //printf("TV1: (%s, %s)\n", fmt_fix_d(TV1->x, 16, 4), fmt_fix_d(TV1->y, 16, 4));
    //printf("TV2: (%s, %s)\n", fmt_fix_d(TV2->x, 16, 4), fmt_fix_d(TV2->y, 16, 4));
    //printf("TV3: (%s, %s)\n", fmt_fix_d(TV3->x, 16, 4), fmt_fix_d(TV3->y, 16, 4));
    
    Fix32Point dir30 = {{
            x_of(V0) - x_of(V3),
            y_of(V0) - y_of(V3),
            z_of(V0) - z_of(V3)}};
    Fix32Point dir32 = {{
            x_of(V2) - x_of(V3),
            y_of(V2) - y_of(V3),
            z_of(V2) - z_of(V3)}};
        
    ufix32_t norm30 = norm_Euclidean(dir30);
    ufix32_t norm32 = norm_Euclidean(dir32);
    
    Fix32Point_2D Tdir30 = {TV0->x - TV3->x,
                            TV0->y - TV3->y};
    Fix32Point_2D Tdir32 = {TV2->x - TV3->x,
                            TV2->y - TV3->y};

    ufix32_t Tnorm30 = norm2D_Euclidean(Tdir30);
    ufix32_t Tnorm32 = norm2D_Euclidean(Tdir32);
    
    //printf("norm30: %s\n",  fmt_ufix_d(norm30,  16, 4));
    //printf("norm32: %s\n",  fmt_ufix_d(norm32,  16, 4));
    //printf("Tnorm30: %s\n", fmt_ufix_d(Tnorm30, 16, 4));
    //printf("Tnorm32: %s\n", fmt_ufix_d(Tnorm32, 16, 4));
    
    // map one world unit to one texture pixit
    TV0->x = (fix32_t)(TV3->x + FIX_MULDIV(norm30, Tdir30.x, Tnorm30));
    TV0->y = (fix32_t)(TV3->y + FIX_MULDIV(norm30, Tdir30.y, Tnorm30));

    TV2->x = (fix32_t)(TV3->x + FIX_MULDIV(norm32, Tdir32.x, Tnorm32));
    TV2->y = (fix32_t)(TV3->y + FIX_MULDIV(norm32, Tdir32.y, Tnorm32));

    TV1->x = (fix32_t)(TV3->x + (TV0->x - TV3->x) + (TV2->x - TV3->x));
    TV1->y = (fix32_t)(TV3->y + (TV0->y - TV3->y) + (TV2->y - TV3->y));
    
    // ensure u,v coordinates are non-negative
    while (TV0->x < 0 || TV1->x < 0 || TV2->x < 0 || TV3->x < 0) {
        TV0->x += (fix32_t)((tex->w)<<16);
        TV1->x += (fix32_t)((tex->w)<<16);
        TV2->x += (fix32_t)((tex->w)<<16);
        TV3->x += (fix32_t)((tex->w)<<16);
    }
    while (TV0->y < 0 || TV1->y < 0 || TV2->y < 0 || TV3->y < 0) {
        TV0->y += (fix32_t)((tex->h)<<16);
        TV1->y += (fix32_t)((tex->h)<<16);
        TV2->y += (fix32_t)((tex->h)<<16);
        TV3->y += (fix32_t)((tex->h)<<16);
    }
    
    //printf("TV0: (%s, %s)\n", fmt_fix_d(TV0->x, 16, 4), fmt_fix_d(TV0->y, 16, 4));
    //printf("TV1: (%s, %s)\n", fmt_fix_d(TV1->x, 16, 4), fmt_fix_d(TV1->y, 16, 4));
    //printf("TV2: (%s, %s)\n", fmt_fix_d(TV2->x, 16, 4), fmt_fix_d(TV2->y, 16, 4));
    //printf("TV3: (%s, %s)\n", fmt_fix_d(TV3->x, 16, 4), fmt_fix_d(TV3->y, 16, 4));
    //putchar('\n');
}
