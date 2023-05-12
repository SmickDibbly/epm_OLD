#include <ctype.h>

#include "src/world/mesh.h"
#include "src/world/world.h"
#include "src/draw/draw.h"
#include "src/draw/textures.h"
#include "src/system/dir.h"

//#define VERBOSITY
#include "verbosity.h"

#undef LOG_LABEL
#define LOG_LABEL "MESH"

typedef struct DIBJFileOverview {
    size_t num_lines;
    
    size_t ofs_v;
    size_t num_v;
    
    size_t ofs_e;
    size_t num_e;
    
    size_t ofs_f;
    size_t num_f;
    
    size_t ofs_vn;
    size_t num_vn;
    
    size_t ofs_en;
    size_t num_en;
    
    size_t ofs_fn;
    size_t num_fn;
    
    size_t ofs_texname;
    size_t num_texname;
    
    size_t ofs_ft;
    size_t num_ft;

    size_t ofs_ftxl;
    size_t num_ftxl;
} DIBJFileOverview;

static size_t read_fix_x(char const *str, fix32_t *out_num);
static void read_line(FILE *in_fp, char *str);
static void file_overview(DIBJFileOverview *data, FILE *in_fp);

static size_t num_texnames = 0;
static char texnames[64][MAX_TEXTURE_NAME_LEN] = {'\0'};

void print_mesh(Mesh *mesh);

void compute_face_normals(WitPoint *vertices, size_t num_faces, Face *faces) {
    for (size_t i_face = 0; i_face < num_faces; i_face++) {
        Face *face = faces+i_face;
        face->flags = 0;
        
        if (face->i_v0 == face->i_v1 ||
            face->i_v1 == face->i_v2 ||
            face->i_v2 == face->i_v0) {
            epm_Log(LT_WARN, "Encountered degenerate polygon: %zu.\n", i_face);
            face->flags |= FC_DEGEN;
            continue;
        }

        bool res = plane_normal(&face->normal,
                                &vertices[face->i_v0],
                                &vertices[face->i_v1],
                                &vertices[face->i_v2]);

        if (!res) {
            epm_Log(LT_WARN, "WARNING: Encountered degenerate polygon during plane normal computation: %zu.\n", i_face);
            face->flags |= FC_DEGEN;
            continue;
        }

        /*printf("HI %.10lf, %.10lf, %.10lf\n",
               FIX_dbl(x_of(face->normal)),
               FIX_dbl(y_of(face->normal)),
               FIX_dbl(z_of(face->normal)));*/
    }
}

void compute_face_brightnesses(size_t num_faces, Face *faces) {
    for (size_t i_face = 0; i_face < num_faces; i_face++) {
        Face *face = faces+i_face;
        
        if (face->flags & FC_DEGEN) {
            face->brightness = 255;
            continue;
        }
        
        fix64_t prod = dot(face->normal, directional_light_vector);   
        fix64_t tmp;
        tmp = (prod * (fix64_t)directional_light_intensity)>>16; // [-1, 1] (.16)
        tmp += (1<<16); // [0, 2] (.16)
        tmp = LSHIFT64(tmp, 7); // [0, 256] (fixed .16)
        tmp >>= 16; // [0, 256] (integer .0)
        dibassert(tmp < 256);
        face->brightness = (uint8_t)tmp;
    }
}

int count_edges_from_faces(size_t num_faces, Face *faces) {
    size_t num_edges = 0;
    size_t i_fv0, i_fv1, i_fv2, i_ev0, i_ev1;
    bool duplicate;

    Edge edges_tmp[MAX_MESH_EDGES];
    
    for (size_t i_face = 0; i_face < num_faces; i_face++) {
        i_fv0 = faces[i_face].i_v0;
        i_fv1 = faces[i_face].i_v1;
        i_fv2 = faces[i_face].i_v2;
        
        i_ev0 = MIN(i_fv0, i_fv1);
        i_ev1 = MAX(i_fv0, i_fv1);
        duplicate = false;
        for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
            if ((edges_tmp[i_edge].i_v0 == i_ev0 && edges_tmp[i_edge].i_v1 == i_ev1)) {
                duplicate = true;
                break; // from i_edge loop
            }
        }
        if ( ! duplicate) { // add to list
            edges_tmp[num_edges].i_v0 = i_ev0;
            edges_tmp[num_edges].i_v1 = i_ev1;
            num_edges++;
        }


        i_ev0 = MIN(i_fv1, i_fv2);
        i_ev1 = MAX(i_fv1, i_fv2);
        duplicate = false;
        for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
            if ((edges_tmp[i_edge].i_v0 == i_ev0 && edges_tmp[i_edge].i_v1 == i_ev1)) {
                duplicate = true;
                break; // from i_edge loop
            }
        }
        if ( ! duplicate) { // add to list
            edges_tmp[num_edges].i_v0 = i_ev0;
            edges_tmp[num_edges].i_v1 = i_ev1;
            num_edges++;
        }        


        i_ev0 = MIN(i_fv2, i_fv0);
        i_ev1 = MAX(i_fv2, i_fv0);
        duplicate = false;
        for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
            if ((edges_tmp[i_edge].i_v0 == i_ev0 && edges_tmp[i_edge].i_v1 == i_ev1)) {
                duplicate = true;
                break; // from i_edge loop
            }
        }
        if ( ! duplicate) { // add to list
            edges_tmp[num_edges].i_v0 = i_ev0;
            edges_tmp[num_edges].i_v1 = i_ev1;
            num_edges++;
        }
    }

    dibassert(num_edges <= INT_MAX);
    
    return (int)num_edges;
}

