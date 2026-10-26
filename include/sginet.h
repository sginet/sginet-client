#ifndef HEADER_SGINET
#define HEADER_SGINET

#ifdef IRIX5
#define THIS_CLIENT_VERSION "V20261026-1000-CL5" /* max 23 chars */
#define THIS_CLIENT_TYPE "CL5" /* must be 3 chars */
#else
#define THIS_CLIENT_VERSION "V20261026-1000-CL6" /* max 23 chars */
#define THIS_CLIENT_TYPE "CL6" /* must be 3 chars */
#endif

#define REG_NAME_SIZE 64 /* model name / machine name buffers incl. NUL */

#define TERM_DEFAULT 0
#define TERM_ATERM 1

#define PROMPT_NAV_ESC    "Arrow keys to navigate. ESC to exit.           "
#define PROMPT_NAV_ENTER  "Arrow keys to navigate. ENTER to select.       "
#define PROMPT_ENTER_ESC  "ENTER to continue. ESC to exit.                "
#define PROMPT_ENTER_QUIT "ENTER to continue. CTRL-C to quit.             "

void checkUpdatesGUI();

#endif /* HEADER_SGINET */
