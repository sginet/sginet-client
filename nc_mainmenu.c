#include <stdlib.h>
#include "include/sginet.h"
#include "include/nc_mainmenu.h"
#include "include/nc_tools.h"
#include "include/cloudAPI.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
/* #define CTRLD 4 */

char *choices[] = {
    "List online systems",
    "List all systems",
	"Search software",
    "Take screenshot",
    "Show system load",
    "Shoutbox",
    "SGUG forum news",
    "System advisor",
    "Check for updates",
    "Exit",
    (char *)NULL,
};

int nc_mainmenu(int *selectedMenu) {
    int yPos;
    int xPos;
    int winWidth;
    int winHeight;
    int done;

    ITEM **my_items;
    int c;
    MENU *my_menu;
    WINDOW *my_menu_win;
    WINDOW *my_menu_sub;
    int n_choices, i;
    char term[128];

    int updAvail = isUpdateAvailable();
    /* start cursing */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  /* no blinky cursor */
    clear();

    /* display terminal settings */
    strcpy(term, " TERM: ");
    strcat(term, getenv("TERM"));

    /* Create items */
    n_choices = ARRAY_SIZE(choices) - 1;
    my_items = (ITEM **)calloc(n_choices + 1, sizeof(ITEM *));
    for (i = 0; i < n_choices; ++i) {
        my_items[i] = new_item(choices[i], "");
    }
    my_items[n_choices] = NULL;  /* null sentinel for new_menu() */

    winHeight = 14;
    winWidth = 60;
    xPos = (COLS - winWidth) / 2;
    yPos = 4;

    my_menu = new_menu((ITEM **)my_items);

    /* Create the window to be associated with the menu */
    my_menu_win = newwin(winHeight, winWidth, yPos, xPos);
    keypad(my_menu_win, TRUE);

    /* Set main window and sub window */
    set_menu_win(my_menu, my_menu_win);
    my_menu_sub = derwin(my_menu_win, winHeight-4, winWidth-2, yPos-1, 1);
    set_menu_sub(my_menu, my_menu_sub);
    set_menu_format(my_menu, winHeight-4, 1);

    /* Set menu mark to the string " * " */
    set_menu_mark(my_menu, " * ");

    /* Print a border around the main window and print a title */
    if (TERM_MODE == TERM_ATERM) {
        box(my_menu_win, 0, 0);
        print_in_middle(my_menu_win, 1, 0, winWidth, "SGINet client - Main menu", COLOR_PAIR(1));
        mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
        mvwhline(my_menu_win, 2, 1, ACS_HLINE, winWidth-2);
        mvwaddch(my_menu_win, 2, winWidth-1, ACS_RTEE);
    } else {
        mvprintw(1, 1, "SGINet client - Main menu");
    }
    refresh();
    /* Post the menu */
    post_menu(my_menu);
    wrefresh(my_menu_win);

    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, 0, PROMPT_NAV_ESC);
    attroff(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - strlen(term) - 1, "%s", term);
    if (updAvail) mvprintw(0, 0, "Update available");
    refresh();

    done = 0;
    while ((c = wgetch(my_menu_win)) != KEY_F(1)) {
        switch (c) {
            case KEY_DOWN:
                menu_driver(my_menu, REQ_DOWN_ITEM);
                break;
            case KEY_UP:
                menu_driver(my_menu, REQ_UP_ITEM);
                break;
            case KEY_NPAGE:
                menu_driver(my_menu, REQ_SCR_DPAGE);
                break;
            case KEY_PPAGE:
                menu_driver(my_menu, REQ_SCR_UPAGE);
                break;
            case 27: /* ESC (but with delay) */
                done = 1;
                break;

            case KEY_RESIZE:
            {
                int cur_idx = item_index(current_item(my_menu));

                unpost_menu(my_menu);
                free_menu(my_menu);
                delwin(my_menu_sub);
                delwin(my_menu_win);

                endwin();
                refresh();
                clear();

                xPos = (COLS - winWidth) / 2;

                my_menu = new_menu((ITEM **)my_items);
                my_menu_win = newwin(winHeight, winWidth, yPos, xPos);
                keypad(my_menu_win, TRUE);
                my_menu_sub = derwin(my_menu_win, winHeight-4, winWidth-2, yPos-1, 1);
                set_menu_win(my_menu, my_menu_win);
                set_menu_sub(my_menu, my_menu_sub);
                set_menu_format(my_menu, winHeight-4, 1);
                set_menu_mark(my_menu, " * ");

                if (TERM_MODE == TERM_ATERM) {
                    box(my_menu_win, 0, 0);
                    print_in_middle(my_menu_win, 1, 0, winWidth, "SGINet client - Main menu", COLOR_PAIR(1));
                    mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
                    mvwhline(my_menu_win, 2, 1, ACS_HLINE, winWidth-2);
                    mvwaddch(my_menu_win, 2, winWidth-1, ACS_RTEE);
                } else {
                    mvprintw(1, 1, "SGINet client - Main menu");
                }
                refresh();

                post_menu(my_menu);
                set_current_item(my_menu, my_items[cur_idx]);

                attron(COLOR_PAIR(2));
                mvprintw(LINES - 1, 0, PROMPT_NAV_ESC);
                attroff(COLOR_PAIR(2));
                mvprintw(LINES - 1, COLS - strlen(term) - 1, "%s", term);
                if (updAvail) mvprintw(0, 0, "Update available");
                refresh();
                break;
            }

            case 343: /* ENTER in iris-ansi */
            case 10: /* ENTER */
            {
                ITEM *cur;
                cur = current_item(my_menu);
                *selectedMenu = item_index(cur);
                done = 1;
                break;
            }
        }
        wrefresh(my_menu_win);
        if (done == 1) break;
    }

    /* Unpost and free all the memory taken up */
    unpost_menu(my_menu);
    free_menu(my_menu);
    for (i = 0; i < n_choices; ++i) {
        free_item(my_items[i]);
    }
    free(my_items);
    delwin(my_menu_sub);
    delwin(my_menu_win);
    clear();
}
