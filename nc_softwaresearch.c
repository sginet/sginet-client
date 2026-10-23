#include "include/sginet.h"
#include "include/nc_softwaresearch.h"
#include "include/nc_tools.h"
#include "include/str_tools.h"
#include "include/cloudAPI.h"

#include <assert.h>
#include <ctype.h>
#include "include/hwinfo_sgi.h"

#define ACTION_NONE 0
#define ACTION_SEARCH 1
#define ACTION_QUIT 99

char searchquery[36];

void searchForm();
void searchResultPage();

void nc_softwaresearch() {
    searchForm();
}

void searchResultPage() {
    int MAXMENUROWCOLS;
    int listHeight;
    int done;
    ITEM **my_items;
    int c;
    MENU *my_menu;
    WINDOW *my_menu_win;
    int n_results, i;
    SoftwareEntry* lstResults;
    char title[32];
    cloudAPI_searchSoftware(&lstResults, &n_results, searchquery);

    /* start cursing */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    /* Calculate width of menu row columns */
    MAXMENUROWCOLS = COLS - 8; /* this must fit in menu width further below */

    sprintf(title, "Software search results: %i", n_results);
    
    my_items = (ITEM **)calloc(n_results + 1, sizeof(ITEM *));
    for (i = 0; i < n_results; ++i) {
        char label[290];
        sprintf(label, "%s: %s", lstResults[i].src, lstResults[i].pkg);
        my_items[i] = new_item(strdup(label), "");
    }
    my_items[n_results] = NULL;  /* null sentinel for new_menu() */
    
    my_menu = new_menu((ITEM **)my_items);
    /* Create the window to be associated with the menu */
    listHeight = LINES - 3;
    my_menu_win = newwin(listHeight, COLS -2, 1, 1); /* (HEIGHT, WIDTH, starty, startx) */
    keypad(my_menu_win, TRUE);

    /* Set main window and sub window */
    set_menu_win(my_menu, my_menu_win);
    set_menu_sub(my_menu, derwin(my_menu_win, (listHeight-4), COLS-6, 3, 1));
    set_menu_format(my_menu, (listHeight-4), 1);

    /* Set menu mark to the string " * " */
    set_menu_mark(my_menu, " * ");

    /* Print a border around the main window and print a title */
    if (TERM_MODE == TERM_ATERM) {
        box(my_menu_win, 0, 0);
        print_in_middle(my_menu_win, 1, 0, COLS-6, title, COLOR_PAIR(1));
        mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
        mvwhline(my_menu_win, 2, 1, ACS_HLINE, COLS-4);
        mvwaddch(my_menu_win, 2, COLS-3, ACS_RTEE);
        
    } else { /* ANSI mode */
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
            case 27: /* ESC (but with delay) */
                done = 1;
                break;
        }
        wrefresh(my_menu_win);
        if (done == 1 ) break;
    }

    /* Unpost and free all the memory taken up */
    unpost_menu(my_menu);
    free_menu(my_menu);
    for (i = 0; i < n_results; ++i) {
        free((void *)item_name(my_items[i]));  /* free strdup'd label */
        free_item(my_items[i]);
    }
    free(my_items);
    
    /* clear screen if no fancy aterm windows borders */
    if (TERM_MODE == TERM_DEFAULT) clear();
    /* endwin(); */
}
int validateSearchForm(FIELD *field[], char * query) {
    snprintf(query, 32, "%s", trim_whitespaces(field_buffer(field[0], 0)));
    if (strlen(query) < 3) return ACTION_NONE;
    return ACTION_SEARCH;  /* OK */
}
void searchForm() {
    int ch;
    int rows;
    int cols;
    int yPos;
    WINDOW *my_form_win;
    int xPos;
    FORM *my_form;
    int winWidth;
    FIELD *field[2];
    int winHeight;
    int action;
    clear();


    /* Initialize curses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);  /* blinky cursor */

    /* Initialize the fields (h, w, top, left, offscrRows, nbuf) */
    field[0] = new_field(1, 32, 4, 27, 0, 0);
    field[1] = NULL;

    /* Set field options */
    set_field_back(field[0], A_UNDERLINE);
    field_opts_off(field[0], O_AUTOSKIP);  /* Don't go to next field when this Field is filled up */

    /* Create the form and display it */
    my_form = new_form(field);
    /* Calculate the area required for the form */
    scale_form(my_form, &rows, &cols);

    winHeight = rows + 4;
    winWidth = cols + 4;
    xPos = (COLS - winWidth) / 2;
    yPos = 2;

    /* Create the window to be associated with the form */
    /* my_form_win = newwin(rows + 4, cols + 4, 2, 4); */
    my_form_win = newwin(winHeight, winWidth, yPos, xPos);
    keypad(my_form_win, TRUE);

    /* Set main window and sub window */
    set_form_win(my_form, my_form_win);
    set_form_sub(my_form, derwin(my_form_win, rows, cols, 2, 2));

    /* Print a border around the main window and print a title */
    if (TERM_MODE == TERM_ATERM) {
    box(my_form_win, 0, 0);
        print_in_middle(my_form_win, 1, 0, cols + 4, "Software search", COLOR_PAIR(1));
    } else { /* ANSI mode */
        mvprintw(1, 1, "Software search");
    }

    post_form(my_form);
    mvwprintw(my_form_win, 3, 2, "%s", "Search packages in SGUG-RSE, SGI Freeware 2004 and nekoware");
    mvwprintw(my_form_win, 6, 2, "%s", "Search query:");
    
    refresh();
    wrefresh(my_form_win);
    mvprintw(LINES - 1, 0, PROMPT_ENTER_QUIT);

    /* relocate blinking cursor to first input field */
    wmove(my_form_win, 6, 29);  /* position cursor at first input field */
    refresh();

    /* Loop through to get user requests */
    /* while((ch = getch()) != KEY_F(1)) */
    action = ACTION_NONE;
    while ((ch = wgetch(my_form_win)) != KEY_F(1) && action == ACTION_NONE) {
        switch (ch) {
            /*case 9:  // TAB
                form_driver(my_form, REQ_NEXT_FIELD);
                form_driver(my_form, REQ_END_LINE);
                break;*/
            case KEY_LEFT:
                form_driver(my_form, REQ_PREV_CHAR);
                break;
            case KEY_RIGHT:
                form_driver(my_form, REQ_NEXT_CHAR);
                break;
            case 127:
                form_driver(my_form, REQ_DEL_PREV);
                break;
            case KEY_DC:  /* DEL */
                form_driver(my_form, REQ_DEL_CHAR);
                break;
            case 27: /* ESC */
                action = ACTION_QUIT;
            case 343: /* ENTER in iris-ansi */
            case 10: /* ENTER */
                form_driver(my_form, REQ_VALIDATION);
                action = validateSearchForm(field, searchquery);
                break;
            default:
                form_driver(my_form, ch);
                break;
        }
        /* printf("char %i\n", ch); */
        if (action != ACTION_NONE) break;
    }

    /* Un post form and free the memory */
    unpost_form(my_form);
    free_form(my_form);
    free_field(field[0]);

    if (action == ACTION_SEARCH) searchResultPage();
    return;
}

