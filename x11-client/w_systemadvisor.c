#include "../include/x11-client.h"
#include "../include/systemadvisor.h"
#include "../include/str_tools.h"

#define MAX_SYSADV_LINE_LENGTH 642

static Widget sysadv_shell = NULL;
static Widget sysadv_text = NULL;

static void loadSystemAdvisorData(void);

static char *sevLabel[] = {
    "INFO",
    "OK",
    "WARNING",
    "HIGH",
    "ERROR",
    (char *)NULL
};

static void sysadvisor_destroy_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    sysadv_shell = NULL; /* release global reference */
    sysadv_text = NULL;
}
static void refresh_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    loadSystemAdvisorData();
}

static void loadSystemAdvisorData(void) {
    AdvisoryList lstAdv;
    char buffer[8192];
    char line[MAX_SYSADV_LINE_LENGTH];
    int i, offset;

    if (sysadv_text == NULL) return;

    /* growing array of advisor messages */
    lstAdv.cnt = 0;
    lstAdv.capacity = 5; /* initial size, startSystemAdvisor reallocates */
    lstAdv.lst = malloc(sizeof(AdvisorEntry) * lstAdv.capacity);
    if (lstAdv.lst == NULL) return;

    busy_begin(sysadv_text);
    startSystemAdvisor(&lstAdv);
    busy_end(sysadv_text);

    offset = 0;
    buffer[0] = '\0';
    for (i = 0; i < lstAdv.cnt; ++i) {
        int sev = lstAdv.lst[i].sev;
        if (sev < SEV_INFO || sev > SEV_ERROR) sev = SEV_INFO;
        snprintf(line, sizeof(line), "%-*.*s| %s\n", 8, 8, sevLabel[sev], lstAdv.lst[i].msg);
        if (offset + strlen(line) < sizeof(buffer)) {
            strcpy(buffer + offset, line);
            offset += strlen(line);
        } else {
            break;
        }
    }
    /* the messages themselves are mostly string literals and are not freed */
    free(lstAdv.lst);
    XmTextSetString(sysadv_text, buffer);
}
void sysadvisor_menu_item_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    Widget parent = (Widget)client_data;
    Widget sysadv_form, scrolled_win, bottom_panel, refresh_btn;
    XmString label;
    Arg args[4];
    int n;

    if (sysadv_shell == NULL) {
        sysadv_shell = XtVaCreatePopupShell("SystemAdvisor",
            topLevelShellWidgetClass, parent,
            XmNwidth, 500,
            XmNheight, 400,
            XmNtitle, "SGINet System Advisor",
            NULL);

        XtAddCallback(sysadv_shell, XmNdestroyCallback, sysadvisor_destroy_callback, NULL);

        sysadv_form = XmCreateForm(sysadv_shell, "sysadv_form", NULL, 0);
        XtManageChild(sysadv_form);

        /* Bottom panel (fixed height) */
        bottom_panel = XmCreateForm(sysadv_form, "bottom_panel", NULL, 0);
        XtVaSetValues(bottom_panel,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_NONE,
            XmNheight, 40,
            NULL);
        XtManageChild(bottom_panel);

        /* Scrolled text area (read-only), above the bottom panel */
        scrolled_win = XmCreateScrolledWindow(sysadv_form, "sysadv_scrolled_win", NULL, 0);
        XtVaSetValues(scrolled_win,
            XmNtopAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, bottom_panel,
            NULL);
        XtManageChild(scrolled_win);

        n = 0;
        XtSetArg(args[n], XmNeditable, False); n++;
        XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
        XtSetArg(args[n], XmNcursorPositionVisible, False); n++;
        sysadv_text = XmCreateText(scrolled_win, "sysadv_text", args, n);
        XtManageChild(sysadv_text);

        /* Refresh button (right) */
        label = XmStringCreateLocalized("Refresh");
        n = 0;
        XtSetArg(args[n], XmNlabelString, label); n++;
        refresh_btn = XmCreatePushButton(bottom_panel, "btnRefresh", args, n);
        XmStringFree(label);
        XtVaSetValues(refresh_btn,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, 1,
            NULL);
        XtAddCallback(refresh_btn, XmNactivateCallback, refresh_cb, NULL);
        XtManageChild(refresh_btn);

        XtPopup(sysadv_shell, XtGrabNone);
        loadSystemAdvisorData();
        return;
    }
    /* Already open: bring it to the front */
    XtPopup(sysadv_shell, XtGrabNone);
    if (XtIsRealized(sysadv_shell)) XRaiseWindow(XtDisplay(sysadv_shell), XtWindow(sysadv_shell));
}
