#include "src/world/geometry.h"
#include "src/misc/mathematics.h"
#include <math.h>

extern ufix32_t norm_Euclidean(Fix32Point u);
extern ufix32_t norm_L1(Fix32Point u);
extern ufix32_t norm_approx13(Fix32Point u);
extern ufix32_t norm_approx9(Fix32Point u);
extern ufix32_t norm_approx8(Fix32Point u);
extern Fix32Point normalize_EMMIR(Fix32Point u);

extern ufix32_t norm2D_Euclidean(Fix32Point_2D u);
extern ufix32_t norm2D_L1(Fix32Point_2D u);
extern ufix32_t norm2D_Doom(Fix32Point_2D u);
extern ufix32_t norm2D_flipcode(Fix32Point_2D u);
extern ufix32_t norm2D_flipcode2(Fix32Point_2D u);

extern fix64_t dot(Fix32Point u, Fix32Point v);
extern fix32_t signdist(Fix32Point pt, Fix32Plane plane);

extern fix32_t dot2D(Fix32Point_2D u, Fix32Point_2D v);
extern fix32_t perp2D(Fix32Point_2D u, Fix32Point_2D v);
extern fix32_t signdist2D(Fix32Point_2D pt, Fix32Line_2D line);

extern Fix32Point_2D closest_point_2D(Fix32Point_2D point, Fix32Seg_2D seg);
    
/* Various 3D and 2D norm functions of various accuracies. */

Fix32Point normalize_EMMIR(Fix32Point u) {
    while (x_of(u) > (1<<16) ||
           y_of(u) > (1<<16) ||
           z_of(u) > (1<<16)) {
        x_of(u) >>= 1;
        y_of(u) >>= 1;
        z_of(u) >>= 1;
    }

    return u;
}

ufix32_t norm_Euclidean(Fix32Point u) {
    fix64_t squared_norm = ((fix64_t)x_of(u) * (fix64_t)x_of(u) +
                            (fix64_t)y_of(u) * (fix64_t)y_of(u) +
                            (fix64_t)z_of(u) * (fix64_t)z_of(u));
    return (ufix32_t)sqrt_ufix32_from_ufix64(squared_norm);
}
ufix32_t norm_L1(Fix32Point u) {
    // always larger than Euclid
    return ABS(x_of(u)) + ABS(y_of(u)) + ABS(z_of(u));
}
ufix32_t norm_approx13(Fix32Point u) {
    // always within +/-13% of Euclid
    fix32_t
        x = ABS(x_of(u)),
        y = ABS(y_of(u)),
        z = ABS(z_of(u));

    fix32_t tmp;
    
    if (x < y) {
        tmp = x;
        x = y;
        y = tmp;
    }
    if (x < z) {
        tmp = x;
        x = z;
        z = tmp;
    }

    // max + (1/4)med + (1/4)min
    return x + ((y+z)>>2);
}
ufix32_t norm_approx9(Fix32Point u) {
    // always within +/-9% of Euclid
    fix32_t
        x = ABS(x_of(u)),
        y = ABS(y_of(u)),
        z = ABS(z_of(u));
    
    fix32_t tmp;
    
    if (x < y) {
        tmp = x;
        x = y;
        y = tmp;
    }
    if (x < z) {
        tmp = x;
        x = z;
        z = tmp;
    }
    if (y < z) {
        tmp = y;
        y = z;
        z = tmp;
    }

    //     max + (5/16)med + (1/4)min
    // 5/16 = 1/4 + 1/16
    return x + (y>>2) + (y>>4) + (z>>2);
}
ufix32_t norm_approx8(Fix32Point u) {
    // always within +/-8% of Euclid
    fix32_t
        x = ABS(x_of(u)),
        y = ABS(y_of(u)),
        z = ABS(z_of(u));
    
    fix32_t tmp;
    
    if (x < y) {
        tmp = x;
        x = y;
        y = tmp;
    }
    if (x < z) {
        tmp = x;
        x = z;
        z = tmp;
    }
    if (y < z) {
        tmp = y;
        y = z;
        z = tmp;
    }

    // max + (11/32)med + (1/4)min
    // 11/32 = 1/4 + 1/16 + 1/32
    return x + (y>>2) + (y>>4) + (y>>5) + (z>>2);
}