epm_Result compute_edges_from_faces
(size_t max_edges, Edge *edges,
 size_t num_faces, Face *faces) {
    size_t num_edges = 0;
    size_t i_fv0, i_fv1, i_fv2, i_ev0, i_ev1;
    bool duplicate;
    
    for (size_t i_face = 0; i_face < num_faces; i_face++) {
        i_fv0 = faces[i_face].i_v0;
        i_fv1 = faces[i_face].i_v1;
        i_fv2 = faces[i_face].i_v2;
        
        i_ev0 = MIN(i_fv0, i_fv1);
        i_ev1 = MAX(i_fv0, i_fv1);
        duplicate = false;
        for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
            if ((edges[i_edge].i_v0 == i_ev0 && edges[i_edge].i_v1 == i_ev1)) {
                duplicate = true;
                break; // from i_edge loop
            }
        }
        if ( ! duplicate) { // add to list
            if (num_edges == max_edges)
                return EPM_ERROR;
            edges[num_edges].i_v0 = i_ev0;
            edges[num_edges].i_v1 = i_ev1;
            num_edges++;
        }


        i_ev0 = MIN(i_fv1, i_fv2);
        i_ev1 = MAX(i_fv1, i_fv2);
        duplicate = false;
        for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
            if ((edges[i_edge].i_v0 == i_ev0 && edges[i_edge].i_v1 == i_ev1)) {
                duplicate = true;
                break; // from i_edge loop
            }
        }
        if ( ! duplicate) { // add to list
            if (num_edges == max_edges)
                return EPM_ERROR;
            edges[num_edges].i_v0 = i_ev0;
            edges[num_edges].i_v1 = i_ev1;
            num_edges++;
        }        


        i_ev0 = MIN(i_fv2, i_fv0);
        i_ev1 = MAX(i_fv2, i_fv0);
        duplicate = false;
        for (size_t i_edge = 0; i_edge < num_edges; i_edge++) {
            if ((edges[i_edge].i_v0 == i_ev0 && edges[i_edge].i_v1 == i_ev1)) {
                duplicate = true;
                break; // from i_edge loop
            }
        }
        if ( ! duplicate) { // add to list
            if (num_edges == max_edges)
                return EPM_ERROR;
            edges[num_edges].i_v0 = i_ev0;
            edges[num_edges].i_v1 = i_ev1;
            num_edges++;
        }
    }

    dibassert(num_edges == max_edges);
    
    return EPM_SUCCESS;
}

void write_world(StaticGeometry const *geo, char const *filename) {
    char path[256];
    strcpy(path, DIR_WORLD);
    strcat(path, filename);
    strcat(path, SUF_WORLD);
    
    FILE *out_fp = fopen(path, "w");
    if (!out_fp) {
        epm_Log(LT_ERROR, "Could not open file for writing: %s\n", path);
        return;
    }

    fprintf(out_fp, "# EWD0\n\n");

    size_t num_V = geo->num_vertices;
    Fix32Point const *V = geo->vertices;
    for (size_t i_V = 0; i_V < num_V; i_V++) {
        fprintf(out_fp, "v  %s  %s  %s\n",
                fmt_fix_x(x_of(V[i_V]), 16),
                fmt_fix_x(y_of(V[i_V]), 16),
                fmt_fix_x(z_of(V[i_V]), 16));
    }
    fputc('\n', out_fp);

    size_t num_F = geo->num_faces;
    Face const *F = geo->faces;
    for (size_t i_F = 0; i_F < num_F; i_F++) {
        fprintf(out_fp, "f  %zu  %zu  %zu\n",
                F[i_F].i_v0, F[i_F].i_v1, F[i_F].i_v2);
    }
    fputc('\n', out_fp);

    for (size_t i_F = 0; i_F < num_F; i_F++) {
        fprintf(out_fp, "fn  %s  %s  %s\n",
                fmt_fix_x(x_of(F[i_F].normal), 16),
                fmt_fix_x(y_of(F[i_F].normal), 16),
                fmt_fix_x(z_of(F[i_F].normal), 16));
    }
    fputc('\n', out_fp);

    
    /* Save texture data */    
    size_t i_i_tex = 0, num_i_texs = 0;
    size_t i_texs[64];
    for (size_t i_F = 0; i_F < num_F; i_F++) {
        size_t i_tex = F[i_F].i_tex;
        bool new = true;
        for (size_t j = 0; j < num_i_texs; j++) {
            if (i_texs[j] == i_tex) {
                new = false;
                break;
            }
        }

        if (new) {
            i_texs[i_i_tex] = i_tex;
            i_i_tex++;
            num_i_texs++;
        }
    }

    for (size_t j = 0; j < num_i_texs; j++) {
        fprintf(out_fp, "texname  \"%s\"\n", textures[i_texs[j]].name);
    }

    fputc('\n', out_fp);

    for (size_t i_F = 0; i_F < num_F; i_F++) {
        size_t i_i_tex = 666;
        for (size_t j = 0; j < num_i_texs; j++) {
            if (i_texs[j] == F[i_F].i_tex) {
                i_i_tex = j;
                break;                
            }
        }
        fprintf(out_fp, "ft  %zu\n", i_i_tex);
    }
    
    fputc('\n', out_fp);

    for (size_t i_F = 0; i_F < num_F; i_F++) {
        fprintf(out_fp, "ftxl %s %s ; %s %s ; %s %s\n",
                fmt_fix_x(F[i_F].tv0.x, 16), fmt_fix_x(F[i_F].tv0.y, 16),
                fmt_fix_x(F[i_F].tv1.x, 16), fmt_fix_x(F[i_F].tv1.y, 16),
                fmt_fix_x(F[i_F].tv2.x, 16), fmt_fix_x(F[i_F].tv2.y, 16));
    }
    
    fclose(out_fp);
}

