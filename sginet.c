#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/sginet.h"
#include "include/cloudAPI.h"
#include "include/fileutils.h"
#include "include/hwinfo_sgi.h"
#include "include/nc_mainmenu.h"
#include "include/nc_listsystems.h"
#include "include/nc_register.h"
#include "include/nc_softwaresearch.h"
#include "include/nc_modelselection.h"
#include "include/nc_showmessage.h"
#include "include/nc_systemadvisor.h"
#include "include/nc_screenshot.h"
#include "include/nc_sysload.h"
#include "include/nc_shoutbox.h"
#include "include/nc_sgug_forum_news.h"
#include "include/nc_tools.h"
#include "include/webserver.h"

#define THIS_CLIENT_BIN "/usr/bin/sginet" /* for online updater, --sginet-bin overrides */

static int run = 1;
int debug = 0;
int G_MODE = MODE_REGISTERED;
int TERM_MODE = TERM_ATERM;
int GUI_MODE = 0; /* set to 1 as soon as ncurses is involved, for cleanup on quit. */
char G_CLIENTID[64];
char G_NEWNAME[65];

void onQuit() {
    if (debug) fprintf(stderr, "[SGINet-client] Shutting down..\n");
    if (GUI_MODE == 1) {
        endwin(); /* destroy ncurses window */
    }
    run = 0;
    exit(0);
}
void handle_signal(int s) {
    if (debug) fprintf(stderr, "[SGINet-client] SIGNAL %i received!\n", s);
    if (s == 15 && G_MODE == MODE_DAEMON) {
        /* send shutdown event to cloud (disabled for now) */
        /* if (debug) fprintf(stderr, "Sending shutdown signal to cloud..\n"); */
        /* cloudAPI_request(REQTYPE_SHUTDOWN); */
    }
    onQuit();
}
void cliSetMyPosition(float posLat, float posLng) {
    char status_msg[512];
    cloudAPI_saveMyPosition(status_msg, posLat, posLng);
    printf("-> %s\n", status_msg);
}
void printBanner() {
    printf("\n");
    printf("                  \\ _____________________ __\n");
    printf("            ____.  \\       .____         /\n");
    printf("            \\_  |     ___. |  _/      _ /\n");
    printf("    _        T  |     T  | |  |__   __\\              _\n");
    printf(" _  \\________:__|_____|_ | |  |\\/_ /   \\ ____  ______/\n");
    printf(" \\ \\\\\\__  _____/|  ____/ | !  |\" T/  \\  \\T   \\ |  __/// /\n");
    printf(" ------|  __/|  |  __/_\\_\\ / /|  |  / \\  \\    \\|  |------\n");
    printf(" ------|  |  |  |_ Z/  _/ X  \\|  |  \\ /  /  \\  !  |------\n");
    printf("    \\dS!  |  |___/_____T_/ \\  !__|\\  \\  /|__Z\\    !NE7\n");
    printf("       |  |  :            _/___/ / \\___/      \\   |\n");
    printf("       .  |               Z     /              \\__|\n");
    printf("          !                  \\ /\n");
    printf("                              \\\n");
    printf("\n");
}
void error_usage() {
    printBanner();
    fprintf(stderr, "SGINet client | flexion 2026 | Version: %s\n\n", THIS_CLIENT_VERSION);
    /* fprintf(stderr, "Options:\n"); */
    fprintf(stderr, "  -s  run as daemon\n");
    fprintf(stderr, "  -i  show SYSID\n");
    fprintf(stderr, "  -r  print version\n");
    fprintf(stderr, "  -p  ping cloud API to report online status and exit\n");
    fprintf(stderr, "  -x  mark machine as offline\n");
    fprintf(stderr, "  -w  start webinterface\n");
    fprintf(stderr, "  -f  force HINV update\n");
/*    fprintf(stderr, "  -b  disable ncurses borders (for older terminals other than aterm/mrxvt)\n"); */
    fprintf(stderr, "  -u  check for new version and upgrade sginet\n");
    fprintf(stderr, "  -c  take a screenshot in 5 sec and upload to sginet.flexion.io\n");
    fprintf(stderr, "  -C  take a screenshot NOW and upload to sginet.flexion.io\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  --renamesystem NAME     rename this system on sginet.flexion.io\n");
    fprintf(stderr, "  --lat x.xxxx --lng x.xxxx  set your position on the map to latitude/longitude\n");
    if (debug) { /* options used when bundled with mogrix */
        fprintf(stderr, "  --ca-cert-bundle PATH   CA certificate bundle\n");
        fprintf(stderr, "  --sginet-bin PATH       sginet binary for updater\n");
    }
    fprintf(stderr, "\n");
    /* fprintf(stderr, "  -d  debug\n"); */
    exit(EXIT_FAILURE);
}
int argParser(int argc, char* argv[]) {
    int i;
    int delaySec;
    float setLat = 0.0f;
    float setLng = 0.0f;
    for (i = 1; i < argc; i += 1) {
        /* double-dash options */
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            if (strcmp(argv[i], "--renamesystem") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: --renamesystem requires a name argument\n");
                    error_usage();
                }
                i++;
                strncpy(G_NEWNAME, argv[i], sizeof(G_NEWNAME) - 1);
                G_NEWNAME[sizeof(G_NEWNAME) - 1] = '\0';
                G_MODE = MODE_RENAMESYSTEM;
            } else if (strcmp(argv[i], "--ca-cert-bundle") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: --ca-cert-bundle requires a path argument\n");
                    error_usage();
                }
                i++;
                cloudAPI_SetCACertBundle(argv[i]);
            } else if (strcmp(argv[i], "--sginet-bin") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: --sginet-bin requires a path argument\n");
                    error_usage();
                }
                i++;
                cloudAPI_SetSGINetBin(argv[i]);

            } else if (strcmp(argv[i], "--lat") == 0 && i + 1 < argc) {
                setLat = atof(argv[i + 1]);
                if (setLat != 0.0F && setLng != 0.0f) {
                    printf("Updating your position to: %f %f\n", setLat, setLng);
                    cliSetMyPosition(setLat, setLng);
                    exit(EXIT_SUCCESS);
                }
                i++; /* skip the next argument since it is the latitude value */
            } else if (strcmp(argv[i], "--lng") == 0 && i + 1 < argc) {
                setLng = atof(argv[i + 1]);
                if (setLat != 0.0F && setLng != 0.0f) {
                    printf("Updating your position to: %f %f\n", setLat, setLng);
                    cliSetMyPosition(setLat, setLng);
                    exit(EXIT_SUCCESS);
                }
                i++; /* skip the next argument since it is the latitude value */
            } else {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                error_usage();
            }
            continue;
        }
        /* single-dash options */
        if (argv[i][1] == 's') {
            G_MODE = MODE_DAEMON;
        } else if (argv[i][1] == 'p') {
            if (debug) fprintf(stderr, "PING MODE\n");
            G_MODE = MODE_DAEMON_PINGONLY;
        } else if (argv[i][1] == 'h') {
            error_usage();
        } else if (argv[i][1] == 'd') {
            debug = 1;
        } else if (argv[i][1] == 'w') {
            G_MODE = MODE_WEBSRV;
        } else if (argv[i][1] == 'f') {
            G_MODE = MODE_FORCE_HINV_UPDATE;
        } else if (argv[i][1] == 'x') {
            G_MODE = MODE_SHUTDOWN;
        } else if (argv[i][1] == 'b') {
            TERM_MODE = 0;
        } else if (argv[i][1] == 'u') {
            G_MODE = MODE_SWUPDATE;
        } else if (argv[i][1] == 'i') {
            printf("SYS_ID: %s\n", G_CLIENTID);
            exit(EXIT_SUCCESS);
        } else if (argv[i][1] == 'r') {
            printf("%s\n", THIS_CLIENT_VERSION); /* IMPORTANT: print version only! output used in 'upload-release' tool */
            exit(EXIT_SUCCESS);
        } else if (argv[i][1] == 'c') {
            #if defined(__sgi)
                delaySec = 6;
                while (delaySec>0) {
                    printf("\rScreenshot in %i seconds..", delaySec);
                    fflush(stdout);
                    sleep(1);
                    delaySec -=1;
                }
                printf("\rTaking screenshot NOW!    \a"); /* \a is for bell/beep */
                if (takeScreenshotAndUpload(0, 0, 0, 0, 0) == 0) {
                    printf("\nYour screenshot is now available for download on sginet.flexion.io\nSystem list > open your machine > click 'View Screenshots'\n");
                } else {
                    printf("There was an error.\n");
                }
            #else
                printf("Screenshot function is not available on this platform\n");
            #endif
            exit(EXIT_SUCCESS);
        } else if (argv[i][1] == 'C') {
            #if defined(__sgi)
                if (takeScreenshotAndUpload(0, 0, 0, 0, 0) == 0) {
                    printf("Your screenshot is now available for download on sginet.flexion.io\nSystem list > open your machine > click 'View Screenshots'\n");
                } else {
                    printf("There was an error.\n");
                }
            #else
                printf("Screenshot function is not available on this platform\n");
            #endif
            exit(EXIT_SUCCESS);
        /* else if (argv[i][1] == 'n') { steps = atoi(argv[i + 1]); } */
        /* else if (argv[i][1] == 'i') { prompt = argv[i + 1]; } */
        } else {
            error_usage();
        }
    }
    return 0;
}
void openMainMenu() {
    int stop = 0;
    int selectedMenu;
    while (stop != 1) {
        nc_mainmenu(&selectedMenu);
        /* printf("selectedMenu: %i\n", selectedMenu); */
        switch (selectedMenu) {
            case 0:
                nc_listsystems(REQTYPE_SHOWONLINESYSTEMS);
                break;
            case 1:
                nc_listsystems(REQTYPE_SHOWALLSYSTEMS);
                break;
            case 2:
                nc_softwaresearch();
                break;
            case 3:
                nc_screenshot();
                break;
            case 4:
                nc_sysload();
                break;
            case 5:
                nc_shoutbox();
                break;
            case 6:
                nc_sgug_forum_news();
                break;
            case 7:
                nc_systemadvisor();
                break;
            case 8:
                checkUpdatesGUI();
                break;
            default:
                stop = 1;  /* stop main menu loop and return to main() to quit app */
                break;
        }
    }
}
void checkUpdatesGUI() {
    /* launched from main menu in ncurses. checks for upgrades only (no download) */
    char* infoTitle = "SGINet software updater";
    int hasUpdates;
    int uRC = cloudAPI_onlineUpdater(geteuid(), 0, &hasUpdates, 0);
    if (uRC != RESPONSE_OK) {
        printf("Online updater failed\n");
        nc_showMessage(infoTitle, "Online update check failed. Try again later.\n");
        return;
    }
    if (hasUpdates == 1) {
        nc_showMessage(infoTitle, "Update available!\n\nTo upgrade sginet to the latest version, quit and run 'sginet -u' as root!\n");
    } else {
        nc_showMessage(infoTitle, "No update available.\nYou're already running the latest version.\n");
    }
}
static void init_daemon() {
    int x;
    pid_t pid;
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); /* terminates this process but forked daemon will stay alive */
    if (setsid() < 0) exit(EXIT_FAILURE); /* daemon process is the leader now */

