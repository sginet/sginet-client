#include "include/nc_register.h"

#include <assert.h>
#include <ctype.h>

#include "include/hwinfo_sgi.h"
#include "include/nc_tools.h"
#include "include/str_tools.h"
#include "include/sginet.h"


int validateForm(FIELD *field[], char *usr, char *machine) {
    snprintf(usr, 32, "%s", trim_whitespaces(field_buffer(field[0], 0)));
    snprintf(machine, 64, "%s", trim_whitespaces(field_buffer(field[1], 0)));
    if (strlen(usr) < 3) return 0;
    if (strlen(machine) < 2) return 0;
    return 1;  /* OK */
}

int nc_launch_reg_form(char *usr, char *machine) {
    char hostname[128];
    int registered;
    /* ncurses registration form */
    FIELD *field[3];
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
    field[0] = new_field(1, 32, 4, 27, 0, 0);
    field[1] = new_field(1, 32, 6, 27, 0, 0);
    field[2] = NULL;

    /* Set field options */
    set_field_back(field[0], A_UNDERLINE);
    field_opts_off(field[0], O_AUTOSKIP);  /* Don't go to next field when this Field is filled up */

    set_field_back(field[1], A_UNDERLINE);
    field_opts_off(field[1], O_AUTOSKIP);

    /* get basic system info */
    getHostname(hostname, sizeof(hostname));
    set_field_buffer(field[1], 0, hostname);
   
    /* Create the form and display it */
    my_form = new_form(field);
    /* Calculate the area required for the form */
    scale_form(my_form, &rows, &cols);

    /* Create the window to be associated with the form */
    my_form_win = newwin(rows + 4, cols + 4, 2, 4);
    keypad(my_form_win, TRUE);

    /* Set main window and sub window */
    set_form_win(my_form, my_form_win);
    set_form_sub(my_form, derwin(my_form_win, rows, cols, 2, 2));

    /* Print a border around the main window and print a title */
    if (TERM_MODE == TERM_ATERM) {
        box(my_form_win, 0, 0);
        print_in_middle(my_form_win, 1, 0, cols + 4, "SGINet Registration", COLOR_PAIR(1));
        
    } else { /* ANSI mode */
        mvprintw(1, 0, "SGINet Registration");
    }

    post_form(my_form);
    mvwprintw(my_form_win, 3, 2, "%s", "Register your SiliconGraphics machine on SGINet");
    mvwprintw(my_form_win, 6, 2, "%s", "Your username:");
    mvwprintw(my_form_win, 8, 2, "%s", "Name of this system:");

    refresh();
    wrefresh(my_form_win);
    mvprintw(LINES - 2, 0, "Use TAB to switch between fields. ENTER to submit. CTRL-C to abort.");

    /* relocate blinking cursor to first input field */
    wmove(my_form_win, 6, 29);  /* position cursor at first input field */
    refresh();

    /* Loop through to get user requests */
    /* while((ch = getch()) != KEY_F(1)) */
    registered = 0;
    while ((ch = wgetch(my_form_win)) != KEY_F(1) && registered == 0) {
        switch (ch) {
            case KEY_DOWN:
                form_driver(my_form, REQ_NEXT_FIELD);
                form_driver(my_form, REQ_END_LINE);
                break;
            case 9:  /* TAB */
                form_driver(my_form, REQ_NEXT_FIELD);
                form_driver(my_form, REQ_END_LINE);
                break;
            case KEY_UP:
                form_driver(my_form, REQ_PREV_FIELD);
                form_driver(my_form, REQ_END_LINE);
                break;
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
                registered = validateForm(field, usr, machine);
                break;
            default:
                form_driver(my_form, ch);
                break;
        }
        /* printf("char %i\n", ch); */
        if (registered == 1) break;
    }

    /* Un post form and free the memory */
    unpost_form(my_form);
    free_form(my_form);
    free_field(field[0]);
    free_field(field[1]);

    endwin();
    return registered;
}