/*
epm_Result read_world(StaticGeometry *geo, char *filename) {
    geo->num_vertices = 0;
    geo->num_edges = 0;
    geo->num_faces = 0;
    
    FILE *in_fp;

    char path[256];
    strcpy(path, DIR_WORLD);
    strcat(path, filename);
    strcat(path, SUF_WORLD);

    epm_Log(LT_INFO, "Loading world from file: %s", path);

    in_fp = fopen(path, "rb");
    if ( ! in_fp) {
        epm_Log(LT_WARN, "File not found: %s", path);
        return EPM_FAILURE;
    }
    
    DIBJFileOverview data = {0};
    file_overview(&data, in_fp);

    vbs_printf("  num_lines: %zu\n", data.num_lines);
    vbs_printf("      ofs_v: %zu\n", data.ofs_v);
    vbs_printf("      num_v: %zu\n", data.num_v);
    vbs_printf("      ofs_e: %zu\n", data.ofs_e);
    vbs_printf("      num_e: %zu\n", data.num_e);
    vbs_printf("      ofs_f: %zu\n", data.ofs_f);
    vbs_printf("      num_f: %zu\n", data.num_f);
    vbs_printf("     ofs_vn: %zu\n", data.ofs_vn);
    vbs_printf("     num_vn: %zu\n", data.num_vn);
    vbs_printf("     ofs_en: %zu\n", data.ofs_en);
    vbs_printf("     num_en: %zu\n", data.num_en);
    vbs_printf("     ofs_fn: %zu\n", data.ofs_fn);
    vbs_printf("     num_fn: %zu\n", data.num_fn);
    vbs_printf("ofs_texname: %zu\n", data.ofs_texname);
    vbs_printf("num_texname: %zu\n", data.num_texname);
    vbs_printf("     ofs_ft: %zu\n", data.ofs_ft);
    vbs_printf("     num_ft: %zu\n", data.num_ft);
    vbs_printf("   ofs_ftxl: %zu\n", data.ofs_ftxl);
    vbs_printf("   num_ftxl: %zu\n", data.num_ftxl);

    if (data.num_fn > 0 && data.num_fn != data.num_f) {
        epm_Log(LT_WARN, "Geo file %s: number of face normal entries does not match number of faces.\n", filename);
        return EPM_FAILURE;
    }
    if (data.num_ft > 0 && data.num_ft != data.num_f) {
        epm_Log(LT_WARN, "Geo file %s: number of face texture entries does not match number of face entries.\n", filename);
        return EPM_FAILURE;
    }
    if (data.num_ftxl > 0 && data.num_ftxl != data.num_f) {
        epm_Log(LT_WARN, "Mesh file %s: number of face texel entries does not match number of face entries.\n", filename);
        return EPM_FAILURE;
    }
    
    char line[256] = {'\0'};    

    if (data.num_v > 0) {
        geo->num_vertices = data.num_v;
        
        fseek(in_fp, data.ofs_v, SEEK_SET);

        size_t i_vertex = 0;
        size_t i_ch;
        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("v ", line)) break;

            i_ch = strlen("v ");
            WitPoint vertex;

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &x_of(vertex));

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &y_of(vertex));
            
            while (line[i_ch] == ' ')  i_ch++;
            i_ch += read_fix_x(line+i_ch, &z_of(vertex));

            geo->vertices[i_vertex] = vertex;
            i_vertex++;
            
            if (feof(in_fp)) break;
        }
        
        dibassert(geo->num_vertices == i_vertex);
    }

    
    if (data.num_f > 0) {
        geo->num_faces = data.num_f;
        
        fseek(in_fp, data.ofs_f, SEEK_SET);

        size_t i_face = 0;    
        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("f ", line)) break;

            size_t i_v0, i_v1, i_v2;
        
            sscanf(line+2, " %zu %zu %zu", &i_v0, &i_v1, &i_v2);
            
            geo->faces[i_face].i_v0 = i_v0;
            geo->faces[i_face].i_v1 = i_v1;
            geo->faces[i_face].i_v2 = i_v2;
            i_face++;

            if (feof(in_fp)) break;
        }

        dibassert(geo->num_faces == i_face);
    }
    
    if (data.num_fn > 0) {
        fseek(in_fp, data.ofs_fn, SEEK_SET);
 
        size_t i_face = 0;
        size_t i_ch = 0;
        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("fn ", line)) break;

            i_ch = strlen("fn ");
            WitPoint normal;

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &x_of(normal));

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &y_of(normal));
            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &z_of(normal));
            
            geo->faces[i_face].normal = normal;
            i_face++;

            if (feof(in_fp)) break;
        }
    }
    
    if (data.num_texname > 0) {
        fseek(in_fp, data.ofs_texname, SEEK_SET);

        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("texname ", line)) break;

            size_t i_ch = strlen("texname ");
            while (line[i_ch] == ' ') i_ch++;
            i_ch++; // skip opening "
            
            size_t i = 0;
            while ((texnames[num_texnames][i++] = line[i_ch++]) != '"')
                ;
            texnames[num_texnames][i-1] = '\0';
            num_texnames++;
            // currently file-local index; still needs to be translated to
            // global texture array index.
                                
            if (feof(in_fp)) break;   
        }
    }
    
    if (data.num_ft > 0) {
        fseek(in_fp, data.ofs_ft, SEEK_SET);
    
        size_t i_face = 0;
        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("ft ", line)) break;

            size_t i_tex;
            
            sscanf(line+strlen("ft "), " %zu", &i_tex);
            geo->faces[i_face].i_tex = i_tex;
            get_texture_by_name(texnames[i_tex], &geo->faces[i_face].i_tex);
            
            i_face++;

            if (feof(in_fp)) break;
        }
    }

    if (data.num_ftxl > 0) {
        fseek(in_fp, data.ofs_ftxl, SEEK_SET);
        
        size_t i_ch = 0;
        size_t i_face = 0;
        while (true) {
            read_line(in_fp, line);
            
            if (line[0] == '#') continue;
            if ( ! prefix("ftxl ", line)) break;
            
            i_ch = strlen("ftxl ");
            Fix32Point_2D tv;

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.x);

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.y);
            
            geo->faces[i_face].tv0 = tv;

            
            while (line[i_ch] == ' ') i_ch++;
            i_ch++; // skip semicolon

            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.x);
            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.y);

            geo->faces[i_face].tv1 = tv;


            while (line[i_ch] == ' ') i_ch++;
            i_ch++; // skip semicolon

            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.x);
            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.y);

            geo->faces[i_face].tv2 = tv;
            
            i_face++;
            
            if (feof(in_fp)) break;
        }
    }

    fclose(in_fp);
    
    num_texnames = 0;

    compute_face_brightnesses(geo->num_faces, geo->faces);
    geo->num_edges = count_edges_from_faces(geo->num_faces, geo->faces);
    compute_edges_from_faces(geo->num_edges, geo->edges,
                             geo->num_faces, geo->faces);


    for (size_t i_texnames = 0; i_texnames < 64; i_texnames++) {
        texnames[i_texnames][0] = '\0';
    }
    
    return EPM_SUCCESS;
}
*/

