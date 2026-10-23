#ifndef HEADER_X11GLOBE_GLUTILITY
#define HEADER_X11GLOBE_GLUTILITY

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glu.h>

/* matrices of the last captureMatrices() call, used for projection of markers and picking */
extern GLdouble g_modelview[16];
extern GLdouble g_projection[16];
extern GLint g_viewport[4];

void captureMatrices(void);
int getScreenCoords(double x, double y, double z, double *screenX, double *screenY, double *screenZ);
void drawFilledCircle(float cx, float cy, float radius);
void draw_rectangle(float x, float y, float w, float h, float r, float g, float b, float alpha);

int initFont(Display *dpy, const char *fontName);
void drawString(const char *str, int x, int y);
int getStringWidth(const char *str);

#endif /* HEADER_X11GLOBE_GLUTILITY */
