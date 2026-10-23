#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/cloudAPI.h"
#include "include/hwinfo_sgi.h"

#define SGINET_SW_ENDPOINT "https://sginet-api.flexion.io/sginet/prod/sginet-sw"
#define SGINET_API "https://sginet-api.flexion.io/sginet/prod/sginet-client"
#define SGINET_API_FILES "https://sginet-api.flexion.io/sginet/prod/sginet-files"
#define SGINET_RSS_ENDPOINT "https://sginet-api.flexion.io/sginet/prod/sginet-rss"
#define SGINET_API_GLOBE "https://sginet-api.flexion.io/sginet/prod/sginet-globe"

#define SW_ALL 0
#define SW_SGUG_RSE 1
#define SW_NEKOWARE 2
#define SW_FREEWARE_SGI 3

extern int G_MODE;
extern char G_CLIENTID[64];
int updateAvailable = 0;
char CA_CERT_BUNDLE[512];
char SGINET_BIN_CLI[512];
static char lastError[256];

void cloudAPI_Init(const char* client_type, const char* client_version, const char* client_bin) {
    strncpy(CLIENT_TYPE, client_type, sizeof(CLIENT_TYPE) - 1);
    CLIENT_TYPE[sizeof(CLIENT_TYPE) - 1] = '\0';
    strncpy(CLIENT_VERSION, client_version, sizeof(CLIENT_VERSION) - 1);
    CLIENT_VERSION[sizeof(CLIENT_VERSION) - 1] = '\0';
    if (SGINET_BIN_CLI[0] == '\0') { /* keep a --sginet-bin override */
        strncpy(SGINET_BIN_CLI, client_bin, sizeof(SGINET_BIN_CLI) - 1);
        SGINET_BIN_CLI[sizeof(SGINET_BIN_CLI) - 1] = '\0';
    }
}

void cloudAPI_SetSGINetBin(const char* path) {
    strncpy(SGINET_BIN_CLI, path, sizeof(SGINET_BIN_CLI) - 1);
    SGINET_BIN_CLI[sizeof(SGINET_BIN_CLI) - 1] = '\0';
}

void cloudAPI_SetCACertBundle(const char* path) {
    strncpy(CA_CERT_BUNDLE, path, sizeof(CA_CERT_BUNDLE) - 1);
    CA_CERT_BUNDLE[sizeof(CA_CERT_BUNDLE) - 1] = '\0';
    if (http_set_ca_bundle(CA_CERT_BUNDLE) != HTTP_OK) {
        fprintf(stderr, "WARNING: %s\n", http_last_error());
    }
}
const char* cloudAPI_lastError(void) {
    /* Reason of the last failed cloudAPI_request(), empty string if none */
    return lastError;
}
int isUpdateAvailable() {
    return updateAvailable;
}
int requestPOSTObj(char *url, struct json_object **jobj, struct json_object *jsonPayload) {
    const char *body;
    HttpBuffer resp;
    int status;
    int rc;
    if (debug) fprintf(stderr, "POST: %s\n", url);
    rc = 0;
    status = 0;
    body = json_object_to_json_string_ext(jsonPayload, JSON_C_TO_STRING_PLAIN);

    *jobj = NULL;
    if (http_post(url, "application/json", body, strlen(body), &resp, &status) != HTTP_OK) {
        fprintf(stderr, "ERROR! POST request failed: %s\n", http_last_error());
    } else {
        enum json_tokener_error jerr;
        if (debug) fprintf(stderr, "%lu bytes loaded\n", (unsigned long)resp.size);
        jerr = json_tokener_success;
        *jobj = json_tokener_parse_verbose(resp.data, &jerr);
        if (debug && *jobj == NULL) {
            fprintf(stderr, "JSON parse error: %s\n", json_tokener_error_desc(jerr));
            fprintf(stderr, "raw response (%lu bytes):\n%s\n", (unsigned long)resp.size, resp.data);
        }
        rc = 1;
    }
    http_buffer_free(&resp);
    return rc;
}
void checkForHINVRequest(struct json_object *jResponse) {
    /* Check response payload for requestHINV:boolean */
    json_object *requestHINV = json_object_object_get(jResponse, "requestHINV");
    if (json_object_get_boolean(requestHINV)) {
        if (debug) fprintf(stderr, "HINV update requested by server.\n");
        cloudAPI_request(REQTYPE_SENDHINV);
    }    
}

