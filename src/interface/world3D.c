#include "src/system/state.h"

#include "src/draw/colors.h"
#include "src/draw/default_layout.h"
#include "src/draw/text.h"
#include "src/draw/draw3D.h"

#include "src/draw/window/window.h"

// weird includes
#include "src/input/dpad.h"
#include "src/world/world.h"
#include "src/world/selection.h"

/* -------------------------------------------------------------------------- */

VirtualDPad_hndl movement_dpad;
VirtualDPad_hndl look_dpad;
// For orthographic projection, the region in camera space visible.
static void do_dpad_move(DPad_Dir dir, ang18_t ang);
static void do_dpad_look(DPad_Dir dir, ang18_t ang);

static epm_Result init_World3D(void);

#include "src/system/config_reader.h"
typedef enum KeyAction {
    KEYACT_MOVE_FORWARD,
    KEYACT_MOVE_BACKWARD,
    KEYACT_MOVE_LEFT,
    KEYACT_MOVE_RIGHT,
    KEYACT_MOVE_UP,
    KEYACT_MOVE_DOWN,
    KEYACT_TURN_UP,
    KEYACT_TURN_DOWN,
    KEYACT_TURN_LEFT,
    KEYACT_TURN_RIGHT,
    KEYACT_TOGGLE_BSP,
    KEYACT_TOGGLE_PROJ,

    NUM_KEYACT
} KeyAction;

static zgl_KeyComplex map[NUM_KEYACT] = {
    [KEYACT_MOVE_FORWARD]  = {ZK_e, 0, LK_e},
    [KEYACT_MOVE_BACKWARD] = {ZK_d, 0, LK_d},
    [KEYACT_MOVE_LEFT]     = {ZK_s, 0, LK_s},
    [KEYACT_MOVE_RIGHT]    = {ZK_f, 0, LK_f},
    [KEYACT_MOVE_UP]       = {ZK_SPC, 0, LK_SPC},
    [KEYACT_MOVE_DOWN]     = {ZK_c, 0, LK_c},
    [KEYACT_TURN_UP]       = {ZK_UP, 0, LK_UP},
    [KEYACT_TURN_DOWN]     = {ZK_DOWN, 0, LK_DOWN},
    [KEYACT_TURN_LEFT]     = {ZK_LEFT, 0, LK_LEFT},
    [KEYACT_TURN_RIGHT]    = {ZK_RIGHT, 0, LK_RIGHT},
    [KEYACT_TOGGLE_BSP]    = {ZK_b, ZGL_ALT_MASK | ZGL_SHIFT_MASK, LK_M_S_b},
    [KEYACT_TOGGLE_PROJ]   = {ZK_p, ZGL_ALT_MASK | ZGL_SHIFT_MASK, LK_M_S_p},
};

static char const *group_str = "Keymap.World3D";

static ConfigMapEntry const cme_keys[NUM_KEYACT] = {
    {"MoveForward", map + KEYACT_MOVE_FORWARD},
    {"MoveBackward", map + KEYACT_MOVE_BACKWARD},
    {"MoveLeft", map + KEYACT_MOVE_LEFT},
    {"MoveRight", map + KEYACT_MOVE_RIGHT},
    {"MoveUp", map + KEYACT_MOVE_UP},
    {"MoveDown", map + KEYACT_MOVE_DOWN},
    {"TurnUp", map + KEYACT_TURN_UP},
    {"TurnDown", map + KEYACT_TURN_DOWN},
    {"TurnLeft", map + KEYACT_TURN_LEFT},
    {"TurnRight", map + KEYACT_TURN_RIGHT},
    {"ToggleBSP", map + KEYACT_TOGGLE_BSP},
    {"ToggleProjectionType", map + KEYACT_TOGGLE_PROJ},
};

static void key_str_to_key(void *var, char const *key_str) {
    zgl_KeyComplex KC;
    LK_str_to_KeyComplex(key_str, &KC);

    printf("(str \"%s\") (zk %i) (lk %i)\n", key_str, KC.zk, KC.lk);
    dibassert(KC.zk != ZK_NONE);
    dibassert(KC.lk != LK_NONE);
    if (KC.zk != ZK_NONE) {
        *(zgl_KeyComplex *)var = KC;
    }
}

static epm_Result epm_LoadWorld3DKeymap(void) {
    read_config_data(group_str, NUM_KEYACT, cme_keys, key_str_to_key);

    return EPM_SUCCESS;
}


