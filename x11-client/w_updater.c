#include "../include/x11-client.h"
#include "../include/cloudAPI.h"

static void update_close_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    XtDestroyWidget(XtParent(w)); /* the dialog shell */
}

void show_update_callback(Widget widget, XtPointer client_data, XtPointer call_data) {
    Widget parent = (Widget)client_data;
    Widget dialog, button;
    XmString message, close_label, title;
    char updaterMessage[256];
    char buf[512];
    int hasUpdates;
    int rc;

    /* check for updates */
    hasUpdates = 0;
    busy_begin(parent);
    rc = cloudAPI_onlineUpdater_X11(geteuid(), 1, &hasUpdates, 0);
    busy_end(parent);
    if (rc != RESPONSE_OK) {
        snprintf(updaterMessage, sizeof(updaterMessage), "Failed to check for updates :(");
    } else if (hasUpdates == 1) {
        snprintf(updaterMessage, sizeof(updaterMessage), "A new version is available.\nTo update, start 'xsginet -u' as root user.\n");
    } else {
        snprintf(updaterMessage, sizeof(updaterMessage), "No update available.");
    }

    /* Create the message string to display in the dialog */
    snprintf(buf, sizeof(buf), "You are using version: %s\n\n%s\n", THIS_CLIENT_VERSION, updaterMessage);
    message = XmStringCreateLtoR(buf, XmFONTLIST_DEFAULT_TAG); /* \n as line breaks, also on Motif 1.2 */
    title = XmStringCreateLocalized("Online update");
    close_label = XmStringCreateLocalized("Close");

    dialog = XmCreateMessageDialog(parent, "update_dialog", NULL, 0);
    XtVaSetValues(dialog,
                  XmNmessageString, message,
                  XmNokLabelString, close_label,
                  XmNdialogTitle, title,
                  NULL);

    XmStringFree(message);
    XmStringFree(title);
    XmStringFree(close_label);

    /* Remove Cancel and Help buttons, keep only OK (renamed to Close) */
    button = XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON);
    if (button) XtUnmanageChild(button);
    button = XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
    if (button) XtUnmanageChild(button);

    XtAddCallback(dialog, XmNokCallback, update_close_cb, NULL);

    XtManageChild(dialog);
}
