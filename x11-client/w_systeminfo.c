#include "../include/x11-client.h"
#include "../include/cloudAPI.h"

#define WRAP_COLUMNS 60      /* long hinv values are word-wrapped to this many characters */
#define GRID_ROW_SPACING 3   /* vertical gap between grid rows in pixels */
#define GRID_COLUMN_GAP 12   /* horizontal gap between label and value column */
extern MachineListEntry *lstSystems;
extern int numSystems;

/* sgicube logo for the header, loaded once (colors stay allocated for the app's lifetime) */
static Pixmap cubePixmap = None;
static Boolean cubeLoaded = False;

/* Copy src into dst, word-wrapped at WRAP_COLUMNS with '\n' line breaks */
static void wrap_value(char *dst, size_t size, const char *src) {
    size_t out = 0;
    int col = 0;
    const char *p = src;

    while (*p && out + 2 < size) {
        const char *word = p;
        size_t len;
        while (*p && *p != ' ') p++;
        len = (size_t)(p - word);

        if (col > 0 && col + 1 + (int)len > WRAP_COLUMNS) {
            dst[out++] = '\n';
            col = 0;
        } else if (col > 0) {
            dst[out++] = ' ';
            col++;
        }
        if (out + len + 1 >= size) len = size - out - 1;
        memcpy(dst + out, word, len);
        out += len;
        col += (int)len;

        while (*p == ' ') p++; /* collapse runs of blanks */
    }
    dst[out] = '\0';
}

static void close_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XtDestroyWidget((Widget)client_data);
}

/* Keep the grid as wide as the scrolled window's clip area so the value column follows resizes */
static void clip_resize_handler(Widget clip, XtPointer client_data, XEvent *event, Boolean *cont) {
    Widget grid = (Widget)client_data;
    Dimension gridW, want;
    XtPointer minW = NULL;
    if (event->type != ConfigureNotify) return;
    /* never narrower than the grid's natural width (stored in userData), scroll instead */
    XtVaGetValues(grid, XmNwidth, &gridW, XmNuserData, &minW, NULL);
    want = (Dimension)event->xconfigure.width;
    if (want < (Dimension)(long)minW) want = (Dimension)(long)minW;
    if (want > 0 && want != gridW) {
        XtVaSetValues(grid, XmNwidth, want, NULL);
    }
}

static Widget create_label(Widget parent, const char *name, const char *text, unsigned char alignment) {
    Widget lbl;
    XmString str = XmStringCreateLtoR((char *)text, XmFONTLIST_DEFAULT_TAG);
    lbl = XtVaCreateManagedWidget(
        name, xmLabelWidgetClass, parent,
        XmNlabelString, str,
        XmNalignment, alignment,
        NULL);
    XmStringFree(str);
    return lbl;
}

