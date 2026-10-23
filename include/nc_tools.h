#include <ncurses.h>
#include <string.h>

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color);
void nc_ensure_term(void);