ufix32_t norm2D_Euclidean(Fix32Point_2D u) {
    // intentionally keeping result as 32.32
    fix64_t squared_norm = ((fix64_t)u.x * (fix64_t)u.x + 
                            (fix64_t)u.y * (fix64_t)u.y);
    return (ufix32_t)sqrt_ufix32_from_ufix64(squared_norm);
}
ufix32_t norm2D_L1(Fix32Point_2D u) {
    // always larger than Euclid
    return ABS(u.x) + ABS(u.y);
}
ufix32_t norm2D_Doom(Fix32Point_2D u) {
    // always larger than Euclid
    
    // Same as P_AproxDistance from p_maputl.c from Doom source code.
    // also appears in 1990 book "Graphics Gems" by Alan W. Paeth

    u.x = ABS(u.x);
    u.y = ABS(u.y);

    // max + min - (1/2)min = max + (1/2)min
    if (u.x < u.y)
        return u.y-(u.x>>1);
    else
        return u.x+(u.y>>1);
}
ufix32_t norm2D_flipcode(Fix32Point_2D u) {
    // https://www.flipcode.com/archives/Fast_Approximate_Distance_Functions.shtml

    u.x = ABS(u.x);
    u.y = ABS(u.y);

    // (123/128)max + (51/128)min
    // 123/128 = 1 - 1/32 - 1/128
    //  51/128 = 1/2 - 1/16 - 1/32 - 1/128
    if (u.x < u.y) { // y is max
        return ((u.y>>0) - (u.y>>5) - (u.y>>7) +
                (u.x>>1) - (u.x>>4) - (u.x>>5) - (u.x>>7));
    }
    else { // x is max
        return ((u.x>>0) - (u.x>>5) - (u.x>>7) +
                (u.y>>1) - (u.y>>4) - (u.y>>5) - (u.y>>7));
    }
}
ufix32_t norm2D_flipcode2(Fix32Point_2D u) {
    // https://www.flipcode.com/archives/Fast_Approximate_Distance_Functions.shtml

    u.x = ABS(u.x);
    u.y = ABS(u.y);

    // (1007/1024)max + (441/1024)min
    // 1007/1024 = ??? TODO
    //  441/1024 = ??? TODO
    if (u.x < u.y) { // y is max
        return -1;
    }
    else { // x is max
        return -1;
    }
}


fix64_t dot(Fix32Point u, Fix32Point v) {
    return
        ((fix64_t)x_of(u)*(fix64_t)x_of(v) +
         (fix64_t)y_of(u)*(fix64_t)y_of(v) +
         (fix64_t)z_of(u)*(fix64_t)z_of(v))>>16;
}
fix32_t signdist(Fix32Point pt, Fix32Plane plane) {
    return (fix32_t)(dot(plane.normal, pt) - plane.constant);
}



fix32_t dot2D(Fix32Point_2D u, Fix32Point_2D v) {
    return (fix32_t)((FIX_MUL(u.x, v.x) + FIX_MUL(u.y, v.y)));
}
fix32_t perp2D(Fix32Point_2D u, Fix32Point_2D v) {
    return (fix32_t)(FIX_MUL(u.x, v.y) - FIX_MUL(u.y, v.x));
}
fix32_t signdist2D(Fix32Point_2D pt, Fix32Line_2D line) {
    return dot2D(line.normal, pt) - line.constant;
}



int normalize
(Fix32Point vec,
 Fix32Point *normalized);
int normalize_secret_float
(Fix32Point vec,
 Fix32Point *normalized);

int set_length
(Fix32Point vec,
 fix32_t length,
 Fix32Point *normalized);
int set_length_secret_flaot
(Fix32Point vec,
 fix32_t length,
 Fix32Point *normalized);


