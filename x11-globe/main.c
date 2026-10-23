#include "include/globe.h"
#include "include/gl_utility.h"
#include "include/notifications.h"
#include "../include/hwinfo_sgi.h"
#include <Xm/Xm.h>
#include <Xm/Protocols.h>
#include <X11/cursorfont.h>
#include <GL/glx.h>
#include <GL/GLwMDrawA.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

#define FPS 25
#define OSD_FONT "-*-helvetica-medium-r-normal--12-*-*-*-p-*-iso8859-1"
#define ALLOW_ONLINE_UPDATE 1
#define STAR_COUNT 1000
/* Stars near the eye crash the Newport (Indy) software point renderer on IRIX 5 */
#define STAR_RADIUS (RADIUS * 20)
#define MARKER_REFRESH_FIRST 180000 /* ms */
#define MARKER_REFRESH_INTERVAL 300000 /* ms */
#define BOTTOM_BAR_HEIGHT 22
#define TEX_CLAMP_TO_EDGE 0x812F /* GL_CLAMP_TO_EDGE (OpenGL 1.2) = GL_CLAMP_TO_EDGE_SGIS */

int debug = 0;
int G_MODE = MODE_DAEMON;
char G_CLIENTID[64];
MarkerList* lstMarkers = NULL;

static XtAppContext app;
static Widget glw = NULL;
static XVisualInfo *visinfo = NULL;
static GLXContext glxContext = NULL;
static GLUquadricObj *quadric = NULL;
static GLuint listGlobe = 0, listGrid = 0, listWireMap = 0, listStars = 0;
static Cursor busyCursor = None;
static long startSec = 0;

static GLdouble g_globeZ; /* eye z of globe center, used for visibility check of markers */
static int winWidth = 800, winHeight = 600;
static int lastMouseX, lastMouseY;
static int dragging = 0;
static int g_wireframe = 1; /* start in wireframe mode, -t or the T/W button for textures */
static int textureLoaded = 0; /* the texture is loaded when textured mode is used the first time */
static int g_redrawUI = 1;
static float rotX = 160.0f, rotY = 50.0f, zoom = 12.0f;
static float rotYdiff = 0.0f, rotXdiff = 0.1f;
static float minZoom = 5.0f, maxZoom = 1.2f, defaultZoom = 2.2f;
static int g_setMyPosition = 0; /* 1=set mode, 2=confirm position set overlay */
static float g_selectedLat, g_selectedLon;
static int isRegistered = 0;
static char username[128];
static int g_showUsers = 1;
static int g_showCities = 1;
static long prevTime = 0; /* used for notifications expiry */
static int textureRes = TEXTURE_1024;
static int startupSequence = 1; /* show startup animation (zoom to globe) */
static float startupSequenceEasingFactor = 0.05f;
static int showStars = 1;
static int useLinearTexture = 0;
static int cliHasLat = 0, cliHasLng = 0;
static float cliLat = 0.0f, cliLng = 0.0f;

static const float COLOR_ONLINE[3] = {0.0f, 1.0f, 0.0f}; /* Green */
static const float COLOR_OFFLINE[3] = {1.0f, 1.0f, 0.0f}; /* Yellow */
static const float COLOR_CITY_LABEL[3] = {1.0f, 1.0f, 1.0f}; /* White */
static const float COLOR_RED[3] = {1.0f, 0.0f, 0.0f}; /* Red */

static int visualAttribs[] = {
    GLX_RGBA, GLX_DOUBLEBUFFER,
    GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1,
    GLX_DEPTH_SIZE, 1,
    None
};

static void initTexture(void);

static String fallback_resources[] = {
    "*title: SGINet Globe",
    NULL
};

/* milliseconds since start */
static long elapsedMs(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (startSec == 0) startSec = tv.tv_sec;
    return (tv.tv_sec - startSec) * 1000L + tv.tv_usec / 1000L;
}

static void busy_begin(void) {
    if (glw == NULL || !XtIsRealized(glw)) return;
    if (busyCursor == None) busyCursor = XCreateFontCursor(XtDisplay(glw), XC_watch);
    XDefineCursor(XtDisplay(glw), XtWindow(glw), busyCursor);
    XFlush(XtDisplay(glw));
}
static void busy_end(void) {
    if (glw == NULL || !XtIsRealized(glw)) return;
    XUndefineCursor(XtDisplay(glw), XtWindow(glw));
    XFlush(XtDisplay(glw));
}