void write_Mesh_dibj_1(Mesh const *mesh, char const *filename) {
    FILE *out_fp = fopen(filename, "w");
    if (!out_fp) {
        epm_Log(LT_ERROR, "Could not open file for writing: %s\n", filename);
        return;
    }

    fprintf(out_fp, "# DIBJ\n\n");

    size_t num_V = mesh->num_vertices;
    Fix32Point const *V = mesh->vertices;
    for (size_t i_V = 0; i_V < num_V; i_V++) {
        fprintf(out_fp, "v  %s  %s  %s\n",
                fmt_fix_x(x_of(V[i_V]), 16),
                fmt_fix_x(y_of(V[i_V]), 16),
                fmt_fix_x(z_of(V[i_V]), 16));
    }
    fputc('\n', out_fp);

    size_t num_F = mesh->num_faces;
    Face const *F = mesh->faces;
    for (size_t i_F = 0; i_F < num_F; i_F++) {
        fprintf(out_fp, "f  %zu  %zu  %zu\n",
                F[i_F].i_v0, F[i_F].i_v1, F[i_F].i_v2);
    }
    fputc('\n', out_fp);

    for (size_t i_F = 0; i_F < num_F; i_F++) {
        fprintf(out_fp, "fn  %s  %s  %s\n",
                fmt_fix_x(x_of(F[i_F].normal), 16),
                fmt_fix_x(y_of(F[i_F].normal), 16),
                fmt_fix_x(z_of(F[i_F].normal), 16));
    }
    fputc('\n', out_fp);

    
    /* Save texture data */    
    size_t i_i_tex = 0, num_i_texs = 0;
    size_t i_texs[64];
    for (size_t i_F = 0; i_F < num_F; i_F++) {
        size_t i_tex = F[i_F].i_tex;
        bool new = true;
        for (size_t j = 0; j < num_i_texs; j++) {
            if (i_texs[j] == i_tex) {
                new = false;
                break;
            }
        }

        if (new) {
            i_texs[i_i_tex] = i_tex;
            i_i_tex++;
            num_i_texs++;
        }
    }

    for (size_t j = 0; j < num_i_texs; j++) {
        fprintf(out_fp, "texname  \"%s\"\n", textures[i_texs[j]].name);
    }

    fputc('\n', out_fp);

    for (size_t i_F = 0; i_F < num_F; i_F++) {
        size_t i_i_tex = 666;
        for (size_t j = 0; j < num_i_texs; j++) {
            if (i_texs[j] == F[i_F].i_tex) {
                i_i_tex = j;
                break;                
            }
        }
        fprintf(out_fp, "ft  %zu\n", i_i_tex);
    }
    
    fputc('\n', out_fp);

    for (size_t i_F = 0; i_F < num_F; i_F++) {
        fprintf(out_fp, "ftxl %s %s ; %s %s ; %s %s\n",
                fmt_fix_x(F[i_F].tv0.x, 16), fmt_fix_x(F[i_F].tv0.y, 16),
                fmt_fix_x(F[i_F].tv1.x, 16), fmt_fix_x(F[i_F].tv1.y, 16),
                fmt_fix_x(F[i_F].tv2.x, 16), fmt_fix_x(F[i_F].tv2.y, 16));
    }
    
    fclose(out_fp);
}



