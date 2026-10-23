#ifndef HEADER_CLOUDAPI
#define HEADER_CLOUDAPI
#include <unistd.h>
#include <sys/stat.h>    /* for file size check */
#include <sys/types.h>   /* for file size check */
#include "http_client.h"
#include <json-c/json.h>

/* version and client type must now be set on init */
static char CLIENT_VERSION[24];
static char CLIENT_TYPE[4];
extern char CA_CERT_BUNDLE[512];
extern char SGINET_BIN_CLI[512];

#define CLIENT_GENERATION 1

#define MODE_ANONYMOUS 0
#define MODE_REGISTERED 1
#define MODE_DAEMON 2
#define MODE_DAEMON_PINGONLY 3
#define MODE_FORCE_HINV_UPDATE 4
#define MODE_RENAMESYSTEM 5
#define MODE_WEBSRV 20
#define MODE_SWUPDATE 90
#define MODE_SHUTDOWN 99

#define RESPONSE_OK 0
#define RESPONSE_ERROR 1
#define RESPONSE_REGISTRATION_REQUIRED 2

#define REQTYPE_HELLO 1
#define REQTYPE_PING 2
#define REQTYPE_SENDHINV 3
#define REQTYPE_SHOWONLINESYSTEMS 4    /* this is a ping with showSystems option */
#define REQTYPE_SHOWALLSYSTEMS 5       /* this is a ping with showSystems option */
#define REQTYPE_SHUTDOWN 99

typedef struct _MachineListEntry {
    int online;
    int cpuCount;
    int uptime;
    char id[128];
    char cpuCountStr[4];
    char ipmodel[5];
    char modelname[65];
    char username[65];
    char machinename[65];
    char gfx0[65];
} MachineListEntry;

typedef struct _LabelAndValue {
    char label[128];
    char value[512];
} LabelAndValue;

typedef struct _SoftwareEntry {
    char src[32];
    char pkg[256];
} SoftwareEntry;
typedef struct _ModelName {
    char name[256];
} ModelName;
typedef struct _S3Policy {
    char url[512];
    char actionStatus[4];
    char bucket[256];
    char algorithm[65];
    char credential[129];
    char date[20];
    char token[2500];
    char policy[2500];
    char signature[65];
    char uploadkey[128];
} S3Policy;
typedef struct _RSSEntry {
    char link[256];
    char title[512];
    char dt[128];
    char creator[128];
    char guid[64];
} RSSEntry;

typedef struct _RSSEntryFull {
    char link[256];
    char title[512];
    char dt[128];
    char creator[128];
    char guid[64];
    char content[4096];
} RSSEntryFull;

typedef struct _ShoutboxEntry {
    char dt[16];
    char user[32];
    char machine[64];
    char msg[256];
} ShoutboxEntry;

typedef struct _ShoutboxList {
  int cnt;
  int capacity;
  ShoutboxEntry * lst;
} ShoutboxList;

/* xsginet-globe markers */
#define MTYPE_USER 1
#define MTYPE_CITY 2
#define MTYPE_THISUSER 3

typedef struct _MarkerEntry {
    char label[128];
    float lat; /* North/South */
    float lon; /* East/West */
    double sX; /* screen x/y and eye z, set by the globe renderer */
    double sY;
    double sZ;
    int mType;
    int online;
    int score; /* importance, used to hide less important cities when zoomed out */
    int visible;
} MarkerEntry;

typedef struct _MarkerList {
  int cnt;
  int capacity;
  MarkerEntry * lst;
} MarkerList;

extern int debug;

void cloudAPI_Init(const char* client_type, const char* client_version, const char* client_bin);
void cloudAPI_SetCACertBundle(const char* path);
void cloudAPI_SetSGINetBin(const char* path);
int cloudAPI_request(int reqtype);
const char* cloudAPI_lastError(void);
int cloudAPI_register(char* username, char* machinename, char* ipmodel, int CPUCount, char* modelname);
int cloudAPI_showSystems(MachineListEntry** lstSystems, int* lstCount, int reqType);
int cloudAPI_getSystemHINVAsList(char* id, LabelAndValue** lstHinv, int* lstCount);
int cloudAPI_searchSoftware(SoftwareEntry** lstSystems, int* lstCount, char* searchquery);
int cloudAPI_getIPModelNames(ModelName** lstModelNames, int* lstCount, char* ipmodel);
int cloudAPI_listRSSNews(RSSEntry** lstResults, int* lstCount, char* action);
int cloudAPI_load_sgug_post_by_ID(RSSEntryFull** post, char* id);
int cloudAPI_onlineUpdater(int uid, int checkOnly, int* updatesAvailable, int verbose);
int cloudAPI_onlineUpdater_X11(int uid, int checkOnly, int* updatesAvailable, int verbose);
int cloudAPI_checkForUpdates(int* hasUpdates, char* downloadURL, int* downloadSize, char* version);
int cloudAPI_getSignedUploadPolicy(char* target, S3Policy** uploadPolicy);
int cloudAPI_getShoutBox(ShoutboxEntry** lstEntries, int* lstCount);
int cloudAPI_postShoutBox(char* msg);
int cloudAPI_renameSystem(char* newname);
int cloudAPI_saveMyPosition(char *returnmsg, float lat, float lng);
int cloudAPI_getMarkers(MarkerList** lstMarkers, int *isRegistered, char *username, size_t usernameSize);
void cloudAPI_freeMarkers(MarkerList *lst);
int downloadFile(char* url, char* targetPath);
int moveFile(char *source, char *target);
int isUpdateAvailable();

#endif /* HEADER_CLOUDAPI */