void jsonAddClientInfo(struct json_object *jPayload) {
    double avg1;
    double avg5;
    double avg15;
    /* Attach client info */
    json_object *clientinfo = json_object_new_object();
    json_object_object_add(clientinfo, "version", json_object_new_string(CLIENT_VERSION));
    json_object_object_add(clientinfo, "clientType", json_object_new_string(CLIENT_TYPE));
    json_object_object_add(clientinfo, "mode", json_object_new_int(G_MODE));
    json_object_object_add(clientinfo, "gen", json_object_new_int(CLIENT_GENERATION));
    json_object_object_add(clientinfo, "clientID", json_object_new_string(G_CLIENTID));
    json_object_object_add(clientinfo, "uptime", json_object_new_int(getUptime()));

    if (parse_uptime_loadavg(&avg1, &avg5, &avg15) == 0) {
        json_object *AVGInfo = json_object_new_object();
        json_object_object_add(AVGInfo, "avg1", json_object_new_double(avg1));
        json_object_object_add(AVGInfo, "avg5", json_object_new_double(avg5));
        json_object_object_add(AVGInfo, "avg15", json_object_new_double(avg15));
        json_object_object_add(clientinfo, "avg", AVGInfo);
    }

    json_object_object_add(jPayload, "client", clientinfo);
}
int cloudAPI_saveMyPosition(char *returnmsg, float lat, float lng) {
    json_object *jPayload;
    struct json_object *jResponse;
    int rc;
    int httpRC;
    json_object *success;
    json_object *msg;
    if (debug) fprintf(stderr, "Saving my location at lat:%f long:%f\n", lat, lng);
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    json_object_object_add(jPayload, "action", json_object_new_string("saveMyPosition"));
    jsonAddClientInfo(jPayload); /* Attach client info */

    json_object_object_add(jPayload, "lat", json_object_new_double(lat));
    json_object_object_add(jPayload, "lng", json_object_new_double(lng));

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API_GLOBE, &jResponse, jPayload);
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        sprintf(returnmsg, "%s", "Failed to connect to SGINet API");
        return RESPONSE_ERROR;
    }
    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success);
    
    msg = json_object_object_get(jResponse, "message");
    if (msg) sprintf(returnmsg, "%s", json_object_get_string(msg));

    if (rc != 1) {
        json_object_put(jResponse); /* clean up */
        printf("Failed to save location\n");
        return RESPONSE_ERROR;
    }
    
    json_object_put(jResponse); /* clean up */
    return RESPONSE_OK;
}
int cloudAPI_searchSoftware(SoftwareEntry** lstResults, int* lstCount, char* searchquery) {
    json_object *jPayload;
    struct json_object *jResponse;
    int maxresults;
    int rc;
    int httpRC;
    json_object *success;
    if (debug) fprintf(stderr, "cloudAPI_searchSoftware(%s)\n", searchquery);
    *lstCount = 0;
    maxresults = 50;
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;

    json_object_object_add(jPayload, "action", json_object_new_string("softwaresearch"));
    json_object_object_add(jPayload, "query", json_object_new_string(searchquery));
    json_object_object_add(jPayload, "scope", json_object_new_int(SW_ALL));
    
    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_SW_ENDPOINT, &jResponse, jPayload);

    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));

    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success) ? RESPONSE_OK : RESPONSE_ERROR;
    if (rc == RESPONSE_OK) {
        /* json_object *systems = json_object_object_get(jResponse, "systems"); */
        json_object *lst = json_object_object_get(jResponse, "lst");
        if (lst) {
            SoftwareEntry *sw;
            int i;
            int cnt = json_object_array_length(lst);
            if (cnt > maxresults) cnt = maxresults;
            sw = malloc(cnt * sizeof(SoftwareEntry));
            if (!lst) {
                fprintf(stderr, "malloc failed\n");
                return RESPONSE_ERROR;
            }
            for (i=0; i<cnt; i++) {
                json_object *entry = json_object_array_get_idx(lst, i);

                json_object *srcname = json_object_object_get(entry, "srcname");
                json_object *fn = json_object_object_get(entry, "fn");
/*                json_object *ver = json_object_object_get(entry, "ver"); */

                sprintf(sw[i].src, "%s", json_object_get_string(srcname));
                sprintf(sw[i].pkg, "%s", json_object_get_string(fn));
            }
             *lstResults = sw;
             *lstCount = cnt;
        }
    }
    json_object_put(jResponse); /* clean up */
    return rc;
}
int cloudAPI_getSystemHINVAsList(char* id, LabelAndValue** lstHinv, int* lstCount) {
    json_object *jPayload;
    struct json_object *jResponse;
    int rc;
    int httpRC;
    json_object *success;
    if (debug) fprintf(stderr, "cloudAPI_getSystemHINVAsList(%s)\n", id);
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    
    json_object_object_add(jPayload, "action", json_object_new_string("getSystemHINVAsList"));
    json_object_object_add(jPayload, "id", json_object_new_string(id));

    jsonAddClientInfo(jPayload); /* Attach client info */

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API, &jResponse, jPayload);
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));

    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success) ? RESPONSE_OK : RESPONSE_ERROR;
    if (rc == RESPONSE_OK) {
        json_object *lst = json_object_object_get(jResponse, "lst");
        if (lst) {
            int i;
            int cnt = json_object_array_length(lst);
            
            LabelAndValue* labelList = malloc(cnt * sizeof(LabelAndValue));
            if (!labelList) {
                fprintf(stderr, "malloc failed\n");
                return RESPONSE_ERROR;
            }
            for (i=0; i<cnt; i++) {
                json_object *entry = json_object_array_get_idx(lst, i);

                json_object *strLabel = json_object_object_get(entry, "label");
                json_object *strText = json_object_object_get(entry, "text");
                
                sprintf(labelList[i].label, "%s", json_object_get_string(strLabel));
                sprintf(labelList[i].value, "%s", json_object_get_string(strText));
            }
             *lstHinv = labelList;
             *lstCount = cnt;
        }
    }
    json_object_put(jResponse); /* clean up */
    return rc;
}