static epm_Result init_World3D(void) {
    epm_LoadWorld3DKeymap();
    
    movement_dpad = register_dpad(map[KEYACT_MOVE_FORWARD].zk,
                                  map[KEYACT_MOVE_LEFT].zk,
                                  map[KEYACT_MOVE_BACKWARD].zk,
                                  map[KEYACT_MOVE_RIGHT].zk);
    look_dpad = register_dpad(map[KEYACT_TURN_UP].zk,
                              map[KEYACT_TURN_LEFT].zk,
                              map[KEYACT_TURN_DOWN].zk,
                              map[KEYACT_TURN_RIGHT].zk);
    
    return EPM_SUCCESS;
}

static epm_Result term_World3D(void) {
    //TODO
    return EPM_SUCCESS;
}


static void draw_World3D(Window *win, zgl_PixelArray *scr_p) {
    draw3D(win, scr_p);
    
    // Drawing a minimal text overlay in the World3D viewport
    char fps[32] = {'\0'};
    sprintf(fps, "FPS: %d", state.timing.local_avg_fps>>16);
    char bsp[64] = {'\0'};
    if ( ! no_bsp)
        sprintf(bsp, "BSP: ON");
    else
        sprintf(bsp, "BSP: OFF");

    draw_BMPFont_string(scr_p, NULL, fps,
                        win->rect.x+8, win->rect.y+4, FC_MONOGRAM2, 0xFFFFFF);
    draw_BMPFont_string(scr_p, NULL, bsp,
                        win->rect.x+8, win->rect.y+24, FC_MONOGRAM2, 0xFFFFFF);
}




/* -------------------------------------------------------------------------- */
// Input
/* -------------------------------------------------------------------------- */

// Input Modes (IM)
// Top-level
#define IM_DEFAULT 0
#define IM_MOUSELOOK 1
#define IM_PREDRAG 2
#define IM_DRAG 3
static int input_mode = IM_DEFAULT;

// Submodes of IM_DRAG
#define IM_DRAG_MOVING  0
#define IM_DRAG_PANNING 1
#define IM_DRAG_LOOKING 2
#define IM_DRAG_BRUSHPAN_X 3
#define IM_DRAG_BRUSHPAN_Y 4
#define IM_DRAG_BRUSHPAN_Z 5
static int drag_mode = IM_DRAG_MOVING;

extern void grab_input(Window *win);
extern void ungrab_input(void);

static void do_PointerPress_default(Window *win, zgl_PointerPressEvent *evt) {
    zgl_LongKeyCode lk = evt->lk;
    
    if (lk == LK_POINTER_MIDDLE) {
        grab_input(win);
        input_mode = IM_MOUSELOOK;
    }

    grab_input(win);
    input_mode = IM_PREDRAG;
}

static void do_PointerPress_mouselook(Window *win, zgl_PointerPressEvent *evt) {
    zgl_LongKeyCode lk = evt->lk;
    (void)lk;
}

static void do_PointerPress_predrag(Window *win, zgl_PointerPressEvent *evt) {
    zgl_LongKeyCode lk = evt->lk;
    (void)lk;
}

static fix32_t brushpan_acc = 0;
static void compute_drag_submode(Window *win, uint8_t mod_flags, uint8_t but_flags) {
    bool 
        LEFT  = but_flags & ZGL_MOUSELEFT_MASK,
        RIGHT = but_flags & ZGL_MOUSERIGHT_MASK,
        SHIFT = mod_flags & ZGL_SHIFT_MASK,
        CTRL  = mod_flags & ZGL_CTRL_MASK;

    if ( ! (LEFT || RIGHT)) {
        ungrab_input();
        input_mode = IM_DEFAULT;
    }
    else {
        grab_input(win);
        input_mode = IM_DRAG;
        if (SHIFT || CTRL) {
            if (LEFT && RIGHT) {
                drag_mode = IM_DRAG_BRUSHPAN_Z;
                brushpan_acc = 0;
            }
            else if (LEFT) {
                drag_mode = IM_DRAG_BRUSHPAN_X;
                brushpan_acc = 0;
                
            }
            else /*if (RIGHT)*/ {
                drag_mode = IM_DRAG_BRUSHPAN_Y;
                brushpan_acc = 0;
            }
        }
        else {
            if (LEFT && RIGHT) {
                drag_mode = IM_DRAG_PANNING;
            }
            else if (LEFT) {
                drag_mode = IM_DRAG_MOVING;
            }
            else /*if (RIGHT)*/ {
                drag_mode = IM_DRAG_LOOKING;
            }
        }    
    }
}

