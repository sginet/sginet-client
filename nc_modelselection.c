#include "include/nc_modelselection.h"

#include <assert.h>
#include <ctype.h>

#include "include/cloudAPI.h"
#include "include/nc_tools.h"
#include "include/str_tools.h"
#include "include/sginet.h"

void nc_enter_modelname(char *modelname);
void nc_model_selection(char *ipmodel, char *modelname) {
    int done;
    int listHeight;
    int selectedNo;
    ITEM **my_items;
    int c;
    MENU *my_menu;
    WINDOW *my_menu_win;
    int n_count, i;
    ModelName *lstModelNames;
    char title[64];
    char labelOther[] ="- other, not in list -";
    int rcIP;

    lstModelNames = NULL;
    n_count = 0;
    selectedNo = 0;
    /* Query list of possible model names for SW IPxx type: */
    rcIP = cloudAPI_getIPModelNames(&lstModelNames, &n_count, ipmodel);
    if (rcIP != RESPONSE_OK) {
        /* only offer "other, not in list" */
        if (debug) fprintf(stderr, "Failed to query model names for: %s \n", ipmodel);
        lstModelNames = NULL;
        n_count = 0;
    }

    /* start cursing */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    sprintf(title, "Select the model from the list:");
    
    /* n_count model items + 1 "Other" item + 1 NULL sentinel for new_menu() */
    my_items = (ITEM **)calloc(n_count + 2, sizeof(ITEM *));
    for (i = 0; i < n_count; ++i) {
        my_items[i] = new_item(strdup(lstModelNames[i].name), "");
    }
    my_items[n_count] = new_item(labelOther, "");
    my_items[n_count + 1] = NULL;  /* null sentinel for new_menu() */

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
    mvprintw(LINES - 1, 0, PROMPT_NAV_ENTER);
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
            case 343: /* ENTER in iris-ansi */
            case 10: /* ENTER */
                {
                    ITEM *cur;
                    cur = current_item(my_menu);
                    selectedNo =  item_index(cur);
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
    for (i = 0; i < n_count; ++i) {
        free((void *)item_name(my_items[i]));  /* free strdup'd model name */
        free_item(my_items[i]);
    }
    free_item(my_items[n_count]);  /* "Other" item (no strdup, labelOther is a local array) */
    free(my_items);

    clear();

    if (selectedNo >= n_count) {
        /* user selected 'other - not in list' */
        nc_enter_modelname(modelname);
    } else {
        /* printf("selectedEntry[%i] = %s\n", selectedNo, lstModelNames[selectedNo].name); */
        snprintf(modelname, REG_NAME_SIZE, "%s", lstModelNames[selectedNo].name);
    }
    if (lstModelNames) free(lstModelNames);
}
int validateModelnameForm(FIELD *field[], char *modelname) {
    snprintf(modelname, REG_NAME_SIZE, "%s", trim_whitespaces(field_buffer(field[0], 0)));
    if (strlen(modelname) < 2) return 0;
    return 1;  /* OK */
}
void nc_enter_modelname(char *modelname) {
    int valid;
    /* ncurses form to enter model name manually */
    FIELD *field[2];
    FORM *my_form;
    WINDOW *my_form_win;
    int ch, rows, cols;

    /* Initialize the fields (h, w, top, left, offscrRows, nbuf) */
    field[0] = new_field(1, 32, 4, 27, 0, 0);
    field[1] = NULL;

    /* Set field options */
    set_field_back(field[0], A_UNDERLINE);
    field_opts_off(field[0], O_AUTOSKIP);  /* Don't go to next field when this Field is filled up */

    my_form = new_form(field);
    scale_form(my_form, &rows, &cols);
    my_form_win = newwin(rows + 4, cols + 4, 2, 4);
    keypad(my_form_win, TRUE);

    set_form_win(my_form, my_form_win);
    set_form_sub(my_form, derwin(my_form_win, rows, cols, 2, 2));

    if (TERM_MODE == TERM_ATERM) {
        box(my_form_win, 0, 0);
        print_in_middle(my_form_win, 1, 0, cols + 4, "SGINet Registration", COLOR_PAIR(1));
        
    } else { /* ANSI mode */
        mvprintw(1, 0, "SGINet Registration");
    }

    post_form(my_form);
    mvwprintw(my_form_win, 3, 2, "%s", "Enter the model name of your SiliconGraphics machine");
    mvwprintw(my_form_win, 6, 2, "%s", "Model name:");
    
    refresh();
    wrefresh(my_form_win);
    mvprintw(LINES - 2, 0, "ENTER to submit.");

    /* relocate blinking cursor to first input field */
    wmove(my_form_win, 6, 29);  /* position cursor at first input field */
    refresh();

    /* Loop through to get user requests */
    /* while((ch = getch()) != KEY_F(1)) */
    valid = 0;
    while ((ch = wgetch(my_form_win)) && valid == 0) {
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
                valid = validateModelnameForm(field, modelname);
                break;
            default:
                form_driver(my_form, ch);
                break;
        }
        if (valid == 1) break;
    }

    /* Un post form and free the memory */
    unpost_form(my_form);
    free_form(my_form);
    free_field(field[0]);
}
