#include "../include/x11-client.h"
#include "../include/cloudAPI.h"
#include <X11/Shell.h>
#include <Xm/Protocols.h>

#include "res/sgicube.xpm"

#define SPLASH_TEXT "Starting up xSGINet"
#define XPM_MAX_COLORS 256
#define XPM_GRAY_LEVELS 32 /* color levels per channel, keeps 8-bit colormaps usable */

static Widget splash_shell = NULL;
static Pixmap splash_pixmap = None;
static unsigned long splash_pixels[XPM_MAX_COLORS];
static int splash_num_pixels = 0;

/* Push the splash to the screen now, the caller is about to block on the cloud API */
static void splash_flush(void) {
    XSync(XtDisplay(splash_shell), False);
    XmUpdateDisplay(splash_shell);
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}
static unsigned short quantize(int v8) {
    int level = v8 * (XPM_GRAY_LEVELS - 1) / 255;
    return (unsigned short)(level * 65535 / (XPM_GRAY_LEVELS - 1));
}
/*
 * Pixel for #RRGGBB: an exact (quantized) color if the colormap has room,
 * otherwise the nearest color allocated so far, black if there is none.
 */
static unsigned long alloc_pixel(Display *dpy, Colormap cmap, const char *spec, unsigned long *track, int *num_track) {
    XColor col, best;
    long dist, best_dist;
    int i;

    col.red = col.green = col.blue = 0;
    if (spec[0] == '#' && strlen(spec) >= 7) {
        col.red = quantize(hexval(spec[1]) * 16 + hexval(spec[2]));
        col.green = quantize(hexval(spec[3]) * 16 + hexval(spec[4]));
        col.blue = quantize(hexval(spec[5]) * 16 + hexval(spec[6]));
    }
    col.flags = DoRed | DoGreen | DoBlue;
    if (XAllocColor(dpy, cmap, &col)) {
        if (track != NULL) track[(*num_track)++] = col.pixel;
        return col.pixel;
    }
    best.pixel = BlackPixel(dpy, DefaultScreen(dpy));
    best_dist = -1;
    for (i = 0; track != NULL && i < *num_track; ++i) {
        XColor c;
        long dr, dg, db;
        c.pixel = track[i];
        XQueryColor(dpy, cmap, &c);
        dr = ((long)c.red - col.red) >> 8;
        dg = ((long)c.green - col.green) >> 8;
        db = ((long)c.blue - col.blue) >> 8;
        dist = dr * dr + dg * dg + db * db;
        if (best_dist < 0 || dist < best_dist) {
            best_dist = dist;
            best.pixel = c.pixel;
        }
    }
    return best.pixel;
}
/*
 * Minimal XPM reader (no libXpm on IRIX 5.x): 1-2 chars per pixel,
 * colors "#RRGGBB" or "None" (drawn in bg). Returns None on bad data.
 * Allocated colors are appended to track (room for XPM_MAX_COLORS) so the
 * caller can free them later, NULL keeps them for the lifetime of the app.
 */
Pixmap pixmap_from_xpm(Widget w, char **xpm, Pixel bg, Dimension *width, Dimension *height,
                       unsigned long *track, int *num_track) {
    Display *dpy = XtDisplay(w);
    Screen *screen = XtScreen(w);
    Colormap cmap;
    int depth;
    int pw, ph, ncolors, cpp, i, x, y;
    char keys[XPM_MAX_COLORS][2];
    unsigned long pixels[XPM_MAX_COLORS];
    XImage *image;
    Pixmap pixmap;
    GC gc;

    if (sscanf(xpm[0], "%d %d %d %d", &pw, &ph, &ncolors, &cpp) != 4) return None;
    if (pw < 1 || ph < 1 || ncolors < 1 || ncolors > XPM_MAX_COLORS || cpp < 1 || cpp > 2) return None;
    XtVaGetValues(w, XmNcolormap, &cmap, XmNdepth, &depth, NULL);

    for (i = 0; i < ncolors; ++i) {
        const char *line = xpm[1 + i];
        const char *spec;
        keys[i][0] = line[0];
        keys[i][1] = cpp > 1 ? line[1] : '\0';
        /* "<chars> <key> <color>", take the last word as the color */
        spec = line + strlen(line);
        while (spec > line + cpp && spec[-1] != ' ' && spec[-1] != '\t') spec--;
        if (strcmp(spec, "None") == 0 || strcmp(spec, "none") == 0) {
            pixels[i] = bg;
        } else {
            pixels[i] = alloc_pixel(dpy, cmap, spec, track, num_track);
        }
    }

    image = XCreateImage(dpy, DefaultVisualOfScreen(screen), depth, ZPixmap, 0, NULL, pw, ph, 32, 0);
    if (image == NULL) return None;
    image->data = malloc(image->bytes_per_line * ph);
    if (image->data == NULL) {
        XDestroyImage(image);
        return None;
    }
    for (y = 0; y < ph; ++y) {
        const char *row = xpm[1 + ncolors + y];
        for (x = 0; x < pw; ++x) {
            const char *px = row + x * cpp;
            unsigned long pixel = bg;
            for (i = 0; i < ncolors; ++i) {
                if (keys[i][0] == px[0] && (cpp == 1 || keys[i][1] == px[1])) {
                    pixel = pixels[i];
                    break;
                }
            }
            XPutPixel(image, x, y, pixel);
        }
    }

    pixmap = XCreatePixmap(dpy, RootWindowOfScreen(screen), pw, ph, depth);
    gc = XCreateGC(dpy, pixmap, 0, NULL);
    XPutImage(dpy, pixmap, gc, image, 0, 0, 0, 0, pw, ph);
    XFreeGC(dpy, gc);
    XDestroyImage(image); /* frees data too */

    *width = pw;
    *height = ph;
    return pixmap;
}

