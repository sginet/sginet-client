/* Volume control bar for the analog output, shown below the systems list */
#include "../include/cloudAPI.h"
#include "../include/x11-client.h"

#if defined(__sgi)
#include <audio.h>
#endif

static Widget volume_bar = NULL;
static Widget scl_volume = NULL;
static Widget tgl_mute = NULL;

/* ---- platform layer: volume in percent (0-100), 0 = ok ---- */
#if defined(__sgi)
/* classic libaudio API: available on IRIX 5.3 and still supported on IRIX 6.5 */
static int audio_initialized = 0;
static int hw_mute = 0;       /* AL_SPEAKER_MUTE_CTL supported */
static int soft_muted = 0;    /* fallback: gain set to 0, previous gain saved */
static int soft_saved = 0;

static int gain_to_percent(long gain) {
    if (gain < 0) gain = 0;
    if (gain > 255) gain = 255;
    return (int)((gain * 100 + 127) / 255);
}
static long percent_to_gain(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return ((long)percent * 255 + 50) / 100;
}
static void audio_init(void) {
#ifdef AL_SPEAKER_MUTE_CTL
    long pv[2];
#endif
    if (audio_initialized) return;
    audio_initialized = 1;
    ALseterrorhandler(0); /* no libaudio messages on stderr */
#ifdef AL_SPEAKER_MUTE_CTL
    pv[0] = AL_SPEAKER_MUTE_CTL;
    pv[1] = 0;
    hw_mute = (ALgetparams(AL_DEFAULT_DEVICE, pv, 2) >= 0);
#endif
}
static int audio_set_gain(long gain) {
    long pv[4];
    pv[0] = AL_LEFT_SPEAKER_GAIN;
    pv[1] = gain;
    pv[2] = AL_RIGHT_SPEAKER_GAIN;
    pv[3] = gain;
    return ALsetparams(AL_DEFAULT_DEVICE, pv, 4) < 0 ? -1 : 0;
}
static int audio_get(int *percent, int *muted) {
    long pv[4];
    audio_init();
    pv[0] = AL_LEFT_SPEAKER_GAIN;
    pv[1] = 0;
    pv[2] = AL_RIGHT_SPEAKER_GAIN;
    pv[3] = 0;
    if (ALgetparams(AL_DEFAULT_DEVICE, pv, 4) < 0) return -1;
    *percent = gain_to_percent(pv[1] > pv[3] ? pv[1] : pv[3]);
    *muted = 0;
#ifdef AL_SPEAKER_MUTE_CTL
    if (hw_mute) {
        pv[0] = AL_SPEAKER_MUTE_CTL;
        pv[1] = 0;
        if (ALgetparams(AL_DEFAULT_DEVICE, pv, 2) >= 0)
            *muted = (pv[1] == AL_SPEAKER_MUTE_ON);
        return 0;
    }
#endif
    if (soft_muted) {
        if (*percent == 0) {
            *percent = soft_saved;
            *muted = 1;
        } else {
            soft_muted = 0; /* gain changed elsewhere (apanel): no longer muted */
        }
    }
    return 0;
}
static int audio_set_volume(int percent) {
    audio_init();
    return audio_set_gain(percent_to_gain(percent));
}
static int audio_set_mute(int muted, int percent) {
    audio_init();
#ifdef AL_SPEAKER_MUTE_CTL
    if (hw_mute) {
        long pv[2];
        pv[0] = AL_SPEAKER_MUTE_CTL;
        pv[1] = muted ? AL_SPEAKER_MUTE_ON : AL_SPEAKER_MUTE_OFF;
        return ALsetparams(AL_DEFAULT_DEVICE, pv, 2) < 0 ? -1 : 0;
    }
#endif
    if (muted) {
        soft_saved = percent;
        soft_muted = 1;
        return audio_set_gain(0);
    }
    soft_muted = 0;
    return audio_set_gain(percent_to_gain(percent));
}
#else
/* Linux (development only): no audio device, keep the state for the UI */
static int stub_percent = 50;
static int stub_muted = 0;

static int audio_get(int *percent, int *muted) {
    *percent = stub_percent;
    *muted = stub_muted;
    return 0;
}
static int audio_set_volume(int percent) {
    stub_percent = percent;
    if (debug) printf("[volume] set volume %d%%\n", percent);
    return 0;
}
static int audio_set_mute(int muted, int percent) {
    stub_muted = muted;
    stub_percent = percent;
    if (debug) printf("[volume] mute %s\n", muted ? "on" : "off");
    return 0;
}
#endif

/* ---- widgets ---- */
static void volume_changed_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmScaleCallbackStruct *cbs = (XmScaleCallbackStruct *)call_data;
    audio_set_volume(cbs->value);
}
static void mute_changed_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct *)call_data;
    int percent = 0;
    XmScaleGetValue(scl_volume, &percent);
    audio_set_mute(cbs->set ? 1 : 0, percent);
    XtSetSensitive(scl_volume, cbs->set ? False : True);
}
/* read the current hardware state, it may have been changed by apanel */
static void volume_sync(void) {
    int percent = 0, muted = 0;
    if (audio_get(&percent, &muted) != 0) {
        XtSetSensitive(scl_volume, False);
        XtSetSensitive(tgl_mute, False);
        return;
    }
    XmScaleSetValue(scl_volume, percent);
    XmToggleButtonSetState(tgl_mute, muted ? True : False, False);
    XtSetSensitive(tgl_mute, True);
    XtSetSensitive(scl_volume, muted ? False : True);
}
Widget volume_create(Widget main_win) {
    Widget lbl;
    XmString str;

    volume_bar = XtVaCreateWidget("volume_bar", xmFormWidgetClass, main_win, NULL);

    str = XmStringCreateLocalized("Volume:");
    lbl = XtVaCreateManagedWidget(
        "lbl_volume", xmLabelWidgetClass, volume_bar,
        XmNlabelString, str,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNleftOffset, 4,
        NULL);
    XmStringFree(str);

    str = XmStringCreateLocalized("Mute");
    tgl_mute = XtVaCreateManagedWidget(
        "tgl_mute", xmToggleButtonWidgetClass, volume_bar,
        XmNlabelString, str,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNrightOffset, 4,
        NULL);
    XmStringFree(str);
    XtAddCallback(tgl_mute, XmNvalueChangedCallback, mute_changed_callback, NULL);

    scl_volume = XtVaCreateManagedWidget(
        "scl_volume", xmScaleWidgetClass, volume_bar,
        XmNorientation, XmHORIZONTAL,
        XmNminimum, 0,
        XmNmaximum, 100,
        XmNvalue, 50,
        XmNshowValue, True,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNbottomOffset, 2,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNleftWidget, lbl,
        XmNleftOffset, 4,
        XmNrightAttachment, XmATTACH_WIDGET,
        XmNrightWidget, tgl_mute,
        XmNrightOffset, 8,
        NULL);
    XtAddCallback(scl_volume, XmNvalueChangedCallback, volume_changed_callback, NULL);
    XtAddCallback(scl_volume, XmNdragCallback, volume_changed_callback, NULL);

    /* the main window places its message window below the work area */
    XtVaSetValues(main_win, XmNmessageWindow, volume_bar, NULL);
    return volume_bar;
}
/* File > Show volume control */
void volume_toggle_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct *)call_data;
    if (volume_bar == NULL) return;
    if (cbs->set) {
        volume_sync();
        XtManageChild(volume_bar);
    } else {
        XtUnmanageChild(volume_bar);
    }
}
