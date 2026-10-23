
#include "include/systemadvisor.h"
#include "include/str_tools.h"
#include <stdlib.h>

char *lstDaemonsOff[] = {
    "sgi_apache ",
    "webface_apache ",
    "appletalk ",
    "timed ",
    "tfxd ",
    "timeslave ",
    "esp ",
    "ipaliases ",
    "ypmaster ",
    "yp ",
    "sendmail ",
    "sendmail_cf ",
    "webface ",
    "named ",
    "rsvpd ",
    "privileges "
};

int startsWith(const char *pre, const char *str) {
    size_t lenpre = strlen(pre), lenstr = strlen(str);
    return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}
AdvisorEntry *addAdvisory(AdvisoryList *lstAdv, int sev, int atype, const char *msg) {
    AdvisorEntry *adv;
    if (lstAdv->cnt == lstAdv->capacity) { /* grow the reserved memory */
        lstAdv->capacity *= 2;
        lstAdv->lst = realloc( lstAdv->lst, sizeof(AdvisorEntry) * lstAdv->capacity);
    }
    adv = (lstAdv->lst + lstAdv->cnt);
    adv->sev = sev;
    adv->atype = atype;
    adv->msg = msg;
    lstAdv->cnt++;
    return adv;
}
int checkRootHomedir(AdvisoryList *lstAdv) {
    struct passwd *pwentry = getpwuid(0);
    if (NULL == pwentry) {
        addAdvisory(lstAdv, SEV_INFO, ADVTYPE_ROOT, "Unable to check home directory of root user");
        return 1;
    } else {
	if (strcmp(pwentry->pw_dir, "/root") == 0) {
            addAdvisory(lstAdv, SEV_OK, ADVTYPE_ROOT, "Home directory of root user is /root");
            return 0;
	}
        if (strcmp(pwentry->pw_dir, "/") == 0) {
            addAdvisory(lstAdv, SEV_HIGH, ADVTYPE_ROOT, "Home directory of root user should be changed from / to /root");
            return 0;
        }

        addAdvisory(lstAdv, SEV_WARN, ADVTYPE_ROOT, "Home directory of root user is not /root");
        return 0;
        
    }
    return 0;
}
int checkRootUser(AdvisoryList *lstAdv) {
    if (geteuid() == 0) {
        addAdvisory(lstAdv, SEV_WARN, ADVTYPE_ROOT, "It is not recommended to be logged in as root user.");
        return 1;
    }
    return 0;
}
int checkServices(AdvisoryList *lstAdv) {
    char line[100];
    int l_svc = sizeof(lstDaemonsOff) / sizeof(lstDaemonsOff[0]);
    int i, iWarn;
    char *cmd = "/sbin/chkconfig | /sbin/grep \" on\"";
    FILE *pipe;
    pipe = popen(cmd, "r");
    if (NULL == pipe) return 1;
    iWarn = 0;
    while (fgets(line, sizeof(line), pipe) != NULL) {
        char thisSVC[100];
        sprintf(thisSVC, "%s", trim_whitespaces(line));
        for (i = 0; i < l_svc; i++) {
            if (startsWith(lstDaemonsOff[i], thisSVC)) {
                /* if (iWarn == 0) addAdvisory(lstAdv, SEV_WARN, "Daemon", "-- Consider turning off the following daemons: --"); */
                char label[256];
                sprintf(label, "Consider turning off daemon: %s", thisSVC);
                addAdvisory(lstAdv, SEV_WARN, ADVTYPE_DAEMON, strdup(label));
                iWarn++;
            }
        }
    }
    if (iWarn < 1) addAdvisory(lstAdv, SEV_OK, ADVTYPE_DAEMON, "No unnecessary daemons found");
    pclose(pipe);
    return 0;  /* OK */
}
int checkX11TrueColor(AdvisoryList *lstAdv) {
    char line[2048];
    /* Unless the graphics in the system are 8-bit (such as an Indy with an XL/8 card) */
    /* enabling Truecolor will improve the user experience. */
    /* Edit /var/X11/xdm/Xservers to be :0 secure /usr/bin/X11/X -bs -nobitscale -c -class TrueColor -depth 24 -solidroot sgilightblue -cursorFG red -cursorBG white */
    char *cmd = "cat /var/X11/xdm/Xservers | /sbin/grep \"pseudomap 4sight\"";
    FILE *pipe;
    pipe = popen(cmd, "r");
    if (NULL == pipe) return 1;
    while (fgets(line, sizeof(line), pipe) != NULL) {
        /* make sure line is not disabled (starts with #) */
        if (startsWith("#", line)) {
            /* if (debug) fprintf(stderr, "X11: disabled line found: %s\n", line); */
        } else {
            addAdvisory(lstAdv, SEV_WARN, ADVTYPE_TRUECOLOR, "Looks like your desktop is not set to TrueColor mode");
        }
    }
    pclose(pipe);
    return 0;  /* OK */
}
int testAddEntries(AdvisoryList *lstAdv) {
    int i;
    int l_svc;
    addAdvisory(lstAdv, SEV_INFO, ADVTYPE_INFO, "Testing advisory entries");
    addAdvisory(lstAdv, SEV_OK, ADVTYPE_INFO, "This is OK");
    
    l_svc = sizeof(lstDaemonsOff) / sizeof(lstDaemonsOff[0]);
    for (i = 0; i < l_svc; i++) {
        char label[256];
        sprintf(label, "Consider turning off daemon: %s", lstDaemonsOff[i]);
        addAdvisory(lstAdv, SEV_WARN, ADVTYPE_DAEMON, strdup(label));
    }
    addAdvisory(lstAdv, SEV_OK, ADVTYPE_INFO, "Report completed");
    return 0;
}
int startSystemAdvisor(AdvisoryList *lstAdv) {
    checkRootUser(lstAdv);
    checkRootHomedir(lstAdv);
    #if defined(__sgi)
        checkX11TrueColor(lstAdv);
        checkServices(lstAdv);
    #else 
        testAddEntries(lstAdv);
    #endif
    if (lstAdv->cnt < 1) addAdvisory(lstAdv, SEV_OK, ADVTYPE_INFO, "System advisor couldn't find any problems.");
    return 0;
}
