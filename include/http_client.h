#ifndef HEADER_HTTP_CLIENT
#define HEADER_HTTP_CLIENT
/*
 * Minimal blocking HTTP/1.1 client with TLS (BearSSL).
 * Replaces libcurl so sginet builds with MIPSpro and runs on stock IRIX.
 */
#include <stdio.h>
#include <stddef.h>
#include "bearssl.h"

#define HTTP_OK 0
#define HTTP_ERROR -1

#define HTTP_TIMEOUT_SEC 60

typedef struct _HttpBuffer {
    char *data;     /* always NUL terminated when not NULL */
    size_t size;
} HttpBuffer;

typedef struct _HttpFormField {
    const char *name;
    const char *value;
    int isFile;
} HttpFormField;

/* compiled-in trust anchors (ta_builtin.c) */
extern br_x509_trust_anchor HTTP_BUILTIN_TAS[];
extern const size_t HTTP_BUILTIN_TAS_NUM;
/* fills in the EC keys of HTTP_BUILTIN_TAS, call before using it */
void http_builtin_tas_init(void);

/* Load additional trust anchors from a PEM CA bundle (added to the built-in ones). */
int http_set_ca_bundle(const char *path);

/* POST body, response body is returned in resp (free with http_buffer_free). */
int http_post(const char *url, const char *contentType, const char *body, size_t bodyLen,
              HttpBuffer *resp, int *status);

/* GET url and write the response body to fp. */
int http_get_to_file(const char *url, FILE *fp, int *status);

/* POST multipart/form-data, fields are sent in the given order. Response body is discarded. */
int http_post_multipart(const char *url, const HttpFormField *fields, int nfields, int *status);

void http_buffer_free(HttpBuffer *buf);

/* Human readable description of the last error. */
const char *http_last_error(void);

#endif /* HEADER_HTTP_CLIENT */
