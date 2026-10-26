#include "../include/x11-client.h"
#include "../include/cloudAPI.h"
#include "../include/hwinfo_sgi.h"
#include "../include/str_tools.h"
#include <Xm/Protocols.h>
#include <Xm/Separator.h>
#include <Xm/SelectioB.h>

/* same limits as the sginet CLI registration form */
#define REG_USER_MIN 3
#define REG_USER_MAX 31
#define REG_NAME_MIN 2
#define REG_NAME_MAX 62

#define REG_MODEL_OTHER "- other, not in list -"

#define REG_INFO_TEXT \
    "Your SGI system has not yet been registered.\n\n" \
    "When you register, HINV data of your system and username/alias will\n" \
    "appear publicly visible on sginet.flexion.io\n\n" \
    "If you do not agree, press Cancel."

static Widget reg_shell = NULL;
static Widget txt_user = NULL;
static Widget txt_machine = NULL;
static ModelName *lstModelNames = NULL;
static int numModelNames = 0;
static int selectedModel = -1; /* list index, numModelNames is "other", -1 none */
static char ipmodel[8];
static int cpuCount = 0;
static void (*on_registered)(void) = NULL;

static void reg_destroy_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    if (lstModelNames) free(lstModelNames);
    lstModelNames = NULL;
    numModelNames = 0;
    selectedModel = -1;
    reg_shell = NULL;
    txt_user = NULL;
    txt_machine = NULL;
}
static void reg_cancel_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Aborted\n");
    exit(EXIT_FAILURE);
}
static void reg_dialog_close_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    XtDestroyWidget(XtParent(w)); /* the dialog shell */
}
static void reg_error(const char *msg) {
    Widget dialog, button;
    XmString message, title;

    message = XmStringCreateLtoR((char *)msg, XmFONTLIST_DEFAULT_TAG);
    title = XmStringCreateLocalized("SGINet Registration");
    dialog = XmCreateErrorDialog(reg_shell, "reg_error_dialog", NULL, 0);
    XtVaSetValues(dialog,
                  XmNmessageString, message,
                  XmNdialogTitle, title,
                  NULL);
    XmStringFree(message);
    XmStringFree(title);

    button = XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON);
    if (button) XtUnmanageChild(button);
    button = XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
    if (button) XtUnmanageChild(button);
    XtAddCallback(dialog, XmNokCallback, reg_dialog_close_cb, NULL);

    XtManageChild(dialog);
}
/* trimmed copy of a text field's value */
static void get_field(Widget field, char *buf, size_t size) {
    char *value;
    value = XmTextFieldGetString(field);
    snprintf(buf, size, "%s", trim_whitespaces(value));
    XtFree(value);
}
static void model_select_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;
    selectedModel = cbs->item_position - 1;
}
/* trimmed username and machine name, 0 (with an error shown) if invalid */
static int get_user_fields(char *usr, size_t usr_size, char *machine, size_t machine_size) {
    get_field(txt_user, usr, usr_size);
    get_field(txt_machine, machine, machine_size);
    if (strlen(usr) < REG_USER_MIN) {
        reg_error("Please enter a username of at least 3 characters.");
        return 0;
    }
    if (strlen(machine) < REG_NAME_MIN) {
        reg_error("Please enter a name for this system.");
        return 0;
    }
    return 1;
}
static void do_register(char *usr, char *machine, char *modelname) {
    void (*done)(void);
    int rc;

    if (debug) printf("[xsginet] registering '%s' '%s' %s '%s' (%d CPUs)\n", usr, machine, ipmodel, modelname, cpuCount);

    busy_begin(reg_shell);
    rc = cloudAPI_register(usr, machine, ipmodel, cpuCount, modelname);
    busy_end(reg_shell);
    if (rc != RESPONSE_OK) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Online registration failed. (%s)", G_CLIENTID);
        reg_error(msg);
        return;
    }
    printf("Your SGI machine has been successfully registered on sginet.flexion.io\n");

    done = on_registered;
    XtDestroyWidget(reg_shell);
    if (done) done();
}
/* second step for "other, not in list": OK of the model name prompt */
static void other_model_ok_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    XmSelectionBoxCallbackStruct *cbs = (XmSelectionBoxCallbackStruct *)call_data;
    char usr[REG_USER_MAX + 1];
    char machine[REG_NAME_MAX + 1];
    char modelname[REG_NAME_MAX + 1];
    char *value;

    value = NULL;
    modelname[0] = '\0';
    if (XmStringGetLtoR(cbs->value, XmFONTLIST_DEFAULT_TAG, &value) && value != NULL) {
        snprintf(modelname, sizeof(modelname), "%s", trim_whitespaces(value));
        XtFree(value);
    }
    XtDestroyWidget(XtParent(w)); /* the prompt's dialog shell */

    if (strlen(modelname) < REG_NAME_MIN) {
        reg_error("Please enter the model name of this system.");
        return;
    }
    if (!get_user_fields(usr, sizeof(usr), machine, sizeof(machine))) return;
    do_register(usr, machine, modelname);
}
static void ask_other_model(void) {
    Widget dialog, child;
    XmString str, title, ok_label;

    str = XmStringCreateLtoR("Enter the model name of your\nSiliconGraphics machine:", XmFONTLIST_DEFAULT_TAG);
    title = XmStringCreateLocalized("SGINet Registration");
    ok_label = XmStringCreateLocalized("Register");
    dialog = XmCreatePromptDialog(reg_shell, "other_model_dialog", NULL, 0);
    XtVaSetValues(dialog,
                  XmNselectionLabelString, str,
                  XmNdialogTitle, title,
                  XmNokLabelString, ok_label,
                  XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL,
                  NULL);
    XmStringFree(str);
    XmStringFree(title);
    XmStringFree(ok_label);

    child = XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT);
    if (child) XtVaSetValues(child, XmNmaxLength, REG_NAME_MAX, NULL);
    child = XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
    if (child) XtUnmanageChild(child);
    XtAddCallback(dialog, XmNokCallback, other_model_ok_cb, NULL);
    XtAddCallback(dialog, XmNcancelCallback, reg_dialog_close_cb, NULL);

    XtManageChild(dialog);
}
static void reg_submit_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    char usr[REG_USER_MAX + 1];
    char machine[REG_NAME_MAX + 1];
    char modelname[REG_NAME_MAX + 1];

    if (!get_user_fields(usr, sizeof(usr), machine, sizeof(machine))) return;
    if (selectedModel < 0 || selectedModel > numModelNames) {
        reg_error("Please select the model of this system from the list.");
        return;
    }
    if (selectedModel == numModelNames) {
        ask_other_model();
        return;
    }
    snprintf(modelname, sizeof(modelname), "%.*s", REG_NAME_MAX, lstModelNames[selectedModel].name);
    do_register(usr, machine, modelname);
}
/* "Label: [text field]" row below prev */
static Widget add_field_row(Widget form, Widget prev, char *name, char *label, int maxLength) {
    Widget field;
    XmString str;

    field = XtVaCreateManagedWidget(name, xmTextFieldWidgetClass, form,
        XmNmaxLength, maxLength,
        XmNcolumns, 32,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, prev,
        XmNtopOffset, 6,
        XmNleftAttachment, XmATTACH_POSITION,
        XmNleftPosition, 35,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    str = XmStringCreateLocalized(label);
    XtVaCreateManagedWidget("lbl", xmLabelWidgetClass, form,
        XmNlabelString, str,
        XmNalignment, XmALIGNMENT_END,
        XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
        XmNtopWidget, field,
        XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
        XmNbottomWidget, field,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_WIDGET,
        XmNrightWidget, field,
        XmNrightOffset, 6,
        NULL);
    XmStringFree(str);
    return field;
}
/*
 * Registration window for an unregistered machine, replaces the sginet CLI
 * registration screens. Calls registered() once the cloud accepted the
 * registration, Cancel or closing the window quits xsginet.
 */
void register_show(Widget parent, void (*registered)(void)) {
    Widget form, lbl_info, model_list, model_sw, sep, btn_register, btn_cancel;
    XmString str;
    Atom wm_delete;
    Arg args[3];
    char hostname[128];
    char buf[64];
    int i, rc;

    on_registered = registered;

    getIPModel(ipmodel);
    cpuCount = getCPUCount();

    /* list of possible model names for the IPxx type of this machine */
    lstModelNames = NULL;
    numModelNames = 0;
    rc = cloudAPI_getIPModelNames(&lstModelNames, &numModelNames, ipmodel);
    if (rc != RESPONSE_OK) {
        if (debug) printf("[xsginet] Failed to query model names for: %s\n", ipmodel);
        lstModelNames = NULL;
        numModelNames = 0;
    }

    reg_shell = XtVaCreatePopupShell("Register",
        topLevelShellWidgetClass, parent,
        XmNtitle, "SGINet Registration",
        XmNdeleteResponse, XmDO_NOTHING,
        NULL);
    XtAddCallback(reg_shell, XmNdestroyCallback, reg_destroy_callback, NULL);
    wm_delete = XmInternAtom(XtDisplay(reg_shell), "WM_DELETE_WINDOW", False);
    XmAddWMProtocolCallback(reg_shell, wm_delete, reg_cancel_cb, NULL);

    form = XtVaCreateManagedWidget("reg_form", xmFormWidgetClass, reg_shell,
        XmNmarginWidth, 12,
        XmNmarginHeight, 12,
        XmNfractionBase, 100,
        NULL);

    str = XmStringCreateLtoR(REG_INFO_TEXT, XmFONTLIST_DEFAULT_TAG);
    lbl_info = XtVaCreateManagedWidget("lbl_info", xmLabelWidgetClass, form,
        XmNlabelString, str,
        XmNalignment, XmALIGNMENT_BEGINNING,
        XmNtopAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);
    XmStringFree(str);

    txt_user = add_field_row(form, lbl_info, "txtUser", "Your username:", REG_USER_MAX);
    XtVaSetValues(txt_user, XmNtopOffset, 16, NULL);

    txt_machine = add_field_row(form, txt_user, "txtMachine", "Name of this system:", REG_NAME_MAX);
    hostname[0] = '\0';
    getHostname(hostname, sizeof(hostname));
    hostname[REG_NAME_MAX] = '\0';
    XmTextFieldSetString(txt_machine, hostname);

    /* known models for this IPxx plus "other, not in list" */
    XtSetArg(args[0], XmNvisibleItemCount, 6);
    XtSetArg(args[1], XmNselectionPolicy, XmBROWSE_SELECT);
    XtSetArg(args[2], XmNscrollBarDisplayPolicy, XmSTATIC);
    model_list = XmCreateScrolledList(form, "model_list", args, 3);
    for (i = 0; i < numModelNames; ++i) {
        str = XmStringCreateLocalized(lstModelNames[i].name);
        XmListAddItem(model_list, str, 0);
        XmStringFree(str);
    }
    str = XmStringCreateLocalized(REG_MODEL_OTHER);
    XmListAddItem(model_list, str, 0);
    XmStringFree(str);
    XtAddCallback(model_list, XmNbrowseSelectionCallback, model_select_cb, NULL);
    XtManageChild(model_list);
    model_sw = XtParent(model_list);
    XtVaSetValues(model_sw,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, txt_machine,
        XmNtopOffset, 6,
        XmNleftAttachment, XmATTACH_POSITION,
        XmNleftPosition, 35,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    snprintf(buf, sizeof(buf), "Model (%s, %d CPU%s):", ipmodel, cpuCount, cpuCount == 1 ? "" : "s");
    str = XmStringCreateLocalized(buf);
    XtVaCreateManagedWidget("lbl_models", xmLabelWidgetClass, form,
        XmNlabelString, str,
        XmNalignment, XmALIGNMENT_END,
        XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
        XmNtopWidget, model_sw,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_WIDGET,
        XmNrightWidget, model_sw,
        XmNrightOffset, 6,
        NULL);
    XmStringFree(str);

    sep = XtVaCreateManagedWidget("sep", xmSeparatorWidgetClass, form,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, model_sw,
        XmNtopOffset, 12,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    str = XmStringCreateLocalized("Register");
    btn_register = XtVaCreateManagedWidget("btnRegister", xmPushButtonWidgetClass, form,
        XmNlabelString, str,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, sep,
        XmNtopOffset, 8,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_POSITION,
        XmNleftPosition, 15,
        XmNrightAttachment, XmATTACH_POSITION,
        XmNrightPosition, 45,
        NULL);
    XmStringFree(str);
    XtAddCallback(btn_register, XmNactivateCallback, reg_submit_cb, NULL);

    str = XmStringCreateLocalized("Cancel");
    btn_cancel = XtVaCreateManagedWidget("btnCancel", xmPushButtonWidgetClass, form,
        XmNlabelString, str,
        XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
        XmNtopWidget, btn_register,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_POSITION,
        XmNleftPosition, 55,
        XmNrightAttachment, XmATTACH_POSITION,
        XmNrightPosition, 85,
        NULL);
    XmStringFree(str);
    XtAddCallback(btn_cancel, XmNactivateCallback, reg_cancel_cb, NULL);

    /* nothing to choose from if the model names query failed */
    if (numModelNames == 0) XmListSelectPos(model_list, 1, True);

    /* ENTER in a text field submits the form */
    XtVaSetValues(form,
        XmNdefaultButton, btn_register,
        XmNcancelButton, btn_cancel,
        XmNinitialFocus, txt_user,
        NULL);

    splash_hide();
    XtPopup(reg_shell, XtGrabNone);
}
