/*
 *  Small (and probably insecure) webserver to serve files from your home directory.
 *  Not intended to be exposed to the internet! Only active when sginet is lanched with -w option.
*/
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#ifndef IRIX5
#include <pthread.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "include/cloudAPI.h"
#include "include/hwinfo_sgi.h"

#define PORT 8064
#define BUFFER_SIZE 2097152 /* 2MB HTML/CSS/JS response buffer and multi-chunk buffer for file downloads */
#define MAX_CONNECTIONS 20

#ifndef SHUT_WR
#define SHUT_WR 1  /* IRIX 5: shutdown() only takes the numbers */
#endif

/* IRIX 5 has no socklen_t type */
#ifdef __sgi
typedef int ws_socklen_t;
#else
typedef socklen_t ws_socklen_t;
#endif

extern int debug;

char webroot[1024];
#define HINVBUFSIZE 32000
#define HINVBUFSIZELINE 512
char hinv[HINVBUFSIZE];
char indexhtml[4096];

#define HTML_HEADER "<html><head><title>SGINet</title><link href=\"/styles/main.css\" rel=\"stylesheet\"/></head><body><header><div class=\"title\">SGINet Web Interface</div><div><a href=\"/\">Home</a> | <a href=\"/hinv\">View HINV</a> | <a href=\"/files/\">Browse files</a></div></header><div class=\"content\">"
#define HTML_FOOTER "</div><footer><a href=\"https://sginet.flexion.io\">SGINet client</a> - flexion 2024</footer></body></html>"
#define PG_NOT_FOUND "<h1>404 Page not found</h1><a href=\"/\">Continue to start page</a>"

#define STYLES_MAIN "body { background-color: #484747; color: white; margin: 0; font-family: Helvetica, Arial, Verdana; } .content { min-height: 360px; background-color: #606060; padding: 12px;} footer { padding: 12px; font-size: 10pt; border-top: 1px solid #9f9c9c;} a { color: #14cced; text-decoration: none; } a:hover { text-decoration: underline; } ul.dir { list-style: square; margin: 3px; font-weight: bold;} ul.file { list-style: none; margin: 3px; } header { padding: 12px; border-bottom: 1px solid #9f9c9c; } header .title { font-weight: bold; margin-bottom: 10px; } .itable TD { padding: 2px 12px 2px 2px; }"

const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

const char *get_mime_type(const char *file_ext) {
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
        return "text/html";
    } else if (strcasecmp(file_ext, "txt") == 0) {
        return "text/plain";
    } else if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(file_ext, "png") == 0) {
        return "image/png";
    } else {
        return "application/octet-stream";
    }
}

int cache_hinv() {
    FILE *fp;
    char buf[HINVBUFSIZELINE];
    if (debug) fprintf(stderr, "Caching HINV data for this system\n");
    if ((fp = popen("/sbin/hinv -v", "r")) == NULL) {
        printf("Error opening pipe!\n");
        return -1;
    }
    snprintf(hinv + strlen(hinv), HINVBUFSIZE, "<h4>HINV of this system</h4><pre>");
    while (fgets(buf, HINVBUFSIZELINE, fp) != NULL) {
        snprintf(hinv + strlen(hinv), HINVBUFSIZE, "%s", buf);
    }
    snprintf(hinv + strlen(hinv), HINVBUFSIZE, "</pre>");
    if (pclose(fp)) {
        printf("Command not found or exited with error status\n");
        return -1;
    }
    return 0;
}

