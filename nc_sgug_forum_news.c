#include "include/nc_sgug_forum_news.h"

#include "include/cloudAPI.h"
#include "include/nc_tools.h"
#include "include/sginet.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD 4

void nc_sgug_forum_news () {
    char *label;
    char *title;
    RSSEntry *lstNews;
    int n_news;
    int i;
    WINDOW *my_menu_win;
    MENU *my_menu;
    int dynColWidth;
    int c;
    int fixedCols;
    int selectedNo;
    ITEM **my_items;
    int MAXMENUROWCOLS;
    int listHeight;
    int done;
    if (COLS < 58) {
        printf("-> Error: Terminal not wide enough! (%i)\n", COLS);
        sleep(2);
        return;
    }
    
    title = "SGUG Forum News";
    cloudAPI_listRSSNews(&lstNews, &n_news, "whatsnew-sgug");

    /* start cursing */
    initscr();
    cbreak();
    noecho();
    curs_set(0);  /* no blinky cursor */
    keypad(stdscr, TRUE);

    /* Calculate width of menu row columns */
    MAXMENUROWCOLS = COLS - 8;
    fixedCols = 12; /* date column + space */
    dynColWidth = MAXMENUROWCOLS - fixedCols;

    my_items = (ITEM **)calloc(n_news + 1, sizeof(ITEM *));
    for (i = 0; i < n_news; ++i) {
        label = calloc(1, MAXMENUROWCOLS + 2 + 1);
        sprintf(label, "%s: %-*.*s",
                lstNews[i].dt,
                dynColWidth, dynColWidth, lstNews[i].title
        );
        my_items[i] = new_item(strdup(label), "");
        free(label);
    }
    my_items[n_news] = NULL;  /* null sentinel for new_menu() */

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
    box(my_menu_win, 0, 0);
    print_in_middle(my_menu_win, 1, 0, COLS - 6, title, COLOR_PAIR(1));
    mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
    mvwhline(my_menu_win, 2, 1, ACS_HLINE, COLS - 4);
    mvwaddch(my_menu_win, 2, COLS - 3, ACS_RTEE);
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

            case 343: /* ENTER in iris-ansi */
            case 10:  /* ENTER */
            {
                ITEM *cur;
                cur = current_item(my_menu);
                selectedNo = item_index(cur);
                nc_view_sgug_forum_post(lstNews[selectedNo].guid);
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
    for (i = 0; i < n_news; ++i) {
        free((void *)item_name(my_items[i]));  /* free strdup'd label */
        free_item(my_items[i]);
    }
    free(my_items);
    /* clear screen if no fancy aterm windows borders */
    if (TERM_MODE == TERM_DEFAULT) clear();
    /* endwin(); */
}


void nc_view_sgug_forum_post(char *id) {
    int yBodyPos;
    int xBodyPos;
    int yPos;
    int xPos;
    int panelHeight;
    int rc;
    int panelWidth;
    RSSEntryFull *post;
    WINDOW *info_window;
    WINDOW *info_window_body;
    PANEL *info_panel;
    PANEL *info_panel_body;

    initscr();
    cbreak();
    curs_set(0);  /* no blinky cursor */
    noecho();

    panelWidth = COLS - 12;
    panelHeight = LINES - 8;
    xPos = (COLS - panelWidth) / 2;
    yPos = (LINES - panelHeight) / 2;

    xBodyPos = 3;
    yBodyPos = yPos + 3;

    info_window = newwin(panelHeight, panelWidth, yPos, xPos);                       /* (HEIGHT, WIDTH, starty, startx) */
    info_window_body = newwin(panelHeight - 5, panelWidth - 4, yBodyPos, xPos + 2);  /* this is where we put the message */

    info_panel = new_panel(info_window);
    info_panel_body = new_panel(info_window_body);

    keypad(info_window, TRUE);  /* this will allow ESC key */

    /* get post by ID */
    rc = cloudAPI_load_sgug_post_by_ID(&post, id);

    box(info_window, 0, 0);

    if (rc != RESPONSE_OK) {    
        print_in_middle(info_window, 1, 0, COLS - 6, "Error", COLOR_PAIR(1));
        mvwaddch(info_window, 2, 0, ACS_LTEE);
        mvwhline(info_window, 2, 1, ACS_HLINE, panelWidth-2);
        mvwaddch(info_window, 2, panelWidth, ACS_RTEE);        
        mvwprintw(info_window_body, 0, 0, "%s", "Failed to load forum message"); /* y, x (within border window) */
    } else {
        int r;
        mvwprintw(info_window, 1, 2, "%s", post->title);
        mvwaddch(info_window, 2, 0, ACS_LTEE);
        mvwhline(info_window, 2, 1, ACS_HLINE, panelWidth - 2);
        mvwaddch(info_window, 2, panelWidth, ACS_RTEE);
        r = 0;
        mvwprintw(info_window_body, r++, 0, "Posted by %s on %s", post->creator, post->dt); /* y, x (within border window) */
        r++;
        mvwprintw(info_window_body, r++, 0, "%s", post->content);
    }
    
    free(post);

    mvprintw(LINES - 1, 0, PROMPT_ENTER_ESC);

    update_panels();
    doupdate();

    /* wait for user input */
    wgetch(info_window);
}
