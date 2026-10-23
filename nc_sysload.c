#include "include/nc_sysload.h"
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "include/hwinfo_sgi.h"
#include "include/nc_tools.h"

int SPINNER_MOD = 2;  /* length of SPINNER array */
char *SPINNER[] = {".", ":"};

int avg_hist_length;  /* cpu avg load histogram */
int avg_hist_pos = 0;
int halfScreenWidth;
int bodyWidth;
int bodyHeight;

static void scan_delay(unsigned long millisec) {
#ifdef IRIX5
    /* IRIX 5 has no nanosleep() */
    struct timeval tv;
    tv.tv_sec = millisec / 1000;
    tv.tv_usec = (millisec % 1000) * 1000L;
    select(0, NULL, NULL, NULL, &tv);
#else
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = millisec * 1000000L;
    while (nanosleep(&req, &req) == -1)
        continue;
#endif
    /* Platform_gettime_realtime(&host->realtime, &host->realtimeMs); */
}
void nDrawCPUBar(int c, int width, int x, int y, int pct, WINDOW *win) {
    int f;
    /* cpuNo+space: 4, [brackets] 2, space + pctNo 100% 5 + extra space, */
    char *strBar = malloc(width + 1);
    char *strBarPtr;
    int barW = width - (4 + 2 + 5 + 1);
    int barFill = barW * pct / 100;  /* load indicator */
    int barEmpty = barW - barFill;
    int strBarPos = 5;  /* current strBar char pos */
    sprintf(strBar, "%3d [", c);
    for (f = 0; f < barFill; f++) {
        strBarPtr = &strBar[strBarPos];
        sprintf(strBarPtr, "|");
        strBarPos++;
    }
    for (f = 0; f < barEmpty; f++) {
        strBarPtr = &strBar[strBarPos];
        sprintf(strBarPtr, ".");
        strBarPos++;
    }
    strBarPtr = &strBar[strBarPos];
    sprintf(strBarPtr, " %3d%%] ", pct);
    mvwprintw(win, y, x, "%s", strBar);
    free(strBar);
}
void nDrawMemBar(int width, int x, int y, int pct, WINDOW *win) {
    int f;
    /* space: 4, [brackets] 2, space + pctNo 100% 5 + extra space, */
    char *strBar = malloc(width + 1);
    char *strBarPtr;
    int barW = width - (4 + 2 + 5 + 1);
    int barFill = barW * pct / 100;  /* load indicator */
    int barEmpty = barW - barFill;
    int strBarPos = 5;  /* current strBar char pos */
    sprintf(strBar, "    [");
    for (f = 0; f < barFill; f++) {
        strBarPtr = &strBar[strBarPos];
        sprintf(strBarPtr, "|");
        strBarPos++;
    }
    for (f = 0; f < barEmpty; f++) {
        strBarPtr = &strBar[strBarPos];
        sprintf(strBarPtr, ".");
        strBarPos++;
    }
    strBarPtr = &strBar[strBarPos];
    sprintf(strBarPtr, " %3d%%] ", pct);
    mvwprintw(win, y, x, "%s", strBar);
    free(strBar);
}
void drawUpdate(WINDOW *win, IRIXMachine *machine, int scancount, int *avg_hist) {
    int yPos;
    int memPct;
    int swapUsedMB;
    int swapTotalMB;
    double avgPct;
    int memUsedMB;
    int cpuCountPerSide;
    int yPosStart;
    int memTotalMB;
    int swapPct;
    int cpuCount;
    int xPos;
    int c;
    int avg_hist_height;
    Machine_scan(machine);  /* reads/updates cpu and memory usage */
    memTotalMB = machine->totalMem / 1024;
    memUsedMB = memTotalMB - (machine->availableMem / 1024);
    swapTotalMB = machine->totalSwap / 1024;
    swapUsedMB = machine->usedSwap / 1024;
    memPct = 100 * memUsedMB / memTotalMB;

    yPos = 0;
    mvwprintw(win, yPos, 0, "Total memory: %i MB  / %i MB (incl. swap)", memUsedMB, memTotalMB);  /* y, x (within border window) */
    nDrawMemBar(halfScreenWidth, halfScreenWidth, yPos, memPct, win);
    yPos++;

    swapPct = 100 * swapUsedMB / swapTotalMB;
    mvwprintw(win, yPos, 0, "Swap: %i MB  / %i MB", swapUsedMB, swapTotalMB);  /* y, x (within border window) */
    nDrawMemBar(halfScreenWidth, halfScreenWidth, yPos, swapPct, win);

    yPos += 2;
    /* CPU Load */
    cpuCount = machine->activeCPUs;
    cpuCountPerSide = cpuCount > 1 ? ((cpuCount + 1) / 2) : 1;
    mvwprintw(win, yPos++, 0, "CPU load (%i active CPUs) %s", cpuCount, SPINNER[scancount % SPINNER_MOD]);
    xPos = 0;
    yPosStart = yPos;
    avgPct = 0;

    /* collect average (first) */
    for (c = 0; c < cpuCount; c++) avgPct += machine->cpuData[c].totalPercent;
    if (cpuCount > 1) avgPct = CLAMP((avgPct / cpuCount), 0.0, 100.0);

    /* draw pct bars for each CPU */
    for (c = 0; c < cpuCount; c++) {
        if (c == cpuCountPerSide) {  /* draw second half of CPUs on right side */
            xPos = halfScreenWidth;
            yPos = yPosStart;  /* rewind to start pos */
        }
        nDrawCPUBar(c, halfScreenWidth, xPos, yPos++, (int)machine->cpuData[c].totalPercent, win);
    }
    yPosStart += cpuCountPerSide;
    /* draw avg histogram */
    avg_hist_height = bodyHeight - yPosStart - 2;
    if (avg_hist_height > 4) { /* hide if remaining height is too small */
        int bPos;
        int a;
        avg_hist[avg_hist_pos] = avgPct;
        avg_hist_pos++;
        if (avg_hist_pos >= avg_hist_length) avg_hist_pos = 0;

        xPos = avg_hist_length;
        bPos = avg_hist_pos;

        for (a = 0; a < avg_hist_length; a++) {
            int h;
            int avg_yPos = yPosStart;
            int barFill = avg_hist_height * avg_hist[bPos] / 100;
            for (h = 0; h < avg_hist_height; h++) {
                if (h < barFill) {
                    mvwprintw(win, (avg_yPos + avg_hist_height - h), xPos, "#");
                } else {
                    mvwprintw(win, (avg_yPos + avg_hist_height - h), xPos, ".");
                }
            }
            xPos--;
            bPos--;
            if (bPos < 0) bPos = avg_hist_length;
            if (xPos < 0) xPos = avg_hist_length;
        }
    }
}
int nc_sysload() {
    char title[256];
    int yBodyPos;
    char ipmodel[32];
    int xBodyPos;
    char OSRelease[64];
    int panelHeight;
    int yPos;
    char hostname[128];
    int show;
    int panelWidth;
    int xPos;
    IRIXMachine *machine;
    int scancount;
    int *avg_hist;
    int a;
    WINDOW *info_window;
    WINDOW *info_window_body;
    PANEL *info_panel;
    PANEL *info_panel_body;

    initscr();
    cbreak();
    noecho();
    curs_set(0);  /* no blinky cursor */
    nodelay(stdscr, TRUE);
    scrollok(stdscr, TRUE);

    panelWidth = COLS - 2;
    panelHeight = LINES - 2;
    bodyHeight = panelHeight - 5;
    bodyWidth = panelWidth - 4;
    halfScreenWidth = bodyWidth / 2;
    xPos = 1;
    yPos = 1;
    xBodyPos = 3;
    yBodyPos = yPos + 4;

    info_window = newwin(panelHeight, panelWidth, yPos, xPos);  /* (HEIGHT, WIDTH, starty, startx) */
    info_window_body = newwin(bodyHeight, bodyWidth, yBodyPos, xPos + 2);

    info_panel = new_panel(info_window);
    info_panel_body = new_panel(info_window_body);

    keypad(info_window, TRUE);  /* this will allow ESC key */

    machine = Machine_new();  /* reads static values like CPU count and pagefile size */


    getHostname(hostname, sizeof(hostname));
    getOSRelease(OSRelease);
    getIPModel(ipmodel);

    sprintf(title, "System Monitor - %s - %s", hostname, OSRelease);
    box(info_window, 0, 0);
    print_in_middle(info_window, 1, 0, COLS - 6, title, COLOR_PAIR(1));
    mvwaddch(info_window, 2, 0, ACS_LTEE);
    mvwhline(info_window, 2, 1, ACS_HLINE, panelWidth - 2);
    mvwaddch(info_window, 2, panelWidth, ACS_RTEE);

    mvprintw(LINES - 1, 0, " Press any key to exit                      ");

    update_panels();
    doupdate();

    scancount = 0;
    show = 1;

    /* prepare cpu avg load histogram */
    avg_hist_length = bodyWidth;
    avg_hist = malloc((avg_hist_length + 1) * sizeof(int));  /* drawUpdate() may touch [avg_hist_length] */
    for (a = 0; a <= avg_hist_length; a++) avg_hist[a] = 0;

    while (show) {
        int ch;
        drawUpdate(info_window_body, machine, scancount, avg_hist);
        ch = getch();
        if (ch != ERR) {
            /* ungetch(ch); */
            show = 0;  /* quit loop */
            break;
        } else {
            wrefresh(info_window_body);
            scancount++;
            scan_delay(950);
        }
    }

    free(avg_hist);
    freeMachineInfo(machine);
    endwin();
}
