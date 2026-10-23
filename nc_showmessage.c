#include "include/sginet.h"
#include "include/nc_showmessage.h"
#include "include/nc_tools.h"

int nc_showMessage(char* title, char *message) {
    int yBodyPos;
    int xBodyPos;
    int yPos;
    int xPos;
    int panelHeight;
    int panelWidth;

    WINDOW *info_window;
    WINDOW *info_window_body;
	PANEL  *info_panel;
    PANEL  *info_panel_body;

	initscr();
	cbreak();
	noecho();

    panelWidth = COLS - 6;
    panelHeight = LINES - 4;
    xPos = 3;
    yPos = 1;
    
    xBodyPos = (TERM_MODE == TERM_ATERM ? 3 : 1);
    yBodyPos = (TERM_MODE == TERM_ATERM ? yPos+4 : 3);
    
	info_window = newwin(panelHeight, panelWidth, yPos, xPos); /* (HEIGHT, WIDTH, starty, startx) */
    info_window_body = newwin(panelHeight-5, panelWidth-4, yBodyPos, xPos+2); /* this is where we put the message */
    
    info_panel = new_panel(info_window);
    info_panel_body = new_panel(info_window_body);

    keypad(info_window, TRUE); /* this will allow ESC key */

    if (TERM_MODE == TERM_ATERM) {
        box(info_window, 0, 0);
        print_in_middle(info_window, 1, 0, COLS - 6, title, COLOR_PAIR(1));
        mvwaddch(info_window, 2, 0, ACS_LTEE);
        mvwhline(info_window, 2, 1, ACS_HLINE, panelWidth-2);
        mvwaddch(info_window, 2, panelWidth, ACS_RTEE);        
        mvwprintw(info_window_body, 0, 0, "%s", message); /* y, x (within border window) */
    } else {  /* ANSI mode */
        mvprintw(0, xPos, "%s", title);
        mvwprintw(info_window_body, 0, 0, "%s", message); /* y, x (within border window) */
    }
    mvprintw(LINES - 1, 0, PROMPT_ENTER_ESC);

	update_panels();
	doupdate();
	
	getch();
	endwin();
}
