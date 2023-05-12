#ifndef TEXTURES_H
#define TEXTURES_H

#include "zigil/zigil.h"
#include "src/misc/epm_includes.h"

#define MAX_TEXTURE_NAME_LEN 63

// as of 2023-04-01 the epm_Texture struct is just a wrapper around a
// zgl_PixelArray; the only extra data is a NAME field.
typedef struct epm_Texture {
    char name[MAX_TEXTURE_NAME_LEN + 1];
    zgl_PixelArray *pixarr;
    uint16_t w, h; // TODO: Check and/or assert in various critical places in
                   // the code that no texture has width larger than some chosen
                   // constant, say 256, 512, or 1024
} epm_Texture;

#define MAX_TEXTURES 64
extern size_t g_num_textures;
extern epm_Texture textures[MAX_TEXTURES];

/** Load an epm_Texture into the global textures[] array. */
extern epm_Texture *load_Texture(char const *filename);
extern epm_Result unload_Texture(epm_Texture *tex);

/** Given the name of a texture, fill in the out_i_tex variable with the index
    to it in the textures[] array. If the texture is not in the array, return
    EPM_FAILURE */
extern epm_Result texture_index_from_name(char const *name, size_t *out_i_tex);

/** Given the name of a texture, fill in the out_i_tex variable with the index
    to it in the textures[] array. If the texture is not in the array, attempt
    to load it and then return the index. If the texture can not be loaded,
    return EPM_FAILURE */
extern epm_Result get_texture_by_name(char const *name, size_t *out_i_tex);

extern void scale_texels_to_world(Fix32Point V0, Fix32Point V1, Fix32Point V2, Fix32Point_2D *TV0, Fix32Point_2D *TV1, Fix32Point_2D *TV2, epm_Texture *tex);

extern void scale_quad_texels_to_world(Fix32Point V0, Fix32Point V1, Fix32Point V2, Fix32Point V3, Fix32Point_2D *TV0, Fix32Point_2D *TV1, Fix32Point_2D *TV2, Fix32Point_2D *TV3, epm_Texture *tex);
    
#endif /* TEXTURES_H */