static void copyJSONString(char *dst, size_t size, json_object *jstr) {
    /* Copy a JSON string into a fixed size buffer, truncated, empty string when field is missing */
    const char *str = json_object_get_string(jstr);
    snprintf(dst, size, "%s", str ? str : "");
}
void cloudAPI_freeMarkers(MarkerList *lst) {
    if (lst == NULL) return;
    if (lst->lst) free(lst->lst);
    free(lst);
}
int cloudAPI_getMarkers(MarkerList** lstMarkers, int *isRegistered, char *username, size_t usernameSize) {
    json_object *jPayload;
    struct json_object *jResponse;
    int rc;
    int httpRC;
    json_object *success;
    json_object *registered;
    json_object *usrname;
    if (debug) fprintf(stderr, "cloudAPI_getMarkers()\n");
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;

    json_object_object_add(jPayload, "action", json_object_new_string("getMarkers"));
    jsonAddClientInfo(jPayload); /* Attach client info */

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API_GLOBE, &jResponse, jPayload);
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1 || jResponse == NULL) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));

    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success) ? RESPONSE_OK : RESPONSE_ERROR;
    if (rc == RESPONSE_OK) {
        json_object *markers = json_object_object_get(jResponse, "markers");
        json_object *lstM = json_object_object_get(markers, "lst");
        if (json_object_is_type(lstM, json_type_array)) {
            MarkerList *newList;
            int i;
            int cnt = json_object_array_length(lstM);
            newList = malloc(sizeof(MarkerList));
            if (newList == NULL) {
                json_object_put(jResponse);
                return RESPONSE_ERROR;
            }
            newList->cnt = cnt;
            newList->capacity = cnt + 1; /* one more entry for the marker the user sets for themselves */
            newList->lst = calloc(newList->capacity, sizeof(MarkerEntry));
            if (newList->lst == NULL) {
                free(newList);
                json_object_put(jResponse);
                return RESPONSE_ERROR;
            }
            for (i = 0; i < cnt; i++) {
                json_object *entry = json_object_array_get_idx(lstM, i);
                json_object *score = json_object_object_get(entry, "s");
                MarkerEntry *m = &newList->lst[i];
                m->mType = json_object_get_int(json_object_object_get(entry, "mType"));
                m->online = json_object_get_int(json_object_object_get(entry, "online"));
                m->lat = (float)json_object_get_double(json_object_object_get(entry, "lat"));
                m->lon = (float)json_object_get_double(json_object_object_get(entry, "lng"));
                m->score = score ? json_object_get_int(score) : 150; /* default: always shown */
                copyJSONString(m->label, sizeof(m->label), json_object_object_get(entry, "label"));
            }
            cloudAPI_freeMarkers(*lstMarkers);
            *lstMarkers = newList;
        }
    }

    /* read return values for user */
    registered = json_object_object_get(jResponse, "isRegistered");
    *isRegistered = json_object_get_boolean(registered);

    usrname = json_object_object_get(jResponse, "username");
    if (usrname) copyJSONString(username, usernameSize, usrname);
    json_object_put(jResponse); /* clean up */
    return rc;
}
int cloudAPI_showSystems(MachineListEntry** lstSystems, int* lstCount, int reqType) {
    json_object *jPayload;
    struct json_object *jResponse;
    int rc;
    int httpRC;
    json_object *success;
    if (debug) fprintf(stderr, "cloudAPI_showSystems(%i)\n", reqType);
    lastError[0] = '\0';
    *lstCount = 0;
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    
    json_object_object_add(jPayload, "action", json_object_new_string("ping"));
    json_object_object_add(jPayload, "showSystems", json_object_new_int(reqType));

    jsonAddClientInfo(jPayload); /* Attach client info */

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API, &jResponse, jPayload);
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        snprintf(lastError, sizeof(lastError), "%s", http_last_error());
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));

    success = json_object_object_get(jResponse, "success");
    if (!json_object_get_boolean(success)) copyJSONString(lastError, sizeof(lastError), json_object_object_get(jResponse, "message"));
    /* rc = json_object_get_boolean(success) ? RESPONSE_OK : RESPONSE_ERROR; */
    /* 202050531 if backend responds with success false, we assume machine is not registered */
    /* TODO: update backend with an error handler for this specific error (The conditional request failed) */
    rc = json_object_get_boolean(success) ? RESPONSE_OK : RESPONSE_REGISTRATION_REQUIRED;
    if (rc == RESPONSE_OK) {
        json_object *systems = json_object_object_get(jResponse, "systems");
        if (systems) {
            json_object *lstSys = json_object_object_get(systems, "lst");
            int i;
            int cnt;
            MachineListEntry* lst;
            if (!json_object_is_type(lstSys, json_type_array)) {
                json_object_put(jResponse);
                return RESPONSE_ERROR;
            }
            cnt = json_object_array_length(lstSys);
            /* malloc(0) may return NULL on IRIX, allocate at least one entry */
            lst = malloc((cnt > 0 ? cnt : 1) * sizeof(MachineListEntry));
            if (!lst) {
                fprintf(stderr, "malloc failed\n");
                json_object_put(jResponse);
                return RESPONSE_ERROR;
            }
            for (i=0; i<cnt; i++) {
                json_object *entry = json_object_array_get_idx(lstSys, i);

                json_object *machinename = json_object_object_get(entry, "machinename");
                json_object *username = json_object_object_get(entry, "username");
                json_object *cpucount = json_object_object_get(entry, "CPUcount");
                json_object *cpucountStr = json_object_object_get(entry, "CPUcountStr");
                json_object *ipmodel = json_object_object_get(entry, "ipmodel");
                json_object *modelname = json_object_object_get(entry, "modelname");
                json_object *gfx0 = json_object_object_get(entry, "gfx0");
                json_object *online = json_object_object_get(entry, "online");
                json_object *id = json_object_object_get(entry, "GSI1SK");
                json_object *uptime = json_object_object_get(entry, "uptime");
                /*{
                    "CPUcount": 128,
                    "CPUcountStr": "128"
                    "username": "flexion",
                    "ipmodel": "IP30",
                    "machinename": "octane2.indy.flexion.ch",
                    "online": 1,
                    "gfx0": "VPro"
                }*/
                lst[i].cpuCount = json_object_get_int(cpucount);
                lst[i].online = json_object_get_int(online);
                lst[i].uptime = json_object_get_int(uptime);
                copyJSONString(lst[i].cpuCountStr, sizeof(lst[i].cpuCountStr), cpucountStr);
                copyJSONString(lst[i].machinename, sizeof(lst[i].machinename), machinename);
                copyJSONString(lst[i].username, sizeof(lst[i].username), username);
                copyJSONString(lst[i].ipmodel, sizeof(lst[i].ipmodel), ipmodel);
                copyJSONString(lst[i].modelname, sizeof(lst[i].modelname), modelname);
                copyJSONString(lst[i].gfx0, sizeof(lst[i].gfx0), gfx0);
                copyJSONString(lst[i].id, sizeof(lst[i].id), id);
            }
             *lstSystems = lst;
             *lstCount = cnt;
        }
    }
    json_object_put(jResponse); /* clean up */
    return rc;
}
int cloudAPI_request(int reqtype) {
    json_object *jPayload;
    struct json_object *jResponse;
    int rc;
    int httpRC;
    json_object *success;
    if (debug) fprintf(stderr, "cloudAPI_request(%i)\n", reqtype);
    lastError[0] = '\0';
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    
    if (reqtype == REQTYPE_HELLO) {
        json_object_object_add(jPayload, "action", json_object_new_string("hello"));
    } else if (reqtype == REQTYPE_PING || reqtype == REQTYPE_SHOWONLINESYSTEMS) {
        json_object_object_add(jPayload, "action", json_object_new_string("ping"));
        /* if (reqtype == REQTYPE_SHOWONLINESYSTEMS || reqtype == REQTYPE_SHOWALLSYSTEMS) json_object_object_add(jPayload, "showSystems", json_object_new_int(reqtype)); */
    } else if (reqtype == REQTYPE_SHUTDOWN) {
        json_object_object_add(jPayload, "action", json_object_new_string("shutdown"));
    } else if (reqtype == REQTYPE_SENDHINV) {
        json_object_object_add(jPayload, "action", json_object_new_string("updateHINV"));
        getHINVObject(jPayload);
    } else {
        json_object_put(jPayload);
        return EXIT_FAILURE;
    }
    
    /* Attach client info */
    jsonAddClientInfo(jPayload);

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API, &jResponse, jPayload);
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        snprintf(lastError, sizeof(lastError), "%s", http_last_error());
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));

    success = json_object_object_get(jResponse, "success");
    if (json_object_get_boolean(success)) {
        rc = RESPONSE_OK;
        if (reqtype == REQTYPE_HELLO) {
            json_object *upd = json_object_object_get(jResponse, "updateAvailable");
            if (upd) updateAvailable = json_object_get_boolean(upd);
        }
        /* Send hinv if requested by server */
        checkForHINVRequest(jResponse);
    } else { /* failed */
        json_object *responseCode;
        rc = RESPONSE_ERROR;
        copyJSONString(lastError, sizeof(lastError), json_object_object_get(jResponse, "message"));
        /* if response.action == "register" return RESPONSE_REGISTRATION_REQUIRED; */
        responseCode = json_object_object_get(jResponse, "code");
        if (responseCode) {
            if (debug) fprintf(stderr, "responseCode: %i\n", json_object_get_int(responseCode));
            if (json_object_get_int(responseCode) == RESPONSE_REGISTRATION_REQUIRED) rc = RESPONSE_REGISTRATION_REQUIRED;
        }
    }

    json_object_put(jResponse); /* clean up */
    return rc;
}