/* --- static geometry, compiled into display lists once --- */
static void drawStars(void) {
    int i;
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (i = 0; i < STAR_COUNT; i++) {
        double latRad = DEG2RAD * (((float)rand() / RAND_MAX) * 180.0f - 90.0f);
        double lonRad = DEG2RAD * (((float)rand() / RAND_MAX) * 360.0f - 180.0f);
        float c = ((float)rand() / RAND_MAX) * (1.0f - 0.2f) + 0.2f;
        glColor3f(c, c, c);
        glVertex3f((float)(STAR_RADIUS * cos(latRad) * cos(lonRad)),
                   (float)(STAR_RADIUS * sin(latRad)),
                   (float)(STAR_RADIUS * cos(latRad) * sin(lonRad)));
    }
    glEnd();
}
static void drawLatLongGrid(void) {
    int lat, lon;
    double latRad, lonRad;
    for (lon = -180; lon <= 180; lon += 15) {
        glBegin(GL_LINE_STRIP);
        for (lat = -90; lat <= 90; lat += 2) {
            latRad = lat * DEG2RAD;
            lonRad = lon * DEG2RAD;
            glVertex3f((float)(RADIUS * cos(latRad) * cos(lonRad)),
                       (float)(RADIUS * sin(latRad)),
                       (float)(RADIUS * cos(latRad) * sin(lonRad)));
        }
        glEnd();
    }
    for (lat = -75; lat <= 75; lat += 15) {
        latRad = lat * DEG2RAD;
        glBegin(GL_LINE_LOOP);
        for (lon = -180; lon <= 180; lon += 2) {
            lonRad = lon * DEG2RAD;
            glVertex3f((float)(RADIUS * cos(latRad) * cos(lonRad)),
                       (float)(RADIUS * sin(latRad)),
                       (float)(RADIUS * cos(latRad) * sin(lonRad)));
        }
        glEnd();
    }
}
static void drawGlobeTextured(int latBands, int lonBands) {
    int lat, lon;
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    for (lat = 0; lat < latBands; ++lat) {
        float lat0 = -90.0f + 180.0f * lat / latBands;
        float lat1 = -90.0f + 180.0f * (lat + 1) / latBands;
        double lat0Rad = DEG2RAD * lat0;
        double lat1Rad = DEG2RAD * lat1;
        float v0 = (lat0 + 90.0f) / 180.0f;
        float v1 = (lat1 + 90.0f) / 180.0f;
        glBegin(GL_QUAD_STRIP);
        for (lon = 0; lon <= lonBands; ++lon) {
            float lonAngle = -180.0f + 360.0f * lon / lonBands;
            double lonRad = DEG2RAD * lonAngle;
            float u = (lonAngle + 180.0f) / 360.0f;
            glTexCoord2f(u, v0);
            glVertex3f((float)(RADIUS * cos(lat0Rad) * cos(lonRad)),
                       (float)(RADIUS * sin(lat0Rad)),
                       (float)(RADIUS * cos(lat0Rad) * sin(lonRad)));
            glTexCoord2f(u, v1);
            glVertex3f((float)(RADIUS * cos(lat1Rad) * cos(lonRad)),
                       (float)(RADIUS * sin(lat1Rad)),
                       (float)(RADIUS * cos(lat1Rad) * sin(lonRad)));
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
}

static void drawMarker(MarkerEntry *mEntry) {
    float cylinderHeight = 0.12f;
    double latRad, lonRad;
    float x, y, z, nx, ny, nz;
    float axisX, axisY, axisZ, angle, circRadius;

    if (mEntry->mType == MTYPE_CITY) {
        mEntry->visible = 0;
        if  (zoom > 2.4f && mEntry->score < 150) return;
        if  (zoom > 2.0f && mEntry->score < 100) return;
        if  (zoom > 1.7f && mEntry->score < 80) return;
        if  (zoom > 1.5f && mEntry->score < 50) return;
        if  (zoom > 1.4f && mEntry->score < 20) return;
    }
    latRad = mEntry->lat * DEG2RAD;
    lonRad = -mEntry->lon * DEG2RAD; /* whoops, longitude on sphere is inverted :-/ */

    /* Surface normal vector (same as position since sphere radius = 1) */
    nx = (float)(cos(latRad) * cos(lonRad));
    ny = (float)sin(latRad);
    nz = (float)(cos(latRad) * sin(lonRad));

    /* Compute position on the sphere surface */
    x = RADIUS * nx;
    y = RADIUS * ny;
    z = RADIUS * nz;

    /* Rotation axis & angle to rotate from +Z to normal */
    axisX = -ny;
    axisY = nx;
    axisZ = 0.0f;
    angle = (float)(acos(nz) * 180.0 / GLOBE_PI);

    /* Project the 3D point to 2D screen coordinates for text label (rendered in second step on 2D) */
    if (mEntry->mType == MTYPE_USER || mEntry->mType == MTYPE_THISUSER) {
        /* user labels are drawn on the tip of the cylinder */
        float angleRad, alength, ax, ay, az, v_z, cosA, sinA, dot_v_a;
        float rotatedX, rotatedY, rotatedZ;
        cylinderHeight = (mEntry->online || mEntry->mType == MTYPE_THISUSER ? 0.13f : 0.10f);
        /* Rotate the (0,0,cylinderHeight) vector by the axis-angle */
        angleRad = (float)(angle * GLOBE_PI / 180.0);

        /* Normalize axis */
        alength = (float)sqrt(axisX*axisX + axisY*axisY + axisZ*axisZ);
        ax = 0.0f; ay = 0.0f; az = 1.0f; /* default (0,0,1) if no rotation */
        if (alength > 0.0001f) {
            ax = axisX / alength;
            ay = axisY / alength;
            az = axisZ / alength;
        }
        v_z = cylinderHeight; /* v_x = v_y = 0 */

        cosA = (float)cos(angleRad);
        sinA = (float)sin(angleRad);
        dot_v_a = v_z*az;

        rotatedX = (ay*v_z)*sinA + ax*dot_v_a*(1 - cosA);
        rotatedY = (-ax*v_z)*sinA + ay*dot_v_a*(1 - cosA);
        rotatedZ = v_z*cosA + az*dot_v_a*(1 - cosA);

        /* top position of cylinder in 3D space */
        getScreenCoords(x + rotatedX, y + rotatedY, z + rotatedZ, &mEntry->sX, &mEntry->sY, &mEntry->sZ);
    } else {
        getScreenCoords(x, y, z, &mEntry->sX, &mEntry->sY, &mEntry->sZ);
    }

    glPushMatrix();
    glTranslatef(x, y, z);
    if (angle > 0.001f && fabs(angle - 180.0f) > 0.001f) {
        glRotatef(angle, axisX, axisY, axisZ);
    } else if (fabs(angle - 180.0f) <= 0.001f) {
        /* 180 degrees rotation case - rotate around X or Y axis */
        glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    }

    circRadius = 0.004f + (zoom - 1.5f) * 0.004f; /* linear scale depending on zoom */
    if (mEntry->mType == MTYPE_USER) {
        if (mEntry->online == 1) glColor3fv(COLOR_ONLINE); else glColor3fv(COLOR_RED);
        /* highlight user with cylinder */
        gluCylinder(quadric, 0.002f, 0.002f, cylinderHeight, 4, 2);
    } else if (mEntry->mType == MTYPE_CITY) {
        if (g_wireframe) glColor3f(1.0f, 1.0f, 1.0f); else glColor3f(0.33f, 0.41f, 0.5f); /* gray */
        drawFilledCircle(0.005f, 0.005f, circRadius - 0.002f);
    } else if (mEntry->mType == MTYPE_THISUSER) {
        glColor3f(1.0f, 1.0f, 0.0f);
        gluCylinder(quadric, 0.003f, 0.003f, cylinderHeight, 4, 2);
    }

    mEntry->visible = 1;
    glPopMatrix();
}

/* --- 2D overlays --- */
static void setOrtho2D(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, winWidth, 0, winHeight);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
static void draw_marker_labels(void) {
    int i;
    int stringWidth;
    MarkerEntry *m;
    if (lstMarkers->cnt < 1) return;
    setOrtho2D();

    for (i = lstMarkers->cnt - 1; i >= 0; i--) {
        m = &lstMarkers->lst[i];
        /* only draw when on visible side of globe, and above bottom bar */
        if (m->visible != 1 || m->sZ <= g_globeZ || m->sY <= BOTTOM_BAR_HEIGHT + 3) continue;
        if (m->mType == MTYPE_THISUSER) {
            glColor3f(0.0f, 1.0f, 0.0f);
            drawString(m->label, (int)m->sX + 10, (int)m->sY);
        } else if (g_showUsers && m->mType == MTYPE_USER) {
            stringWidth = getStringWidth(m->label);
            draw_rectangle((float)m->sX + 5, (float)m->sY - 6, stringWidth + 10, 20.0f, 0.0f, 0.0f, 0.0f, 0.3f);
            if (m->online) glColor3fv(COLOR_ONLINE); else glColor3fv(COLOR_OFFLINE);
            drawString(m->label, (int)m->sX + 10, (int)m->sY);
        } else if (g_showCities && m->mType == MTYPE_CITY) {
            glColor3fv(COLOR_CITY_LABEL);
            drawString(m->label, (int)m->sX + 10, (int)m->sY);
        }
    }
}
static void draw_bottom_bar(void) {
    setOrtho2D();

    /* main bottom bar background */
    draw_rectangle(0, 0, winWidth, BOTTOM_BAR_HEIGHT, 0.14f, 0.50f, 0.70f, 0.8f);

    glColor3f(0.0f, 0.0f, 0.0f);
    drawString("Zoom:", 12, 7);

    /* zoom control buttons */
    draw_rectangle(50, 4, 15, 14, 0.0f, 0.0f, 0.0f, 1.0f); /* zoom in */
    draw_rectangle(68, 4, 15, 14, 0.0f, 0.0f, 0.0f, 1.0f); /* zoom out */

    /* "set your location" button */
    if (g_setMyPosition == 1) {
        draw_rectangle(100, 4, 103, 14, 0.6f, 0.0f, 0.0f, 1.0f);
    } else {
        draw_rectangle(100, 4, 103, 14, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    glColor3f(0.0f, 0.0f, 0.0f);
    drawString("Show:", 215, 7);
    draw_rectangle(256, 4, 40, 14, 0.0f, 0.0f, 0.0f, 1.0f); /* Cities */
    draw_rectangle(301, 4, 40, 14, 0.0f, 0.0f, 0.0f, 1.0f); /* Users */
    draw_rectangle(346, 4, 19, 14, 0.0f, 0.0f, 0.0f, 1.0f); /* Wireframe/Texture */

    /* -- now add labels on top of buttons (depth test is off, drawing order decides) -- */
    glColor3f(1.0f, 1.0f, 1.0f);
    drawString("+", 54, 8);
    drawString("-", 72, 8);

    if (isRegistered == 1) {
        drawString("Set your location", 104, 7);
        drawString(username, 380, 7);
    } else {
        drawString("Unregistered", 104, 7);
    }

    if (g_showCities == 1) glColor3f(0.0f, 1.0f, 0.0f); else glColor3f(0.7f, 0.7f, 0.7f);
    drawString("Cities", 260, 7);

    if (g_showUsers == 1) glColor3f(0.0f, 1.0f, 0.0f); else glColor3f(0.7f, 0.7f, 0.7f);
    drawString("Users", 305, 7);

    glColor3f(1.0f, 1.0f, 1.0f);
    if (g_wireframe == 1) {
        drawString("W", 350, 7);
    } else {
        drawString("T", 352, 7);
    }
}
static void drawSetMyPositionConfirmationOverlay(void) {
    /* Draw semi-transparent box */
    draw_rectangle(30, 55, 250, 80, 0.2f, 0.2f, 0.2f, 0.8f);
    /* YES button */
    draw_rectangle(40, 70, 65, 20, 0.0f, 0.7f, 0.0f, 0.9f);
    /* CANCEL button */
    draw_rectangle(120, 70, 65, 20, 0.4f, 0.4f, 0.4f, 0.9f);

    glColor3f(1.0f, 1.0f, 1.0f);
    drawString("Save this as your location?", 40, 110);
    drawString("Save", 45, 75);
    drawString("Cancel", 125, 75);
}
static void draw_osd(void) {
    glDisable(GL_DEPTH_TEST); /* Disable depth test for overlay */
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    draw_bottom_bar();
    if (g_setMyPosition == 2) drawSetMyPositionConfirmationOverlay();
    draw_marker_labels();
    drawNotifications(winWidth, winHeight);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST); /* enable depth test again for 3D sphere etc */
}

static void display(void) {
    int i;
    GLdouble screenX, screenY;
    MarkerEntry *m;
    if (glxContext == NULL) return;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Position camera onto sphere */
    gluLookAt(0.0, 0.0, zoom,  /* eye position */
              0.0, 0.0, 0.0,   /* center of scene */
              0.0, 1.0, 0.0);  /* up */

    glRotatef(rotY, 1, 1, 0);
    glRotatef(rotX, 0, 1, 0);
    captureMatrices();

    /* eye z of the globe center */
    getScreenCoords(0.0, 0.0, 0.0, &screenX, &screenY, &g_globeZ);
    g_globeZ += 0.5; /* add some more */

    if (showStars) {
        /* background only, drawn first; avoids the depth-tested point path of Newport */
        glDisable(GL_DEPTH_TEST);
        glCallList(listStars);
        glEnable(GL_DEPTH_TEST);
    }

    if (g_wireframe != 1) {
        if (!textureLoaded) {
            initTexture();
            textureLoaded = 1;
        }
        glCallList(listGlobe);
    } else {
        glCallList(listWireMap);
    }

    if (g_wireframe) glColor3f(0.48f, 0.48f, 0.68f); else glColor3f(0.68f, 0.68f, 0.68f);
    glLineWidth(1.0f);
    glCallList(listGrid);

    if (startupSequence == 0) {
        for (i = 0; i < lstMarkers->cnt; i++) {
            m = &lstMarkers->lst[i];
            if (m->mType == MTYPE_THISUSER
                || (g_showCities && m->mType == MTYPE_CITY)
                || (g_showUsers && m->mType == MTYPE_USER)) {
                drawMarker(m);
            }
        }
    }

    if (startupSequence == 1) {
        if (zoom - 0.1f > defaultZoom) {
            zoom += (defaultZoom - zoom) * startupSequenceEasingFactor;
        } else {
            startupSequence = 0;
        }
    } else {
        draw_osd(); /* Draw 2D overlays */
    }
    GLwDrawingAreaSwapBuffers(glw);
}

static void tick(XtPointer client_data, XtIntervalId *id) {
    long currTime, delay;
    float deltaTime;
    int hasNotificationChanges;
    if (dragging != 1) {
        rotY += rotYdiff;
        rotX += rotXdiff;
        if (rotX > 360.0f) rotX -= 360.0f;
        else if (rotX < 0.0f) rotX += 360.0f;
        if (rotY > 360.0f) rotY -= 360.0f;
        else if (rotY < 0.0f) rotY += 360.0f;
    }

    currTime = elapsedMs();
    deltaTime = (currTime - prevTime) / 1000.0f;
    prevTime = currTime;

    hasNotificationChanges = updateNotifications(deltaTime);

    if (hasNotificationChanges || startupSequence || rotYdiff != 0.0f || rotXdiff != 0.0f) {
        g_redrawUI = 1;
    }
    if (g_redrawUI) {
        g_redrawUI = 0;
        display();
    }
    /* FPS limitation, rendering time included */
    delay = 1000 / FPS - (elapsedMs() - currTime);
    if (delay < 1) delay = 1;
    XtAppAddTimeOut(app, (unsigned long)delay, tick, NULL);
}
static void reshape(int w, int h) {
    if (h == 0) h = 1;
    winWidth = w;
    winHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* --- mouse handling --- */
static void mouseMotion(int x, int y) {
    if (!dragging) return;
    /* rot diff (global) used to retain rotation when releasing mouse button */
    rotXdiff = (x - lastMouseX) * 0.5f;
    rotYdiff = (y - lastMouseY) * 0.5f;

    /* limit rotation speed (vertical) */
    if (rotYdiff > 1.0f) rotYdiff = 1.0f;
    else if (rotYdiff < -1.0f) rotYdiff = -1.0f;

    /* limit rotation speed (horizontal) */
    if (rotXdiff > 1.5f) rotXdiff = 1.5f;
    else if (rotXdiff < -1.5f) rotXdiff = -1.5f;

    rotY += rotYdiff;
    rotX += rotXdiff;

    if (rotX > 360.0f) rotX -= 360.0f;
    else if (rotX < 0.0f) rotX += 360.0f;

    if (rotY > 360.0f) rotY -= 360.0f;
    else if (rotY < 0.0f) rotY += 360.0f;

    lastMouseX = x;
    lastMouseY = y;
    g_redrawUI = 1;
}
static void zoomView(int zoomIn) {
    if (zoomIn == 1) zoom -= 0.1f; else zoom += 0.1f;
    if (zoom < maxZoom) zoom = maxZoom;
    if (zoom > minZoom) zoom = minZoom;
    if (debug) printf("Zoom: %f\n", zoom);
}
static void onBottomBarClick(int x) {
    if (x >= 50 && x <= 64) {
        zoomView(1);
    } else if (x >= 68 && x <= 82) {
        zoomView(0);
    } else if (x >= 100 && x <= 200) {
        if (isRegistered == 0) {
            addNotification("To submit your location, this machine must be registered on sginet first", 5.0f);
        } else {
            g_setMyPosition = g_setMyPosition == 1 ? 0 : 1;
        }
    } else if (x >= 256 && x <= 296) {
        g_showCities = g_showCities == 1 ? 0 : 1;
    } else if (x >= 301 && x <= 341) {
        g_showUsers = g_showUsers == 1 ? 0 : 1;
    } else if (x >= 346 && x <= 365) {
        g_wireframe = g_wireframe == 1 ? 0 : 1;
    }
    g_redrawUI = 1;
}
/* Pick the clicked point on the globe, using the matrices of the frame the user saw */
static void setMyPosition(int x, int y) {
    double winX, winY;
    double posNear[3], posFar[3], rayDir[3];
    double len, b, c, discriminant, t, ix, iy, iz;
    float lat, lon;
    MarkerEntry* myMarker;

    /* Flip y coordinate for OpenGL coordinate system */
    winX = (double)x;
    winY = (double)(g_viewport[3] - y);

    gluUnProject(winX, winY, 0.0, g_modelview, g_projection, g_viewport, &posNear[0], &posNear[1], &posNear[2]);
    gluUnProject(winX, winY, 1.0, g_modelview, g_projection, g_viewport, &posFar[0], &posFar[1], &posFar[2]);

    rayDir[0] = posFar[0] - posNear[0];
    rayDir[1] = posFar[1] - posNear[1];
    rayDir[2] = posFar[2] - posNear[2];

    len = sqrt(rayDir[0]*rayDir[0] + rayDir[1]*rayDir[1] + rayDir[2]*rayDir[2]);
    rayDir[0] /= len;
    rayDir[1] /= len;
    rayDir[2] /= len;

    /* Ray-sphere intersection (sphere radius = RADIUS at origin), a = 1 for the normalized ray */
    b = 2.0 * (posNear[0]*rayDir[0] + posNear[1]*rayDir[1] + posNear[2]*rayDir[2]);
    c = posNear[0]*posNear[0] + posNear[1]*posNear[1] + posNear[2]*posNear[2] - RADIUS*RADIUS;

    discriminant = b*b - 4*c;
    if (discriminant < 0) return; /* No intersection */

    t = (-b - sqrt(discriminant)) / 2.0;
    ix = posNear[0] + t*rayDir[0];
    iy = posNear[1] + t*rayDir[1];
    iz = posNear[2] + t*rayDir[2];

    /* Convert sphere coordinates to lat/lon in degrees */
    lat = (float)(asin(iy / RADIUS) / DEG2RAD);
    lon = (float)(-atan2(iz, ix) / DEG2RAD); /* whoops, longitude is inverted :-/ */

    g_selectedLat = lat;
    g_selectedLon = lon;
    if (debug) printf("Clicked: Latitude = %.6f, Longitude = %.6f\n", lat, lon);
    myMarker = findMyMarkerEntry();
    if (myMarker) {
        myMarker->lat = lat;
        myMarker->lon = lon;
    } else {
        addMarker("You", MTYPE_THISUSER, lat, lon, 1);
    }
    g_setMyPosition = 2; /* show confirmation dialog */
}
static void mouseButton(unsigned int button, int pressed, int x, int y) {
    int yi = winHeight - y; /* inverted y, from bottom of window */
    if (button == Button1 && y > (winHeight - BOTTOM_BAR_HEIGHT)) {
        /* left click on bottom bar */
        if (!pressed) onBottomBarClick(x);
        return;
    }

    if (button == Button1 && pressed && g_setMyPosition == 2) { /* save position confirmation prompt visible */
        if (yi > 70 && yi < 90) {
            if (x > 40 && x < 105) {
                char status_msg[512];
                g_setMyPosition = 0; /* hide dialog */
                status_msg[0] = '\0';
                busy_begin();
                cloudAPI_saveMyPosition(status_msg, g_selectedLat, g_selectedLon);
                busy_end();
                addNotification(status_msg, 3.0f);
            } else if (x > 120 && x < 185) {
                g_setMyPosition = 0;
            }
        }
    }

    g_redrawUI = 1;
    if (button != Button1 || !pressed) {
        dragging = 0;
        return;
    }

    dragging = 1;
    rotXdiff = 0.0f;
    rotYdiff = 0.0f;
    lastMouseX = x;
    lastMouseY = y;

    if (g_setMyPosition == 1) setMyPosition(x, y);
}

/* --- GLwMDrawingArea callbacks --- */
static int hasEdgeClamp(void) {
    const char *version = (const char *)glGetString(GL_VERSION);
    const char *ext = (const char *)glGetString(GL_EXTENSIONS);
    int major = 1, minor = 0;
    if (version != NULL && sscanf(version, "%d.%d", &major, &minor) == 2) {
        if (major > 1 || (major == 1 && minor >= 2)) return 1;
    }
    return ext != NULL && strstr(ext, "GL_SGIS_texture_edge_clamp") != NULL;
}
/*
 * Only one texture is used, so it goes to the default texture without texture objects
 * (OpenGL 1.0 on IRIX 5.3 has no glGenTextures/glBindTexture)
 */
static void initTexture(void) {
    const unsigned char *pixels;
    int width, height;
    GLint maxSize;
    GLenum wrap;

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (textureRes == TEXTURE_1024 && maxSize < 1024) {
        printf("Max. texture size is %d, using low resolution textures (512)\n", (int)maxSize);
        textureRes = TEXTURE_512;
    }

    if (useLinearTexture) { /* GL_LINEAR = blurred */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    } else { /* use crisp textures */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); /* crisp when zoomed in */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); /* blurred when zoomed out */
    }
    wrap = hasEdgeClamp() ? TEX_CLAMP_TO_EDGE : GL_CLAMP;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    while (glGetError() != GL_NO_ERROR) ; /* clear old errors */
    pixels = getWorldmapTexture(textureRes, &width, &height);
    /* internal format 3 instead of GL_RGB, OpenGL 1.0 accepts only 1-4 */
    glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    if (glGetError() != GL_NO_ERROR && textureRes == TEXTURE_1024) {
        /* e.g. not enough texture memory */
        printf("Unable to load 1024 texture, using low resolution textures (512)\n");
        textureRes = TEXTURE_512;
        pixels = getWorldmapTexture(textureRes, &width, &height);
        glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    }
}
static void ginit_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    Dimension width, height;

    glxContext = glXCreateContext(XtDisplay(w), visinfo, NULL, True);
    if (glxContext == NULL) {
        fprintf(stderr, "Unable to create an OpenGL context\n");
        exit(EXIT_FAILURE);
    }
    GLwDrawingAreaMakeCurrent(w, glxContext);

    if (debug) {
        GLint depthBits;
        glGetIntegerv(GL_DEPTH_BITS, &depthBits);
        printf("GL_RENDERER: %s\n", (const char *)glGetString(GL_RENDERER));
        printf("GL_VERSION: %s\n", (const char *)glGetString(GL_VERSION));
        printf("GL_DEPTH_BITS: %i\n", (int)depthBits);
    }

    glEnable(GL_DEPTH_TEST);
    if (!initFont(XtDisplay(w), OSD_FONT)) fprintf(stderr, "Unable to load a font, no text will be shown\n");
    quadric = gluNewQuadric();

    listGlobe = glGenLists(4);
    listGrid = listGlobe + 1;
    listWireMap = listGlobe + 2;
    listStars = listGlobe + 3;
    glNewList(listGlobe, GL_COMPILE);
    drawGlobeTextured(36, 72);
    glEndList();
    glNewList(listGrid, GL_COMPILE);
    drawLatLongGrid();
    glEndList();
    glNewList(listWireMap, GL_COMPILE);
    drawWireframeMap();
    glEndList();
    glNewList(listStars, GL_COMPILE);
    drawStars();
    glEndList();

    XtVaGetValues(w, XmNwidth, &width, XmNheight, &height, NULL);
    reshape(width, height);
}
static void expose_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    GLwDrawingAreaCallbackStruct *cb = (GLwDrawingAreaCallbackStruct *)call_data;
    if (cb->event != NULL && cb->event->type == Expose && cb->event->xexpose.count > 0) return;
    display();
}
static void resize_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    GLwDrawingAreaCallbackStruct *cb = (GLwDrawingAreaCallbackStruct *)call_data;
    if (glxContext == NULL) return;
    reshape(cb->width, cb->height);
    g_redrawUI = 1;
}
static void input_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    GLwDrawingAreaCallbackStruct *cb = (GLwDrawingAreaCallbackStruct *)call_data;
    XEvent *ev = cb->event;
    switch (ev->type) {
    case ButtonPress:
        mouseButton(ev->xbutton.button, 1, ev->xbutton.x, ev->xbutton.y);
        break;
    case ButtonRelease:
        mouseButton(ev->xbutton.button, 0, ev->xbutton.x, ev->xbutton.y);
        break;
    case MotionNotify:
        if (ev->xmotion.state & Button1Mask) mouseMotion(ev->xmotion.x, ev->xmotion.y);
        break;
    default:
        break;
    }
}

