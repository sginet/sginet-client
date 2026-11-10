/* gr_osview style CPU load bars (one row per CPU), shown below the volume control. IRIX only. */
#include "../include/hwinfo_sgi.h"
#include "../include/x11-client.h"
#include <Xm/DrawingA.h>

#if defined(__sgi)

#define CPULOAD_INTERVAL 250 /* ms between samples */
#define CPULOAD_MAX_CPUS 64
#define CPULOAD_BAR_H 8       /* height of one CPU bar in pixels */
#define CPULOAD_GAP 3         /* vertical gap between bars */
#define CPULOAD_MARGIN 3      /* border around all bars */
#define CPULOAD_FRAME 50      /* ms between redraws while a bar falls off */
#define CPULOAD_FALLOFF 2000  /* ms for a segment to fall from 100% to 0% (like gr_osview) */

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
static int frame_ms = 0;  /* ms since the last sample */
/* displayed percentages: rise at once to a new sample, decrease slowly */
static double shown[CPULOAD_MAX_CPUS][SEG_IDLE];

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
    int bar_w, inner_w, inner_h, x_end, c, s, x, y, seg_w;
    double *pct;

    if (cpuload_bar == NULL || !XtIsRealized(cpuload_bar) || machine == NULL) return;
    dpy = XtDisplay(cpuload_bar);
    win = XtWindow(cpuload_bar);
    if (gc == NULL) {
        gc = XCreateGC(dpy, win, 0, NULL);
        alloc_colors(cpuload_bar);
    }

    XtVaGetValues(cpuload_bar, XmNwidth, &width, NULL);
    bar_w = (int)width - 2 * CPULOAD_MARGIN;
    /* the 1px black border is inside the bar, the segments fill the inner area */
    inner_w = bar_w - 2;
    inner_h = CPULOAD_BAR_H - 2;
    if (inner_w < 1 || inner_h < 1) return;
    x_end = CPULOAD_MARGIN + 1 + inner_w;

    for (c = 0; c < num_cpus; c++) {
        pct = shown[c];

        y = CPULOAD_MARGIN + c * (CPULOAD_BAR_H + CPULOAD_GAP);
        XSetForeground(dpy, gc, BlackPixelOfScreen(XtScreen(cpuload_bar)));
        XDrawRectangle(dpy, win, gc, CPULOAD_MARGIN, y, bar_w - 1, CPULOAD_BAR_H - 1);

        x = CPULOAD_MARGIN + 1;
        y++;
        for (s = 0; s < SEG_IDLE; s++) {
            seg_w = (int)(inner_w * pct[s] / 100.0 + 0.5);
            if (seg_w > x_end - x) seg_w = x_end - x;
            if (seg_w <= 0) continue;
            XSetForeground(dpy, gc, seg_pixel[s]);
            XFillRectangle(dpy, win, gc, x, y, seg_w, inner_h);
            x += seg_w;
        }
        /* idle takes the rest, so every pixel of the bar is painted (no flicker) */
        if (x < x_end) {
            XSetForeground(dpy, gc, seg_pixel[SEG_IDLE]);
            XFillRectangle(dpy, win, gc, x, y, x_end - x, inner_h);
        }
    }
}

static void cpuload_expose_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    cpuload_draw();
}

/* move the displayed values towards the last sample: up at once, down by the falloff rate.
   Returns 1 if any displayed value changed. */
static int cpuload_step(int jump) {
    double step = 100.0 * CPULOAD_FRAME / CPULOAD_FALLOFF;
    double target[SEG_IDLE];
    CPUData *cpu;
    int c, s, changed = 0;
    for (c = 0; c < num_cpus; c++) {
        cpu = &machine->cpuData[c];
        target[SEG_USER] = cpu->pct_normal;
        target[SEG_SYS] = cpu->pct_kernel + cpu->pct_irq;
        target[SEG_WAIT] = cpu->pct_wait;
        for (s = 0; s < SEG_IDLE; s++) {
            if (shown[c][s] != target[s]) changed = 1;
            if (jump || target[s] >= shown[c][s] || shown[c][s] - step < target[s])
                shown[c][s] = target[s];
            else
                shown[c][s] -= step;
        }
    }
    return changed;
}

static void cpuload_tick(XtPointer client_data, XtIntervalId *id) {
    tick_id = 0;
    frame_ms += CPULOAD_FRAME;
    if (frame_ms >= CPULOAD_INTERVAL) {
        frame_ms = 0;
        Machine_scanCPU(machine);
    }
    if (cpuload_step(0)) cpuload_draw();
    tick_id = XtAppAddTimeOut(XtWidgetToApplicationContext(cpuload_bar), CPULOAD_FRAME, cpuload_tick, NULL);
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
        Machine_scanCPU(machine); /* baseline, the first sample shows the load since now */
        cpuload_step(1);          /* no falloff from values of a previous show */
        frame_ms = 0;
        if (tick_id == 0)
            tick_id = XtAppAddTimeOut(XtWidgetToApplicationContext(cpuload_bar), CPULOAD_FRAME, cpuload_tick, NULL);
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
