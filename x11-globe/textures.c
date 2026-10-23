/* Compiled-in world map textures (GIMP RGB C-Source dumps) */
#include "include/globe.h"
#include "res/worldmap_512.c"
#include "res/worldmap_1024.c"

const unsigned char* getWorldmapTexture(int textureRes, int *width, int *height) {
    if (textureRes == TEXTURE_1024) {
        *width = worldmap_1024.width;
        *height = worldmap_1024.height;
        return worldmap_1024.pixel_data;
    }
    *width = worldmap_512.width;
    *height = worldmap_512.height;
    return worldmap_512.pixel_data;
}