static void postread(Mesh *mesh) {
    compute_face_normals(mesh->vertices, mesh->num_faces, mesh->faces);
    compute_face_brightnesses(mesh->num_faces, mesh->faces);

    mesh->num_edges = count_edges_from_faces(mesh->num_faces, mesh->faces);
    mesh->edges = zgl_Malloc(mesh->num_edges * sizeof(*(mesh->edges)));
    epm_Result res =
        compute_edges_from_faces(mesh->num_edges, mesh->edges,
                                 mesh->num_faces, mesh->faces);

    if (res == EPM_ERROR) {
        epm_Log(LT_ERROR, "Maximum number of edges smaller than actual number of edges.\n");
        exit(0); // TODO: Handle properly.
    }

    // Temporarily: all textures and texels the same. TODO: Unrestrict.
    for (size_t i_face = 0; i_face < mesh->num_faces; i_face++) {
        Face *face = mesh->faces + i_face;

        face->i_tex = 3;
        epm_Texture *tex = &textures[face->i_tex];
        face->tv0 = (Fix32Point_2D){(int)(tex->w << 16) - 1, 0};
        face->tv1 = (Fix32Point_2D){0, 0};
        face->tv2 = (Fix32Point_2D){0, (int)(tex->h << 16) - 1};   
    }

}

/*
static void validate_data(DIBJFileOverview const *_data) {
    DIBJFileOverview data = *_data;
}
*/

static void file_overview(DIBJFileOverview *data, FILE *in_fp) {
    char line[256] = {'\0'};
    size_t ofs_line;

    rewind(in_fp);
    
    size_t i_ch = 0;
    while (true) {
        ofs_line = ftell(in_fp);
        while ((line[i_ch] = (char)fgetc(in_fp)) != '\n' && line[i_ch] != EOF) {
            i_ch++;
        }

        data->num_lines++;
        
        if (line[0] == '\n') { // line was just a newline-character
            i_ch = 0;
            continue;
        }
        else if (line[i_ch] == EOF) {
            break;
        }

        i_ch++;
        line[i_ch] = '\0';

        if (line[0] == 'v' && line[1] == ' ') {
            if (data->ofs_v == 0) { // first vertex
                data->ofs_v = ofs_line;
            }
            data->num_v++;
        }
        else if (line[0] == 'e' && line[1] == ' ') {
            if (data->ofs_e == 0) { // first vertex
                data->ofs_e = ofs_line;
            }
            data->num_e++;
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            if (data->ofs_f == 0) { // first vertex
                data->ofs_f = ofs_line;
            }
            data->num_f++;
        }
        else if (line[0] == 'v' && line[1] == 'n' && line[2] == ' ') {
            if (data->ofs_vn == 0) { // first vertex
                data->ofs_vn = ofs_line;
            }
            data->num_vn++;
        }
        else if (line[0] == 'e' && line[1] == 'n' && line[2] == ' ') {
            if (data->ofs_en == 0) { // first vertex
                data->ofs_en = ofs_line;
            }
            data->num_en++;
        }
        else if (line[0] == 'f' && line[1] == 'n' && line[2] == ' ') {
            if (data->ofs_fn == 0) { // first vertex
                data->ofs_fn = ofs_line;
            }
            data->num_fn++;
        }
        else if (line[0] == 'f' && line[1] == 't' && line[2] == ' ') {
            if (data->ofs_ft == 0) { // first vertex
                data->ofs_ft = ofs_line;
            }
            data->num_ft++;
        }
        else if (prefix("texname ", line)) {
            if (data->ofs_texname == 0) { // first vertex
                data->ofs_texname = ofs_line;
            }
            data->num_texname++;
        }
        else if (prefix("ftxl ", line)) {
            if (data->ofs_ftxl == 0) { // first vertex
                data->ofs_ftxl = ofs_line;
            }
            data->num_ftxl++;
        }

        data->num_lines++;
        i_ch = 0;
    }

    rewind(in_fp);
}

static size_t read_fix_x(char const *str, fix32_t *out_num) {
    fix32_t res = 0;
    bool neg = false;
    size_t i = 0;
    
    if (str[i] == '-') {
        neg = true;
        i++;
    }
    
    while (true) {
        switch (str[i]) {
        case '.':
            break;
        case '0':
            res <<= 4;
            res += 0x0;
            break;
        case '1':
            res <<= 4;
            res += 0x1;
            break;
        case '2':
            res <<= 4;
            res += 0x2;
            break;
        case '3':
            res <<= 4;
            res += 0x3;
            break;
        case '4':
            res <<= 4;
            res += 0x4;
            break;
        case '5':
            res <<= 4;
            res += 0x5;
            break;
        case '6':
            res <<= 4;
            res += 0x6;
            break;
        case '7':
            res <<= 4;
            res += 0x7;
            break;
        case '8':
            res <<= 4;
            res += 0x8;
            break;
        case '9':
            res <<= 4;
            res += 0x9;
            break;
        case 'A':
            res <<= 4;
            res += 0xA;
            break;
        case 'B':
            res <<= 4;
            res += 0xB;
            break;
        case 'C':
            res <<= 4;
            res += 0xC;
            break;
        case 'D':
            res <<= 4;
            res += 0xD;
            break;
        case 'E':
            res <<= 4;
            res += 0xE;
            break;
        case 'F':
            res <<= 4;
            res += 0xF;
            break;
        default:
            goto Done;
        }
        i++;
    }

 Done:
    if (neg)
        res = -res;

    *out_num = res;
    
    return i;
}