bool overlap_Rect_2D(const Fix32Rect_2D *r1, const Fix32Rect_2D *r2) {
    // check for degeneracy
    if (r1->w == 0 || r1->h == 0 || r2->w == 0 || r2->h == 0)
        return false;

    
    fix32_t rec1[4] = {r1->x, r1->y, r1->x+r1->w-1, r1->y+r1->h-1};
    fix32_t rec2[4] = {r2->x, r2->y, r2->x+r2->w-1, r2->y+r2->h-1};
   
    return !(rec1[2] <= rec2[0] ||
             rec2[2] <= rec1[0] ||
             rec1[3] <= rec2[1] ||
             rec2[3] <= rec1[1]);
}
bool overlap_Rect(const Fix32Rect *r1, const Fix32Rect *r2) {
    // check for degeneracy
    if (r1->dv[I_X] == 0 || r1->dv[I_Y] == 0 || r1->dv[I_Z] == 0 ||
        r2->dv[I_X] == 0 || r2->dv[I_Y] == 0 || r2->dv[I_Z] == 0)
        return false;

    fix32_t rec1[6] = {x_of(*r1), y_of(*r1), z_of(*r1),
                       x_of(*r1) + dx_of(*r1)-1,
                       y_of(*r1) + dy_of(*r1)-1,
                       z_of(*r1) + dz_of(*r1)-1};
    fix32_t rec2[6] = {x_of(*r2), y_of(*r2), z_of(*r2),
                       x_of(*r2) + dx_of(*r2)-1,
                       y_of(*r2) + dy_of(*r2)-1,
                       z_of(*r2) + dz_of(*r2)-1};
    
    return !(rec1[2] <= rec2[0] ||
             rec2[2] <= rec1[0] ||
             rec1[3] <= rec2[1] ||
             rec2[3] <= rec1[1] ||
             rec1[4] <= rec2[2] ||
             rec2[4] <= rec1[2]);
}





Fix32Point midpoint
(Fix32Point pt0,
 Fix32Point pt1) {
    Fix32Point result;
    x_of(result) = (x_of(pt0) + x_of(pt1)) >> 1;
    y_of(result) = (y_of(pt0) + y_of(pt1)) >> 1;
    z_of(result) = (z_of(pt0) + z_of(pt1)) >> 1;

    return result;
}



Fix32Point_2D closest_point_2D
(Fix32Point_2D point,
 Fix32Seg_2D seg) {
    if (seg.pt0.x == seg.pt1.x && seg.pt0.y == seg.pt1.y)
        return seg.pt1;
    
    // 2022-08-10 does this even work?
    Fix32Point_2D P = point, L = seg.pt0, R = seg.pt1, I;

    Fix32Point_2D P_L, R_L;
    P_L.x = P.x - L.x;
    P_L.y = P.y - L.y;
    R_L.x = R.x - L.x;
    R_L.y = R.y - L.y;

    //TODO make safe for equal endpoints
    fix64_t dot = (((fix64_t)(P_L.x) * (fix64_t)(R_L.x) + (fix64_t)(P_L.y) * (fix64_t)(R_L.y)));
    fix64_t norm = (((fix64_t)(R_L.x) * (fix64_t)(R_L.x) + (fix64_t)(R_L.y) * (fix64_t)(R_L.y)));


    if (dot < 0) {
        return L;
    }
    else if (dot > norm) {
        return R;
    }
    else {
        fix32_t t = (fix32_t)(dot / (norm>>16));
        I.x = (fix32_t)(L.x + FIX_MUL(t, R_L.x));
        I.y = (fix32_t)(L.y + FIX_MUL(t, R_L.y));
        
        return I;
    }
}


