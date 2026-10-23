#include "include/globe.h"
#include "include/gl_utility.h"
#include <GL/glx.h>

#define CIRCLE_SEGMENTS 16

GLdouble g_modelview[16];
GLdouble g_projection[16];
GLint g_viewport[4];

static XFontStruct *osdFont = NULL;
static GLuint fontListBase = 0;

/* Reading matrices back stalls the pipeline, so do it once per frame instead of once per marker */
void captureMatrices(void) {
    glGetDoublev(GL_MODELVIEW_MATRIX, g_modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, g_projection);
    glGetIntegerv(GL_VIEWPORT, g_viewport);
}
int getScreenCoords(double x, double y, double z, double *screenX, double *screenY, double *screenZ) {
    /* Project the 3D point into screen coordinates */
    if (!gluProject(x, y, z, g_modelview, g_projection, g_viewport, screenX, screenY, screenZ)) return 0;

    /* z in eye coordinates, compared with the globe center to hide markers on the back side */
    *screenZ = g_modelview[2]*x + g_modelview[6]*y + g_modelview[10]*z + g_modelview[14];
    return 1;
}
void drawFilledCircle(float cx, float cy, float radius) {
    static float unitCircle[CIRCLE_SEGMENTS + 1][2];
    static int initialized = 0;
    int i;
    if (!initialized) {
        for (i = 0; i <= CIRCLE_SEGMENTS; i++) {
            double theta = 2.0 * GLOBE_PI * i / CIRCLE_SEGMENTS;
            unitCircle[i][0] = (float)cos(theta);
            unitCircle[i][1] = (float)sin(theta);
        }
        initialized = 1;
    }
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy); /* center of circle */
    for (i = 0; i <= CIRCLE_SEGMENTS; i++) {
        glVertex2f(cx + radius * unitCircle[i][0], cy + radius * unitCircle[i][1]);
    }
    glEnd();
}
void draw_rectangle(float x, float y, float w, float h, float r, float g, float b, float alpha) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, alpha);
    glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_BLEND);
}
int initFont(Display *dpy, const char *fontName) {
    /* Needs a current GL context. Falls back to the "fixed" font when fontName is not available. */
    unsigned int first, last;
    osdFont = XLoadQueryFont(dpy, fontName);
    if (osdFont == NULL) {
        fprintf(stderr, "Font %s not found, using fixed\n", fontName);
        osdFont = XLoadQueryFont(dpy, "fixed");
        if (osdFont == NULL) return 0;
    }
    first = osdFont->min_char_or_byte2;
    last = osdFont->max_char_or_byte2;
    if (last > 255) last = 255;
    fontListBase = glGenLists(256);
    if (fontListBase == 0) return 0;
    glXUseXFont(osdFont->fid, first, last - first + 1, fontListBase + first);
    return 1;
}
void drawString(const char *str, int x, int y) {
    if (fontListBase == 0) return;
    /* Note: color must be set before rasterPos, and texture must be turned off */
    glDisable(GL_TEXTURE_2D);
    glRasterPos2i(x, y);
    glListBase(fontListBase);
    glCallLists((GLsizei)strlen(str), GL_UNSIGNED_BYTE, (const GLvoid *)str);
}
int getStringWidth(const char *str) {
    if (osdFont == NULL) return 0;
    return XTextWidth(osdFont, str, (int)strlen(str));
}
