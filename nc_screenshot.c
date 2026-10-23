#include "include/nc_screenshot.h"
#include "include/cloudAPI.h"
#include "include/fileutils.h"
#include "include/nc_tools.h"
#include "include/sginet.h"

int nc_screenshot() {
    int yBodyPos;
    int xBodyPos;
    int yPos;
    int xPos;
    int panelHeight;
    int panelWidth;
    char msg[128];
    int delaySec = 6;
    WINDOW *info_window;
    PANEL *info_panel;

    clear();
    initscr();
    cbreak();
    noecho();

    panelWidth = 40;   /* COLS - 6; */
    panelHeight = 10;  /* LINES - 4; */
    xPos = (COLS - panelWidth) / 2;
    yPos = 1;

    xBodyPos = (TERM_MODE == TERM_ATERM ? 3 : 1);
    yBodyPos = (TERM_MODE == TERM_ATERM ? yPos + 4 : 3);

    info_window = newwin(panelHeight, panelWidth, yPos, xPos);  /* (HEIGHT, WIDTH, starty, startx) */
    info_panel = new_panel(info_window);

    keypad(info_window, TRUE);  /* this will allow ESC key */

    if (TERM_MODE == TERM_ATERM) {
        box(info_window, 0, 0);
        print_in_middle(info_window, 1, 0, panelWidth, "Screenshot", COLOR_PAIR(1));
        mvwaddch(info_window, 2, 0, ACS_LTEE);
        mvwhline(info_window, 2, 1, ACS_HLINE, panelWidth - 2);
        mvwaddch(info_window, 2, panelWidth, ACS_RTEE);
    } else {  /* ANSI mode */
        mvprintw(0, xPos, "Screenshot");
    }
    /*    mvprintw(LINES - 1, 0, PROMPT_ENTER_ESC); */

    update_panels();
    doupdate();

    while (delaySec > 0) {
        sprintf(msg, "Taking screenshot in %i seconds", delaySec);
        mvwprintw(info_window, 4, 2, "%s", msg);  /* y, x (within border window) */
        update_panels();
        doupdate();
        sleep(1);
        delaySec -= 1;
    }
    printf("\a");  /* \a is for bell/beep */
    
    mvwprintw(info_window, 4, 2, "%s", "Taking screenshot NOW         ");  /* y, x (within border window) */
    update_panels();
    doupdate();

    takeScreenshotAndUpload(0, 0, 0, 0, 0);
    
    mvwprintw(info_window, 4, 2, "%s", "Your screenshot is now available");
    mvwprintw(info_window, 5, 2, "%s", "for download on:");
    mvwprintw(info_window, 6, 2, "%s", "https://sginet.flexion.io");
    mvwprintw(info_window, 8, 2, "%s", "ENTER to continue");
    update_panels();
    doupdate();
    getch();
    endwin();
    clear();
}
