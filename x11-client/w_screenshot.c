#include "../include/x11-client.h"
#include "../include/cloudAPI.h"
#include "../include/fileutils.h"

static void btnScreenshot_callback(Widget w, XtPointer client_data, XtPointer call_data);
static void btnScreenshotRegion_callback(Widget w, XtPointer client_data, XtPointer call_data);
static void screenshot_countdown(XtPointer client_data, XtIntervalId *id);

static Widget scr_dialog_shell = NULL; /* single instance, there is only one screenshot window */
static Widget scr_parent = NULL;

static int countdown_active = 0;
static int screenshotMode = SCREENSHOT_FULLSCREEN;
static int scr_region_x1 = 0;
static int scr_region_x2 = 0;
static int scr_region_y1 = 0;
static int scr_region_y2 = 0;

static void dlg_destroy_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    scr_dialog_shell = NULL; /* release global reference */
}
static Widget add_button(Widget row, char *name, XtCallbackProc cb, int delaySec) {
    Widget button = XmCreatePushButton(row, name, NULL, 0);
    XtAddCallback(button, XmNactivateCallback, cb, (XtPointer)(long)delaySec);
    XtManageChild(button);
    return button;
}
void screenshot_menu_item_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    Widget row, scr_label;
    XmString str;
    Arg args[10];
    int n;

    scr_parent = (Widget)client_data;
    if (scr_dialog_shell == NULL) {
        n = 0;
        XtSetArg(args[n], XmNtitle, "Screenshot"); n++;
        XtSetArg(args[n], XmNwidth, 180); n++;
        XtSetArg(args[n], XmNmaxWidth, 180); n++;
        XtSetArg(args[n], XmNminWidth, 180); n++;
        XtSetArg(args[n], XmNdeleteResponse, XmDESTROY); n++; /* dlg_destroy_callback is called when user closes window */
        scr_dialog_shell = XmCreateDialogShell(scr_parent, "screenshot_dialog", args, n);
        XtAddCallback(scr_dialog_shell, XmNdestroyCallback, dlg_destroy_callback, NULL);

        row = XmCreateRowColumn(scr_dialog_shell, "rowcol", NULL, 0);
        scr_label = XmCreateLabel(row, "dspScreenshot", NULL, 0);
        str = XmStringCreateLocalized("Take a screenshot");
        XtVaSetValues(scr_label, XmNlabelString, str, NULL);
        XmStringFree(str);
        XtManageChild(scr_label);

        add_button(row, "Now", btnScreenshot_callback, 0);
        add_button(row, "In 5 seconds", btnScreenshot_callback, 5);
        add_button(row, "In 10 seconds", btnScreenshot_callback, 10);
        add_button(row, "Region", btnScreenshotRegion_callback, 0);

        XtManageChild(row); /* managing the child of a dialog shell pops it up */
        return;
    }
    /* Already open: bring it to the front */
    XtPopup(scr_dialog_shell, XtGrabNone);
    if (XtIsRealized(scr_dialog_shell)) XRaiseWindow(XtDisplay(scr_dialog_shell), XtWindow(scr_dialog_shell));
}
static void show_result(int rc) {
    Widget dialog, button;
    XmString message, title;

    dialog = XmCreateInformationDialog(scr_parent, "confirmation_dialog", NULL, 0);
    if (rc == RESPONSE_OK) {
        message = XmStringCreateLtoR("Screenshot is now available for \ndownload on sginet.flexion.io", XmFONTLIST_DEFAULT_TAG);
    } else {
        message = XmStringCreateLocalized("Failed to save screenshot");
    }
    title = XmStringCreateLocalized("Screenshot");
    XtVaSetValues(dialog,
                  XmNmessageString, message,
                  XmNdialogTitle, title,
                  NULL);
    XmStringFree(message);
    XmStringFree(title);

    /* Remove the Help and Cancel buttons, keep only OK */
    button = XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
    if (button) XtUnmanageChild(button);
    button = XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON);
    if (button) XtUnmanageChild(button);
    XtManageChild(dialog);
}
/* "Screenshot (5)" while counting down, plain "Screenshot" for sec <= 0 */
static void set_countdown_title(int sec) {
    char title[32];
    if (scr_dialog_shell == NULL) return; /* closed by the user during the countdown */
    if (sec > 0) {
        sprintf(title, "Screenshot (%i)", sec);
    } else {
        sprintf(title, "Screenshot");
    }
    XtVaSetValues(scr_dialog_shell, XmNtitle, title, NULL);
}
/*
 * Timer chain, the event loop keeps running:
 * remaining > 0: show the seconds in the dialog title, tick again in 1s
 * remaining == 0: hide the dialog, give the server time to unmap it
 * remaining < 0: take the screenshot
 */
