#ifndef HEADER_X11GLOBE
#define HEADER_X11GLOBE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "../../include/cloudAPI.h"

#define GLOBE_PI 3.14159265358979323846
#define DEG2RAD (3.14159265358979323846f/180.0f)
#define RADIUS 1.0f

#define TEXTURE_512 0
#define TEXTURE_1024 1

#ifdef IRIX5
#define THIS_CLIENT_VERSION "V20261026-1000-XG5" /* max 23 chars */
#define THIS_CLIENT_TYPE "XG5" /* must be 3 chars */
#else
#define THIS_CLIENT_VERSION "V20261026-1000-XG6" /* max 23 chars */
#define THIS_CLIENT_TYPE "XG6" /* must be 3 chars */
#endif
#define THIS_CLIENT_BIN "/usr/bin/xsginet-globe" /* for online updater, --sginet-bin overrides */

typedef struct {
    float lat;
    float lon;
} LatLon;

extern MarkerList* lstMarkers;
extern char G_CLIENTID[64];
extern int debug;

/* markers.c */
void initMarkerList(void);
MarkerEntry* findMyMarkerEntry(void);
MarkerEntry* addMarker(const char *label, int mType, float lat, float lon, int online);

/* mapshapes.c */
void drawWireframeMap(void);

/* textures.c */
const unsigned char* getWorldmapTexture(int textureRes, int *width, int *height);

#endif /* HEADER_X11GLOBE */