/* Borderless splash window centered on the screen, shown while startup waits for the cloud */
void splash_show(Widget parent) {
    Widget form, logo;
    XmString str;
    Pixel bg, fg;
    Dimension width, height, logo_w, logo_h;
    Screen *screen;

    if (splash_shell != NULL) return;

    /* white like the logo background, thin black window border instead of a Motif frame */
    screen = XtScreen(parent);
    bg = WhitePixelOfScreen(screen);
    fg = BlackPixelOfScreen(screen);
    splash_shell = XtVaCreatePopupShell("splash",
        overrideShellWidgetClass, parent,
        XmNbackground, bg,
        XmNborderWidth, 1,
        XmNborderColor, fg,
        NULL);

    form = XtVaCreateManagedWidget("splash_form", xmFormWidgetClass, splash_shell,
        XmNbackground, bg,
        XmNmarginWidth, 32,
        XmNmarginHeight, 20,
        NULL);

    splash_num_pixels = 0;
    splash_pixmap = pixmap_from_xpm(form, sgicube_xpm, bg, &logo_w, &logo_h, splash_pixels, &splash_num_pixels);

    logo = XtVaCreateManagedWidget("logo", xmLabelWidgetClass, form,
        XmNlabelType, splash_pixmap != None ? XmPIXMAP : XmSTRING,
        XmNlabelPixmap, splash_pixmap != None ? splash_pixmap : XmUNSPECIFIED_PIXMAP,
        XmNbackground, bg,
        XmNalignment, XmALIGNMENT_CENTER,
        XmNtopAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);
    if (splash_pixmap == None) XtUnmanageChild(logo);

    str = XmStringCreateLocalized(SPLASH_TEXT);
    XtVaCreateManagedWidget("splash_text", xmLabelWidgetClass, form,
        XmNlabelString, str,
        XmNbackground, bg,
        XmNforeground, fg,
        XmNalignment, XmALIGNMENT_CENTER,
        XmNtopAttachment, splash_pixmap != None ? XmATTACH_WIDGET : XmATTACH_FORM,
        XmNtopWidget, logo,
        XmNtopOffset, 12,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        NULL);
    XmStringFree(str);

    XtRealizeWidget(splash_shell);
    XtVaGetValues(splash_shell, XmNwidth, &width, XmNheight, &height, NULL);
    XtVaSetValues(splash_shell,
        XmNx, (Position)((WidthOfScreen(screen) - (int)width) / 2),
        XmNy, (Position)((HeightOfScreen(screen) - (int)height) / 2),
        NULL);
    XtPopup(splash_shell, XtGrabNone);
    busy_begin(splash_shell);
    splash_flush();
}
void splash_hide(void) {
    Display *dpy;
    Colormap cmap;
    if (splash_shell == NULL) return;
    dpy = XtDisplay(splash_shell);
    XtVaGetValues(splash_shell, XmNcolormap, &cmap, NULL);
    XtDestroyWidget(splash_shell);
    splash_shell = NULL;
    if (splash_pixmap != None) XFreePixmap(dpy, splash_pixmap);
    splash_pixmap = None;
    /* give the logo colors back, 8-bit colormaps are small */
    if (splash_num_pixels > 0) XFreeColors(dpy, cmap, splash_pixels, splash_num_pixels, 0);
    splash_num_pixels = 0;
}

/* Error window without an application window behind it: OK or closing it quits xsginet */
static void fatal_exit_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    exit(EXIT_FAILURE);
}
void show_fatal_error(Widget parent, const char *msg) {
    Widget shell, box, button;
    XmString message, ok_label;
    Atom wm_delete;

    printf("Error: %s\n", msg);

    shell = XtVaCreatePopupShell("fatal_error",
        topLevelShellWidgetClass, parent,
        XmNtitle, "SGINet",
        XmNdeleteResponse, XmDO_NOTHING,
        NULL);
    wm_delete = XmInternAtom(XtDisplay(shell), "WM_DELETE_WINDOW", False);
    XmAddWMProtocolCallback(shell, wm_delete, fatal_exit_cb, NULL);

    message = XmStringCreateLtoR((char *)msg, XmFONTLIST_DEFAULT_TAG);
    ok_label = XmStringCreateLocalized("Quit");
    box = XtVaCreateManagedWidget("fatal_box", xmMessageBoxWidgetClass, shell,
        XmNdialogType, XmDIALOG_ERROR,
        XmNmessageString, message,
        XmNokLabelString, ok_label,
        NULL);
    XmStringFree(message);
    XmStringFree(ok_label);

    button = XmMessageBoxGetChild(box, XmDIALOG_CANCEL_BUTTON);
    if (button) XtUnmanageChild(button);
    button = XmMessageBoxGetChild(box, XmDIALOG_HELP_BUTTON);
    if (button) XtUnmanageChild(button);
    XtAddCallback(box, XmNokCallback, fatal_exit_cb, NULL);

    XtPopup(shell, XtGrabNone);
}