static void screenshot_countdown(XtPointer client_data, XtIntervalId *id) {
    int remaining = (int)(long)client_data;
    int rc;
    if (remaining > 0) {
        set_countdown_title(remaining);
        XtAppAddTimeOut(XtWidgetToApplicationContext(scr_parent), 1000,
                        screenshot_countdown, (XtPointer)(long)(remaining - 1));
        return;
    }
    if (remaining == 0) {
        set_countdown_title(0);
        if (scr_dialog_shell) XtPopdown(scr_dialog_shell);
        XtAppAddTimeOut(XtWidgetToApplicationContext(scr_parent), 500,
                        screenshot_countdown, (XtPointer)(long)-1);
        return;
    }
    countdown_active = 0;
    XBell(XtDisplay(scr_parent), 0);
    busy_begin(scr_parent);
    rc = takeScreenshotAndUpload(screenshotMode, scr_region_x1, scr_region_x2, scr_region_y1, scr_region_y2);
    busy_end(scr_parent);
    show_result(rc);
}
static void start_countdown(int delaySec) {
    countdown_active = 1;
    XtAppAddTimeOut(XtWidgetToApplicationContext(scr_parent), 1,
                    screenshot_countdown, (XtPointer)(long)delaySec);
}
static void btnScreenshot_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (countdown_active) return; /* dialog stays open while counting down */
    screenshotMode = SCREENSHOT_FULLSCREEN;
    start_countdown((int)(long)client_data);
}
static void rect_from(int x0, int y0, int x1, int y1, int *x, int *y, int *w, int *h) {
    *x = (x0 < x1) ? x0 : x1;
    *y = (y0 < y1) ? y0 : y1;
    *w = abs(x1 - x0);
    *h = abs(y1 - y0);
}
static void btnScreenshotRegion_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    Display *dpy;
    int screen, width, height;
    Window root, win;
    XSetWindowAttributes attrs;
    GC gc;
    XEvent ev;
    int drawing, start_x, start_y, prev_x, prev_y, cur_x, cur_y;
    int rx, ry, rw, rh;
    int selected;

    if (countdown_active) return;
    screenshotMode = SCREENSHOT_REGION;
    XtPopdown(scr_dialog_shell);

    dpy = XtDisplay(scr_dialog_shell);
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    width  = DisplayWidth(dpy, screen);
    height = DisplayHeight(dpy, screen);

    /* Fullscreen override-redirect window without background, used to drag a rectangle */
    attrs.override_redirect = True;
    attrs.background_pixmap = None;
    win = XCreateWindow(dpy, root, 0, 0, width, height, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWOverrideRedirect | CWBackPixmap, &attrs);
    XSelectInput(dpy, win, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask);
    XMapRaised(dpy, win);

    /* XOR drawing of the rectangle */
    gc = XCreateGC(dpy, win, 0, NULL);
    XSetFunction(dpy, gc, GXxor);
    XSetForeground(dpy, gc, WhitePixel(dpy, screen) ^ BlackPixel(dpy, screen));
    XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
    XSetSubwindowMode(dpy, gc, IncludeInferiors);

    XGrabPointer(dpy, win, True,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);

    drawing = 0;
    selected = 0;
    start_x = start_y = prev_x = prev_y = cur_x = cur_y = 0;
    for (;;) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            break; /* any key aborts */
        } else if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
            drawing = 1;
            start_x = prev_x = cur_x = ev.xbutton.x;
            start_y = prev_y = cur_y = ev.xbutton.y;
        } else if (ev.type == MotionNotify && drawing) {
            /* erase previous rectangle by drawing it again (XOR), then draw the new one */
            rect_from(start_x, start_y, prev_x, prev_y, &rx, &ry, &rw, &rh);
            XDrawRectangle(dpy, win, gc, rx, ry, rw, rh);
            cur_x = ev.xmotion.x;
            cur_y = ev.xmotion.y;
            rect_from(start_x, start_y, cur_x, cur_y, &rx, &ry, &rw, &rh);
            XDrawRectangle(dpy, win, gc, rx, ry, rw, rh);
            prev_x = cur_x;
            prev_y = cur_y;
        } else if (ev.type == ButtonRelease && ev.xbutton.button == Button1 && drawing) {
            rect_from(start_x, start_y, prev_x, prev_y, &rx, &ry, &rw, &rh);
            XDrawRectangle(dpy, win, gc, rx, ry, rw, rh);

            rect_from(start_x, start_y, cur_x, cur_y, &rx, &ry, &rw, &rh);
            /* scrsave counts from the lower left corner */
            scr_region_x1 = rx;
            scr_region_x2 = rx + rw;
            scr_region_y1 = height - ry;
            scr_region_y2 = height - ry - rh;
            if (debug) printf("scrsave: from lower left: %d %d %d %d\n", scr_region_x1, scr_region_x2, scr_region_y1, scr_region_y2);
            selected = (rw > 0 && rh > 0);
            break;
        }
    }

    XUngrabKeyboard(dpy, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);
    XDestroyWindow(dpy, win);
    XFreeGC(dpy, gc);
    XFlush(dpy);

    if (selected) start_countdown(0);
}
