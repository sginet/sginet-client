#ifndef HEADER_X11CLIENT
#define HEADER_X11CLIENT

/* Motif 1.2 API only: IRIX ships Motif 1.2 headers by default */
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/List.h>
#include <Xm/PushB.h>
#include <Xm/MessageB.h>
#include <Xm/ScrolledW.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/DialogS.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAX_SYSTEM_LIST_LINE_LENGTH 1024
#ifdef IRIX5
#define THIS_CLIENT_VERSION "V20261028-1711-X_5" /* max 23 chars */
#define THIS_CLIENT_TYPE "X_5" /* must be 3 chars */
#else
#define THIS_CLIENT_VERSION "V20261028-1711-X_6" /* max 23 chars */
#define THIS_CLIENT_TYPE "X_6" /* must be 3 chars */
#endif
#define SCREENSHOT_FULLSCREEN 0
#define SCREENSHOT_REGION 1

#define MSG_NO_CONNECTION "Unable to communicate with sginet.flexion.io"

extern char G_CLIENTID[64];

int loadSystemsListOnline(void);
int argParser(int argc, char* argv[]);
void error_usage(void);
Widget attachMainMenuBar(Widget main_win);

/* busy cursor while blocking on the cloud API (main.c) */
void busy_begin(Widget w);
void busy_end(Widget w);

void about_callback(Widget w, XtPointer client_data, XtPointer call_data);
void sysinfo_callback(Widget w, XtPointer client_data, XtPointer call_data);
void shoutbox_menu_item_callback(Widget w, XtPointer client_data, XtPointer call_data);
void screenshot_menu_item_callback(Widget w, XtPointer client_data, XtPointer call_data);
void sysadvisor_menu_item_callback(Widget w, XtPointer client_data, XtPointer call_data);
void show_update_callback(Widget widget, XtPointer client_data, XtPointer call_data);
void show_swsearch_callback(Widget widget, XtPointer client_data, XtPointer call_data);
void onQuit(void);

/* startup splash screen and fatal errors (w_startup.c) */
void splash_show(Widget parent);
void splash_hide(void);
void show_fatal_error(Widget parent, const char *msg);

/* registration form (w_register.c) */
void register_show(Widget parent, void (*registered)(void));

#endif /* HEADER_X11CLIENT */
