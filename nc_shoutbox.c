#include "include/nc_shoutbox.h"

#include "include/nc_tools.h"
#include "include/str_tools.h"
#include "include/sginet.h"
#include "include/cloudAPI.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

void nc_shoutbox() {
    int currentRow;
    int relaunch;
    int MAXMENUROWCOLS;
    int listHeight;
    int done;
    ITEM **my_items;
    MENU *my_menu;
    WINDOW *my_menu_win;
    int i, count, c;
    char *label;
    char *title = "SGINet ShoutBox";

    ShoutboxEntry *lstSB;
    cloudAPI_getShoutBox(&lstSB, &count);

    /* start (n)cursing */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  /* no blinky cursor */

    /* Calculate width of menu row columns */
    MAXMENUROWCOLS = COLS - 8;      /* this must fit in menu width further below */
    currentRow = 0;
    my_items = (ITEM **)calloc(count*2 + 1, sizeof(ITEM *));
    for (i = 0; i < count; ++i) {
        label = calloc(1, 371);
        sprintf(label, "--- <%s> %s@%s:",
            lstSB[i].dt,
            lstSB[i].user,
            lstSB[i].machine
        );
        my_items[currentRow++] = new_item(strdup(label), "");
        free(label);

    /* second row */
        label = calloc(1, 371);
        sprintf(label, "%s", lstSB[i].msg);
        my_items[currentRow++] = new_item(strdup(label), "");
        free(label);
    }
    my_items[currentRow] = NULL;  /* null sentinel for new_menu() */

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
    mvprintw(LINES - 1, 0, "ENTER to post a message. SPACE to refresh              "); /* PROMPT_NAV_ESC); */
    attroff(COLOR_PAIR(2));
    refresh();
    done = 0;
    relaunch = 0;
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
            case 32: /* SPACE = reload */
                done = 1;
                relaunch = 1;
                break;
            case 27:  /* ESC (but with delay) */
                done = 1;
                break;
            case 343: /* ENTER in iris-ansi */
            case 10:  /* ENTER */
            {
                nc_shoutbox_postmsg(&lstSB, &count);
                redrawwin(my_menu_win);
                done = 1;
                relaunch = 1;
                break;
            }
        }
        wrefresh(my_menu_win);
        if (done == 1) break;
    }

    /* Unpost and free all the memory taken up */
    unpost_menu(my_menu);
    free_menu(my_menu);
    for (i = 0; i < currentRow; ++i) {
        free((void *)item_name(my_items[i]));  /* free strdup'd label */
        free_item(my_items[i]);
    }
    free(my_items);
    free(lstSB);

    /* clear screen if no fancy aterm windows borders */
    if (TERM_MODE == TERM_DEFAULT) clear();
    if (relaunch == 1) nc_shoutbox(); /* not the best way, but OK for now */
}

int validateSBForm(FIELD *field[], char *msg) {
    snprintf(msg, 255, "%s", trim_whitespaces(field_buffer(field[0], 0)));
    if (strlen(msg) < 1) return 0;
    return 1;  /* OK */
}
void nc_shoutbox_postmsg() {
    int post;
    int fldSize;
    int done;
    /* ncurses registration form */
    char msg[255];
    FIELD *field[2];
    FORM *my_form;
    WINDOW *my_form_win;
    int ch, rows, cols;

    /* Initialize curses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);  /* blinky cursor */

    /* Initialize the fields (h, w, top, left, offscrRows, nbuf) */
    fldSize = (COLS > 255 ? 255 : COLS-13);
    field[0] = new_field(1, fldSize, 4, 1, 0, 0);
    field[1] = NULL;

    /* Set field options */
    set_field_back(field[0], A_UNDERLINE);
    field_opts_off(field[0], O_AUTOSKIP);  /* Don't go to next field when this Field is filled up */

    /* Create the form and display it */
    my_form = new_form(field);
    /* Calculate the area required for the form */
    scale_form(my_form, &rows, &cols);

    /* Create the window to be associated with the form */
    my_form_win = newwin(rows + 6, cols + 4, 2, 4);
    keypad(my_form_win, TRUE);

    /* Set main window and sub window */
    set_form_win(my_form, my_form_win);
    set_form_sub(my_form, derwin(my_form_win, rows, cols, 2, 2));

    /* Print a border around the main window and print a title */
    if (TERM_MODE == TERM_ATERM) {
        box(my_form_win, 0, 0);
        print_in_middle(my_form_win, 1, 0, cols + 4, "SGINet ShoutBox", COLOR_PAIR(1));
        
    } else { /* ANSI mode */
        mvprintw(1, 0, "SGINet ShoutBox");
    }

    post_form(my_form);
    mvwprintw(my_form_win, 3, 2, "%s", "Post a message in the shoutbox, visible to all SGINet members.");
    /* mvwprintw(my_form_win, 6, 2, "%s", "Message:"); */

    refresh();
    wrefresh(my_form_win);
   /* mvprintw(LINES - 2, 0, "Use TAB to switch between fields. ENTER to submit. CTRL-C to abort."); */

    /* relocate blinking cursor to first input field */
    wmove(my_form_win, 6, 3);  /* position cursor at first input field */
    refresh();

    /* Loop through to get user requests */
    done = 0;
    post = 0;
    while ((ch = wgetch(my_form_win)) != KEY_F(1) && done == 0) {
        switch (ch) {
            case KEY_LEFT:
                form_driver(my_form, REQ_PREV_CHAR);
                break;
            case KEY_RIGHT:
                form_driver(my_form, REQ_NEXT_CHAR);
                break;
            case KEY_BACKSPACE:
            case 127:
                form_driver(my_form, REQ_DEL_PREV);
                break;
            case KEY_DC:  /* DEL */
                form_driver(my_form, REQ_DEL_CHAR);
                break;
            case 343: /* ENTER in iris-ansi */
            case 10: /* ENTER */
                form_driver(my_form, REQ_VALIDATION);
                done = validateSBForm(field, msg);
                if (done == 1) post = 1;
                break;
            case 27:  /* ESC (but with delay) */
                done = 1;
                break;
            default:
                form_driver(my_form, ch);
                break;
        }
        /* printf("char %i\n", ch); */
        if (done == 1) break;
    }
    if (post) cloudAPI_postShoutBox(msg);

    /* Un post form and free the memory */
    unpost_form(my_form);
    free_form(my_form);
    free_field(field[0]);
    free_field(field[1]);

    endwin();
}