int cloudAPI_register(char* username, char* machinename, char* ipmodel, int CPUCount, char* modelname) {
    json_object *jPayload;
    int rc;
    struct json_object *jResponse;
    int httpRC;
    json_object *success;
    if (debug) fprintf(stderr, "cloudAPI_register\n");
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    json_object_object_add(jPayload, "action", json_object_new_string("register"));
    json_object_object_add(jPayload, "clientID", json_object_new_string(G_CLIENTID));
    json_object_object_add(jPayload, "username", json_object_new_string(username));
    json_object_object_add(jPayload, "machinename", json_object_new_string(machinename));
    json_object_object_add(jPayload, "ipmodel", json_object_new_string(ipmodel));
    json_object_object_add(jPayload, "modelname", json_object_new_string(modelname));
    json_object_object_add(jPayload, "CPUcount", json_object_new_int(CPUCount));
    json_object_object_add(jPayload, "clientVersion", json_object_new_string(CLIENT_VERSION));

    httpRC = requestPOSTObj(SGINET_API, &jResponse, jPayload);
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success);
    if (rc != 1) {
        json_object_put(jResponse); /* clean up */
        printf("Registration failed\n");
        return RESPONSE_ERROR;
    }
    /* Send hinv if requested by server */
    checkForHINVRequest(jResponse);

    json_object_put(jResponse); /* clean up */
    return RESPONSE_OK;
}
int cloudAPI_getIPModelNames(ModelName** lstModelNames, int* lstCount, char* ipmodel) {
    int cnt;
    json_object *jPayload;
    int rc;
    int i;
    struct json_object *jResponse;
    int httpRC;
    json_object *success;
    json_object *lstModels;
    ModelName *lst;
    if (debug) fprintf(stderr, "cloudAPI_getIPModelNames(%s)\n", ipmodel);
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    json_object_object_add(jPayload, "action", json_object_new_string("IP2Model"));
    json_object_object_add(jPayload, "clientID", json_object_new_string(G_CLIENTID));
    json_object_object_add(jPayload, "IPModel", json_object_new_string(ipmodel));
    json_object_object_add(jPayload, "clientVersion", json_object_new_string(CLIENT_VERSION));
    
    httpRC = requestPOSTObj(SGINET_API, &jResponse, jPayload);
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success);
    if (rc != 1) {
        json_object_put(jResponse); /* clean up */
        printf("Query IP model failed\n");
        return RESPONSE_ERROR;
    }
    
    lstModels = json_object_object_get(jResponse, "modelnames");
    cnt = json_object_array_length(lstModels);
    if (debug) fprintf(stderr, "model names found: %i\n", cnt);
    
    lst = malloc(cnt * sizeof(ModelName));
    if (!lst) {
        fprintf(stderr, "malloc failed\n");
        return RESPONSE_ERROR;
    }
    for (i=0; i<cnt; i++) {
        json_object *entry = json_object_array_get_idx(lstModels, i);
        sprintf(lst[i].name, "%s", json_object_get_string(entry));
    }
    *lstModelNames = lst;
    *lstCount = cnt;
    
    json_object_put(jResponse); /* clean up */
    return RESPONSE_OK;
}
int moveFile(char *source, char *target) {
  int status=0;
  FILE *fp=NULL;
  char command[1200];
  if (snprintf(command, sizeof(command), "mv %s %s", source, target) >= (int)sizeof(command)) return 0;
  if ((fp=popen(command,"r")) != NULL) status = (pclose(fp) == 0); /* exit status of mv */
  return (status);
}