int listDirectory(char *filepath, char *listing, int maxLen) {
    char sep[2];
    char parentpath[512];
    char fullpath[512];
    DIR *folder;
    struct dirent *entry;
    struct stat filestat;
    int webroot_len = strlen(webroot) + 1;

    if ((folder = opendir(filepath)) == NULL) {
        snprintf(listing, maxLen, "Failed to read directory");
        return -1;
    }

    snprintf(listing, maxLen, "<h4>Path: %s</h4><ul class=\"dir\">", filepath);
    sep[0] = '\0';
    if ((strlen(filepath) != webroot_len)) { /* we are in a subfolder */
        char *lastslash;
        /* remove last folder part from path to nav up */
        sprintf(parentpath, "%s", filepath + webroot_len);
        lastslash = strrchr(parentpath, '/');
        if (lastslash) {
             *(lastslash) = 0;
        } else {
            parentpath[0] = '\0';
        }
        snprintf(listing + strlen(listing), maxLen - strlen(listing), "<li><a href=\"/files/%s\">.. </a>", parentpath);
        sprintf(sep, "/");
    }

    /* list directories first */
    while ((entry = readdir(folder)) != NULL) {
        if (strcmp("..", entry->d_name) != 0 && strcmp(".", entry->d_name) != 0) {
            sprintf(fullpath, "%s/%s", filepath, entry->d_name);
            stat(fullpath, &filestat);
            if (S_ISDIR(filestat.st_mode)) {  /* directory */
                /* sep = "/" separator. empty when in webroot */
                snprintf(listing + strlen(listing), maxLen - strlen(listing), "<li><a href=\"/files/%s%s%s\">%s</a>", filepath + webroot_len, sep, entry->d_name, entry->d_name);
            }
        }
    }
    snprintf(listing + strlen(listing), maxLen - strlen(listing), "</ul><ul class=\"file\">");

    /* list files */
    rewinddir(folder);
    while ((entry = readdir(folder)) != NULL) {
        if (strcmp("..", entry->d_name) != 0 && strcmp(".", entry->d_name) != 0) {
            sprintf(fullpath, "%s/%s", filepath, entry->d_name);
            stat(fullpath, &filestat);
            if (S_ISREG(filestat.st_mode)) {  /* regular file */

                snprintf(listing + strlen(listing), maxLen - strlen(listing), "<li><a href=\"/file/%s%s%s\">%s</a>", filepath + webroot_len, sep, entry->d_name, entry->d_name);
            }
        }
    }
    snprintf(listing + strlen(listing), maxLen - strlen(listing), "</ul>");
    closedir(folder);

    return 0;
}

char *url_decode(const char *src) {
    size_t i;
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t decoded_len = 0;

    /* decode %2x to hex */
    for (i = 0; i < src_len; i++) {
        if (src[i] == '%' && i + 2 < src_len) {
            int hex_val;
            sscanf(src + i + 1, "%2x", &hex_val);
            decoded[decoded_len++] = hex_val;
            i += 2;
        } else {
            decoded[decoded_len++] = src[i];
        }
    }
    decoded[decoded_len] = '\0';
    return decoded;
}

void formatUptime(int n, char *timeStr) {
    int hour;
    int minutes;
    int seconds;
    int day = n / (24 * 3600);
    n = n % (24 * 3600);
    hour = n / 3600;
    n %= 3600;
    minutes = n / 60;
    n %= 60;
    seconds = n;
    sprintf(timeStr, "%i days %i hours %i minutes %i seconds", day, hour, minutes, seconds);
}

