#include "src/misc/epm_includes.h"

#include "src/input/input.h"

#include "src/draw/draw.h"
#include "src/draw/colors.h"
#include "src/draw/default_layout.h"

#include "src/draw/window/window_registry.h"

#include "src/draw/viewport/viewport.h"
#include "src/draw/viewport/viewport_structs.h"

#include "src/draw/draw3D.h"

#define GRAY_TOP    118
#define GRAY_BODY   88
#define GRAY_BOTTOM 58

static zgl_PixelArray *icons;

epm_Result load_icons(void) {
    icons = zgl_ReadBMP("../assets/icons.bmp");

    return EPM_SUCCESS;
}

epm_Result unload_icons(void) {
    zgl_DestroyPixelArray(icons);

    return EPM_SUCCESS;
}

void draw_viewbar(Window *win, zgl_PixelArray *scr_p) {
    Viewport *p_VP = (Viewport *)win->data;

    int32_t
        x = win->rect.x,
        y = win->rect.y,
        w = win->rect.w,
        h = win->rect.h;

    zgl_Pixel btn_lighting = {
        .x = x + 3 + 0*(22 + 3),
        .y = y + 3,
    };
    zgl_Pixel btn_wireframe = {
        .x = x + 3 + 1*(22 + 3),
        .y = y + 3,
    };
    zgl_Pixel btn_3D = {
        .x = x + w - 12 - 4*(22 + 3),
        .y = y + 3,
    };
    zgl_Pixel btn_top = {
        .x = x + w - 12 - 3*(22 + 3),
        .y = y + 3,
    };
    zgl_Pixel btn_side = {
        .x = x + w - 12 - 2*(22 + 3),
        .y = y + 3,
    };
    zgl_Pixel btn_front = {
        .x = x + w - 12 - 1*(22 + 3),
        .y = y + 3,
    };

    zgl_GrayRect(scr_p, x, y, w, h, color_viewbar_bg & 0xFF);
    
    switch (p_VP->mapped_p_VPI->i_VPI) {
    case VPI_WORLD_3D: {
        if (*p_lighting) {
            zgl_Blit(scr_p, btn_lighting.x, btn_lighting.y, icons, 22,  4*22, 22, 22);
        }          
        else {
            zgl_Blit(scr_p, btn_lighting.x, btn_lighting.y, icons, 0,  4*22, 22, 22);
        }
            
        if (*p_wireframe) {
            zgl_Blit(scr_p, btn_wireframe.x, btn_wireframe.y, icons, 22,  5*22, 22, 22);
        }
        else {
            zgl_Blit(scr_p, btn_wireframe.x, btn_wireframe.y, icons, 0,  5*22, 22, 22);
        }
                    
        zgl_Blit(scr_p, btn_top.x,   btn_top.y,   icons, 0,  0*22, 22, 22);
        zgl_Blit(scr_p, btn_side.x,  btn_side.y,  icons, 0,  1*22, 22, 22);
        zgl_Blit(scr_p, btn_front.x, btn_front.y, icons, 0,  2*22, 22, 22);
        zgl_Blit(scr_p, btn_3D.x,    btn_3D.y,    icons, 22, 3*22, 22, 22);
    }
        break;
    case VPI_WORLD_TOP: {
        zgl_Blit(scr_p, btn_top.x,   btn_top.y,   icons, 22, 0*22, 22, 22);
        zgl_Blit(scr_p, btn_side.x,  btn_side.y,  icons, 0,  1*22, 22, 22);
        zgl_Blit(scr_p, btn_front.x, btn_front.y, icons, 0,  2*22, 22, 22);
        zgl_Blit(scr_p, btn_3D.x,    btn_3D.y,    icons, 0,  3*22, 22, 22);
    }
        break;
    case VPI_WORLD_SIDE: {
        zgl_Blit(scr_p, btn_top.x,   btn_top.y,   icons, 0,  0*22, 22, 22);
        zgl_Blit(scr_p, btn_side.x,  btn_side.y,  icons, 22, 1*22, 22, 22);
        zgl_Blit(scr_p, btn_front.x, btn_front.y, icons, 0,  2*22, 22, 22);
        zgl_Blit(scr_p, btn_3D.x,    btn_3D.y,    icons, 0,  3*22, 22, 22);
    }
        break;
    case VPI_WORLD_FRONT: {
        zgl_Blit(scr_p, btn_top.x,   btn_top.y,   icons, 0,  0*22, 22, 22);
        zgl_Blit(scr_p, btn_side.x,  btn_side.y,  icons, 0,  1*22, 22, 22);
        zgl_Blit(scr_p, btn_front.x, btn_front.y, icons, 22, 2*22, 22, 22);
        zgl_Blit(scr_p, btn_3D.x,    btn_3D.y,    icons, 0,  3*22, 22, 22);
    }
        break;
    case VPI_FILESELECT:
    case VPI_LOG:
    default:
        break;
    }
}

