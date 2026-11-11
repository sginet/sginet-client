#include "../include/cloudAPI.h"
#include "../include/hwinfo_sgi.h"
#include "../include/x11-client.h"
#include <X11/cursorfont.h>
#include <Xm/Protocols.h>
#include <sys/wait.h>

#include "res/camera.xpm"

#define REFRESH_INTERVAL 300000 /* refresh systems list every 5 minutes */
#define THIS_CLIENT_BIN "/usr/bin/xsginet" /* for online updater, --sginet-bin overrides */
#define GLOBE_BINARY "/usr/bin/xsginet-globe"

int debug = 0;
static int show_splash = 1; /* --nosplash */
static int restore_position = 1; /* --norestore-position */
static int restore_width = 1;    /* --norestore-width */
int G_MODE = MODE_DAEMON;

char G_CLIENTID[64];
static Widget scrolled_systems_list = NULL;
static Widget lbl_online = NULL;
static Widget bottom_area = NULL; /* volume + CPU load bars below the systems list */
/* section visibility from ~/.sginet-properties, read in main() */
static int show_volume = 0;
static int show_cpu_load = 0;
static Cursor busy_cursor = None;
static XtIntervalId refresh_timeout_id = 0;
static XtAppContext app;
Widget toplevel;
MachineListEntry *lstSystems = NULL;
int numSystems = 0;

String fallback_resources[] = {
    /* IRIX look (IRIX Interactive Desktop schemes), ignored by other Motif builds */
    "*sgiMode: true",
    "*useSchemes: all",

    "*menubar*fontList: -*-helvetica-medium-r-normal-*-14-*-*-*-*-*-*-*",

    /* fully qualified to override the IRIX scheme XmList defaults; monospace keeps the columns aligned */
    "SGINet*scrolled_systems_listSW.scrolled_systems_list.fontList: -misc-fixed-bold-r-normal--13-120-75-75-c-70-iso8859-1",
    "SGINet*scrolled_systems_listSW.scrolled_systems_list.foreground: #0602d8",
    "SGINet*main_win*lbl_online.fontList: -*-helvetica-bold-r-normal-*-14-*-*-*-*-*-*-*",

    "HINV*hinvWin.scrolledWin.textWidget.fontList: -misc-fixed-medium-r-normal--14-*-*-*-c-*-iso8859-1",
    "HINV*hinvWin.scrolledWin.textWidget.foreground: #0602d8",
    /* initial window size in characters, the window shrink-wraps the text widget */
    "HINV*hinvWin.scrolledWin.textWidget.columns: 80",
    "HINV*hinvWin.scrolledWin.textWidget.rows: 30",

    "*screenshot_dialog*dspScreenshot*foreground: #0602d8",
    "*screenshot_dialog*dspScreenshot*fontList: -*-clean-*-*-*-*-12-*-*-*-*-*-*-*",
    "*screenshot_dialog*dspScreenshot2*foreground: #0602d8",
    "*screenshot_dialog*dspScreenshot2*fontList: -*-clean-*-*-*-*-12-*-*-*-*-*-*-*",

    "*splash*splash_text.fontList: -*-helvetica-bold-r-normal-*-14-*-*-*-*-*-*-*",

    "*about_dialog*fontList: -*-clean-*-*-*-*-13-*-*-*-*-*-*-*",
    "*update_dialog*fontList: -*-clean-*-*-*-*-13-*-*-*-*-*-*-*",

    "*SoftwareSearch*sws_scrolled_win*fontList: -*-clean-*-*-*-*-13-*-*-*-*-*-*-*",
    "*SoftwareSearch*btnSubmit*fontList: -*-clean-*-*-*-*-12-*-*-*-*-*-*-*",

    "*chat_form*sb_scrolled_win*fontList: -*-clean-*-*-*-*-13-*-*-*-*-*-*-*",
    "*chat_form*btnSubmit*fontList: -*-clean-*-*-*-*-12-*-*-*-*-*-*-*",
    "*chat_form*btnRefresh*fontList: -*-clean-*-*-*-*-12-*-*-*-*-*-*-*",
    "*chat_form*input_field*fontList: -*-clean-*-*-*-*-12-*-*-*-*-*-*-*",

    NULL
};

