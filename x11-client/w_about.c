#include "../include/x11-client.h"
#include "../include/cloudAPI.h"

/* Callback to show About dialog */
void about_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    Widget parent = (Widget)client_data;
    Widget about_dialog, help_button, cancel_button;
    XmString message, title;

    message = XmStringCreateLtoR("X11 motif client for SGINet\n\nsginet.flexion.io\n\nxSGINet: " THIS_CLIENT_VERSION "\n\n(c) flexion 2025", XmFONTLIST_DEFAULT_TAG);
    title = XmStringCreateLocalized("About xSGINet");

    /* Create an Information Dialog */
    about_dialog = XmCreateInformationDialog(parent, "about_dialog", NULL, 0);
    XtVaSetValues(about_dialog,
                  XmNmessageString, message,
                  XmNdialogTitle, title,
                  NULL);

    XmStringFree(message);
    XmStringFree(title);

    /* Remove the Help and Cancel buttons, keep only OK */
    help_button = XmMessageBoxGetChild(about_dialog, XmDIALOG_HELP_BUTTON);
    cancel_button = XmMessageBoxGetChild(about_dialog, XmDIALOG_CANCEL_BUTTON);
    if (help_button) XtUnmanageChild(help_button);
    if (cancel_button) XtUnmanageChild(cancel_button);

    /* Manage and popup the dialog */
    XtManageChild(about_dialog);
}