static void build_header(Widget header, MachineListEntry *sys) {
    Widget icon, title, byline, subtitle;
    Pixel white;
    Dimension w, h;
    char text[512];

    /* white banner because our cube logo has white backgroudn */
    white = WhitePixelOfScreen(XtScreen(header));
    XtVaSetValues(header, XmNbackground, white, NULL);

    icon = XtVaCreateManagedWidget(
        "icon", xmLabelWidgetClass, header,
        XmNbackground, white,
        XmNtopAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        NULL);
    if (!cubeLoaded) {
        cubePixmap = pixmap_from_xpm(icon, sgicube_xpm_data(), white, &w, &h, NULL, NULL);
        cubeLoaded = True;
    }
    if (cubePixmap != None) {
        XtVaSetValues(icon, XmNlabelType, XmPIXMAP, XmNlabelPixmap, cubePixmap, NULL);
    } else {
        XtUnmanageChild(icon);
    }

    title = create_label(header, "title", sys->machinename, XmALIGNMENT_BEGINNING);
    XtVaSetValues(title,
        XmNbackground, white,
        XmNtopAttachment, XmATTACH_FORM,
        XmNtopOffset, 8,
        XmNleftAttachment, cubePixmap != None ? XmATTACH_WIDGET : XmATTACH_FORM,
        XmNleftWidget, icon,
        XmNleftOffset, 10,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    snprintf(text, sizeof(text), "by %s", sys->username);
    byline = create_label(header, "byline", text, XmALIGNMENT_BEGINNING);
    XtVaSetValues(byline,
        XmNbackground, white,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, title,
        XmNtopOffset, 2,
        XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
        XmNleftWidget, title,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    if (sys->gfx0[0] != '\0') {
        snprintf(text, sizeof(text), "%s (%s), %s CPU%s\nGFX: %s",
            sys->modelname, sys->ipmodel, sys->cpuCountStr, sys->cpuCount == 1 ? "" : "s",
            sys->gfx0);
    } else {
        snprintf(text, sizeof(text), "%s (%s), %s CPU%s",
            sys->modelname, sys->ipmodel, sys->cpuCountStr, sys->cpuCount == 1 ? "" : "s");
    }
    subtitle = create_label(header, "subtitle", text, XmALIGNMENT_BEGINNING);
    XtVaSetValues(subtitle,
        XmNbackground, white,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, byline,
        XmNtopOffset, 2,
        XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
        XmNleftWidget, title,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);
}

/* One row per hinv entry: right-aligned bold label column, value column takes the rest */
static void build_grid(Widget grid, LabelAndValue *lst, int count) {
    Widget *labels;
    Widget prev = NULL;
    Dimension maxW = 0, lw;
    char name[32];
    char wrapped[1024];
    int i;

    labels = (Widget *)XtMalloc(count * sizeof(Widget));
    for (i = 0; i < count; ++i) {
        Widget val;

        snprintf(name, sizeof(name), "lbl_%d", i);
        labels[i] = create_label(grid, name, lst[i].label, XmALIGNMENT_END);
        XtVaSetValues(labels[i],
            XmNtopAttachment, prev ? XmATTACH_WIDGET : XmATTACH_FORM,
            XmNtopWidget, prev,
            XmNtopOffset, GRID_ROW_SPACING,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, 6,
            XmNrecomputeSize, False,
            NULL);

        wrap_value(wrapped, sizeof(wrapped), lst[i].value);
        snprintf(name, sizeof(name), "val_%d", i);
        val = create_label(grid, name, wrapped, XmALIGNMENT_BEGINNING);
        XtVaSetValues(val,
            XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNtopWidget, labels[i],
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, labels[i],
            XmNleftOffset, GRID_COLUMN_GAP,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, 6,
            NULL);

        XtVaGetValues(labels[i], XmNwidth, &lw, NULL);
        if (lw > maxW) maxW = lw;
        /* next row starts below the taller of label and (possibly wrapped) value */
        prev = val;
    }
    for (i = 0; i < count; ++i) {
        XtVaSetValues(labels[i], XmNwidth, maxW, NULL);
    }
    XtFree((char *)labels);
}

void sysinfo_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;
    Widget popup, form, header, sepTop, scrolledWin, grid, sepBottom, btnClose, clip;
    LabelAndValue *lstHINV;
    LabelAndValue errorEntry;
    MachineListEntry *sys;
    int numItems, rc, pos;
    char title[128];

    pos = cbs->item_position - 1; /* Motif positions are 1-based */
    if (lstSystems == NULL || pos < 0 || pos >= numSystems) return;
    sys = &lstSystems[pos];

    /* Query hinv of system by GSI1 ID */
    lstHINV = NULL;
    numItems = 0;
    busy_begin(w);
    rc = cloudAPI_getSystemHINVAsList(sys->id, &lstHINV, &numItems);
    busy_end(w);

    snprintf(title, sizeof(title), "%s - HINV", sys->machinename);
    popup = XtVaAppCreateShell(
        "HINV", "ScrolledTextPopup",
        topLevelShellWidgetClass, XtDisplay(w),
        XmNtitle, title,
        XmNdeleteResponse, XmDESTROY,
        NULL);

    form = XtVaCreateWidget("hinvForm", xmFormWidgetClass, popup, NULL);

    header = XtVaCreateManagedWidget(
        "header", xmFormWidgetClass, form,
        XmNmarginWidth, 10,
        XmNmarginHeight, 8,
        XmNtopAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);
    build_header(header, sys);

    sepTop = XtVaCreateManagedWidget(
        "sepTop", xmSeparatorWidgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, header,
        XmNtopOffset, 0,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    btnClose = XtVaCreateManagedWidget(
        "btnClose", xmPushButtonWidgetClass, form,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNbottomOffset, 8,
        XmNrightAttachment, XmATTACH_FORM,
        XmNrightOffset, 10,
        NULL);
    {
        XmString str = XmStringCreateLocalized("Close");
        XtVaSetValues(btnClose, XmNlabelString, str, NULL);
        XmStringFree(str);
    }
    XtAddCallback(btnClose, XmNactivateCallback, close_callback, (XtPointer)popup);

    sepBottom = XtVaCreateManagedWidget(
        "sepBottom", xmSeparatorWidgetClass, form,
        XmNbottomAttachment, XmATTACH_WIDGET,
        XmNbottomWidget, btnClose,
        XmNbottomOffset, 8,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    /* initial size from fallback resources (HINV*scrolledWin.width/height in main.c) */
    scrolledWin = XtVaCreateManagedWidget(
        "scrolledWin", xmScrolledWindowWidgetClass, form,
        XmNscrollingPolicy, XmAUTOMATIC,
        XmNscrollBarDisplayPolicy, XmAS_NEEDED,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, sepTop,
        XmNtopOffset, 4,
        XmNbottomAttachment, XmATTACH_WIDGET,
        XmNbottomWidget, sepBottom,
        XmNbottomOffset, 4,
        XmNleftAttachment, XmATTACH_FORM,
        XmNleftOffset, 4,
        XmNrightAttachment, XmATTACH_FORM,
        XmNrightOffset, 4,
        NULL);

    grid = XtVaCreateWidget("grid", xmFormWidgetClass, scrolledWin, NULL);
    if (rc != RESPONSE_OK || lstHINV == NULL || numItems == 0) {
        snprintf(errorEntry.label, sizeof(errorEntry.label), "%s", "Error:");
        snprintf(errorEntry.value, sizeof(errorEntry.value), "%s",
            rc != RESPONSE_OK ? MSG_NO_CONNECTION : "No hardware inventory available");
        build_grid(grid, &errorEntry, 1);
    } else {
        build_grid(grid, lstHINV, numItems);
    }
    if (lstHINV) free(lstHINV);
    {
        XtWidgetGeometry pref;
        pref.request_mode = 0;
        XtQueryGeometry(grid, NULL, &pref);
        XtVaSetValues(grid, XmNuserData, (XtPointer)(long)pref.width, NULL);
    }
    XtManageChild(grid);

    XtVaGetValues(scrolledWin, XmNclipWindow, &clip, NULL);
    if (clip) {
        XtAddEventHandler(clip, StructureNotifyMask, False, clip_resize_handler, (XtPointer)grid);
    }

    XtManageChild(form);
    XtRealizeWidget(popup);
}