fix64_t triangle_area_3D(Fix64Point P, Fix64Point Q, Fix64Point R) {
    Fix64Point A, B;
    x_of(A) = x_of(Q) - x_of(P);
    y_of(A) = y_of(Q) - y_of(P);
    z_of(A) = z_of(Q) - z_of(P);
    x_of(B) = x_of(R) - x_of(P);
    y_of(B) = y_of(R) - y_of(P);
    z_of(B) = z_of(R) - z_of(P);
    //16.16
    
    fix64_t dir_x = (y_of(A)*z_of(B) - z_of(A)*y_of(B))>>(24);
    fix64_t dir_y = (z_of(A)*x_of(B) - x_of(A)*z_of(B))>>(24);
    fix64_t dir_z = (x_of(A)*y_of(B) - y_of(A)*x_of(B))>>(24);
    //32.8

    
    fix64_t norm = ((dir_x)*(dir_x) +
                    (dir_y)*(dir_y) +
                    (dir_z)*(dir_z)); // squared
    
    fix64_t prev_guess = 0;
    fix64_t guess = 1LL<<16; // 1 as (47)1.16
    while (ABS(prev_guess - guess) > 10 && guess != 0) {
        prev_guess = guess;
        guess = ( ( guess  ) + ((norm )/( guess  )) )>>1;
        //      ( (47)1.16 ) + ((32.32)/((47)1.16))  =  (47)1.16 + 31.16
    }
    
    return guess;
}



fix64_t triangle_area_3D2(Fix64Point P, Fix64Point Q, Fix64Point R) {
    Fix64Point A, B;
    x_of(A) = x_of(Q) - x_of(P);
    y_of(A) = y_of(Q) - y_of(P);
    z_of(A) = z_of(Q) - z_of(P);
    x_of(B) = x_of(R) - x_of(P);
    y_of(B) = y_of(R) - y_of(P);
    z_of(B) = z_of(R) - z_of(P);
    //16.16
    
    fix64_t dir_x = (y_of(A)*z_of(B) - z_of(A)*y_of(B))>>(16+8);
    fix64_t dir_y = (z_of(A)*x_of(B) - x_of(A)*z_of(B))>>(16+8);
    fix64_t dir_z = (x_of(A)*y_of(B) - y_of(A)*x_of(B))>>(16+8);
    //32.8

    fix64_t norm = ((dir_x)*(dir_x) +
                    (dir_y)*(dir_y) +
                    (dir_z)*(dir_z)); // squared
    fix64_t prev_guess = 0;
    fix64_t guess = 1<<16; // 1 as (47)1.16
    while (ABS(prev_guess - guess) > 10 && guess != 0) {
        prev_guess = guess;
        guess = ( ( guess  ) + ((norm )/( guess  )) )>>1;
        //      ( (47)1.16 ) + ((32.32)/((47)1.16))  =  (47)1.16 + 31.16
    }
    
    return guess;
}