void buildIndexPage() {
    char timeStr[64];
    int swapPct;
    int memPct;
    int swapUsedMB;
    int swapTotalMB;
    int memUsedMB;
    IRIXMachine *machine;
    int memTotalMB;
    char hostname[128];
    char OSRelease[64];
    char ipmodel[32];
    
    getHostname(hostname, sizeof(hostname));
    getOSRelease(OSRelease);
    getIPModel(ipmodel);

    machine = Machine_new();  /* reads static values like CPU count and pagefile size */
    Machine_scan(machine);  /* reads/updates cpu and memory usage */
    memTotalMB = machine->totalMem / 1024;
    memUsedMB = memTotalMB - (machine->availableMem / 1024);
    swapTotalMB = machine->totalSwap / 1024;
    swapUsedMB = machine->usedSwap / 1024;
    memPct = 100 * memUsedMB / memTotalMB;
    swapPct = swapTotalMB > 0 ? (100 * swapUsedMB / swapTotalMB) : 0;

    formatUptime(getUptime(), timeStr);    

    sprintf(indexhtml, "<h4>Welcome to SGINet on %s (%s)</h4>", hostname, ipmodel);
    sprintf(indexhtml + strlen(indexhtml), "<table class=\"itable\"><tr><td>OS:</td><td>%s</td></tr>", OSRelease);
    
    sprintf(indexhtml + strlen(indexhtml), "<td>Total memory (+swap):</td><td>%i MB / %i MB (%i%% used)</td></tr>", memUsedMB, memTotalMB, memPct);
    sprintf(indexhtml + strlen(indexhtml), "<td>Swap:</td><td>%i MB / %i MB (%i%% used)</td></tr>", swapUsedMB, swapTotalMB, swapPct);
    sprintf(indexhtml + strlen(indexhtml), "<tr><td>Uptime:</td><td>%s</td></tr>", timeStr);
    sprintf(indexhtml + strlen(indexhtml), "<tr><td>SGINet version:</td><td>%s</td></tr></table>", CLIENT_VERSION);
}
void send404(char *response, int client_fd) {
    snprintf(response, BUFFER_SIZE, "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n" HTML_HEADER PG_NOT_FOUND HTML_FOOTER);
    send(client_fd, response, strlen(response), 0);
}
void handle_direct_file_download(const char *file_name, char *response, int client_fd) {
    char file_ext[32];
    const char *mime_type;
    struct stat file_stat;
    int n;
    /* This will stream (large) files directly from the fs, without using the rather small response buffer for HTML/CSS stuff */

    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1) {
	if (debug) fprintf(stderr, "File not found: %s\n", file_name);
        send404(response, client_fd);
        return;
    }

    /* get file extension */
    strcpy(file_ext, get_file_extension(file_name));
    mime_type = get_mime_type(file_ext);

    /* build HTTP header */
    snprintf(response, BUFFER_SIZE, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", mime_type);

    /* get file size */
    fstat(file_fd, &file_stat);

    /* send the header */
    send(client_fd, response, strlen(response), 0);
    
    /* send file content (Note: sendfile() is not available on IRIX) */
    n = 1;
    while (n) {
	char *fbuffer = (char *)malloc(BUFFER_SIZE * sizeof(char)+1); /* use same buffer size as for HTML output */
        n = read(file_fd, fbuffer, BUFFER_SIZE);
        if (debug) fprintf(stderr, "read chunk (n:%i)\n", n);
        send(client_fd, fbuffer, n, 0);
	    free(fbuffer);
    }
    shutdown(client_fd, SHUT_WR);
}
void http_send_static_html(const char *urlpath, char *response, int client_fd) {
    /* response is always text/html */
    char *responsePage;
    if (strcmp("index.html", urlpath) == 0) {
        buildIndexPage();
        responsePage = indexhtml;
    } else if (strcmp("hinv", urlpath) == 0) {
        if (strlen(hinv)<1) cache_hinv();
        responsePage = hinv;
    } else {  /* 404 not found */
        send404(response, client_fd);
        return;
    }
    /* 200 OK */
    snprintf(response, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n" HTML_HEADER "%s" HTML_FOOTER,
             responsePage);
    send(client_fd, response, strlen(response), 0);
}
void http_send_static_styles(const char *urlpath, char *response, int client_fd) {
    /* response is always text/css */
    char *responsePage;
    if (strcmp("styles/main.css", urlpath) == 0) {
        responsePage = STYLES_MAIN;
    } else {  /* 404 not found */
        send404(response, client_fd);
        return;
    }
    /* 200 OK */
    snprintf(response, BUFFER_SIZE, "HTTP/1.1 200 OK\r\nContent-Type: text/css\r\nCache-Control: max-age=259200\r\n\r\n %s", responsePage);
    send(client_fd, response, strlen(response), 0);
}
void http_handle_list_files(char *realpath, char *response, int client_fd) {
    int maxListingLength = 32095;
    char *listing = (char *)malloc(BUFFER_SIZE * sizeof(char)+1);
    if (debug) fprintf(stderr, "List directory: %s\n", realpath);
    listDirectory(realpath, listing, BUFFER_SIZE);
    snprintf(response, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n" HTML_HEADER "%s" HTML_FOOTER,
             listing);
    send(client_fd, response, strlen(response), 0);
    free(listing);
}
int strStartsWith(const char *pre, const char *str) {
    size_t lenpre = strlen(pre), lenstr = strlen(str);
    return lenstr < lenpre ? 0 : (memcmp(pre, str, lenpre) == 0 ? 1 : 0);
}
/* Finds the path in a request starting with "GET /<path> HTTP/1" (IRIX 5 has no regex.h) */
static int parse_get_path(const char *req, size_t len, const char **path) {
    size_t i;
    if (len < 5 || strncmp(req, "GET /", 5) != 0) return -1;
    for (i = 5; i < len && req[i] != ' ' && req[i] != '\0'; i++)
        ;
    if (len - i < 7 || strncmp(req + i, " HTTP/1", 7) != 0) return -1;
    *path = req + 5;
    return (int)(i - 5);
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

    /* receive request data from client and store into buffer */
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        /* Handle GET requests: extract and decode path */
        const char *url_path;
        int url_path_len = parse_get_path(buffer, (size_t)bytes_received, &url_path);
        if (url_path_len >= 0) {
            char *response;
            char *url_path_decoded;
            buffer[5 + url_path_len] = '\0';
            url_path_decoded = url_decode(url_path);

            response = (char *)malloc(BUFFER_SIZE * sizeof(char));
            
            if (debug) fprintf(stderr, "GET: %s\n", url_path_decoded);

            if (strstr(url_path_decoded, "..") || strstr(url_path_decoded, "%2e%2e") || strstr(url_path_decoded, "%2E%2E")) { /* not 100% safe :) */
                printf("[WARNING] Blocked possible directory traversal request: %s\n", url_path_decoded);
                send404(response, client_fd);
            } else if (strlen(url_path_decoded) == 0 || strcmp("index.html", url_path_decoded) == 0) {
                http_send_static_html("index.html", response, client_fd);
            } else if (strStartsWith("styles/", url_path_decoded)) {
                http_send_static_styles(url_path_decoded, response, client_fd);
            } else if (strStartsWith("files/", url_path_decoded)) {
                char real_path[1024];
                sprintf(real_path, "%s%s", webroot, url_path_decoded + 5);  /* substract "file/" prefix (5 chars) */
                http_handle_list_files(real_path, response, client_fd);
            } else if (strStartsWith("file/", url_path_decoded)) {
                char real_filename[1024];
                sprintf(real_filename, "%s%s", webroot, url_path_decoded + 4);  /* substract "file/" prefix (5 chars) */
                if (debug) fprintf(stderr, "Download file (%s)\n", real_filename);
                handle_direct_file_download(real_filename, response, client_fd);
            } else {  /* check for static html page mappings */
                http_send_static_html(url_path_decoded, response, client_fd);
            }
            
            free(response);
            free(url_path_decoded);
        }
    }
    close(client_fd);
    free(arg);
    free(buffer);
    return NULL;
}

