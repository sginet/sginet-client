/* gr_osview style CPU load bars (one row per CPU), shown below the volume control. IRIX only. */
#include "../include/hwinfo_sgi.h"
#include "../include/x11-client.h"
#include <Xm/DrawingA.h>

#if defined(__sgi)

#define CPULOAD_INTERVAL 500 /* ms between samples */
#define CPULOAD_MAX_CPUS 64
#define CPULOAD_BAR_H 8       /* height of one CPU bar in pixels */
#define CPULOAD_GAP 3         /* vertical gap between bars */
#define CPULOAD_MARGIN 3      /* border around all bars */

/* segment order from left to right, gr_osview colors */
enum { SEG_USER, SEG_SYS, SEG_WAIT, SEG_IDLE, SEG_COUNT };
static const char *seg_color_names[SEG_COUNT] = {
    "blue",   /* user */
    "red",    /* sys + intr */
    "yellow", /* wio + sxbrk */
    "green"   /* idle */
};

static Widget cpuload_bar = NULL;
static IRIXMachine *machine = NULL;
static int num_cpus = 0;
static GC gc = NULL;
static Pixel seg_pixel[SEG_COUNT];
static XtIntervalId tick_id = 0;

static void alloc_colors(Widget w) {
    Display *dpy = XtDisplay(w);
    Colormap cmap;
    XColor exact, screen;
    int i;
    XtVaGetValues(w, XmNcolormap, &cmap, NULL);
    for (i = 0; i < SEG_COUNT; i++) {
        if (XAllocNamedColor(dpy, cmap, seg_color_names[i], &screen, &exact)) {
            seg_pixel[i] = screen.pixel;
        } else {
            seg_pixel[i] = (i == SEG_IDLE) ? WhitePixelOfScreen(XtScreen(w)) : BlackPixelOfScreen(XtScreen(w));
        }
    }
}

static void cpuload_draw(void) {
    Display *dpy;
    Window win;
    Dimension width = 0;
    int bar_w, c, s, x, y, seg_w;
    double pct[SEG_COUNT];
    CPUData *cpu;

    if (cpuload_bar == NULL || !XtIsRealized(cpuload_bar) || machine == NULL) return;
    dpy = XtDisplay(cpuload_bar);
    win = XtWindow(cpuload_bar);
    if (gc == NULL) {
        gc = XCreateGC(dpy, win, 0, NULL);
        alloc_colors(cpuload_bar);
    }

    XtVaGetValues(cpuload_bar, XmNwidth, &width, NULL);
    bar_w = (int)width - 2 * CPULOAD_MARGIN;
    if (bar_w < 1) return;

    for (c = 0; c < num_cpus; c++) {
        cpu = &machine->cpuData[c];
        pct[SEG_USER] = cpu->pct_normal;
        pct[SEG_SYS] = cpu->pct_kernel + cpu->pct_irq;
        pct[SEG_WAIT] = cpu->pct_wait;

        x = CPULOAD_MARGIN;
        y = CPULOAD_MARGIN + c * (CPULOAD_BAR_H + CPULOAD_GAP);
        for (s = 0; s < SEG_IDLE; s++) {
            seg_w = (int)(bar_w * pct[s] / 100.0 + 0.5);
            if (seg_w > CPULOAD_MARGIN + bar_w - x) seg_w = CPULOAD_MARGIN + bar_w - x;
            if (seg_w <= 0) continue;
            XSetForeground(dpy, gc, seg_pixel[s]);
            XFillRectangle(dpy, win, gc, x, y, seg_w, CPULOAD_BAR_H);
            x += seg_w;
        }
        /* idle takes the rest, so every pixel of the bar is painted (no flicker) */
        if (x < CPULOAD_MARGIN + bar_w) {
            XSetForeground(dpy, gc, seg_pixel[SEG_IDLE]);
            XFillRectangle(dpy, win, gc, x, y, CPULOAD_MARGIN + bar_w - x, CPULOAD_BAR_H);
        }
    }
}

static void cpuload_expose_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    cpuload_draw();
}

static void cpuload_tick(XtPointer client_data, XtIntervalId *id) {
    tick_id = 0;
    Machine_scanCPU(machine);
    cpuload_draw();
    tick_id = XtAppAddTimeOut(XtWidgetToApplicationContext(cpuload_bar), CPULOAD_INTERVAL, cpuload_tick, NULL);
}

Widget cpuload_create(Widget parent) {
    Dimension height;

    machine = Machine_new(); /* CPU count, per-CPU tick counters */
    if (machine == NULL || machine->existingCPUs < 1) return NULL;
    num_cpus = machine->existingCPUs;
    if (num_cpus > CPULOAD_MAX_CPUS) num_cpus = CPULOAD_MAX_CPUS;

    height = 2 * CPULOAD_MARGIN + num_cpus * CPULOAD_BAR_H + (num_cpus - 1) * CPULOAD_GAP;
    cpuload_bar = XtVaCreateWidget(
        "cpuload_bar", xmDrawingAreaWidgetClass, parent,
        XmNheight, height,
        XmNresizePolicy, XmRESIZE_NONE,
        XmNmarginWidth, 0,
        XmNmarginHeight, 0,
        NULL);
    XtAddCallback(cpuload_bar, XmNexposeCallback, cpuload_expose_callback, NULL);
    return cpuload_bar;
}

/* File > Show CPU load */
void cpuload_toggle_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct *)call_data;
    if (cpuload_bar == NULL) return;
    if (cbs->set) {
        Machine_scanCPU(machine); /* baseline, the first tick shows the load since now */
        if (tick_id == 0)
            tick_id = XtAppAddTimeOut(XtWidgetToApplicationContext(cpuload_bar), CPULOAD_INTERVAL, cpuload_tick, NULL);
    } else {
        if (tick_id != 0) {
            XtRemoveTimeOut(tick_id);
            tick_id = 0;
        }
    }
    bottom_area_show(cpuload_bar, cbs->set ? True : False);
}

#else /* !__sgi: disabled if not sgi */

Widget cpuload_create(Widget parent) {
    return NULL;
}
void cpuload_toggle_callback(Widget w, XtPointer client_data, XtPointer call_data) {
}

#endif