uint8_t sideof_plane(WitPoint V, WitPoint planeV, WitPoint planeN) {
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


uint8_t sideof_tri(WitPoint P, Triangle tri) {
    fix64_t
        Ax = x_of(tri.v0) - x_of(P),
        Ay = y_of(tri.v0) - y_of(P),
        Az = z_of(tri.v0) - z_of(P),

        Bx = x_of(tri.v1) - x_of(P),
        By = y_of(tri.v1) - y_of(P),
        Bz = z_of(tri.v1) - z_of(P),

        Cx = x_of(tri.v2) - x_of(P),
        Cy = y_of(tri.v2) - y_of(P),
        Cz = z_of(tri.v2) - z_of(P);

    // now as if camera point P is at origin, and as if the triangle is A->B->C

    fix64_t S =
        + Ax*((By*Cz-Cy*Bz)>>16)
        - Ay*((Bx*Cz-Cx*Bz)>>16)
        + Az*((Bx*Cy-Cx*By)>>16);
    // keep as .32 since we don't need to compute with it further.

    if (S == 0)
        return SIDE_MID;
    else if (S < 0)
        return SIDE_FRONT;
    else
        return SIDE_BACK;
}

bool plane_normal(Fix32Point *out_N, Fix32Point *P, Fix32Point *Q, Fix32Point *R) {
    //printf("P: (%X, %X, %X)\n", x_of(*P), y_of(*P), z_of(*P));
    //printf("Q: (%X, %X, %X)\n", x_of(*Q), y_of(*Q), z_of(*Q));
    //printf("R: (%X, %X, %X)\n", x_of(*R), y_of(*R), z_of(*R));
    
    Fix64Point A, B;
    x_of(A) = ((fix64_t)x_of(*Q) - (fix64_t)x_of(*P));
    y_of(A) = ((fix64_t)y_of(*Q) - (fix64_t)y_of(*P));
    z_of(A) = ((fix64_t)z_of(*Q) - (fix64_t)z_of(*P));
    x_of(B) = ((fix64_t)x_of(*R) - (fix64_t)x_of(*P));
    y_of(B) = ((fix64_t)y_of(*R) - (fix64_t)y_of(*P));
    z_of(B) = ((fix64_t)z_of(*R) - (fix64_t)z_of(*P));
    //16.16

    //printf("A: (%lX, %lX, %lX)\n", x_of(A), y_of(A), z_of(A));
    //printf("B: (%lX, %lX, %lX)\n", x_of(B), y_of(B), z_of(B));

    // overflow when UINT32_MAX
    fix64_t dir_x = ((y_of(A)>>4)*(z_of(B)>>4) - (z_of(A)>>4)*(y_of(B)>>4));
    fix64_t dir_y = ((z_of(A)>>4)*(x_of(B)>>4) - (x_of(A)>>4)*(z_of(B)>>4));
    fix64_t dir_z = ((x_of(A)>>4)*(y_of(B)>>4) - (y_of(A)>>4)*(x_of(B)>>4));
    //32.32

    //printf("(%lX, %lX, %lX)\n", dir_x>>16, dir_y>>16, dir_z>>16);
    //putchar('\n');


    // We need to square dir_x>>16, dir_y>>16, and dir_z>>16, so to avoid
    // overflow we halve them until they are less than 1<<32. This is okay since
    // the ultimate goal here is to normalize the direction vector. We're just
    // doing some of the normalization early.
    while (ABS(dir_x) >= 1LL<<46 || ABS(dir_y) >= 1LL<<46 || ABS(dir_z) >= 1LL<<46) {
        dir_x >>= 1;
        dir_y >>= 1;
        dir_z >>= 1;
    }
    
    fix64_t norm = ((dir_x>>16)*(dir_x>>16) +
                    (dir_y>>16)*(dir_y>>16) +
                    (dir_z>>16)*(dir_z>>16)); // squared
    //64.16 TODO
    norm = (fix64_t)sqrt_ufix32_from_ufix64(norm);
    //16.16
    
    if (norm == 0) {
        printf("Cannot compute normal of degenerate triangle. Did you mean to compute its normal?\n");
        return false;
    }

    x_of(*out_N) = (fix32_t)((dir_x)/norm);
    y_of(*out_N) = (fix32_t)((dir_y)/norm);
    z_of(*out_N) = (fix32_t)((dir_z)/norm);
    //16.16
    
    return true;
}

Fix32Point_2D rotate_Fix32Point_2D(Fix32Point_2D pt, ang18_t ang) {
    Fix32Point_2D result;
    
    fix32_t cos, sin;
    cossin18(&cos, &sin, ang);
    result.x = (fix32_t)(+ FIX_MUL(pt.x, cos) + FIX_MUL(pt.y, sin));
    result.y = (fix32_t)(- FIX_MUL(pt.x, sin) + FIX_MUL(pt.y, cos));
    
    return result;
}

void Mat4Mul(Vector4 *out_vec, Matrix4 mat, Vector4 vec) {
    *out_vec[0] = (mat[0][0]*vec[0] + mat[0][1]*vec[1] +
                   mat[0][2]*vec[2] + mat[0][3]*vec[3])>>16;
    *out_vec[1] = (mat[1][0]*vec[0] + mat[1][1]*vec[1] +
                   mat[1][2]*vec[2] + mat[1][3]*vec[3])>>16;
    *out_vec[2] = (mat[2][0]*vec[0] + mat[2][1]*vec[1] +
                   mat[2][2]*vec[2] + mat[2][3]*vec[3])>>16;
    *out_vec[3] = (mat[3][0]*vec[0] + mat[3][1]*vec[1] +
                   mat[3][2]*vec[2] + mat[3][3]*vec[3])>>16;
}



bool point_in_tri(WitPoint P, WitPoint V0, WitPoint V1, WitPoint V2) {
    // this function assumes that the point is coplanar with the triangle

    // I predict that most polygons can be rejected simply by virtue of the
    // intersection point lying outside an AABB of the triangle. This check also
    // helps keep the later area computations from overflowing, since the point
    // of intersection isn't "far" from the triangle.
    fix32_t xmax = MAX(MAX(x_of(V0), x_of(V1)), x_of(V2)) + (1<<16);
    fix32_t xmin = MIN(MIN(x_of(V0), x_of(V1)), x_of(V2)) - (1<<16);
    fix32_t ymax = MAX(MAX(y_of(V0), y_of(V1)), y_of(V2)) + (1<<16);
    fix32_t ymin = MIN(MIN(y_of(V0), y_of(V1)), y_of(V2)) - (1<<16);
    fix32_t zmax = MAX(MAX(z_of(V0), z_of(V1)), z_of(V2)) + (1<<16);
    fix32_t zmin = MIN(MIN(z_of(V0), z_of(V1)), z_of(V2)) - (1<<16);

    if (x_of(P) < xmin || x_of(P) > xmax ||
        y_of(P) < ymin || y_of(P) > ymax ||
        z_of(P) < zmin || z_of(P) > zmax) {
        return false;
    }

#define as_F64P(V) (Fix64Point){{x_of(V), y_of(V), z_of(V)}}
    Fix64Point V0_64 = as_F64P(V0);
    Fix64Point V1_64 = as_F64P(V1);
    Fix64Point V2_64 = as_F64P(V2);
    Fix64Point P_64 = as_F64P(P);

    fix64_t area =  triangle_area_3D(V0_64, V1_64, V2_64);
    fix64_t area0 = triangle_area_3D(P_64, V1_64, V2_64);
    fix64_t area1 = triangle_area_3D(P_64, V0_64, V2_64);
    fix64_t area2 = triangle_area_3D(P_64, V0_64, V1_64);
    
    dibassert(area >= 0 && area0 >= 0 && area1 >= 0 && area2 >= 0);

    /*
    printf("%8lx + %8lx + %8lx = %8lx\n", area0, area1, area2, area0 + area1 + area2);
    printf("%41lx\n", area);

    fix64_t area0_div = (area0<<16)/area;
    fix64_t area1_div = (area1<<16)/area;
    fix64_t area2_div = (area2<<16)/area;

    printf("%8lx + %8lx + %8lx = %8lx\n", area0_div, area1_div, area2_div, area0_div + area1_div + area2_div);
    printf("%41lx\n", 1L<<16);
    
    putchar('\n');
    */

    #define PREC_LOSS 1
    // In the following test, we disregard some low bits since sometimes the sum
    // of areas will be just a tiny bit above the total triangle area even when
    // the point is actually in the triangle.
    if ((area0 + area1 + area2)>>PREC_LOSS > area>>PREC_LOSS) {
        //printf("face %zu: intersection point outside visible triangle\n", i_face);
        return false;
    }
    #undef PREC_LOSS
    
    return true;
}



bool point_in_tri2(WitPoint P, WitPoint V0, WitPoint V1, WitPoint V2) {
    // this function assumes that the point is coplanar with the triangle
    // TODO: Only need to check if the PIXEL inside the SCREEN TRIANGLE
#define as_F64P(V) (Fix64Point){{x_of(V), y_of(V), z_of(V)}}
    //potentially new closest point; must ensure point is actually
    //within triangle
    Fix64Point V0_64 = as_F64P(V0);
    Fix64Point V1_64 = as_F64P(V1);
    Fix64Point V2_64 = as_F64P(V2);
    Fix64Point P_64 = as_F64P(P);
    
    fix64_t area =  triangle_area_3D(V0_64, V1_64, V2_64);
    fix64_t area0 = triangle_area_3D(P_64, V1_64, V2_64);
    fix64_t area1 = triangle_area_3D(P_64, V0_64, V2_64);
    fix64_t area2 = triangle_area_3D(P_64, V0_64, V1_64);

    area0 = (area0<<16)/area;
    area1 = (area1<<16)/area;
    area2 = (area2<<16)/area;

    if (area0 < 0 || area1 < 0 || area2 < 0 ||
        area0 + area1 + area2 > 1<<16) {
        //printf("face %zu: intersection point outside visible triangle\n", i_face);
        return false;
    }

    return true;
}