/** Reads one line of file stream STARTING FROM NEXT NON-SPACE. This skips right over lines consisting only of whitespace. */
static void read_line(FILE *in_fp, char *str) {
    char ch;
    
    while (isspace(ch = (char)fgetc(in_fp)))
        ;

    if (ch == EOF)
        return;

    str[0] = ch;
    size_t i_ch = 1;
    while (true) {
        str[i_ch] = (char)fgetc(in_fp);
        if (str[i_ch] == '\n' || str[i_ch] == EOF) break;
        i_ch++;
    }

    str[i_ch + 1] = '\0';
}

epm_Result load_Mesh_dibj_1(Mesh *mesh, char *filename) {
    mesh->num_vertices = 0;
    mesh->num_edges = 0;
    mesh->num_faces = 0;
    
    FILE *in_fp;

    in_fp = fopen(filename, "rb");
    if ( ! in_fp) {
        epm_Log(LT_ERROR, "File not found: %s\n", filename);
    }
    
    DIBJFileOverview data = {0};
    file_overview(&data, in_fp);

    vbs_printf("  num_lines: %zu\n", data.num_lines);
    vbs_printf("      ofs_v: %zu\n", data.ofs_v);
    vbs_printf("      num_v: %zu\n", data.num_v);
    vbs_printf("      ofs_e: %zu\n", data.ofs_e);
    vbs_printf("      num_e: %zu\n", data.num_e);
    vbs_printf("      ofs_f: %zu\n", data.ofs_f);
    vbs_printf("      num_f: %zu\n", data.num_f);
    vbs_printf("     ofs_vn: %zu\n", data.ofs_vn);
    vbs_printf("     num_vn: %zu\n", data.num_vn);
    vbs_printf("     ofs_en: %zu\n", data.ofs_en);
    vbs_printf("     num_en: %zu\n", data.num_en);
    vbs_printf("     ofs_fn: %zu\n", data.ofs_fn);
    vbs_printf("     num_fn: %zu\n", data.num_fn);
    vbs_printf("ofs_texname: %zu\n", data.ofs_texname);
    vbs_printf("num_texname: %zu\n", data.num_texname);
    vbs_printf("     ofs_ft: %zu\n", data.ofs_ft);
    vbs_printf("     num_ft: %zu\n", data.num_ft);
    vbs_printf("   ofs_ftxl: %zu\n", data.ofs_ftxl);
    vbs_printf("   num_ftxl: %zu\n", data.num_ftxl);

    if (data.num_fn > 0 && data.num_fn != data.num_f) {
        epm_Log(LT_WARN, "Mesh file %s: number of face normal entries does not match number of faces.\n", filename);
        return EPM_FAILURE;
    }
    if (data.num_ft > 0 && data.num_ft != data.num_f) {
        epm_Log(LT_WARN, "Mesh file %s: number of face texture entries does not match number of face entries.\n", filename);
        return EPM_FAILURE;
    }
    if (data.num_ftxl > 0 && data.num_ftxl != data.num_f) {
        epm_Log(LT_WARN, "Mesh file %s: number of face texel entries does not match number of face entries.\n", filename);
        return EPM_FAILURE;
    }
    
    char line[256] = {'\0'};    

    if (data.num_v > 0) {
        mesh->num_vertices = data.num_v;
        mesh->vertices = zgl_Malloc(mesh->num_vertices * sizeof(*(mesh->vertices)));
        
        fseek(in_fp, data.ofs_v, SEEK_SET);

        size_t i_vertex = 0;
        size_t i_ch;
        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("v ", line)) break;

            i_ch = strlen("v ");
            WitPoint vertex;

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &x_of(vertex));

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &y_of(vertex));
            
            while (line[i_ch] == ' ')  i_ch++;
            i_ch += read_fix_x(line+i_ch, &z_of(vertex));

            mesh->vertices[i_vertex] = vertex;
            i_vertex++;
            
            if (feof(in_fp)) break;
        }
        
        dibassert(mesh->num_vertices == i_vertex);
    }


    
    if (data.num_f > 0) {
        mesh->num_faces = data.num_f;
        mesh->faces = zgl_Malloc(mesh->num_faces * sizeof(*(mesh->faces)));
        
        fseek(in_fp, data.ofs_f, SEEK_SET);

        size_t i_face = 0;    
        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("f ", line)) break;

            size_t i_v0, i_v1, i_v2;
        
            sscanf(line+2, " %zu %zu %zu", &i_v0, &i_v1, &i_v2);
            
            mesh->faces[i_face].i_v0 = i_v0;
            mesh->faces[i_face].i_v1 = i_v1;
            mesh->faces[i_face].i_v2 = i_v2;
            i_face++;

            if (feof(in_fp)) break;
        }

        dibassert(mesh->num_faces == i_face);
    }
    
    if (data.num_fn > 0) {
        fseek(in_fp, data.ofs_fn, SEEK_SET);
 
        size_t i_face = 0;
        size_t i_ch = 0;
        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("fn ", line)) break;

            i_ch = strlen("fn ");
            WitPoint normal;

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &x_of(normal));

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &y_of(normal));
            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &z_of(normal));
            
            mesh->faces[i_face].normal = normal;
            i_face++;

            if (feof(in_fp)) break;
        }
    }
    
    if (data.num_texname > 0) {
        fseek(in_fp, data.ofs_texname, SEEK_SET);

        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("texname ", line)) break;

            size_t i_ch = strlen("texname ");
            while (line[i_ch] == ' ') i_ch++;
            i_ch++; // skip opening "
            
            size_t i = 0;
            while ((texnames[num_texnames][i++] = line[i_ch++]) != '"')
                ;
            texnames[num_texnames][i-1] = '\0';
            num_texnames++;
            // currently file-local index; still needs to be translated to
            // global texture array index.
                                
            if (feof(in_fp)) break;   
        }
    }
    
    if (data.num_ft > 0) {
        fseek(in_fp, data.ofs_ft, SEEK_SET);
    
        size_t i_face = 0;
        while (true) {
            read_line(in_fp, line);

            if (line[0] == '#') continue;
            if ( ! prefix("ft ", line)) break;

            size_t i_tex;
            
            sscanf(line+strlen("ft "), " %zu", &i_tex);
            mesh->faces[i_face].i_tex = i_tex;
            get_texture_by_name(texnames[i_tex], &mesh->faces[i_face].i_tex);
            
            i_face++;

            if (feof(in_fp)) break;
        }
    }

    if (data.num_ftxl > 0) {
        fseek(in_fp, data.ofs_ftxl, SEEK_SET);
        
        size_t i_ch = 0;
        size_t i_face = 0;
        while (true) {
            read_line(in_fp, line);
            
            if (line[0] == '#') continue;
            if ( ! prefix("ftxl ", line)) break;
            
            i_ch = strlen("ftxl ");
            Fix32Point_2D tv;

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.x);

            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.y);
            
            mesh->faces[i_face].tv0 = tv;

            
            while (line[i_ch] == ' ') i_ch++;
            i_ch++; // skip semicolon

            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.x);
            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.y);

            mesh->faces[i_face].tv1 = tv;


            while (line[i_ch] == ' ') i_ch++;
            i_ch++; // skip semicolon

            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.x);
            
            while (line[i_ch] == ' ') i_ch++;
            i_ch += read_fix_x(line+i_ch, &tv.y);

            mesh->faces[i_face].tv2 = tv;
            
            i_face++;

            vbs_printf("ftxl  %s %s ; %s %s ; %s %s\n",
                       fmt_fix_x(mesh->faces[i_face].tv0.x, 16),
                       fmt_fix_x(mesh->faces[i_face].tv0.y, 16),
                       fmt_fix_x(mesh->faces[i_face].tv1.x, 16),
                       fmt_fix_x(mesh->faces[i_face].tv1.y, 16),
                       fmt_fix_x(mesh->faces[i_face].tv2.x, 16),
                       fmt_fix_x(mesh->faces[i_face].tv2.y, 16));
            
            if (feof(in_fp)) break;
        }
    }

    fclose(in_fp);
    
    num_texnames = 0;