/* Main window geometry will be saved in ~/.sginet-properties */
#define WINDOW_MIN_WIDTH 200
#define WINDOW_MIN_HEIGHT 100
static int win_x, win_y, win_w, win_h;
static int win_geometry_valid = 0;

/* top-left of the client window (inside the window manager frame) in root window coordinates.
   Restored with StaticGravity the window manager places the client window there again (4Dwm
   does this anyway), saving the frame corner would drift by the decoration size on every start */
static void window_position(Window w, int *x, int *y) {
    Window child;
    int rx, ry;
    if (XTranslateCoordinates(XtDisplay(toplevel), w, RootWindowOfScreen(XtScreen(toplevel)),
            0, 0, &rx, &ry, &child)) {
        *x = rx;
        *y = ry;
    }
}
static void window_geometry_track(Widget w, XtPointer client_data, XEvent *event, Boolean *cont) {
    if (event->type == ConfigureNotify) {
        win_w = event->xconfigure.width;
        win_h = event->xconfigure.height;
    } else if (event->type != ReparentNotify && event->type != MapNotify) {
        return;
    }
    window_position(XtWindow(toplevel), &win_x, &win_y);
    win_geometry_valid = 1;
}
/* after realize: start from the current size, the final position follows with Map/ReparentNotify */
static void window_geometry_init(void) {
    Dimension w = 0, h = 0;
    XtVaGetValues(toplevel, XmNwidth, &w, XmNheight, &h, NULL);
    win_w = w;
    win_h = h;
    window_position(XtWindow(toplevel), &win_x, &win_y);
    win_geometry_valid = 1;
}
static void window_geometry_save(void) {
    if (!win_geometry_valid) return;
    property_set_int(PROP_WINDOW_X, win_x);
    property_set_int(PROP_WINDOW_Y, win_y);
    property_set_int(PROP_WINDOW_WIDTH, win_w);
    property_set_int(PROP_WINDOW_HEIGHT, win_h);
}
/* before realize: saved size/position, limited to the screen; a -geometry option wins */
static void window_geometry_restore(void) {
    static char geometry[64]; /* the shell copies it, static to be safe */
    String current = NULL;
    Dimension cur_w = 0;
    Screen *scr = XtScreen(toplevel);
    int sw = WidthOfScreen(scr), sh = HeightOfScreen(scr);
    int x, y, w, h, use_pos, use_width;
    char *p = geometry;

    XtVaGetValues(toplevel, XmNgeometry, &current, XmNwidth, &cur_w, NULL);
    if (current != NULL) return;
    w = property_get_int(PROP_WINDOW_WIDTH, 0);
    h = property_get_int(PROP_WINDOW_HEIGHT, 0);
    if (w <= 0 || h <= 0) return; /* first run: default size, the window manager places it */
    x = property_get_int(PROP_WINDOW_X, 0);
    y = property_get_int(PROP_WINDOW_Y, 0);
    /* suppressed by the command line option or in the properties file */
    use_pos = restore_position && property_get_int(PROP_RESTORE_POSITION, 1);
    use_width = restore_width && property_get_int(PROP_RESTORE_WIDTH, 1);
    if (!use_width) w = cur_w; /* default width, used to keep the position on the screen */

    if (w < WINDOW_MIN_WIDTH) w = WINDOW_MIN_WIDTH;
    if (h < WINDOW_MIN_HEIGHT) h = WINDOW_MIN_HEIGHT;
    if (w > sw) w = sw;
    if (h > sh) h = sh;
    if (x > sw - w) x = sw - w;
    if (y > sh - h) y = sh - h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    /* "WxH+X+Y", the width ("xH") and the position may be left out */
    if (use_width) p += sprintf(p, "%d", w);
    p += sprintf(p, "x%d", h);
    if (use_pos) sprintf(p, "+%d+%d", x, y);
    if (debug) printf("[xsginet] restoring window geometry %s\n", geometry);
    XtVaSetValues(toplevel, XmNgeometry, geometry, NULL);
    if (use_pos) XtVaSetValues(toplevel, XmNwinGravity, StaticGravity, NULL);
}
static void wm_delete_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    onQuit();
}

