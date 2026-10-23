#include "include/nc_listsystems.h"

#include "include/cloudAPI.h"
#include "include/nc_tools.h"
#include "include/sginet.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD 4

static void listsystems_calc_cols(int *col1, int *col2, int *col3, int *col4, int *col5) {
    int MAXMENUROWCOLS = COLS - 8;
    int fixedColsTotalWidth = 4 + 3;
    int spacersWidth = 15;
    int remainingCols = MAXMENUROWCOLS - fixedColsTotalWidth - spacersWidth;
    int dynColWidth = remainingCols / 4;
    if (remainingCols < 1) fprintf(stderr, "Warning! not enough space left to display row\n");
    *col1 = dynColWidth;
    *col2 = 3;
    *col3 = dynColWidth;
    *col4 = dynColWidth;
    *col5 = dynColWidth;
}

static void listsystems_build_items(ITEM **my_items, MachineListEntry *lstSystems, int n_systems,
                                    int col1, int col2, int col3, int col4, int col5) {
    int i;
    char *up = "UP";
    char *down = "- ";
    int MAXMENUROWCOLS = COLS - 8;
    char *label = malloc(MAXMENUROWCOLS + 2);
    for (i = 0; i < n_systems; ++i) {
        snprintf(label, MAXMENUROWCOLS + 2, "%s | %-*.*s | %*.*s | %-*.*s | %-*.*s | %-*.*s",
                 (lstSystems[i].online == 1 ? up : down),
                 col1, col1, lstSystems[i].modelname,
                 col2, col2, lstSystems[i].cpuCountStr,
                 col3, col3, lstSystems[i].machinename,
                 col4, col4, lstSystems[i].gfx0,
                 col5, col5, lstSystems[i].username);
        my_items[i] = new_item(strdup(label), "");
    }
    free(label);
    my_items[n_systems] = NULL;  /* null sentinel for new_menu() */
}

static void listsystems_free_items(ITEM **my_items, int n_systems) {
    int i;
    for (i = 0; i < n_systems; ++i) {
        free((void *)item_name(my_items[i]));
        free_item(my_items[i]);
    }
}

void nc_listsystems(int reqType) {
    int selectedNo;
    int col1;
    int col2;
    int col3;
    int col4;
    int col5;
    int listHeight;
    int done;
    ITEM **my_items;
    int c;
    MENU *my_menu;
    WINDOW *my_menu_win;
    WINDOW *my_menu_sub;
    int n_systems, i;
    MachineListEntry *lstSystems;
    char title[32];

    /* Query online/all systems */
    cloudAPI_showSystems(&lstSystems, &n_systems, reqType);

    /* start cursing */
    initscr();
    cbreak();
    noecho();
    curs_set(0);  /* no blinky cursor */
    keypad(stdscr, TRUE);

    if (reqType == REQTYPE_SHOWALLSYSTEMS) {
        sprintf(title, "Systems: %i", n_systems);
    } else {
        sprintf(title, "Systems online: %i", n_systems);
    }

    /* column widths (fixed and variable) */
    /* fix  var         fix   fix  var           var */
    /* UP | username | IPxx | 16 | machinename | gfx */
    listsystems_calc_cols(&col1, &col2, &col3, &col4, &col5);

    /* Array needs n_systems items plus a NULL sentinel that new_menu() uses as terminator. */
    my_items = (ITEM **)calloc(n_systems + 1, sizeof(ITEM *));
    listsystems_build_items(my_items, lstSystems, n_systems, col1, col2, col3, col4, col5);

    my_menu = new_menu((ITEM **)my_items);

    /* Create the window to be associated with the menu */
    listHeight = LINES - 3;
    my_menu_win = newwin(listHeight, COLS - 2, 1, 1);  /* (HEIGHT, WIDTH, starty, startx) */
    keypad(my_menu_win, TRUE);                         /* this will allow ESC key */

    /* Set main window and sub window */
    set_menu_win(my_menu, my_menu_win);
    my_menu_sub = derwin(my_menu_win, (listHeight - 4), COLS - 6, 3, 1);
    set_menu_sub(my_menu, my_menu_sub);
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

            case KEY_RESIZE:
            {
                int cur_idx = item_index(current_item(my_menu));

                unpost_menu(my_menu);
                free_menu(my_menu);
                listsystems_free_items(my_items, n_systems);
                delwin(my_menu_sub);
                delwin(my_menu_win);

                endwin();
                refresh();
                clear();

                listsystems_calc_cols(&col1, &col2, &col3, &col4, &col5);
                listsystems_build_items(my_items, lstSystems, n_systems, col1, col2, col3, col4, col5);

                my_menu = new_menu((ITEM **)my_items);
                listHeight = LINES - 3;
                my_menu_win = newwin(listHeight, COLS - 2, 1, 1);
                keypad(my_menu_win, TRUE);
                set_menu_win(my_menu, my_menu_win);
                my_menu_sub = derwin(my_menu_win, (listHeight - 4), COLS - 6, 3, 1);
                set_menu_sub(my_menu, my_menu_sub);
                set_menu_format(my_menu, (listHeight - 4), 1);
                set_menu_mark(my_menu, " * ");

                if (TERM_MODE == TERM_ATERM) {
                    box(my_menu_win, 0, 0);
                    print_in_middle(my_menu_win, 1, 0, COLS - 6, title, COLOR_PAIR(1));
                    mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
                    mvwhline(my_menu_win, 2, 1, ACS_HLINE, COLS - 4);
                    mvwaddch(my_menu_win, 2, COLS - 3, ACS_RTEE);
                } else {
                    mvprintw(0, 0, "%s", title);
                }
                refresh();

                post_menu(my_menu);
                set_current_item(my_menu, my_items[cur_idx < n_systems ? cur_idx : 0]);

                attron(COLOR_PAIR(2));
                mvprintw(LINES - 1, 0, PROMPT_NAV_ESC);
                attroff(COLOR_PAIR(2));
                refresh();
                break;
            }

            case 343: /* ENTER in iris-ansi */
            case 10:  /* ENTER */
            {
                ITEM *cur;
                cur = current_item(my_menu);
                selectedNo = item_index(cur);
                nc_viewsystem(lstSystems[selectedNo].id, lstSystems[selectedNo].machinename);
                redrawwin(my_menu_win);
                break;
            }
        }
        wrefresh(my_menu_win);
        if (done == 1) break;
    }

    /* Unpost and free all the memory taken up */
    unpost_menu(my_menu);
    free_menu(my_menu);
    listsystems_free_items(my_items, n_systems);
    free(my_items);
    delwin(my_menu_sub);
    delwin(my_menu_win);
    free(lstSystems);

    /* clear screen if no fancy aterm windows borders */
    /* if (TERM_MODE == TERM_DEFAULT) clear(); */
}