#ifdef VERBOSITY
    print_mesh(mesh);
#endif

    compute_face_brightnesses(mesh->num_faces, mesh->faces);

    mesh->num_edges = count_edges_from_faces(mesh->num_faces, mesh->faces);
    mesh->edges = zgl_Malloc(mesh->num_edges * sizeof(*(mesh->edges)));
    compute_edges_from_faces(mesh->num_edges, mesh->edges,
                             mesh->num_faces, mesh->faces);


    for (size_t i_texnames = 0; i_texnames < 64; i_texnames++) {
        texnames[i_texnames][0] = '\0';
    }

    return EPM_SUCCESS;
}

void load_Mesh_dibj_0(Mesh *mesh, char *filename) {
    int i_vertex = 0;
    int i_face = 0;
    mesh->num_vertices = 0;
    mesh->num_edges = 0;
    mesh->num_faces = 0;
    
    FILE *in_fp;

    in_fp = fopen(filename, "rb");
    if (!in_fp) {
        epm_Log(LT_ERROR, "File not found: %s\n", filename);
    }

    char line[256] = {'\0'};
    int i = 0;
    
    while (true) {
        while ((line[i++] = (char)fgetc(in_fp)) != '\n') {
            if (line[i-1] == EOF) goto NextRead;
        }
        line[i] = '\0';
        i = 0;
        
        double x, y, z;
        
        if (line[0] == 'v' && line[1] == ' ') {
            sscanf(line+2, " %lf %lf %lf", &x, &y, &z);
            x_of(mesh->vertices[i_vertex]) = (fix32_t)(x * FIX_P16_ONE);
            y_of(mesh->vertices[i_vertex]) = (fix32_t)(y * FIX_P16_ONE);
            z_of(mesh->vertices[i_vertex]) = (fix32_t)(z * FIX_P16_ONE);
            mesh->num_vertices++;
            i_vertex++;
        }
    }

 NextRead:
    fseek(in_fp, 0, SEEK_SET);
    
    i = 0;
    while (true) {
        while ((line[i++] = (char)fgetc(in_fp)) != '\n') {
            if (line[i-1] == EOF) goto EndRead;
        }
        line[i] = '\0';
        i = 0;
        
        size_t i_v0, i_v1, i_v2;
        
        if (line[0] == 'f' && line[1] == ' ') {
            sscanf(line+2, " %zu %zu %zu", &i_v0, &i_v1, &i_v2);
            
            mesh->faces[i_face].i_v0 = i_v0;
            mesh->faces[i_face].i_v1 = i_v1;
            mesh->faces[i_face].i_v2 = i_v2;
            mesh->num_faces++;
            i_face++;
        }
    }

 EndRead:
    fclose(in_fp);
    postread(mesh);
}

