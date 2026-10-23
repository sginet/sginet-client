#ifndef HEADER_SYSADV
#define HEADER_SYSADV

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#define SEV_INFO 0
#define SEV_OK 1
#define SEV_WARN 2
#define SEV_HIGH 3
#define SEV_ERROR 4

#define ADVTYPE_INFO 0
#define ADVTYPE_ROOT 1
#define ADVTYPE_DAEMON 2
#define ADVTYPE_TRUECOLOR 3

typedef struct _AdvisorEntry {
    int sev;
    int atype;
    const char *msg;
} AdvisorEntry;

typedef struct _AdvisoryList {
  int cnt;
  int capacity;
  AdvisorEntry * lst;
} AdvisoryList;

int startSystemAdvisor(AdvisoryList *lstAdv);

#endif /* HEADER_SYSADV */
