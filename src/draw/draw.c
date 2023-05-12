#include "zigil/zigil.h"

#include "src/misc/epm_includes.h"
#include "src/ntsc/crt_core.h"

#include "src/draw/default_layout.h"
#include "src/draw/textures.h"
#include "src/draw/colors.h"

#include "src/draw/window/window.h"
#include "src/draw/window/window_registry.h"

#include "src/draw/viewport/viewport.h"
#include "src/draw/viewport/viewport_structs.h"

#include "src/draw/draw.h"

// weird includes
#include "src/system/loop.h"
#include "src/system/config_reader.h"

//#define VERBOSITY
#include "verbosity.h"

#undef LOG_LABEL
#define LOG_LABEL "DRAW"


bool do_CRT = false;
//#define NTSC_CRT
#ifdef NTSC_CRT
static struct CRT crt;
static int const noise = 12;
static int const progressive = 0;
zgl_PixelArray *video_out;

static int const ntsc_format = CRT_PIX_FORMAT_BGRA;
static int const ntsc_raw = 0;
static int const ntsc_as_color = 1;
static int ntsc_field = 1;
static int const ntsc_hue = 0;
static int const ntsc_xoffset = -3;
static int const ntsc_yoffset = 5;

static epm_Result apply_CRT(void);
static void fade_phosphors(void);
#endif

static zgl_PixelArray *scr_p;

static Window root_window = {
    .name = "Root Window",
    .dragged = false,
    .last_ptr_x = 0,
    .last_ptr_y = 0,
    .cursor = ZC_arrow,
    .winfncs = {
        .draw = NULL,
        .onPointerPress = NULL,
        .onPointerRelease = NULL,
        .onPointerMotion = NULL,
        .onPointerEnter = NULL,
        .onPointerLeave = NULL,
    }
};
static WindowNode _root_node = {.win = &root_window, NULL};
WindowNode *const root_node = &_root_node;

extern int draw_textoverlay(zgl_PixelArray *scr_p);

bool show_textoverlay = false;

extern void init_sidebar(void);
extern void init_menubar(void);
extern void init_dropdownmenu(void);
extern void init_viewports(void);
extern void epm_InitText(void);
extern void epm_TermText(void);
extern void epm_InitTextOverlay(void);

epm_Result epm_InitDraw(void) {
#ifdef NTSC_CRT
    scr_p = zgl_CreatePixelArray(w, h);
    video_out = &fb_info.fb;
    crt_init(&crt, video_out->w, video_out->h, ntsc_format,
             (unsigned char*)video_out->pixels);
    crt.blend = 1;
    crt.scanlines = 1;
#else
    scr_p = &fb_info.fb;
#endif
    
    epm_InitText();
    epm_InitTextOverlay();
    epm_Result init_Draw3D(void);
    init_Draw3D();

    
    read_color_config();
    
    configure_default_layout(scr_p);

    _epm_Log("INIT.DRAW", LT_INFO, "Initializing windows.");
    
    init_menubar();
    init_sidebar();
    init_dropdownmenu();
    extern void load_icons(void);
    load_icons();
    

    _epm_Log("INIT.DRAW", LT_INFO, "Initializing viewports.");
    init_viewports();
    

    _epm_Log("INIT.DRAW", LT_INFO, "Linking windows.");
    root_window.rect.x = root_window.rect.y = 0;
    root_window.rect.w = scr_p->w;
    root_window.rect.h = scr_p->h;
    link_WindowNode(sidebar_node, root_node);
    link_WindowNode(view_container_node, root_node);
    link_WindowNode(menubar_node, root_node);
    for (VPCode i_VP = 1; i_VP < NUM_VP; i_VP++) {
        link_WindowNode(viewport_bar_nodes[i_VP], viewport_wrap_nodes[i_VP]);
        link_WindowNode(viewport_VPI_nodes[i_VP], viewport_wrap_nodes[i_VP]);
    }

#ifdef VERBOSITY
    print_WindowTree(root_node);
#endif
    
    _epm_Log("INIT.DRAW", LT_INFO, "Preloading textures.");
    // Loading textures. TODO: This can be done on an as-needed basis upon
    // loading a world.
    size_t tmp;
    if (EPM_FAILURE == get_texture_by_name("256", &tmp)) {
        exit(0);
    }
    if (EPM_FAILURE == get_texture_by_name("grass256", &tmp)) {
        exit(0);
    }
    if (EPM_FAILURE == get_texture_by_name("dirt09_256", &tmp)) {
        exit(0);
    }
    if (EPM_FAILURE == get_texture_by_name("rock256", &tmp)) {
        exit(0);
    }
    if (EPM_FAILURE == get_texture_by_name("brick02_128", &tmp)) {
        exit(0);
    }
    
    return EPM_SUCCESS;
}

epm_Result epm_TermDraw(void) {
    extern void unload_icons(void);
    unload_icons();
    
    for (size_t i_tex = 0; i_tex < g_num_textures; i_tex++) {
        unload_Texture(textures + i_tex);
    }

    epm_TermText();
    
    return EPM_SUCCESS;
}

epm_Result epm_Render(void) {
    zgl_ZeroEntire(scr_p);
   
    draw_WindowTree(root_node, scr_p);
    
    if (show_textoverlay) {
        draw_textoverlay(scr_p);
    }

#ifdef NTSC_CRT
    if (do_CRT) {
        fade_phosphors();
        apply_CRT();
    }
    else {
        memcpy(video_out->pixels, scr_p->pixels, scr_p->w*scr_p->h*sizeof(zgl_Color));
    }
#endif
       
    zgl_VideoUpdate();

    return EPM_CONTINUE;
}

#ifdef NTSC_CRT
static void fade_phosphors(void) {
    unsigned int c;

    zgl_Color *v = video_out->pixels;

    for (size_t i = 0; i < video_out->w * video_out->h; i++) {
        c = v[i] & 0xffffff;
        v[i] = ((c >> 1 & 0x7f7f7f) +
                (c >> 2 & 0x3f3f3f) +
                (c >> 3 & 0x1f1f1f) +
                (c >> 4 & 0x0f0f0f));
    }
}

static epm_Result apply_CRT(void) {
    static struct NTSC_SETTINGS ntsc = {0};
    
    ntsc.data = (unsigned char *)scr_p->pixels;
    ntsc.format = ntsc_format;
    ntsc.w = scr_p->w;
    ntsc.h = scr_p->h;
    ntsc.as_color = ntsc_as_color;
    ntsc.field = ntsc_field & 1;
    ntsc.raw = ntsc_raw;
    ntsc.hue = ntsc_hue;
    ntsc.xoffset = ntsc_xoffset;
    ntsc.yoffset = ntsc_yoffset;
    
    if (ntsc.field == 0) {
        ntsc.frame ^= 1;
    }
    
    crt_modulate(&crt, &ntsc);
    crt_demodulate(&crt, noise);
    if (!progressive) {
        ntsc_field ^= 1;
    }
    return EPM_SUCCESS;
}
#endif