void do_PointerPress_viewbar(Window *win, zgl_PointerPressEvent *evt) {
    Viewport *p_VP = (Viewport *)win->data;
    
    zgl_Pixel btn_lighting = {
        .x = win->rect.x + 3 + 0*(22 + 3),
        .y = win->rect.y + 3,
    };
    zgl_Pixel btn_wireframe = {
        .x = win->rect.x + 3 + 1*(22 + 3),
        .y = win->rect.y + 3,
    };
    zgl_Pixel btn_3D = {
        .x = win->rect.x + win->rect.w - 12 - 4*(22 + 3),
        .y = win->rect.y + 3,
    };
    zgl_Pixel btn_top = {
        .x = win->rect.x + win->rect.w - 12 - 3*(22 + 3),
        .y = win->rect.y + 3,
    };
    zgl_Pixel btn_side = {
        .x = win->rect.x + win->rect.w - 12 - 2*(22 + 3),
        .y = win->rect.y + 3,
    };
    zgl_Pixel btn_front = {
        .x = win->rect.x + win->rect.w - 12 - 1*(22 + 3),
        .y = win->rect.y + 3,
    };


    int x = evt->x;
    int y = evt->y;

    if (p_VP->mapped_p_VPI->i_VPI == VPI_WORLD_3D) {
        if (x >= btn_lighting.x &&
            x <= btn_lighting.x + 22 &&
            y >= btn_lighting.y &&
            y <= btn_lighting.y + 22)
            epm_ToggleLighting();
        if (x >= btn_wireframe.x &&
            x <= btn_wireframe.x + 22 &&
            y >= btn_wireframe.y &&
            y <= btn_wireframe.y + 22)
            epm_ToggleWireframe();
    }
    
    if (x >= btn_3D.x &&
        x <= btn_3D.x + 28 &&
        y >= btn_3D.y &&
        y <= btn_3D.y + 28) {
        epm_SetVPInterface(p_VP->i_VP, VPI_WORLD_3D);
    }
    else if (x >= btn_top.x &&
             x <= btn_top.x + 28 &&
             y >= btn_top.y &&
             y <= btn_top.y + 28) {
        epm_SetVPInterface(p_VP->i_VP, VPI_WORLD_TOP);
    }
    else if (x >= btn_side.x &&
             x <= btn_side.x + 28 &&
             y >= btn_side.y &&
             y <= btn_side.y + 28) {
        epm_SetVPInterface(p_VP->i_VP, VPI_WORLD_SIDE);
    }
    else if (x >= btn_front.x &&
             x <= btn_front.x + 28 &&
             y >= btn_front.y &&
             y <= btn_front.y + 28) {
        epm_SetVPInterface(p_VP->i_VP, VPI_WORLD_FRONT);
    }

    epm_SetInputFocus(&p_VP->VPI_node);
}




/* -------------------------------------------------------------------------- */

WindowFunctions winfncs_viewbar = {
    .draw = draw_viewbar,
    .onPointerPress = do_PointerPress_viewbar,
    .onPointerRelease = NULL,
    .onPointerMotion = NULL,
    .onPointerEnter = NULL,
    .onPointerLeave = NULL,
    .onKeyPress = NULL,
    .onKeyRelease = NULL,
};