int cloudAPI_onlineUpdater_X11(int uid, int checkOnly, int* updatesAvailable, int verbose) {
    /* Online updater for the X11 clients (xsginet, xsginet-globe) */
    /* attention, executed as root when uid = 0 */
    int rc;    
    int hasUpdates;
    int downloadSize;
    char downloadURL[256];
    char version[32];
    char tempfile[sizeof(SGINET_BIN_CLI) + 16];
    const char *binName;

    binName = strrchr(SGINET_BIN_CLI, '/');
    binName = binName ? binName + 1 : SGINET_BIN_CLI;
    snprintf(tempfile, sizeof(tempfile), "/tmp/%s.tmp", binName);

    if (access(SGINET_BIN_CLI, F_OK) != 0) {
        printf("Error: Unable to locate your current %s binary in: %s\n", binName, SGINET_BIN_CLI);
        return RESPONSE_ERROR;
    }
    
    if (verbose) printf("You are using version: %s\n", CLIENT_VERSION);
    if (debug) fprintf(stderr, "Checking for online updates..\n");
    
    rc = cloudAPI_checkForUpdates(&hasUpdates, downloadURL, &downloadSize, version);
    if (rc != RESPONSE_OK) {
        if (verbose) printf("Failed to check for updates\n");
        return rc;
    }

    if (hasUpdates==1) {
        if (verbose) printf("Update available: %s\n", version);

        if (uid == 0 && checkOnly !=1) { /* user is root */
            struct stat st;
            int fRC;
            printf("Press ENTER to download and install update, or CTRL-C to abort\n");
            getchar();
            printf("Downloading.. (%d bytes)\n", downloadSize);
            
            downloadFile(downloadURL, tempfile);
            /* check file size (don't want to pull in a pile of md5 dependencies) */
            if (stat(tempfile, &st) != 0) {
                printf("Update failed. Unable to verify downloaded data! [e01]\n");
                return RESPONSE_ERROR;
            }
            if (debug) fprintf(stderr, "Size of downloaded file: %lld\nUpdating permissions..\n", (long long)st.st_size);
            
            chmod(tempfile, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

            if (st.st_size != downloadSize) {
                printf("Error! Size of downloaded update is incorrect! [e02]\n");
                return RESPONSE_ERROR;
            }
            printf("Installing update..\n");

            fRC = moveFile(tempfile, SGINET_BIN_CLI);
            if (fRC!=1) {
                printf("Error! Failed to install new %s binary! [e03]\n", binName);
                return RESPONSE_ERROR;
            }
        } else {
            if (verbose) printf("To upgrade %s to the latest version run '%s -u' as root\n", binName, binName);
        }
    } else {
        if (verbose) printf("No updates available. You are running the latest version\n");
    }
    *updatesAvailable = hasUpdates;
    return RESPONSE_OK;
}
int cloudAPI_onlineUpdater(int uid, int checkOnly, int* updatesAvailable, int verbose) {
    int rc;
    /* attention, executed as root when uid = 0 */
    
    int hasUpdates;
    int downloadSize;
    char downloadURL[256];
    char version[32];
    char* tempfile = "/tmp/sginet.tmp";

    if (access(SGINET_BIN_CLI, F_OK) != 0) {
        printf("Error: Unable to locate your current sginet binary in: %s\n", SGINET_BIN_CLI);
        return RESPONSE_ERROR;
    }
    
    if (verbose) printf("You are using version: %s\n", CLIENT_VERSION);
    if (debug) fprintf(stderr, "Checking for online updates..\n");
    
    rc = cloudAPI_checkForUpdates(&hasUpdates, downloadURL, &downloadSize, version);

    if (hasUpdates==1) {
        if (verbose) printf("Update available: %s\n", version);

        if (uid == 0 && checkOnly !=1) { /* user is root */
            struct stat st;
            int fRC;
            printf("Press ENTER to download and install update, or CTRL-C to abort\n");
            getchar();
            printf("Downloading.. (%d bytes)\n", downloadSize);
            
            downloadFile(downloadURL, tempfile);
            /* check file size (don't want to pull in a pile of md5 dependencies) */
            if (stat(tempfile, &st) != 0) {
                printf("Update failed. Unable to verify downloaded data! [e01]\n");
                return RESPONSE_ERROR;
            }
            if (debug) fprintf(stderr, "Size of downloaded file: %lld\nUpdating permissions..\n", (long long)st.st_size);
            
            chmod(tempfile, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

            if (st.st_size != downloadSize) {
                printf("Error! Size of downloaded update is incorrect! [e02]\n");
                return RESPONSE_ERROR;
            }
            printf("Installing update..\n");

            fRC = moveFile(tempfile, SGINET_BIN_CLI);
            if (fRC!=1) {
                printf("Error! Failed to install new sginet binary! [e03]\n");
                return RESPONSE_ERROR;
            }

            printf("Update installed! sginet-daemon restart recommended. \n(sudo /etc/init.d/sginet-daemon restart)\n");
        } else {
            if (verbose) printf("To upgrade sginet to the latest version run 'sginet -u' as root\n");
        }
    } else {
        if (verbose) printf("No updates available. You are running the latest version\n");
    }
    *updatesAvailable = hasUpdates;
    return RESPONSE_OK;
}
int cloudAPI_checkForUpdates(int* hasUpdates, char* downloadURL, int* downloadSize, char* version) {
    json_object *jPayload;
    struct json_object *jResponse;
    int rc;
    int httpRC;
    json_object *success;
    *hasUpdates = 0;
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;

    json_object_object_add(jPayload, "action", json_object_new_string("check-updates"));
    
    /* Attach client info */
    jsonAddClientInfo(jPayload);

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API, &jResponse, jPayload);

    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));

    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success) ? RESPONSE_OK : RESPONSE_ERROR;
    if (rc == RESPONSE_OK) {
        json_object *hasUpdate = json_object_object_get(jResponse, "hasUpdate");
        if (json_object_get_boolean(hasUpdate) == 1) {
            json_object *url;
            json_object *latestVersion;

            json_object *fSize = json_object_object_get(jResponse, "fileSize");
            *downloadSize = json_object_get_int(fSize);

            url = json_object_object_get(jResponse, "url");
            sprintf(downloadURL, "%s", json_object_get_string(url));
            
            latestVersion = json_object_object_get(jResponse, "latestVersion");
            sprintf(version, "%s", json_object_get_string(latestVersion));

            *hasUpdates = 1;
        }
    }
    json_object_put(jResponse); /* clean up */
    return rc;
}
int downloadFile(char* url, char* targetPath) {
    FILE *fp;
    int status;
    int rc;
    if (debug) fprintf(stderr, "Downloading file: %s\nTarget path: %s\n", url, targetPath);
    status = 0;
    fp = fopen(targetPath, "wb");
    if (fp == NULL) {
        fprintf(stderr, "ERROR! Cannot write %s\n", targetPath);
        return RESPONSE_ERROR;
    }
    rc = http_get_to_file(url, fp, &status);
    fclose(fp);
    if (rc != HTTP_OK) {
        fprintf(stderr, "ERROR! Download failed: %s\n", http_last_error());
        return RESPONSE_ERROR;
    }
    if (status < 200 || status > 299) {
        fprintf(stderr, "ERROR! Download failed: HTTP status %d\n", status);
        return RESPONSE_ERROR;
    }
    return RESPONSE_OK;
}
int cloudAPI_getSignedUploadPolicy(char* target, S3Policy** uploadPolicy) {
    json_object *jPayload;
    int rc;
    json_object *url;
    json_object *actionstatus;
    S3Policy *pol;
    struct json_object *jResponse;
    int httpRC;
    json_object *success;
    json_object *uploadkey;
    json_object *policy;
    json_object *fields;
    json_object *bucket;
    json_object *algorithm;
    json_object *credential;
    json_object *date;
    json_object *token;
    json_object *policyData;
    json_object *signature;
    if (debug) fprintf(stderr, "cloudAPI_getSignedUploadPolicy(%s)\n", target);

    pol = malloc(sizeof(S3Policy));
    if (!pol) {
        fprintf(stderr, "Malloc failed for S3Policy\n");
        return RESPONSE_ERROR;
    }
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    json_object_object_add(jPayload, "action", json_object_new_string("getSignedUploadPolicy"));
    json_object_object_add(jPayload, "target", json_object_new_string(target));
        
    jsonAddClientInfo(jPayload); /* Attach client info */

    /* json_object_object_add(jPayload, "accesskey", json_object_new_string("-")); */
    
    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API_FILES, &jResponse, jPayload);
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success);
    if (rc != 1) {
        json_object_put(jResponse); /* clean up */
        printf("Failed to get signed upload policy!\n");
        return RESPONSE_ERROR;
    }

    uploadkey = json_object_object_get(jResponse, "uploadkey"); /* s3 key prefix, incl. users sub directory */
    sprintf(pol->uploadkey, "%s", json_object_get_string(uploadkey));

    policy = json_object_object_get(jResponse, "policy");
    url = json_object_object_get(policy, "url");    
    sprintf(pol->url, "%s", json_object_get_string(url));
    
    fields = json_object_object_get(policy, "fields");

    actionstatus = json_object_object_get(fields, "success_action_status");
    sprintf(pol->actionStatus, "%s", json_object_get_string(actionstatus));

    bucket = json_object_object_get(fields, "bucket");
    sprintf(pol->bucket, "%s", json_object_get_string(bucket));

    algorithm = json_object_object_get(fields, "X-Amz-Algorithm");
    sprintf(pol->algorithm, "%s", json_object_get_string(algorithm));

    credential = json_object_object_get(fields, "X-Amz-Credential");
    sprintf(pol->credential, "%s", json_object_get_string(credential));

    date = json_object_object_get(fields, "X-Amz-Date");
    sprintf(pol->date, "%s", json_object_get_string(date));
  
    token = json_object_object_get(fields, "X-Amz-Security-Token");
    sprintf(pol->token, "%s", json_object_get_string(token));

    policyData = json_object_object_get(fields, "Policy");
    sprintf(pol->policy, "%s", json_object_get_string(policyData));

    signature = json_object_object_get(fields, "X-Amz-Signature");
    sprintf(pol->signature, "%s", json_object_get_string(signature));
    
    *uploadPolicy = pol;

    json_object_put(jResponse); /* clean up */
    return RESPONSE_OK;
}
int cloudAPI_postShoutBox(char* msg) {
    json_object *jPayload;
    struct json_object *jResponse;
    int rc;
    int httpRC;
    json_object *success;
    if (debug) fprintf(stderr, "cloudAPI_postShoutBox()\n");
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    json_object_object_add(jPayload, "action", json_object_new_string("postShoutbox"));
    json_object_object_add(jPayload, "message", json_object_new_string(msg));
    jsonAddClientInfo(jPayload); /* Attach client info */
    
    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API, &jResponse, jPayload);

    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));

    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success) ? RESPONSE_OK : RESPONSE_ERROR;
    json_object_put(jResponse); /* clean up */
    return rc;
}
int cloudAPI_getShoutBox(ShoutboxEntry** lstEntries, int* lstCount) {
    json_object *jPayload;
    struct json_object *jResponse;
    int maxresults;
    int rc;
    int httpRC;
    json_object *success;
    if (debug) fprintf(stderr, "cloudAPI_getShoutBox()\n");
    *lstCount = 0;
    maxresults = 50;
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    json_object_object_add(jPayload, "action", json_object_new_string("getShoutbox"));

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API, &jResponse, jPayload);

    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));

    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success) ? RESPONSE_OK : RESPONSE_ERROR;
    if (rc == RESPONSE_OK) {
        json_object *lst = json_object_object_get(jResponse, "lst");
        if (lst) {
            ShoutboxEntry *sbEntry;
            int i;
            int cnt = json_object_array_length(lst);
            if (cnt > maxresults) cnt = maxresults;
            sbEntry = malloc(cnt * sizeof(ShoutboxEntry));
            if (!lst) {
                fprintf(stderr, "malloc failed\n");
                return RESPONSE_ERROR;
            }
            for (i=0; i<cnt; i++) {
                json_object *entry = json_object_array_get_idx(lst, i);

                json_object *dt = json_object_object_get(entry, "dt");
                json_object *user = json_object_object_get(entry, "username");
                json_object *machine = json_object_object_get(entry, "machine");
                json_object *msg = json_object_object_get(entry, "msg");

                sprintf(sbEntry[i].dt, "%s", json_object_get_string(dt));
                sprintf(sbEntry[i].user, "%s", json_object_get_string(user));
                sprintf(sbEntry[i].machine, "%s", json_object_get_string(machine));
                sprintf(sbEntry[i].msg, "%s", json_object_get_string(msg));
            }
             *lstEntries = sbEntry;
             *lstCount = cnt;
        }
    }
    json_object_put(jResponse); /* clean up */
    return rc;
}
int cloudAPI_load_sgug_post_by_ID(RSSEntryFull** post, char* id) {
    json_object *content;
    json_object *link;
    json_object *dt;
    json_object *creator;
    json_object *jPayload;
    int rc;
    json_object *title;
    RSSEntryFull *fpost;
    struct json_object *jResponse;
    int httpRC;
    json_object *success;
    json_object *rcpost;
    if (debug) fprintf(stderr, "cloudAPI_load_sgug_post_by_ID(%s)\n", id);
    fpost = malloc(sizeof(RSSEntryFull));
    if (!fpost) {
        fprintf(stderr, "Malloc failed for RSSEntry\n");
        return RESPONSE_ERROR;
    }
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    json_object_object_add(jPayload, "action", json_object_new_string("sgug-getPostByID"));
    json_object_object_add(jPayload, "id", json_object_new_string(id));

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_RSS_ENDPOINT, &jResponse, jPayload);
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success);
    if (rc != 1) {
        printf("Failed to load RSSEntry!\n");
        json_object_put(jResponse); /* clean up */
        return RESPONSE_ERROR;
    }
    rcpost = json_object_object_get(jResponse, "post");

    title = json_object_object_get(rcpost, "title");
    creator = json_object_object_get(rcpost, "creator");
    dt = json_object_object_get(rcpost, "dt");
    link = json_object_object_get(rcpost, "link");
    content = json_object_object_get(rcpost, "content");

    sprintf(fpost->title, "%s", json_object_get_string(title));
    sprintf(fpost->link, "%s", json_object_get_string(link));
    sprintf(fpost->creator, "%s", json_object_get_string(creator));
    sprintf(fpost->dt, "%s", json_object_get_string(dt));
    sprintf(fpost->content, "%s", json_object_get_string(content));

    *post = fpost;

    json_object_put(jResponse); /* clean up */
    return RESPONSE_OK;
}
int cloudAPI_listRSSNews(RSSEntry** lstResults, int* lstCount, char* action) {
    json_object *jPayload;
    struct json_object *jResponse;
    int maxresults;
    int rc;
    int httpRC;
    json_object *success;
    if (debug) fprintf(stderr, "cloudAPI_listRSSNews(%s)\n", action);
    *lstCount = 0;
    maxresults = 50;
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;

    json_object_object_add(jPayload, "action", json_object_new_string(action));

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_RSS_ENDPOINT, &jResponse, jPayload);

    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));

    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success) ? RESPONSE_OK : RESPONSE_ERROR;
    if (rc == RESPONSE_OK) {
        /* json_object *systems = json_object_object_get(jResponse, "systems"); */
        json_object *lst = json_object_object_get(jResponse, "lst");
        if (lst) {
            RSSEntry *rss;
            int i;
            int cnt = json_object_array_length(lst);
            if (cnt > maxresults) cnt = maxresults;
            rss = malloc(cnt * sizeof(RSSEntry));
            if (!lst) {
                fprintf(stderr, "malloc failed\n");
                return RESPONSE_ERROR;
            }
            for (i=0; i<cnt; i++) {
                json_object *entry = json_object_array_get_idx(lst, i);

                json_object *title = json_object_object_get(entry, "title");
                json_object *creator = json_object_object_get(entry, "creator");
                json_object *dt = json_object_object_get(entry, "dt");
                json_object *guid = json_object_object_get(entry, "guid");
                sprintf(rss[i].title, "%s", json_object_get_string(title));
                sprintf(rss[i].creator, "%s", json_object_get_string(creator));
                sprintf(rss[i].dt, "%s", json_object_get_string(dt));
                sprintf(rss[i].guid, "%s", json_object_get_string(guid));
            }
             *lstResults = rss;
             *lstCount = cnt;
        }
    }
    json_object_put(jResponse); /* clean up */
    return rc;
}
int cloudAPI_renameSystem(char* newname) {
    json_object *jPayload;
    int rc;
    struct json_object *jResponse;
    int httpRC;
    json_object *success;
    if (debug) fprintf(stderr, "cloudAPI_renameSystem(%s)\n", newname);
    jPayload = json_object_new_object();
    if (!jPayload) return RESPONSE_ERROR;
    json_object_object_add(jPayload, "action", json_object_new_string("renameSystem"));
    json_object_object_add(jPayload, "clientID", json_object_new_string(G_CLIENTID));
    json_object_object_add(jPayload, "newName", json_object_new_string(newname));

    if (debug) fprintf(stderr, "sending: %s\n", json_object_to_json_string_ext(jPayload, JSON_C_TO_STRING_PRETTY));
    httpRC = requestPOSTObj(SGINET_API, &jResponse, jPayload);
    if (debug) fprintf(stderr, "response: %s\n", json_object_to_json_string_ext(jResponse, JSON_C_TO_STRING_PRETTY));
    json_object_put(jPayload); /* cleanup payload json obj */

    if (httpRC != 1) {
        json_object_put(jResponse); /* clean up */
        if (debug) fprintf(stderr, "Failed to connect to SGINet API: (%i)\n", httpRC);
        return RESPONSE_ERROR;
    }
    success = json_object_object_get(jResponse, "success");
    rc = json_object_get_boolean(success);
    if (rc != 1) {
        json_object_put(jResponse); /* clean up */
        printf("Rename failed\n");
        return RESPONSE_ERROR;
    }
    json_object_put(jResponse); /* clean up */
    return RESPONSE_OK;
}