WitPoint g_vertices[MAX_MESH_VERTICES];
Face g_faces[MAX_MESH_FACES];

void load_Mesh_obj(Mesh *mesh, char *filename) {
    int i_vertex = 0;
    int i_face = 0;
    mesh->num_vertices = 0;
    mesh->num_edges = 0;
    mesh->num_faces = 0;
    
    FILE *in_fp;

    in_fp = fopen(filename, "rb");
    if (!in_fp) {
        epm_Log(LT_ERROR, "File not found: %s\n", filename);
        return;
    }
    
    char line[256] = {'\0'};
    int i = 0;
    
    while (true) {
        while ((line[i++] = (char)fgetc(in_fp)) != '\n') {
            if (line[i-1] == EOF) goto Next;
        }
        line[i] = '\0';
        i = 0;
        
        double x, y, z;
        
        if (line[0] == 'v' && line[1] == ' ') {
            sscanf(line+2, " %lf %lf %lf", &x, &y, &z);
            // obj files use y as up/down, but EPM uses z up/down
            x_of(g_vertices[i_vertex]) = (fix32_t)(x * FIX_P16_ONE);
            y_of(g_vertices[i_vertex]) = (fix32_t)(z * FIX_P16_ONE);
            z_of(g_vertices[i_vertex]) = (fix32_t)(y * FIX_P16_ONE);
            mesh->num_vertices++;
            i_vertex++;
        }
    }

 Next:

    mesh->vertices = zgl_Malloc(mesh->num_vertices * sizeof(*(mesh->vertices)));
    memcpy(mesh->vertices, g_vertices, mesh->num_vertices * sizeof(*(mesh->vertices)));

    fseek(in_fp, 0, SEEK_SET);
    
    i = 0;
    while (true) {
        while ((line[i++] = (char)fgetc(in_fp)) != '\n') {
            if (line[i-1] == EOF) goto EndRead;
        }
        line[i] = '\0';
        i = 0;
        
        size_t i_v0, i_v1, i_v2;
        
        if (line[0] == 'f' && line[1] == ' ') {
            sscanf(line+2, " %zu %zu %zu", &i_v0, &i_v1, &i_v2);
            // obj files index vertices starting at 1
            i_v0--;
            i_v1--;
            i_v2--;
            
            g_faces[i_face].i_v0 = i_v0;
            g_faces[i_face].i_v1 = i_v1;
            g_faces[i_face].i_v2 = i_v2;
            mesh->num_faces++;
            i_face++;
        }
    }

 EndRead:
    mesh->faces = zgl_Malloc(mesh->num_faces * sizeof(*(mesh->faces)));
    memcpy(mesh->faces, g_faces, mesh->num_faces * sizeof(*(mesh->faces)));
    
    fclose(in_fp);
    postread(mesh);
}


void destroy_mesh(void);

void print_mesh(Mesh *mesh) {
    for (size_t i_v = 0; i_v < mesh->num_vertices; i_v++) {
        printf("v  %s  %s  %s\n",
               fmt_fix_x(x_of(mesh->vertices[i_v]), 16),
               fmt_fix_x(y_of(mesh->vertices[i_v]), 16),
               fmt_fix_x(z_of(mesh->vertices[i_v]), 16));
    }
    putchar('\n');

    for (size_t i_f = 0; i_f < mesh->num_faces; i_f++) {
        printf("f  %zu  %zu  %zu\n",
               mesh->faces[i_f].i_v0,
               mesh->faces[i_f].i_v1,
               mesh->faces[i_f].i_v2);
    }
    putchar('\n');

    for (size_t i_f = 0; i_f < mesh->num_faces; i_f++) {
        printf("fn  %s  %s  %s\n",
               fmt_fix_x(x_of(mesh->faces[i_f].normal), 16),
               fmt_fix_x(y_of(mesh->faces[i_f].normal), 16),
               fmt_fix_x(z_of(mesh->faces[i_f].normal), 16));
    }
    putchar('\n');

    for (size_t i_f = 0; i_f < mesh->num_faces; i_f++) {
        printf("ft  %zu : %s\n", mesh->faces[i_f].i_tex,
               textures[mesh->faces[i_f].i_tex].name);
    }
    putchar('\n');

    for (size_t i_f = 0; i_f < mesh->num_faces; i_f++) {
        printf("ftxl  %s %s ; %s %s ; %s %s\n",
               fmt_fix_x(mesh->faces[i_f].tv0.x, 16),
               fmt_fix_x(mesh->faces[i_f].tv0.y, 16),
               fmt_fix_x(mesh->faces[i_f].tv1.x, 16),
               fmt_fix_x(mesh->faces[i_f].tv1.y, 16),
               fmt_fix_x(mesh->faces[i_f].tv2.x, 16),
               fmt_fix_x(mesh->faces[i_f].tv2.y, 16));
    }
    putchar('\n');
}