/* current height of a managed widget including its border, 0 while hidden */
static int managed_height(Widget w) {
    Dimension h = 0, bw = 0;
    if (!XtIsManaged(w)) return 0;
    XtVaGetValues(w, XmNheight, &h, XmNborderWidth, &bw, NULL);
    return h + 2 * bw;
}
void bottom_area_show(Widget child, Boolean show, Boolean resize_window) {
    WidgetList children;
    Cardinal num_children = 0, i;
    XtWidgetGeometry pref;
    Dimension shell_w = 0, shell_h = 0;
    int any = 0, old_h, new_h, target;
    if (bottom_area == NULL || child == NULL) return;

    if (!XtIsRealized(toplevel)) resize_window = False;
    if (resize_window) XtVaGetValues(toplevel, XmNwidth, &shell_w, XmNheight, &shell_h, NULL);

    old_h = managed_height(bottom_area);
    if (show) XtManageChild(child);
    else XtUnmanageChild(child);

    /* new height: sum of the visible strips' preferred heights
       (the row column has no margins or spacing, and may not have re-laid out yet) */
    new_h = 0;
    XtVaGetValues(bottom_area, XmNchildren, &children, XmNnumChildren, &num_children, NULL);
    for (i = 0; i < num_children; i++) {
        if (!XtIsManaged(children[i])) continue;
        any = 1;
        XtQueryGeometry(children[i], NULL, &pref);
        new_h += pref.height + 2 * pref.border_width;
    }
    if (any) XtManageChild(bottom_area);
    else XtUnmanageChild(bottom_area);

    if (!resize_window) return;
    target = (int)shell_h + new_h - old_h;
    if (target < 1) target = 1;
    XtVaSetValues(toplevel, XmNwidth, shell_w, XmNheight, (Dimension)(target + 1), NULL);
    XtVaSetValues(toplevel, XmNwidth, shell_w, XmNheight, (Dimension)target, NULL);

    /* the new size arrives later with ConfigureNotify, save the requested one now */
    if (win_geometry_valid) {
        win_w = shell_w;
        win_h = target;
    }
    window_geometry_save();
}
/* Watch cursor on the shell of w while a (blocking) cloud API call runs */
static Widget shell_of(Widget w) {
    while (w != NULL && !XtIsShell(w)) w = XtParent(w);
    return w;
}
void busy_begin(Widget w) {
    Widget shell = shell_of(w);
    if (shell == NULL || !XtIsRealized(shell)) return;
    if (busy_cursor == None) busy_cursor = XCreateFontCursor(XtDisplay(shell), XC_watch);
    XDefineCursor(XtDisplay(shell), XtWindow(shell), busy_cursor);
    XmUpdateDisplay(shell);
    XFlush(XtDisplay(shell));
}
void busy_end(Widget w) {
    Widget shell = shell_of(w);
    if (shell == NULL || !XtIsRealized(shell)) return;
    XUndefineCursor(XtDisplay(shell), XtWindow(shell));
    XFlush(XtDisplay(shell));
}

static Widget create_menu_item(Widget menu, char *label) {
    Widget item;
    XmString str;
    str = XmStringCreateLocalized(label);
    item = XtVaCreateManagedWidget(
        label, xmPushButtonWidgetClass, menu,
        XmNlabelString, str,
        NULL);
    XmStringFree(str);
    return item;
}
static void refresh_menu_item_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    loadSystemsListOnline();
}
static void quit_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    onQuit();
}
/* Launch the globe viewer detached: the intermediate child exits right away, so no zombie is left behind */
static void globe_menu_item_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    pid_t pid;
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        if (fork() == 0) {
            execl(GLOBE_BINARY, GLOBE_BINARY, (char *)NULL);
            perror(GLOBE_BINARY);
        }
        _exit(0);
    }
    waitpid(pid, NULL, 0);
}

