#include "include/nc_tools.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <term.h>

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color) {
    int length, x, y;
    float temp;

    if (win == NULL)
        win = stdscr;
    getyx(win, y, x);
    if (startx != 0)
        x = startx;
    if (starty != 0)
        y = starty;
    if (width == 0)
        width = 80;

    length = strlen(string);
    temp = (width - length) / 2;
    x = startx + (int)temp;
    wattron(win, color);
    mvwprintw(win, y, x, "%s", string);
    wattroff(win, color);
    refresh();
}

/*
 * If $TERM is unknown to ncurses (neither in the terminfo database nor compiled
 * in as a fallback), switch to the closest built-in entry so initscr() doesn't
 * exit with "Error opening terminal". Must be called before the first initscr().
 */
void nc_ensure_term(void) {
    static char envbuf[32];
    const char* term = getenv("TERM");
    const char* fallback;
    int err;

    if (term != NULL && term[0] != '\0' && setupterm((char*)term, 1, &err) == OK) {
        del_curterm(cur_term);
        return;
    }

    if (term == NULL || term[0] == '\0') {
        fallback = "vt100";
    } else if (strncmp(term, "iris", 4) == 0) {
        fallback = "iris-ansi";
    } else if (strncmp(term, "vt", 2) == 0) {
        fallback = "vt100";
    } else if (strncmp(term, "screen", 6) == 0) {
        fallback = "screen-256color";
    } else if (strncmp(term, "tmux", 4) == 0) {
        fallback = "tmux-256color";
    } else if (strstr(term, "rxvt") != NULL) {
        fallback = "rxvt";
    } else {
        /* practically every terminal emulator in use today is xterm compatible */
        fallback = "xterm";
    }

    sprintf(envbuf, "TERM=%s", fallback);
    putenv(envbuf);
}
