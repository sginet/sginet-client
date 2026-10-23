#include "include/nc_systemadvisor.h"

#include "include/str_tools.h"
#include "include/nc_tools.h"
#include "include/sginet.h"
#include "include/systemadvisor.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
char *sevLabel[] = {
    "INFO",
    "OK",
    "WARNING",
    "HIGH",
    "ERROR",
    (char *)NULL,
};

void nc_systemadvisor() {
    int MAXMENUROWCOLS;
    int listHeight;
    int done;
    ITEM **my_items;
    MENU *my_menu;
    WINDOW *my_menu_win;
    int i, c;
    char *title = "System Advisor Report";

    /* prepare growing array of advisor messages */
    AdvisoryList *lstAdv = malloc(sizeof(AdvisoryList));
    memset(lstAdv, 0, sizeof(*lstAdv));
    lstAdv->cnt = 0;
    lstAdv->capacity = 5;  /* initial size, triggers reallocate */
    lstAdv->lst = malloc(sizeof(AdvisorEntry) * lstAdv->capacity);

    startSystemAdvisor(lstAdv);
        
    /* start (n)cursing */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    /* Calculate width of menu row columns */
    MAXMENUROWCOLS = COLS - 8;      /* this must fit in menu width further below */

    my_items = (ITEM **)calloc(lstAdv->cnt + 1, sizeof(ITEM *));
    for (i = 0; i < lstAdv->cnt; ++i) {
        char label[290];
        sprintf(label, "%-*.*s| %s", 8, 8, sevLabel[lstAdv->lst[i].sev], lstAdv->lst[i].msg);
        my_items[i] = new_item(strdup(label), "");
    }
    my_items[lstAdv->cnt] = NULL;  /* null for new_menu() */

    my_menu = new_menu((ITEM **)my_items);
    /* Create the window to be associated with the menu */
    listHeight = LINES - 3;
    my_menu_win = newwin(listHeight, COLS - 2, 1, 1);  /* (HEIGHT, WIDTH, starty, startx) */
    keypad(my_menu_win, TRUE);                         /* this will allow ESC key */

    /* Set main window and sub window */
    set_menu_win(my_menu, my_menu_win);
    set_menu_sub(my_menu, derwin(my_menu_win, (listHeight - 4), COLS - 6, 3, 1));
    set_menu_format(my_menu, (listHeight - 4), 1);

    /* Set menu mark to the string " * " */
    set_menu_mark(my_menu, " * ");

    /* Print a border around the main window and print a title */
    if (TERM_MODE == TERM_ATERM) {
        box(my_menu_win, 0, 0);
        print_in_middle(my_menu_win, 1, 0, COLS - 6, title, COLOR_PAIR(1));
        mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
        mvwhline(my_menu_win, 2, 1, ACS_HLINE, COLS - 4);
        mvwaddch(my_menu_win, 2, COLS - 3, ACS_RTEE);

    } else {  /* ANSI mode */
        mvprintw(0, 0, "%s", title);
    }
    refresh();

    /* Post the menu */
    post_menu(my_menu);
    wrefresh(my_menu_win);

    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, 0, PROMPT_NAV_ESC);
    attroff(COLOR_PAIR(2));
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
            case 27:  /* ESC (but with delay) */
                done = 1;
                break;
        }
        wrefresh(my_menu_win);
        if (done == 1) break;
    }

    /* Unpost and free all the memory taken up */
    unpost_menu(my_menu);
    free_menu(my_menu);
    for (i = 0; i < lstAdv->cnt; ++i) {
        free((void *)item_name(my_items[i]));  /* free strdup'd label */
        free_item(my_items[i]);
    }
    free(my_items);

    free(lstAdv);

    /* clear screen if no fancy aterm windows borders */
    if (TERM_MODE == TERM_DEFAULT) clear();
}