static void do_PointerPress_drag(Window *win, zgl_PointerPressEvent *evt) {
    compute_drag_submode(win, evt->mod_flags, evt->but_flags);
        
    return;
}

static void do_PointerPress_World3D(Window *win, zgl_PointerPressEvent *evt) {
    switch (input_mode) {
    case IM_DEFAULT:
        do_PointerPress_default(win, evt);
        break;
    case IM_MOUSELOOK:
        do_PointerPress_mouselook(win, evt);
        break;
    case IM_PREDRAG:
        do_PointerPress_predrag(win, evt);
        break;
    case IM_DRAG:
        do_PointerPress_drag(win, evt);
        break;
    default:
        dibassert(false);
    }
}

static void do_PointerRelease_World3D(Window *win, zgl_PointerReleaseEvent *evt) {
    zgl_LongKeyCode lk = evt->lk;

    switch (input_mode) {
    case IM_PREDRAG:
        ungrab_input();
        input_mode = IM_DEFAULT;
        if (lk == LK_POINTER_LEFT) {
            if (evt->mod_flags & ZGL_SHIFT_MASK) {
                select_brush(win, zgl_GetPointerPos());
            }
            else if (evt->mod_flags & ZGL_CTRL_MASK) {
                select_face(win, zgl_GetPointerPos());
            }
            else {
                select_one_face(win, zgl_GetPointerPos());
            }
        }
        else if (lk == LK_POINTER_RIGHT) {
            select_vertex(win, zgl_GetPointerPos());
        }
        break;
    case IM_DRAG:
        compute_drag_submode(win, evt->mod_flags, evt->but_flags);
        break;
    case IM_DEFAULT:
    case IM_MOUSELOOK:
    default:
        break;
    }
}

//static uint32_t snaplog = 4;
static uint32_t snap = (1<<(4+16));

#define round_nearest_pow2(NUM, POW2) \
    (((NUM) & ((POW2)>>1)) ? ((NUM) & ~((POW2)-1)) + (POW2) : ((NUM) & ~((POW2)-1)))


WitPoint snap_point(WitPoint in) {
    fix32_t
        out_x = x_of(in),
        out_y = y_of(in),
        out_z = z_of(in);

    out_x = round_nearest_pow2(out_x, snap);
    out_y = round_nearest_pow2(out_y, snap);
    out_z = round_nearest_pow2(out_z, snap);
    
    return (WitPoint){{out_x, out_y, out_z}};
}

static void snap_brush_selection(void) {
    WitPoint snapped = snap_point(brushsel.POR);
    fix32_t dx = x_of(snapped) - x_of(brushsel.POR);
    fix32_t dy = y_of(snapped) - y_of(brushsel.POR);
    fix32_t dz = z_of(snapped) - z_of(brushsel.POR);

    if ( ! (dx || dy || dz)) return; // already snapped
    
    for (BrushSelectionNode *node = brushsel.head; node; node = node->next) {
        Brush *brush = node->brush;

        x_of(brush->POR) += dx;
        y_of(brush->POR) += dx;
        z_of(brush->POR) += dx;
                
        WitPoint *v = brush->vertices;
        for (size_t i_v = 0; i_v < brush->num_vertices; i_v++) {
            x_of(v[i_v]) += dx;
            y_of(v[i_v]) += dy;
            z_of(v[i_v]) += dz;
        }
    }
    brushsel.POR = snapped;
}

