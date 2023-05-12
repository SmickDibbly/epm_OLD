#include "zigil/zigil_event.h"

#include "src/misc/epm_includes.h"

#include "src/system/state.h"
#include "src/system/config_reader.h"
#include "src/draw/draw.h"

#include "src/draw/viewport/viewport.h"

#include "src/world/world.h"

extern bool do_CRT;
extern bool show_textoverlay;

typedef enum KeyAction {
    KEYACT_EXIT,
    KEYACT_CRT,
    KEYACT_TEXT_OVERLAY,
    KEYACT_FPSCAP,
    KEYACT_SHOW_LOGVIEW,
    KEYACT_QUAD_LAYOUT,
    KEYACT_MONO_TOP_LAYOUT,
    KEYACT_MONO_SIDE_LAYOUT,
    KEYACT_MONO_FRONT_LAYOUT,
    KEYACT_MONO_3D_LAYOUT,
    KEYACT_NEXT_VP,
    KEYACT_PREV_VP,
    KEYACT_NEXT_VPL,
    KEYACT_PREV_VPL,
    KEYACT_NEXT_VPI,
    KEYACT_PREV_VPI,

    NUM_KEYACT
} KeyAction;

static zgl_LongKeyCode map[NUM_KEYACT] = {
    [KEYACT_EXIT]         = LK_C_q,
    [KEYACT_CRT]          = LK_M_c,
    [KEYACT_TEXT_OVERLAY] = LK_F3,
    [KEYACT_FPSCAP]       = LK_F5,
    [KEYACT_SHOW_LOGVIEW]      = LK_M_S_l,
    [KEYACT_QUAD_LAYOUT]       = LK_M_S_q,
    [KEYACT_MONO_TOP_LAYOUT]   = LK_M_S_t,
    [KEYACT_MONO_SIDE_LAYOUT]  = LK_M_S_s,
    [KEYACT_MONO_FRONT_LAYOUT] = LK_M_S_f,
    [KEYACT_MONO_3D_LAYOUT]    = LK_M_S_3,
    [KEYACT_NEXT_VP]  = LK_C_COMMA,
    [KEYACT_PREV_VP]  = LK_NONE,
    [KEYACT_NEXT_VPL] = LK_M_l,
    [KEYACT_PREV_VPL] = LK_M_k,
    [KEYACT_NEXT_VPI] = LK_M_PERIOD,
    [KEYACT_PREV_VPI] = LK_M_COMMA,
};

static char const *group_str = "Keymap.Global";

static ConfigMapEntry const cme_keys[NUM_KEYACT] = {
    {"Exit",              map + KEYACT_EXIT},
    {"ToggleCRTFilter",   map + KEYACT_CRT},
    {"ToggleTextOverlay", map + KEYACT_TEXT_OVERLAY},
    {"ToggleFPSCap",      map + KEYACT_FPSCAP},
    {"ShowLogview",       map + KEYACT_SHOW_LOGVIEW},
    {"QuadLayout",        map + KEYACT_QUAD_LAYOUT},
    {"MonoTopLayout",     map + KEYACT_MONO_TOP_LAYOUT},
    {"MonoSideLayout",    map + KEYACT_MONO_SIDE_LAYOUT},
    {"MonoFrontLayout",   map + KEYACT_MONO_FRONT_LAYOUT},
    {"Mono3DLayout",      map + KEYACT_MONO_3D_LAYOUT},
    {"NextViewport",      map + KEYACT_NEXT_VP},
    {"PrevViewport",      map + KEYACT_PREV_VP},
    {"NextLayout",        map + KEYACT_NEXT_VPL},
    {"PrevLayout",        map + KEYACT_PREV_VPL},
    {"NextInterface",     map + KEYACT_NEXT_VPI},
    {"PrevInterface",     map + KEYACT_PREV_VPI},
};

static void key_str_to_key(void *var, char const *key_str) {
    zgl_LongKeyCode LK = LK_str_to_LK(key_str);
    
    if (LK != LK_NONE) {
        *(zgl_LongKeyCode *)var = LK;
    }
}


epm_Result epm_LoadGlobalKeymap(void) {
    read_config_data(group_str, NUM_KEYACT, cme_keys, key_str_to_key);

    return EPM_SUCCESS;
}
epm_Result epm_UnloadGlobalKeymap(void) {
    // TODO

    return EPM_SUCCESS;
}

epm_Result do_KeyPress_global(zgl_KeyPressEvent *evt) {
    zgl_LongKeyCode key = evt->lk;
    
    if (key == map[KEYACT_EXIT]) {
        zgl_PushCloseEvent();
    }
    else if (key == map[KEYACT_CRT]) {
        do_CRT = !do_CRT;
    }
    else if (key == map[KEYACT_TEXT_OVERLAY]) {
        show_textoverlay = !show_textoverlay;
    }
    else if (key == map[KEYACT_FPSCAP]) {
        state.timing.fpscapped = !state.timing.fpscapped;
    }
    else if (key == map[KEYACT_SHOW_LOGVIEW]) {
        epm_SetVPInterface(VP_TL, VPI_LOG);
    }
    else if (key == map[KEYACT_QUAD_LAYOUT]) {
        epm_SetVPLayout(VPL_QUAD);
    }
    else if (key == map[KEYACT_MONO_TOP_LAYOUT]) {
        epm_SetVPLayout(VPL_MONO);
        epm_SetVPInterface(VP_FULL, VPI_WORLD_TOP);
    }
    else if (key == map[KEYACT_MONO_SIDE_LAYOUT]) {
        epm_SetVPLayout(VPL_MONO);
        epm_SetVPInterface(VP_FULL, VPI_WORLD_SIDE);
    }
    else if (key == map[KEYACT_MONO_FRONT_LAYOUT]) {
        epm_SetVPLayout(VPL_MONO);
        epm_SetVPInterface(VP_FULL, VPI_WORLD_FRONT);
    }
    else if (key == map[KEYACT_MONO_3D_LAYOUT]) {
        epm_SetVPLayout(VPL_MONO);
        epm_SetVPInterface(VP_FULL, VPI_WORLD_3D);        
    }
    else if (key == map[KEYACT_NEXT_VP]) {
        epm_NextActiveVP();
    }
    else if (key == map[KEYACT_PREV_VP]) {
        epm_PrevActiveVP();
    }
    else if (key == map[KEYACT_NEXT_VPI]) {
        epm_NextVPInterface();
    }
    else if (key == map[KEYACT_PREV_VPI]) {
        epm_PrevVPInterface();
    }
    else if (key == map[KEYACT_NEXT_VPL]) {
        epm_NextVPLayout();
    }
    else if (key == map[KEYACT_PREV_VPL]) {
        epm_PrevVPLayout();
    }

    return EPM_SUCCESS;
}

epm_Result do_KeyRelease_global(zgl_KeyReleaseEvent *evt) {
    (void)evt;

    return EPM_SUCCESS;
}