void nc_viewsystem(char *id, char *machinename) {
    LabelAndValue *lstHINV;
    int c;
    int n_count;
    int resized;
    WINDOW *info_window;
    WINDOW *info_window_body;
    PANEL *info_panel;
    PANEL *info_panel_body;

    initscr();
    cbreak();
    noecho();

    /* Query hinv once. reused across any redraws caused by resize */
    cloudAPI_getSystemHINVAsList(id, &lstHINV, &n_count);

    resized = 0;
    do {
        int r;
        int panelWidth = COLS - 12;
        int panelHeight = LINES - 8;
        int xPos = (COLS - panelWidth) / 2;
        int yPos = (LINES - panelHeight) / 2;
        int yBodyPos = (TERM_MODE == TERM_ATERM ? yPos + 4 : 3);

        info_window = newwin(panelHeight, panelWidth, yPos, xPos);                       /* (HEIGHT, WIDTH, starty, startx) */
        info_window_body = newwin(panelHeight - 5, panelWidth - 4, yBodyPos, xPos + 2);  /* this is where we put the message */
        info_panel = new_panel(info_window);
        info_panel_body = new_panel(info_window_body);
        keypad(info_window, TRUE);  /* this will allow ESC key */

        box(info_window, 0, 0);
        print_in_middle(info_window, 1, 0, COLS - 6, machinename, COLOR_PAIR(1));
        mvwaddch(info_window, 2, 0, ACS_LTEE);
        mvwhline(info_window, 2, 1, ACS_HLINE, panelWidth - 2);
        mvwaddch(info_window, 2, panelWidth, ACS_RTEE);

        for (r = 0; r < n_count; r++) {
            mvwprintw(info_window_body, r, 0, "%s", lstHINV[r].label);   /* y, x (within border window) */
            mvwprintw(info_window_body, r, 20, "%s", lstHINV[r].value);  /* y, x (within border window) */
        }
        mvwprintw(info_window_body, r, 0, "%s", "");  /* place cursor on new line */

        mvprintw(LINES - 1, 0, PROMPT_ENTER_ESC);

        update_panels();
        doupdate();

        c = wgetch(info_window);  /* wait for user input */
        if (c == KEY_RESIZE) {
            del_panel(info_panel_body);
            del_panel(info_panel);
            delwin(info_window_body);
            delwin(info_window);
            endwin();
            refresh();
            clear();
            resized = 1;
        }
    } while (c == KEY_RESIZE);

    del_panel(info_panel_body);
    del_panel(info_panel);
    delwin(info_window_body);
    delwin(info_window);
    free(lstHINV);

    /* If the terminal was resized while the dialog was open, push KEY_RESIZE */
    /* back into the input queue so nc_listsystems redraws the background too. */
    if (resized) ungetch(KEY_RESIZE);
}