/*    signal(SIGCHLD, handle_signal); */
/*    signal(SIGHUP, handle_signal); */

    pid = fork(); /* fork again */
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
}
int main(int argc, char* argv[]) {
    int rc;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* Read SYSID */
    getSYSID(G_CLIENTID);
    /* Parse command line options. This can override G_MODE */
    argParser(argc, argv);
    /* init API after reading args */
    cloudAPI_Init(THIS_CLIENT_TYPE, THIS_CLIENT_VERSION, THIS_CLIENT_BIN);

    if (debug) fprintf(stderr, "G_CLIENTID: %s\n", G_CLIENTID);
    if (G_MODE == MODE_DAEMON_PINGONLY) {
        /* Just ping. assume machine has already been registered. */
        rc = cloudAPI_request(REQTYPE_PING);
        if (rc != RESPONSE_OK) {
            printf("Error: Could not connect to SGINet backend\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (G_MODE == MODE_DAEMON) {
        /* init.d daemon mode, send ping every 12 minutes */
        /* daemonize in a forked task */
        init_daemon();
        while (1) {
            rc = cloudAPI_request(REQTYPE_PING);
            if (rc != RESPONSE_OK) {
                printf("Error: Could not connect to SGINet backend\n");
            }
            sleep(720); /* 12 minutes */
        }
        return EXIT_SUCCESS;
    }
    if (G_MODE == MODE_WEBSRV) {
        /* start webserver */
        startWebserver();        
        return EXIT_SUCCESS;
    }
    if (G_MODE == MODE_SHUTDOWN) {
        /* Mark system as offline */
        rc = cloudAPI_request(REQTYPE_SHUTDOWN);
        if (rc != RESPONSE_OK) {
            printf("Error: Could not connect to SGINet backend\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (G_MODE == MODE_FORCE_HINV_UPDATE) {
        /* -f command line option to force updating HINV */
        rc = cloudAPI_request(REQTYPE_SENDHINV);
        if (rc != RESPONSE_OK) {
            printf("Error: Could not connect to SGINet backend\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (G_MODE == MODE_SWUPDATE) {
        /* -u command line option to check and upgrade sginet binary */
        /* must be root! */

        int hasUpdates;
        rc = cloudAPI_onlineUpdater(geteuid(), 0, &hasUpdates, 1);
        if (rc != RESPONSE_OK) {
            printf("Online updater failed\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (G_MODE == MODE_RENAMESYSTEM) {
        rc = cloudAPI_renameSystem(G_NEWNAME);
        if (rc != RESPONSE_OK) {
            printf("Error: Could not rename system\n");
            return EXIT_FAILURE;
        }
        printf("System renamed to '%s'\n", G_NEWNAME);
        return EXIT_SUCCESS;
    }

    /* interactive mode from here on, make sure ncurses can handle $TERM */
    nc_ensure_term();

    rc = cloudAPI_request(REQTYPE_HELLO);

    if (rc == RESPONSE_ERROR) {
        if (cloudAPI_lastError()[0] != '\0') {
            printf("Error: %s\n", cloudAPI_lastError());
        } else {
            printf("Error: Could not connect to SGINet backend\n");
        }
        return EXIT_FAILURE;
    } else if (rc == RESPONSE_REGISTRATION_REQUIRED) {
        char modelname[63];
        char ipmodel[5];
        char machine[63];
        char usr[32];
        int CPUCount;
        char *infoMsg;
        int m_count;
        char *infoTitle;
        ModelName *lstModelNames;
        int rcRegScreen;
        int regRC;
        if (debug) fprintf(stderr, "Please register first\n");
        GUI_MODE = 1;
        infoTitle = "Welcome to SGINet";
        infoMsg = "Your SGI system has not yet been registered.\n\nWhen you register, HINV data of your system and username/alias will\nappear publicly visible on sginet.flexion.io\n\nIf you do not agree, press CTRL-C now.\n\nHint: sginet client works best in aterm and mrxvt. In winterm set\nyour TERM to iris-ansi for the borders to work.\n\nPress ENTER to continue";
        nc_showMessage(infoTitle, infoMsg);


        getIPModel(ipmodel);        
        CPUCount = getCPUCount();
        
        /* Query list of possible model names for SW IPxx type of this macheen */
        nc_model_selection(ipmodel, modelname);
        if (debug) fprintf(stderr, "Selected model name is '%s'\n", modelname);

        rcRegScreen = nc_launch_reg_form(usr, machine);
        if (rcRegScreen != 1) {
            printf("Aborted\n");
            return EXIT_FAILURE;
        }
        if (debug) fprintf(stderr, "Selected model name is now '%s'\n", modelname);

        regRC = cloudAPI_register(usr, machine, ipmodel, CPUCount, modelname);
        if (regRC != RESPONSE_OK) {
            printf("Online registration failed. (%s)\n", G_CLIENTID);
            return EXIT_FAILURE;
        } else {
            printf("\n                      Welcome to SGINet!\n");
            printBanner();
            printf("\nYour SGI machine has been successfully registered on sginet.flexion.io\nPlease restart the sginet CLI.\n\n");
        }
        return EXIT_SUCCESS;
    }
    GUI_MODE = 1;
    openMainMenu();
    
    clear();
    endwin();
    return EXIT_SUCCESS;
}
