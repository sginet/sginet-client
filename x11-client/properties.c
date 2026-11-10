/* User settings in ~/.sginet-properties */
#include "../include/cloudAPI.h"
#include "../include/x11-client.h"
#include <errno.h>

#define PROPERTIES_FILE ".sginet-properties"
#define PROP_LINE_MAX 1024

static void properties_path(char *path, size_t size) {
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') home = "/tmp";
    snprintf(path, size, "%s/%s", home, PROPERTIES_FILE);
}

/* returns a pointer to the value if line is "key=value" (spaces around the key are ignored), else NULL */
static char *match_key(char *line, const char *key) {
    size_t len = strlen(key);
    while (*line == ' ' || *line == '\t') line++;
    if (strncmp(line, key, len) != 0) return NULL;
    line += len;
    while (*line == ' ' || *line == '\t') line++;
    if (*line != '=') return NULL;
    return line + 1;
}

int property_get_int(const char *key, int default_value) {
    char path[1024];
    char line[PROP_LINE_MAX];
    char *value;
    int result = default_value;
    FILE *f;

    properties_path(path, sizeof(path));
    f = fopen(path, "r");
    if (f == NULL) return default_value;
    while (fgets(line, sizeof(line), f) != NULL) {
        value = match_key(line, key);
        if (value != NULL) result = atoi(value); /* last one wins */
    }
    fclose(f);
    return result;
}

/* rewrite the file with key=value replaced (or appended), via a temp file and rename() */
int property_set_int(const char *key, int value) {
    char path[1024];
    char tmp_path[1100];
    char line[PROP_LINE_MAX];
    FILE *in, *out;
    int written = 0;
    size_t len;

    properties_path(path, sizeof(path));
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    out = fopen(tmp_path, "w");
    if (out == NULL) {
        if (debug) printf("[properties] cannot write %s: %s\n", tmp_path, strerror(errno));
        return -1;
    }
    in = fopen(path, "r");
    if (in != NULL) {
        while (fgets(line, sizeof(line), in) != NULL) {
            if (match_key(line, key) != NULL) {
                if (!written) fprintf(out, "%s=%d\n", key, value);
                written = 1; /* drop duplicates */
                continue;
            }
            fputs(line, out);
            len = strlen(line);
            if (len > 0 && line[len - 1] != '\n') fputc('\n', out);
        }
        fclose(in);
    }
    if (!written) fprintf(out, "%s=%d\n", key, value);
    if (fclose(out) != 0 || rename(tmp_path, path) != 0) {
        if (debug) printf("[properties] cannot save %s: %s\n", path, strerror(errno));
        unlink(tmp_path);
        return -1;
    }
    if (debug) printf("[properties] %s=%d\n", key, value);
    return 0;
}
