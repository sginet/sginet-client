#include "include/globe.h"

/* empty list, so lstMarkers is never NULL even if the cloud API can't be reached */
void initMarkerList(void) {
    lstMarkers = malloc(sizeof(MarkerList));
    if (lstMarkers == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }
    lstMarkers->cnt = 0;
    lstMarkers->capacity = 5;
    lstMarkers->lst = calloc(lstMarkers->capacity, sizeof(MarkerEntry));
    if (lstMarkers->lst == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }
}
MarkerEntry* findMyMarkerEntry(void) {
    int i;
    for (i = 0; i < lstMarkers->cnt; ++i) {
        if (lstMarkers->lst[i].mType == MTYPE_THISUSER) return &lstMarkers->lst[i];
    }
    return NULL;
}
MarkerEntry* addMarker(const char *label, int mType, float lat, float lon, int online) {
    MarkerEntry *mEntry;
    if (lstMarkers->cnt == lstMarkers->capacity) { /* grow the reserved memory */
        int newCapacity = lstMarkers->capacity * 2 + 1;
        MarkerEntry *newLst = realloc(lstMarkers->lst, sizeof(MarkerEntry) * newCapacity);
        if (newLst == NULL) return NULL;
        lstMarkers->lst = newLst;
        lstMarkers->capacity = newCapacity;
    }

    mEntry = lstMarkers->lst + lstMarkers->cnt;
    memset(mEntry, 0, sizeof(MarkerEntry));
    strncpy(mEntry->label, label, sizeof(mEntry->label) - 1);
    mEntry->mType = mType;
    mEntry->lat = lat;
    mEntry->lon = lon;
    mEntry->online = online;

    lstMarkers->cnt++;
    return mEntry;
}