/* --- application --- */
static void onQuit(void) {
    if (debug) printf("[xsginet-globe] Shutting down..\n");
    cloudAPI_freeMarkers(lstMarkers);
    lstMarkers = NULL;
    exit(0);
}
static void quit_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    onQuit();
}
static void handle_signal(int s) {
    if (debug) printf("\n\n[xsginet-globe] SIGNAL %i received!\n\n", s);
    onQuit();
}
static int cliSetMyPosition(float posLat, float posLng) {
    char status_msg[512];
    int rc;
    status_msg[0] = '\0';
    printf("Updating your position to: %f %f\n", posLat, posLng);
    rc = cloudAPI_saveMyPosition(status_msg, posLat, posLng);
    printf("-> %s\n", status_msg);
    return rc == RESPONSE_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
/* -u command line option to check and upgrade xsginet-globe binary, must be root! */
static int checkAppUpdate(void) {
    int hasUpdates;
    int rc;
    if (!ALLOW_ONLINE_UPDATE) {
        printf("Online update is disabled in this version\n");
        return EXIT_SUCCESS;
    }
    hasUpdates = 0;
    rc = cloudAPI_onlineUpdater_X11(geteuid(), 0, &hasUpdates, 1);
    if (rc != RESPONSE_OK) {
        printf("Online updater failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
static void printBanner(void) {
    printf("\n");
    printf("                  \\ _____________________ __\n");
    printf("            ____.  \\       .____         /\n");
    printf("            \\_  |     ___. |  _/      _ /\n");
    printf("    _        T  |     T  | |  |__   __\\              _\n");
    printf(" _  \\________:__|_____|_ | |  |\\/_ /   \\ ____  ______/\n");
    printf(" \\ \\\\\\__  _____/|  ____/ | !  |\" T/  \\  \\T   \\ |  __/// /\n");
    printf(" ------|  __/|  |  __/_\\_\\ / /|  |  / \\  \\    \\|  |------\n");
    printf(" ------|  |  |  |_ Z/  _/ X  \\|  |  \\ /  /  \\  !  |------\n");
    printf("    \\dS!  |  |___/_____T_/ \\  !__|\\  \\  /|__Z\\    !NE7\n");
    printf("       |  |  :            _/___/ / \\___/      \\   |\n");
    printf("       .  |               Z     /              \\__|\n");
    printf("          !                  \\ /\n");
    printf("                              \\\n");
    printf("\n");
}
static void error_usage(void) {
    printBanner();
    fprintf(stderr, "xSGINet-globe | flexion 2025 | Version: %s\n\n", THIS_CLIENT_VERSION);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h  show this help screen\n");
    fprintf(stderr, "  -l  use low resolution textures (512)\n");
    fprintf(stderr, "  -t  start with textures (default: wireframe)\n");
    fprintf(stderr, "  -b  use linear (blurred) textures\n");
    fprintf(stderr, "  -n  disable starfield\n");
    fprintf(stderr, "  -u  check for new version and upgrade\n");
    fprintf(stderr, "  -i  show SYSID\n");
    fprintf(stderr, "  -r  print version\n");
    fprintf(stderr, "  -d  debug mode\n");
    fprintf(stderr, "  --lat x.xxxx --lng x.xxxx  set your position to latitude/longitude value\n");
    if (debug) {
        fprintf(stderr, "  --ca-cert-bundle PATH  add CA certificates from a PEM bundle\n");
        fprintf(stderr, "  --sginet-bin PATH      xsginet-globe binary for updater\n");
    }
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}
/*
 * Handles our own options and removes them from argv, everything else is
 * left for XtVaAppInitialize (which would take "-d" as "-display").
 * Returns the new argc.
 */
static int argParser(int argc, char* argv[]) {
    int i, n;
    n = 1;
    for (i = 1; i < argc; i += 1) {
        char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            error_usage();
        } else if (strcmp(a, "-d") == 0) {
            debug = 1;
        } else if (strcmp(a, "-n") == 0) {
            showStars = 0;
        } else if (strcmp(a, "-i") == 0) {
            printf("SYS_ID: %s\n", G_CLIENTID);
            exit(EXIT_SUCCESS);
        } else if (strcmp(a, "-u") == 0) {
            G_MODE = MODE_SWUPDATE;
        } else if (strcmp(a, "-r") == 0) {
            printf("%s\n", THIS_CLIENT_VERSION); /* IMPORTANT: print version only! output used in 'upload-release' tool */
            exit(EXIT_SUCCESS);
        } else if (strcmp(a, "-l") == 0) {
            printf("Using low resolution textures (512)\n");
            printf("Intro animation disabled\n");
            printf("Starfield disabled\n");
            printf("City labels disabled\n");
            textureRes = TEXTURE_512;
            zoom = defaultZoom;
            g_showCities = 0;
            startupSequence = 0;
            showStars = 0;
        } else if (strcmp(a, "-t") == 0) {
            g_wireframe = 0;
        } else if (strcmp(a, "-b") == 0) {
            useLinearTexture = 1;
        } else if (strcmp(a, "--lat") == 0) {
            if (i + 1 >= argc) error_usage();
            i += 1;
            cliLat = (float)atof(argv[i]);
            cliHasLat = 1;
        } else if (strcmp(a, "--lng") == 0) {
            if (i + 1 >= argc) error_usage();
            i += 1;
            cliLng = (float)atof(argv[i]);
            cliHasLng = 1;
        } else if (strcmp(a, "--ca-cert-bundle") == 0) {
            if (i + 1 >= argc) error_usage();
            i += 1;
            cloudAPI_SetCACertBundle(argv[i]);
        } else if (strcmp(a, "--sginet-bin") == 0) {
            if (i + 1 >= argc) error_usage();
            i += 1;
            cloudAPI_SetSGINetBin(argv[i]);
        } else {
            argv[n++] = a; /* X Toolkit option, or unknown (checked after Xt init) */
        }
    }
    argv[n] = NULL;
    return n;
}
static void loadMarkers(void) {
    float myLat = 0.0f, myLon = 0.0f;
    MarkerEntry *myMarker;
    /* keep the position the user is just setting, the new list doesn't have it */
    myMarker = g_setMyPosition != 0 ? findMyMarkerEntry() : NULL;
    if (myMarker) {
        myLat = myMarker->lat;
        myLon = myMarker->lon;
    }
    if (cloudAPI_getMarkers(&lstMarkers, &isRegistered, username, sizeof(username)) != RESPONSE_OK) {
        printf("Unable to load markers from sginet\n");
    }
    if (myMarker && findMyMarkerEntry() == NULL) addMarker("You", MTYPE_THISUSER, myLat, myLon, 1);
}
static void refreshMarkerData(XtPointer client_data, XtIntervalId *id) {
    if (debug) printf("Chronos: Updating markers\n");
    busy_begin();
    loadMarkers();
    busy_end();
    g_redrawUI = 1;
    XtAppAddTimeOut(app, MARKER_REFRESH_INTERVAL, refreshMarkerData, NULL);
}
static void initAppData(void) {
    char msgWelcome[512];
    /* Query Cloud API for markers */
    loadMarkers();

    if (debug) printf("isRegistered: %i | Username: %s\n", isRegistered, username);
    if (isRegistered == 1) {
        snprintf(msgWelcome, sizeof(msgWelcome), "Welcome, %s", username);
    } else {
        snprintf(msgWelcome, sizeof(msgWelcome), "Read-only mode. Start sginet CLI first to register!");
    }
    addNotification(msgWelcome, 10.0f);

    /* check for update */
    if (ALLOW_ONLINE_UPDATE) {
        int hasUpdates = 0;
        if (cloudAPI_onlineUpdater_X11(geteuid(), 1, &hasUpdates, 0) != RESPONSE_OK) {
            printf("Online updater failed\n");
        } else if (hasUpdates == 1) {
            addNotification("New version available. Run 'xsginet-globe -u' as root.", 12.0f);
        }
    }
    g_redrawUI = 1;
}
int main(int argc, char** argv) {
    Widget toplevel;
    Display *dpy;
    Atom wmDelete;
#if XtSpecificationRelease >= 6
    int xargc;      /* X11R6 (IRIX 6.5) */
#else
    Cardinal xargc; /* X11R5 (IRIX 5.x) */
#endif

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);

    cloudAPI_Init(THIS_CLIENT_TYPE, THIS_CLIENT_VERSION, THIS_CLIENT_BIN);

    /* Read SYSID */
    getSYSID(G_CLIENTID);

    /* Parse command line options */
    argc = argParser(argc, argv);

    if (G_MODE == MODE_SWUPDATE) return checkAppUpdate();

    if (cliHasLat || cliHasLng) {
        if (!cliHasLat || !cliHasLng) {
            fprintf(stderr, "--lat and --lng are both required\n");
            return EXIT_FAILURE;
        }
        if (cliLat < -90.0f || cliLat > 90.0f || cliLng < -180.0f || cliLng > 180.0f) {
            fprintf(stderr, "Invalid position, latitude must be -90..90 and longitude -180..180\n");
            return EXIT_FAILURE;
        }
        return cliSetMyPosition(cliLat, cliLng);
    }

    xargc = argc;
    toplevel = XtVaAppInitialize(&app, "SGINetGlobe", NULL, 0, &xargc, argv, fallback_resources, NULL);
    if (xargc > 1) {
        fprintf(stderr, "Unknown option: %s\n", argv[1]);
        error_usage();
    }
    dpy = XtDisplay(toplevel);

    visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), visualAttribs);
    if (visinfo == NULL) {
        fprintf(stderr, "No double buffered RGB visual with depth buffer available (OpenGL/GLX missing?)\n");
        return EXIT_FAILURE;
    }

    glw = XtVaCreateManagedWidget("globe", glwMDrawingAreaWidgetClass, toplevel,
        GLwNvisualInfo, visinfo,
        XmNwidth, winWidth,
        XmNheight, winHeight,
        NULL);
    XtAddCallback(glw, GLwNginitCallback, ginit_cb, NULL);
    XtAddCallback(glw, GLwNexposeCallback, expose_cb, NULL);
    XtAddCallback(glw, GLwNresizeCallback, resize_cb, NULL);
    XtAddCallback(glw, GLwNinputCallback, input_cb, NULL);

    /* quit when the window is closed */
    wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XtVaSetValues(toplevel, XmNdeleteResponse, XmDO_NOTHING, NULL);
    XmAddWMProtocolCallback(toplevel, wmDelete, quit_cb, NULL);

    /* load data before the window opens, so the intro animation doesn't stall */
    initMarkerList();
    initAppData();

    XtRealizeWidget(toplevel);
    prevTime = elapsedMs();

    XtAppAddTimeOut(app, 1000 / FPS, tick, NULL);
    XtAppAddTimeOut(app, MARKER_REFRESH_FIRST, refreshMarkerData, NULL);

    XtAppMainLoop(app);
    return 0;
}