void getUserHomeDir() {
    struct passwd *pwentry = getpwuid(getuid());
    if (NULL == pwentry) {
        printf("Unable to get home directory of user\n");
        return;
    }
    sprintf(webroot, "%s", pwentry->pw_dir);
}

int startWebserver() {
    int reuse = 1;
    int server_fd;
    struct sockaddr_in server_addr;
    
    if (debug) fprintf(stderr, "Verbose output enabled\n");

    /* set webserver root directory (users home directory) */
    getUserHomeDir();
    if (strlen(webroot) < 1) sprintf(webroot, "/tmp");  /* fallback to tmp */

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    /* set socket reuse option */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("Failed to set socket reuse option\n");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind to port");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("Failed to setup listener");
        exit(EXIT_FAILURE);
    }

    printf("\n  SGINet web server listening on port: %d\n  Sharing directory: %s\n\n", PORT, webroot);
    
#ifdef IRIX5
    /* IRIX 5 has no POSIX threads, connections are handled by forked children */
    signal(SIGCHLD, SIG_IGN);
#endif
    while (1) {
#ifdef IRIX5
        pid_t pid;
#else
        pthread_t thread_id;
#endif
        struct sockaddr_in client_addr;
        ws_socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("Client connection failed");
            continue;
        }
#ifdef IRIX5
        /* new connections as separate processes */
        pid = fork();
        if (pid == 0) {
            close(server_fd);
            handle_client(client_fd);
            _exit(0);
        }
        if (pid < 0) perror("fork");
        close(*client_fd);
        free(client_fd);
#else
        /* new connections as separate threads */
        pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
        pthread_detach(thread_id);
#endif
    }

    close(server_fd);
    return 0;
}
/*
int main(int argc, char *argv[]) {
    startWebserver();
    return 0;
}
*/