static void do_PointerMotion_World3D(Window *win, zgl_PointerMotionEvent *evt) {
    int dx = evt->dx;
    int dy = evt->dy;
    
    switch (input_mode) {
    case IM_DEFAULT:
        zgl_SetCursor(ZC_cross);
        break;
    case IM_MOUSELOOK:
        cam.mouse_angular_motion = true;
        cam.mouse_angular_h = -LSHIFT32(dx, 6);
        cam.mouse_angular_v = LSHIFT32(dy, 6);
        break;
    case IM_PREDRAG:
        compute_drag_submode(win, evt->mod_flags, evt->but_flags);
        break;
    case IM_DRAG:
        switch (drag_mode) {
        case IM_DRAG_LOOKING:
            cam.mouse_angular_motion = true;
            cam.mouse_angular_h = -LSHIFT32(dx, 6);
            cam.mouse_angular_v = LSHIFT32(dy, 6);
            break;
        case IM_DRAG_MOVING:
            cam.mouse_angular_motion = true;
            cam.mouse_angular_h = -LSHIFT32(dx, 6);
            cam.mouse_angular_v = 0;
            
            cam.mouse_motion = true;
            cam.mouse_motion_vel.v[I_X] = -((cam.view_vec_XY.x * dy)>>1);
            cam.mouse_motion_vel.v[I_Y] = -((cam.view_vec_XY.y * dy)>>1);
            cam.mouse_motion_vel.v[I_Z] = 0;
            break;
        case IM_DRAG_PANNING:
            cam.mouse_motion = true;
            cam.mouse_motion_vel.v[I_X] = (cam.view_vec_XY.y * dx)>>1;
            cam.mouse_motion_vel.v[I_Y] = -(cam.view_vec_XY.x * dx)>>1;
            cam.mouse_motion_vel.v[I_Z] = -(fixify(dy)>>1);
            break;
        case IM_DRAG_BRUSHPAN_X: {
            if (brushsel.head == NULL) {
                add_selected_brush(&brushsel, frame);
            }
            
            snap_brush_selection();
            
            int world_dx = -(fixify(dx)>>1);
            brushpan_acc += world_dx;

            if ( ABS(brushpan_acc) <= snap) break;

            if (brushpan_acc < 0) {
                world_dx = (brushpan_acc & ~(snap-1)) + snap;
                brushpan_acc = (brushpan_acc & (snap-1)) - snap;
            }
            else {
                world_dx = (brushpan_acc & ~(snap-1));
                brushpan_acc = (brushpan_acc & (snap-1));
            }

            x_of(brushsel.POR) += world_dx;
            for (BrushSelectionNode *node = brushsel.head; node; node = node->next) {
                Brush *brush = node->brush;
                x_of(brush->POR) += world_dx;
                
                WitPoint *v = brush->vertices;
                for (size_t i_v = 0; i_v < brush->num_vertices; i_v++) {
                    x_of(v[i_v]) += world_dx;                    
                }
            }
                
            if (LK_states[LK_LSHIFT]) x_of(cam.pos) += world_dx;
           
        }
            break;
        case IM_DRAG_BRUSHPAN_Y: {
            if (brushsel.head == NULL) {
                add_selected_brush(&brushsel, frame);
            }
                        
            snap_brush_selection();
            
            int world_dy = -(fixify(dx)>>1);
            brushpan_acc += world_dy;

            if ( ABS(brushpan_acc) <= snap) break;

            if (brushpan_acc < 0) {
                world_dy = (brushpan_acc & ~(snap-1)) + snap;
                brushpan_acc = (brushpan_acc & (snap-1)) - snap;
            }
            else {
                world_dy = (brushpan_acc & ~(snap-1));
                brushpan_acc = (brushpan_acc & (snap-1));
            }

            y_of(brushsel.POR) += world_dy;
            for (BrushSelectionNode *node = brushsel.head; node; node = node->next) {
                Brush *brush = node->brush;
                y_of(brush->POR) += world_dy;
                WitPoint *v = brush->vertices;
                for (size_t i_v = 0; i_v < brush->num_vertices; i_v++) {
                    y_of(v[i_v]) += world_dy;
                }
            }

            if (LK_states[LK_LSHIFT]) y_of(cam.pos) += world_dy;
        }
            break;
        case IM_DRAG_BRUSHPAN_Z: {
            if (brushsel.head == NULL) {
                add_selected_brush(&brushsel, frame);
            }
            
            snap_brush_selection();
            
            int world_dz = -(fixify(dy)>>1);
            brushpan_acc += world_dz;

            if ( ABS(brushpan_acc) <= snap) break;

            if (brushpan_acc < 0) {
                world_dz = (brushpan_acc & ~(snap-1)) + snap;
                brushpan_acc = (brushpan_acc & (snap-1)) - snap;
            }
            else {
                world_dz = (brushpan_acc & ~(snap-1));
                brushpan_acc = (brushpan_acc & (snap-1));
            }
            
            z_of(brushsel.POR) += world_dz;
            for (BrushSelectionNode *node = brushsel.head; node; node = node->next) {
                Brush *brush = node->brush;
                z_of(brush->POR) += world_dz;
                WitPoint *v = brush->vertices;
                for (size_t i_v = 0; i_v < brush->num_vertices; i_v++) {
                    z_of(v[i_v]) += world_dz;
                }
            }

            if (LK_states[LK_LSHIFT]) z_of(cam.pos) += world_dz;

        }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

}
static void do_PointerEnter_World3D(Window *win, zgl_PointerEnterEvent *evt) {
    (void)win, (void)evt;
    zgl_SetCursor(ZC_cross);
}
static void do_PointerLeave_World3D(Window *win, zgl_PointerLeaveEvent *evt) {
    (void)evt;
    (void)win;
}
static void do_KeyPress_World3D(Window *win, zgl_KeyPressEvent *evt) {
    (void)win;
    DPad_Dir dir;
    ang18_t ang;

    zgl_LongKeyCode lk = evt->lk;
    zgl_KeyCode zk = evt->zk;
    if (zk == ZK_LSHIFT || zk == ZK_LCTRL || zk == ZK_LALT) {
        compute_drag_submode(win, evt->mod_flags, mouse_states);
    }
    else if (lk == LK_M_0) {
        epm_SetTriangleDrawFunction(0);
    }
    else if (lk == LK_M_1) {
        epm_SetTriangleDrawFunction(1);
    }
    else if (lk == LK_M_2) {
        epm_SetTriangleDrawFunction(2);
    }
    else if (lk == LK_M_3) {
        epm_SetTriangleDrawFunction(3);
    }
    else if (lk == LK_S_u) {
        clear_vertex_selection(&vertexsel);
        sel_clear(&sel_face);
        clear_brush_selection(&brushsel);
    }
    else if (lk == LK_S_b) {
        select_all_brush_faces();
    }
    else if (zk == map[KEYACT_MOVE_UP].zk) {
        cam.key_vertical_motion = true;
        cam.key_vertical_vel = CAMERA_MAX_SPEED;
    }
    else if (zk == map[KEYACT_MOVE_DOWN].zk) {
        cam.key_vertical_motion = true;
        cam.key_vertical_vel = -CAMERA_MAX_SPEED;
    }
    else if (zk == map[KEYACT_MOVE_FORWARD].zk ||
             zk == map[KEYACT_MOVE_BACKWARD].zk ||
             zk == map[KEYACT_MOVE_LEFT].zk ||
             zk == map[KEYACT_MOVE_RIGHT].zk) {
        if (EPM_ERROR != get_dpad_state(movement_dpad, &dir, &ang))
            do_dpad_move(dir, ang);
    }
    else if (zk == map[KEYACT_TURN_UP].zk ||
             zk == map[KEYACT_TURN_DOWN].zk ||
             zk == map[KEYACT_TURN_LEFT].zk ||
             zk == map[KEYACT_TURN_RIGHT].zk) {
        if (EPM_ERROR != get_dpad_state(look_dpad, &dir, &ang))
            do_dpad_look(dir, ang);
    }
    else if (lk == map[KEYACT_TOGGLE_BSP].lk) {
        no_bsp = ! no_bsp;
    }
    else if (lk == map[KEYACT_TOGGLE_PROJ].lk) {
        epm_ToggleProjectionType();
    }
}

static void do_KeyRelease_World3D(Window *win, zgl_KeyReleaseEvent *evt) {
    (void)win, (void)evt;
    DPad_Dir dir;
    ang18_t ang;
    
    zgl_KeyCode key = evt->zk;
    if (key == ZK_LSHIFT || key == ZK_LCTRL || key == ZK_LALT) {
        compute_drag_submode(win, evt->mod_flags, mouse_states);
    }
    else if (key == map[KEYACT_MOVE_UP].zk) {
        cam.key_vertical_motion = false;
        cam.key_vertical_vel = 0;
    }
    else if (key == map[KEYACT_MOVE_DOWN].zk) {
        cam.key_vertical_motion = false;
        cam.key_vertical_vel = 0;
    }
    else if (key == map[KEYACT_MOVE_FORWARD].zk ||
             key == map[KEYACT_MOVE_BACKWARD].zk ||
             key == map[KEYACT_MOVE_LEFT].zk ||
             key == map[KEYACT_MOVE_RIGHT].zk) {
        if (EPM_ERROR != get_dpad_state(movement_dpad, &dir, &ang))
            do_dpad_move(dir, ang);
    }
    else if (key == map[KEYACT_TURN_UP].zk ||
             key == map[KEYACT_TURN_DOWN].zk ||
             key == map[KEYACT_TURN_LEFT].zk ||
             key == map[KEYACT_TURN_RIGHT].zk ||
             evt->zk == ZK_LEFT) {
        if (EPM_ERROR != get_dpad_state(look_dpad, &dir, &ang))
            do_dpad_look(dir, ang);
    }
}



static fix32_t dir_cos18[9] = {
    [DPAD_NULL] = 0 /*not used*/,
    [U] = 0X10000,
    [UL] = 0XB504,
    [L] = 0,
    [DL] = 0XFFFF4AFC,
    [D] = 0XFFFF0000,
    [DR] = 0XFFFF4AFC,
    [R] = 0,
    [UR] = 0XB504,
};

static fix32_t dir_sin18[9] = {
    [DPAD_NULL] = 0 /*not used*/,
    [U] = 0,
    [UL] = 0XB504,
    [L] = 0X10000,
    [DL] = 0XB504,
    [D] = 0,
    [DR] = 0XFFFF4AFC,
    [R] = 0XFFFF0000,
    [UR] = 0XFFFF4AFC,
};

static void do_dpad_look(DPad_Dir dir, ang18_t ang) {
    (void)ang;
    
    if (dir == DPAD_NULL) {
        cam.key_angular_motion = false;
        cam.key_angular_h = 0;
        cam.key_angular_v = 0;
        return;
    }
    else {
        cam.key_angular_motion = true;
        cam.key_angular_h = (int32_t)FIX_MUL(CAMERA_MAX_TURNSPEED, dir_sin18[dir]);
        cam.key_angular_v = (int32_t)-FIX_MUL(CAMERA_MAX_TURNSPEED, dir_cos18[dir]);
        return;       
    }
}

static void do_dpad_move(DPad_Dir dir, ang18_t ang) {
    if (dir == DPAD_NULL) {
        cam.key_motion = false;
        return;
    }
    else {
        cam.key_motion = true;
        cam.key_motion_angle = ang;
        return;
    }
}

    
#include "src/draw/viewport/viewport_structs.h"

void onMap_World3D(ViewportInterface *vpi, Viewport *vp) {
    void epm_UpdateFrustumHeight(Window *win);
    epm_UpdateFrustumHeight(&vp->VPI_win);
}

ViewportInterface interface_World3D = {
    .i_VPI = VPI_WORLD_3D,
    .mapped_i_VP = VP_NONE,
    .windata = NULL,
    .onUnmap = NULL,
    .onMap = onMap_World3D,
    .init = init_World3D,
    .term = term_World3D,
    .winfncs = {
        .draw = draw_World3D,
        .onPointerPress = do_PointerPress_World3D,
        .onPointerRelease = do_PointerRelease_World3D,
        .onPointerMotion = do_PointerMotion_World3D,
        .onPointerEnter = do_PointerEnter_World3D,
        .onPointerLeave = do_PointerLeave_World3D,
        .onKeyPress = do_KeyPress_World3D,
        .onKeyRelease = do_KeyRelease_World3D,
    },
    .name = "World 3D"
};


#include "src/input/command.h"

static void CMDH_snap(int argc, char **argv, char *output_str) {
    (void)argc;
    int val = atoi(argv[1]);

    switch (val) {
    case 1:
        snap = 1 << (0 + 16);
        break;
    case 2:
        snap = 1 << (1 + 16);
        break;
    case 4:
        snap = 1 << (2 + 16);
        break;
    case 8:
        snap = 1 << (3 + 16);
        break;
    case 16:
        snap = 1 << (4 + 16);
        break;
    case 32:
        snap = 1 << (5 + 16);
        break;
    case 64:
        snap = 1 << (6 + 16);
        break;
    case 128:
        snap = 1 << (7 + 16);
        break;
    case 256:
        snap = 1 << (8 + 16);
        break;
    default:
        sprintf(output_str, "Snap level must be a power of 2 between 1 and 256.");
        break;
    }
}

epm_Command const CMD_snap = {
    .name = "snap",
    .argc_min = 1,
    .argc_max = 1,
    .handler = CMDH_snap,
};
