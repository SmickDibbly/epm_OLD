#ifndef PLAYER_H
#define PLAYER_H

#include "src/world/geometry.h"

//Player "jogs" at 16 units per tic, so that's 120 * 16 = 1920 units per second. Since average human jogging speed is around 8 km/h, or 2.222 m/s, this means that 1920 units is around 2.222 meters. So 1 meter is approximately 864 units. The world square is therefore around 75 meters per side.
#define PLAYER_MAX_SPEED (1 << 16) // Wits per tic
#define PLAYER_MIN_SPEED (1 << 9) // Wits per tic
#define PLAYER_MAX_ACCELERATION (1 << 10)
#define PLAYER_MAX_TURNSPEED (128 << 2) // ang18's per tic
#define PLAYER_REL_VIEW_Z fixify(40) // Wits
#define PLAYER_COLLISION_RADIUS fixify(16) // Wits
#define PLAYER_COLLISION_BOX_REACH (PLAYER_COLLISION_RADIUS + fixify(8)) // Wits
#define PLAYER_COLLISION_HEIGHT fixify(48) // Wits
#define PLAYER_MAX_VIEW_ANGLE_V (ANG18_PIBY2)

typedef struct Player {
    // Pawn
    WitPoint pos; // x and y are center, z is foot level

    // Looker
    Wit rel_view_z; // height of eyeline above z = foot level
    ang18_t view_angle_h; // valid in [0.0, 65536.0) ie. entire range, and
                           // determines the view vector x and y
    ang18_t view_angle_v; // valid between [0.0, 32768.0), and determines the
                           // view vector z

    bool key_angular_motion;
    int32_t key_angular_h;
    int32_t key_angular_v;

    bool mouse_angular_motion;
    int32_t mouse_angular_h;
    int32_t mouse_angular_v;
    
    WitPoint view_vec;
    Fix32Point_2D view_vec_XY;
    
    // Mover
    bool key_motion;
    ang18_t key_motion_angle;
    bool mouse_motion;
    WitPoint mouse_motion_vel;
    
    WitPoint vel; // determined by above

    // Accelerator
    WitPoint acc;
    
    // Collider
    Wit collision_radius;
    Wit collision_height;
    Wit collision_box_reach;

    // Has health
    uint16_t armor;
    uint16_t health;
} Player;

extern void onTic_player(void *self);

#endif /* PLAYER_H */