Widget attachMainMenuBar(Widget main_win) {
    Widget menubar, file_menu, tools_menu, item;
    XmString str;

    menubar = XmCreateMenuBar(main_win, "menubar", NULL, 0);
    XtManageChild(menubar);

    /* File menu */
    file_menu = XmCreatePulldownMenu(menubar, "file_menu", NULL, 0);
    XtVaCreateManagedWidget(
        "File", xmCascadeButtonWidgetClass, menubar,
        XmNsubMenuId, file_menu,
        NULL);

    item = create_menu_item(file_menu, "Refresh online systems");
    XtAddCallback(item, XmNactivateCallback, refresh_menu_item_callback, NULL);

    str = XmStringCreateLocalized("Show volume control");
    item = XtVaCreateManagedWidget(
        "Show volume control", xmToggleButtonWidgetClass, file_menu,
        XmNlabelString, str,
        XmNvisibleWhenOff, True,
        XmNset, show_volume ? True : False,
        NULL);
    XmStringFree(str);
    XtAddCallback(item, XmNvalueChangedCallback, volume_toggle_callback, NULL);

    str = XmStringCreateLocalized("Show CPU load");
    item = XtVaCreateManagedWidget(
        "Show CPU load", xmToggleButtonWidgetClass, file_menu,
        XmNlabelString, str,
        XmNvisibleWhenOff, True,
        XmNset, show_cpu_load ? True : False,
        NULL);
    XmStringFree(str);
    XtAddCallback(item, XmNvalueChangedCallback, cpuload_toggle_callback, NULL);

    item = create_menu_item(file_menu, "Check for updates");
    XtAddCallback(item, XmNactivateCallback, show_update_callback, (XtPointer)main_win);

    item = create_menu_item(file_menu, "About xSGINet");
    XtAddCallback(item, XmNactivateCallback, about_callback, (XtPointer)main_win);

    XtVaCreateManagedWidget("sep_quit", xmSeparatorWidgetClass, file_menu, NULL);

    item = create_menu_item(file_menu, "Quit");
    XtAddCallback(item, XmNactivateCallback, quit_callback, NULL);

    /* Tools menu */
    tools_menu = XmCreatePulldownMenu(menubar, "tools_menu", NULL, 0);
    XtVaCreateManagedWidget(
        "Tools", xmCascadeButtonWidgetClass, menubar,
        XmNsubMenuId, tools_menu,
        NULL);

    item = create_menu_item(tools_menu, "Search software");
    XtAddCallback(item, XmNactivateCallback, show_swsearch_callback, (XtPointer)main_win);

    item = create_menu_item(tools_menu, "System advisor");
    XtAddCallback(item, XmNactivateCallback, sysadvisor_menu_item_callback, (XtPointer)main_win);

    item = create_menu_item(tools_menu, "Shoutbox");
    XtAddCallback(item, XmNactivateCallback, shoutbox_menu_item_callback, (XtPointer)main_win);

    item = create_menu_item(tools_menu, "View globe..");
    if (access(GLOBE_BINARY, X_OK) == 0) {
        XtAddCallback(item, XmNactivateCallback, globe_menu_item_callback, NULL);
    } else {
        XtSetSensitive(item, False);
    }

    item = create_menu_item(tools_menu, "Screenshot..");
    XtAddCallback(item, XmNactivateCallback, screenshot_menu_item_callback, (XtPointer)main_win);

    return menubar;
}
static void formatUptime(char *buf, size_t size, int seconds) {
    int days, hours, minutes;
    if (seconds <= 60) {
        snprintf(buf, size, "-");
        return;
    }
    days = seconds / 86400;
    seconds %= 86400;
    hours = seconds / 3600;
    seconds %= 3600;
    minutes = seconds / 60;

    if (days > 0) {
        snprintf(buf, size, "Up %d days %dh %dmin", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(buf, size, "Up %dh %dmin", hours, minutes);
    } else {
        snprintf(buf, size, "Up %dmin", minutes);
    }
}
static void updateOnlineLabel(void) {
    XmString str;
    char buf[32];
    if (lbl_online == NULL) return;
    sprintf(buf, "Online: %d", numSystems);
    str = XmStringCreateLocalized(buf);
    XtVaSetValues(lbl_online, XmNlabelString, str, NULL);
    XmStringFree(str);
}

static Dimension labelTextOffset(Widget w) {
    Dimension highlight = 0, shadow = 0, margin_w = 0, margin_l = 0;
    XtVaGetValues(w,
        XmNhighlightThickness, &highlight,
        XmNshadowThickness, &shadow,
        XmNmarginWidth, &margin_w,
        XmNmarginLeft, &margin_l,
        NULL);
    return highlight + shadow + margin_w + margin_l;
}

/* move the "Online: $n" text to align with "File" menu text */
static void alignOnlineLabel(Widget menubar) {
    Widget cascade;
    Position cascade_x = 0, menubar_x = 0, label_x = 0, bar_x = 0;
    Dimension margin_l = 0;
    int target, current;

    if (lbl_online == NULL) return;
    cascade = XtNameToWidget(menubar, "File");
    if (cascade == NULL) return;

    XtVaGetValues(cascade, XmNx, &cascade_x, NULL);
    XtVaGetValues(menubar, XmNx, &menubar_x, NULL);
    XtVaGetValues(XtParent(lbl_online), XmNx, &bar_x, NULL);
    XtVaGetValues(lbl_online, XmNx, &label_x, XmNmarginLeft, &margin_l, NULL);

    target = menubar_x + cascade_x + labelTextOffset(cascade);
    current = bar_x + label_x + labelTextOffset(lbl_online) - margin_l;
    if (target > current)
        XtVaSetValues(lbl_online, XmNmarginLeft, (Dimension)(target - current), NULL);
}
int loadSystemsListOnline(void) {
    int numItems, i, rc;
    MachineListEntry *lstNew;
    XmString strLine;
    char lineBuffer[MAX_SYSTEM_LIST_LINE_LENGTH];
    char strUptime[256];

    /* Query online systems. The current list stays valid until the new one arrived. */
    lstNew = NULL;
    numItems = 0;
    busy_begin(scrolled_systems_list);
    rc = cloudAPI_showSystems(&lstNew, &numItems, REQTYPE_SHOWONLINESYSTEMS);
    busy_end(scrolled_systems_list);
    if (rc == RESPONSE_REGISTRATION_REQUIRED) {
        if (cloudAPI_lastError()[0] != '\0') printf("Error: %s\n", cloudAPI_lastError());
        printf("This system is not registered on SGINet\n");
        if (lstNew) free(lstNew);
        return rc;
    }
    if (rc != RESPONSE_OK || lstNew == NULL || numItems < 1) {
        if (cloudAPI_lastError()[0] != '\0') printf("Error: %s\n", cloudAPI_lastError());
        printf("%s\n", MSG_NO_CONNECTION);
        if (lstNew) free(lstNew);
        return RESPONSE_ERROR;
    }

    if (lstSystems) free(lstSystems);
    lstSystems = lstNew;
    numSystems = numItems;

    XmListDeleteAllItems(scrolled_systems_list);
    for (i = 0; i < numItems; ++i) {
        formatUptime(strUptime, sizeof(strUptime), lstSystems[i].uptime);
        snprintf(lineBuffer, MAX_SYSTEM_LIST_LINE_LENGTH, "%-15.15s | %-15.15s | %-25.25s | %-25.25s | %s",
            lstSystems[i].username,
            lstSystems[i].modelname,
            lstSystems[i].machinename,
            lstSystems[i].gfx0,
            strUptime
        );
        strLine = XmStringCreateLocalized(lineBuffer);
        XmListAddItem(scrolled_systems_list, strLine, i+1);
        XmStringFree(strLine);
    }

    updateOnlineLabel();

    return RESPONSE_OK;
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
void error_usage(void) {
    printBanner();
    fprintf(stderr, "xSGINet client | flexion 2025 | Version: %s\n\n", THIS_CLIENT_VERSION);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h  show this help screen\n");
    fprintf(stderr, "  -i  show SYSID\n");
    fprintf(stderr, "  -r  print version\n");
    fprintf(stderr, "  -u  check for new version and upgrade xsginet\n");
    fprintf(stderr, "  -d  debug mode\n");
    fprintf(stderr, "  --nosplash  do not show the splash screen on startup\n");
    fprintf(stderr, "  --norestore-position  do not restore the saved window position\n");
    fprintf(stderr, "  --norestore-width     do not restore the saved window width\n");
    if (debug) {
        fprintf(stderr, "  --ca-cert-bundle PATH  add CA certificates from a PEM bundle\n");
        fprintf(stderr, "  --sginet-bin PATH      xsginet binary for updater\n");
    }
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}
/*
 * Handles our own options and removes them from argv, everything else is
 * left for XtVaAppInitialize (which would take "-d" as "-display").
 * Returns the new argc.
 */
int argParser(int argc, char* argv[]) {
    int i, n;
    n = 1;
    for (i = 1; i < argc; i += 1) {
        char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            error_usage();
        } else if (strcmp(a, "-d") == 0) {
            debug = 1;
        } else if (strcmp(a, "-i") == 0) {
            printf("SYS_ID: %s\n", G_CLIENTID);
            exit(EXIT_SUCCESS);
        } else if (strcmp(a, "-u") == 0) {
            G_MODE = MODE_SWUPDATE;
        } else if (strcmp(a, "-r") == 0) {
            printf("%s\n", THIS_CLIENT_VERSION); /* IMPORTANT: print version only! output used in 'upload-release' tool */
            exit(EXIT_SUCCESS);
        } else if (strcmp(a, "--nosplash") == 0) {
            show_splash = 0;
        } else if (strcmp(a, "--norestore-position") == 0) {
            restore_position = 0;
        } else if (strcmp(a, "--norestore-width") == 0) {
            restore_width = 0;
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
static void systemlist_refresh_function(XtPointer client_data, XtIntervalId *id) {
    refresh_timeout_id = 0; /* fired, no longer valid */
    if (loadSystemsListOnline() != RESPONSE_OK) printf("Error: Could not refresh systems list\n");
    refresh_timeout_id = XtAppAddTimeOut(app, REFRESH_INTERVAL, systemlist_refresh_function, client_data);
}
void onQuit(void) {
    if (debug) printf("[xsginet] Shutting down..\n");
    window_geometry_save();
    if (lstSystems != NULL) {
        if (debug) printf("[xsginet] freeing lstSystems\n");
        free(lstSystems);
        lstSystems = NULL;
    }
    exit(0);
}
static void handle_signal(int s) {
    if (debug) printf("\n\n[xsginet] SIGNAL %i received!\n\n", s);
    onQuit();
}
/* Camera button on the right of the "Online: $n" bar, opens the screenshot window */
static void attachScreenshotButton(Widget bar, Widget main_win) {
    Widget btn;
    Pixmap pixmap;
    Pixel bg;
    Dimension w, h;
    XmString str;

    btn = XtVaCreateManagedWidget(
        "btnScreenshot", xmPushButtonWidgetClass, bar,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNrightOffset, 4,
        XmNhighlightThickness, 0,
        XmNmarginWidth, 2,
        XmNmarginHeight, 1,
        NULL);
    XtAddCallback(btn, XmNactivateCallback, screenshot_menu_item_callback, (XtPointer)main_win);

    XtVaGetValues(btn, XmNbackground, &bg, NULL);
    pixmap = pixmap_from_xpm(btn, camera_xpm, bg, &w, &h, NULL, NULL);
    if (pixmap != None) {
        XtVaSetValues(btn, XmNlabelType, XmPIXMAP, XmNlabelPixmap, pixmap, NULL);
    } else {
        str = XmStringCreateLocalized("Screenshot");
        XtVaSetValues(btn, XmNlabelString, str, NULL);
        XmStringFree(str);
    }
}
/* Build and show the main window, the splash stays up while the systems list loads */
static void start_main_window(void) {
    Widget main_win, scrolled_win, main_win_menubar, online_bar;
    Arg args[1];
    int rc;

    if (show_splash) splash_show(toplevel);

    main_win = XmCreateMainWindow(toplevel, "main_win", NULL, 0);
    XtManageChild(main_win);

    main_win_menubar = attachMainMenuBar(main_win);

    /* static vertical scrollbar to prevent unwanted gap on resize */
    XtSetArg(args[0], XmNscrollBarDisplayPolicy, XmSTATIC);
    scrolled_systems_list = XmCreateScrolledList(main_win, "scrolled_systems_list", args, 1);

    /* "Online: $n" and the screenshot button between menubar and system list */
    online_bar = XtVaCreateManagedWidget("online_bar", xmFormWidgetClass, main_win, NULL);
    attachScreenshotButton(online_bar, main_win);
    lbl_online = XtVaCreateManagedWidget(
        "lbl_online", xmLabelWidgetClass, online_bar,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNalignment, XmALIGNMENT_BEGINNING,
        XmNmarginWidth, 6,
        XmNmarginHeight, 4,
        NULL);

    rc = loadSystemsListOnline();
    splash_hide();
    /* do not start if unable to connect */
    if (rc != RESPONSE_OK) {
        show_fatal_error(toplevel, cloudAPI_lastError()[0] != '\0' ? cloudAPI_lastError() : MSG_NO_CONNECTION);
        return;
    }

    XtAddCallback(scrolled_systems_list, XmNdefaultActionCallback, sysinfo_callback, NULL);
    XtManageChild(scrolled_systems_list);

    /* Use the parent scrolled window as the work area */
    scrolled_win = XtParent(scrolled_systems_list);
    XmMainWindowSetAreas(main_win, main_win_menubar, online_bar, NULL, NULL, scrolled_win);

    /* message window below the list: volume control above CPU load, both hidden until enabled in the File menu */
    bottom_area = XtVaCreateWidget(
        "bottom_area", xmRowColumnWidgetClass, main_win,
        XmNorientation, XmVERTICAL,
        XmNpacking, XmPACK_TIGHT,
        XmNmarginWidth, 0,
        XmNmarginHeight, 0,
        XmNspacing, 0,
        NULL);
    volume_create(bottom_area);
    cpuload_create(bottom_area);
    XtVaSetValues(main_win, XmNmessageWindow, bottom_area, NULL);

    /* Schedule background refresh task */
    refresh_timeout_id = XtAppAddTimeOut(app, REFRESH_INTERVAL, systemlist_refresh_function, (XtPointer)app);

    /* restore the saved sections before realize: they are part of the initial
       window size, no window resize on startup */
    if (show_volume) volume_show(True, False);
    if (show_cpu_load) cpuload_show(True, False);

    /* track size and position for saving on quit */
    XtAddEventHandler(toplevel, StructureNotifyMask, False, window_geometry_track, NULL);
    XtRealizeWidget(toplevel);
    window_geometry_init();
    alignOnlineLabel(main_win_menubar);
}
int main(int argc, char *argv[]) {
    int rc;
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

    show_volume = property_get_int(PROP_SHOW_VOLUME, 0);
    show_cpu_load = property_get_int(PROP_SHOW_CPU_LOAD, 0);

    if (debug) printf("G_CLIENTID: %s\n", G_CLIENTID);

    if (G_MODE == MODE_SWUPDATE) {
        /* -u command line option to check and upgrade xsginet binary, must be root! */
        int hasUpdates;
        hasUpdates = 0;
        rc = cloudAPI_onlineUpdater_X11(geteuid(), 0, &hasUpdates, 1);
        if (rc != RESPONSE_OK) {
            printf("Online updater failed\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    xargc = argc;
    toplevel = XtVaAppInitialize(
        &app, "SGINet", NULL, 0, &xargc, argv, fallback_resources,
        XmNwidth, 300,
        XmNheight, 200,
        XmNtitle, "SGINet",
        NULL
    );
    if (xargc > 1) {
        fprintf(stderr, "Unknown option: %s\n", argv[1]);
        error_usage();
    }
    window_geometry_restore();
    /* window manager close button: quit like File > Quit, so the geometry is saved */
    XtVaSetValues(toplevel, XmNdeleteResponse, XmDO_NOTHING, NULL);
    XmAddWMProtocolCallback(toplevel, XmInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW", False),
        wm_delete_callback, NULL);

    /* Say hello to the cloud, an unregistered machine must register first */
    if (show_splash) splash_show(toplevel);
    rc = cloudAPI_request(REQTYPE_HELLO);
    if (rc == RESPONSE_REGISTRATION_REQUIRED) {
        if (debug) printf("[xsginet] Please register first\n");
        register_show(toplevel, start_main_window);
    } else if (rc != RESPONSE_OK) {
        splash_hide();
        show_fatal_error(toplevel, cloudAPI_lastError()[0] != '\0' ? cloudAPI_lastError() : "Could not connect to SGINet backend");
    } else {
        start_main_window();
    }

    XtAppMainLoop(app);

    return 0;
}
