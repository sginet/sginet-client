#include "../include/x11-client.h"
#include "../include/cloudAPI.h"

static void submit_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void refresh_cb(Widget w, XtPointer client_data, XtPointer call_data);

static Widget chat_text = NULL;
static Widget input_field = NULL;
static Widget chat_shell = NULL;

#define SB_ROW_LENGTH 512

static void shoutbox_destroy_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    chat_shell = NULL; /* release global reference */
    chat_text = NULL;
    input_field = NULL;
}
static void loadShoutBoxEntries(void) {
    int numItems, i, rc;
    ShoutboxEntry *lstSB;
    char buffer[8192]; /* max 50 entries of ~400 chars, the rest is cut off */
    char line[SB_ROW_LENGTH];
    int offset;

    if (chat_text == NULL) return;

    lstSB = NULL;
    numItems = 0;
    busy_begin(chat_text);
    rc = cloudAPI_getShoutBox(&lstSB, &numItems);
    busy_end(chat_text);

    offset = 0;
    buffer[0] = '\0';
    if (rc != RESPONSE_OK || lstSB == NULL) {
        snprintf(buffer, sizeof(buffer), "%s\n", MSG_NO_CONNECTION);
        numItems = 0;
    }
    for (i = 0; i < numItems; ++i) {
        snprintf(line, sizeof(line), "--- <%s> %s@%s:\n%s\n\n",
            lstSB[i].dt,
            lstSB[i].user,
            lstSB[i].machine,
            lstSB[i].msg
        );
        if (offset + strlen(line) < sizeof(buffer)) {
            strcpy(buffer + offset, line);
            offset += strlen(line);
        } else {
            break;
        }
    }
    if (lstSB) free(lstSB);
    if (debug) printf("textWidget buffer offset: %i\n", offset);
    XmTextSetString(chat_text, buffer);
}
void shoutbox_menu_item_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    Widget parent = (Widget)client_data;
    Widget chat_form, scrolled_win, bottom_panel;
    Widget submit_btn, refresh_btn;
    XmString label;
    Arg args[4];
    int n;

    if (chat_shell == NULL) {
        chat_shell = XtVaCreatePopupShell("ShoutBox",
            topLevelShellWidgetClass, parent,
            XmNwidth, 500,
            XmNheight, 400,
            XmNtitle, "SGINet ShoutBox",
            NULL);

        XtAddCallback(chat_shell, XmNdestroyCallback, shoutbox_destroy_callback, NULL);

        chat_form = XmCreateForm(chat_shell, "chat_form", NULL, 0);
        XtManageChild(chat_form);

        /* Bottom panel (fixed height) */
        bottom_panel = XmCreateForm(chat_form, "bottom_panel", NULL, 0);
        XtVaSetValues(bottom_panel,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_NONE,
            XmNheight, 40,
            NULL);
        XtManageChild(bottom_panel);

        /* Scrolled chat text area (read-only), above the bottom panel */
        scrolled_win = XmCreateScrolledWindow(chat_form, "sb_scrolled_win", NULL, 0);
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
        chat_text = XmCreateText(scrolled_win, "chat_text", args, n);
        XtManageChild(chat_text);

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
            XmNrightOffset, 5,
            NULL);
        XtAddCallback(refresh_btn, XmNactivateCallback, refresh_cb, NULL);
        XtManageChild(refresh_btn);

        /* Submit button, left of refresh */
        label = XmStringCreateLocalized("Submit");
        n = 0;
        XtSetArg(args[n], XmNlabelString, label); n++;
        submit_btn = XmCreatePushButton(bottom_panel, "btnSubmit", args, n);
        XmStringFree(label);
        XtVaSetValues(submit_btn,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_WIDGET,
            XmNrightWidget, refresh_btn,
            XmNrightOffset, 5,
            NULL);
        XtAddCallback(submit_btn, XmNactivateCallback, submit_cb, NULL);
        XtManageChild(submit_btn);

        /* Input field, fills the rest. ENTER submits too. */
        input_field = XmCreateTextField(bottom_panel, "input_field", NULL, 0);
        XtVaSetValues(input_field,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_WIDGET,
            XmNrightWidget, submit_btn,
            XmNrightOffset, 20,
            XmNmaxLength, 254,
            NULL);
        XtAddCallback(input_field, XmNactivateCallback, submit_cb, NULL);
        XtManageChild(input_field);

        XtPopup(chat_shell, XtGrabNone);
        loadShoutBoxEntries();
        return;
    }
    /* Already open: bring it to the front */
    XtPopup(chat_shell, XtGrabNone);
    if (XtIsRealized(chat_shell)) XRaiseWindow(XtDisplay(chat_shell), XtWindow(chat_shell));
}

static void submit_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    char msg[255];
    char *input;
    int rc;

    input = XmTextFieldGetString(input_field);
    if (input == NULL) return;
    if (strlen(input) == 0) {
        XtFree(input);
        return;
    }
    snprintf(msg, sizeof(msg), "%s", input);
    XtFree(input);

    busy_begin(w);
    rc = cloudAPI_postShoutBox(msg);
    busy_end(w);
    if (rc != RESPONSE_OK) {
        XmTextSetString(chat_text, "Failed to post your message.\n");
        return; /* keep the input, so it can be sent again */
    }
    XmTextFieldSetString(input_field, "");
    loadShoutBoxEntries();
}

static void refresh_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    loadShoutBoxEntries();
}
