#include "../include/x11-client.h"
#include "../include/cloudAPI.h"

#define SWS_ROW_LENGTH 300

static Widget sws_shell = NULL;
static Widget searchresult_text = NULL;
static Widget input_field = NULL;

static void submit_cb(Widget w, XtPointer client_data, XtPointer call_data);

static void swsearch_destroy_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    sws_shell = NULL; /* release global reference */
    searchresult_text = NULL;
    input_field = NULL;
}
void show_swsearch_callback(Widget widget, XtPointer client_data, XtPointer call_data) {
    Widget parent = (Widget)client_data;
    Widget search_form, scrolled_win, bottom_panel;
    Widget submit_btn;
    XmString label;
    Arg args[4];
    int n;

    if (sws_shell == NULL) {
        sws_shell = XtVaCreatePopupShell("SoftwareSearch",
            topLevelShellWidgetClass, parent,
            XmNwidth, 500,
            XmNheight, 400,
            XmNtitle, "Software Search",
            NULL);

        XtAddCallback(sws_shell, XmNdestroyCallback, swsearch_destroy_callback, NULL);

        search_form = XmCreateForm(sws_shell, "search_form", NULL, 0);
        XtManageChild(search_form);

        /* Bottom panel (fixed height) */
        bottom_panel = XmCreateForm(search_form, "bottom_panel", NULL, 0);
        XtVaSetValues(bottom_panel,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_NONE,
            XmNheight, 40,
            NULL);
        XtManageChild(bottom_panel);

        /* Scrolled result text area (read-only), above the bottom panel */
        scrolled_win = XmCreateScrolledWindow(search_form, "sws_scrolled_win", NULL, 0);
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
        searchresult_text = XmCreateText(scrolled_win, "searchresult_text", args, n);
        XtManageChild(searchresult_text);

        /* Search button (right) */
        label = XmStringCreateLocalized("Search");
        n = 0;
        XtSetArg(args[n], XmNlabelString, label); n++;
        submit_btn = XmCreatePushButton(bottom_panel, "btnSubmit", args, n);
        XmStringFree(label);
        XtVaSetValues(submit_btn,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, 10,
            NULL);
        XtAddCallback(submit_btn, XmNactivateCallback, submit_cb, NULL);
        XtManageChild(submit_btn);

        /* Input field, fills the rest. ENTER searches too. */
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

        XtPopup(sws_shell, XtGrabNone);
        return;
    }
    /* Already open: bring it to the front */
    XtPopup(sws_shell, XtGrabNone);
    if (XtIsRealized(sws_shell)) XRaiseWindow(XtDisplay(sws_shell), XtWindow(sws_shell));
}

static void submit_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    int numItems, offset, i, rc;
    SoftwareEntry *lstResults;
    char query[255];
    char *input;
    char buffer[32000]; /* max 50 results of up to 300 chars */
    char line[SWS_ROW_LENGTH];

    input = XmTextFieldGetString(input_field);
    if (input == NULL) return;
    if (strlen(input) == 0) {
        XtFree(input);
        return;
    }
    snprintf(query, sizeof(query), "%s", input);
    XtFree(input);

    lstResults = NULL;
    numItems = 0;
    busy_begin(w);
    rc = cloudAPI_searchSoftware(&lstResults, &numItems, query);
    busy_end(w);

    offset = 0;
    buffer[0] = '\0';
    if (rc != RESPONSE_OK) {
        snprintf(buffer, sizeof(buffer), "%s\n", MSG_NO_CONNECTION);
        numItems = 0;
    } else {
        if (lstResults == NULL) numItems = 0;
        snprintf(buffer, sizeof(buffer), "Packages found: %i\n\n", numItems);
        offset = strlen(buffer);
    }
    for (i = 0; i < numItems; ++i) {
        snprintf(line, sizeof(line), "%-17.17s> %s\n",
            lstResults[i].src,
            lstResults[i].pkg
        );
        if (offset + strlen(line) < sizeof(buffer)) {
            strcpy(buffer + offset, line);
            offset += strlen(line);
        } else {
            if (debug) printf("buffer full\n");
            break;
        }
    }
    if (lstResults) free(lstResults);
    if (debug) printf("textWidget buffer offset: %i\n", offset);
    XmTextSetString(searchresult_text, buffer);
    XmTextFieldSetString(input_field, "");
}
