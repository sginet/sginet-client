#include "../include/x11-client.h"
#include "../include/cloudAPI.h"

#define MAX_HINV_LIST_LINE_LENGTH 642
#define LABEL_WIDTH 18
extern MachineListEntry *lstSystems;
extern int numSystems;

void sysinfo_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;
    Widget popup, mainWin, scrolledWin, textWidget;
    LabelAndValue *lstHINV;
    int numItems, i, rc, pos;
    char buffer[8192];
    char line[MAX_HINV_LIST_LINE_LENGTH];
    int offset;
    Arg args[2];
    Cardinal n;

    pos = cbs->item_position - 1; /* Motif positions are 1-based */
    if (lstSystems == NULL || pos < 0 || pos >= numSystems) return;

    /* Query hinv of system by GSI1 ID */
    lstHINV = NULL;
    numItems = 0;
    busy_begin(w);
    rc = cloudAPI_getSystemHINVAsList(lstSystems[pos].id, &lstHINV, &numItems);
    busy_end(w);

    offset = 0;
    buffer[0] = '\0';
    if (rc != RESPONSE_OK || lstHINV == NULL) {
        snprintf(buffer, sizeof(buffer), "%s\n", MSG_NO_CONNECTION);
        numItems = 0;
    }
    for (i = 0; i < numItems; ++i) {
        /* "%-18s" left-justify in a field 18 characters wide */
        snprintf(line, sizeof(line), "%-*s %s\n", LABEL_WIDTH, lstHINV[i].label, lstHINV[i].value);
        if (offset + strlen(line) < sizeof(buffer)) {
            strcpy(buffer + offset, line);
            offset += strlen(line);
        } else {
            break;
        }
    }
    if (lstHINV) free(lstHINV);
    if (debug) printf("textWidget buffer offset: %i\n", offset);

    popup = XtVaAppCreateShell(
        "HINV", "ScrolledTextPopup",
        topLevelShellWidgetClass, XtDisplay(w),
        XmNtitle, "HINV",
        XmNdeleteResponse, XmDESTROY,
        NULL);

    /* size comes from textWidget rows/columns (fallback resources in main.c) */
    mainWin = XmCreateMainWindow(popup, "hinvWin", NULL, 0);
    XtManageChild(mainWin);

    scrolledWin = XmCreateScrolledWindow(mainWin, "scrolledWin", NULL, 0);
    XtManageChild(scrolledWin);

    /* editMode must be set at creation, a single-line text forces rows to 1 */
    n = 0;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNeditable, False); n++;
    textWidget = XmCreateText(scrolledWin, "textWidget", args, n);
    XtManageChild(textWidget);
    XmTextSetString(textWidget, buffer);

    XmScrolledWindowSetAreas(scrolledWin, NULL, NULL, textWidget);
    XmMainWindowSetAreas(mainWin, NULL, NULL, NULL, NULL, scrolledWin);

    XtRealizeWidget(popup);
}
