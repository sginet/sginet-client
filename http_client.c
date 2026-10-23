/*
 * Minimal blocking HTTP/1.1 client with TLS 1.2 (BearSSL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifndef __sgi
#include <sys/select.h>
#endif

#include "include/http_client.h"

#ifdef __sgi
typedef int http_socklen_t;
#else
typedef socklen_t http_socklen_t;
#endif

#define HTTP_USER_AGENT "SGINet-client/1.0"
#define HTTP_LINE_MAX 8192
#define HTTP_RBUF_SIZE 4096
#define HTTP_SEED_FILE ".sginet_rnd"
/* The server answers a ClientHello within milliseconds. Some connections never get an
   answer (seen with S3 from IRIX 5), so give up early and retry with a new connection. */
#define HTTP_HANDSHAKE_TIMEOUT_SEC 10
#define HTTP_HANDSHAKE_ATTEMPTS 3

extern int debug;

typedef struct _HttpUrl {
    int tls;
    char host[256];
    unsigned short port;
    char *path;     /* path + query, always starts with '/' */
} HttpUrl;

typedef struct _HttpConn {
    int fd;
    int tls;
    int timeoutSec; /* for each wait on the socket */
    int timedOut;
    int peerClosed; /* read returned EOF */
    int sysErr;     /* errno of the last failed socket call */
    unsigned long bytesIn;  /* raw socket byte counts, for debugging */
    unsigned long bytesOut;
    br_ssl_client_context sc;
    br_x509_minimal_context xc;
    br_sslio_context ioc;
    unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
    unsigned char rbuf[HTTP_RBUF_SIZE];
    size_t rpos;
    size_t rlen;
} HttpConn;

typedef int (*HttpBodySink)(void *ctx, const unsigned char *data, size_t len);
typedef int (*HttpBodySource)(HttpConn *c, void *ctx);

static char g_err[512];

/* trust anchors loaded with http_set_ca_bundle() (includes the built-in ones), NULL = built-in only */
static const br_x509_trust_anchor *g_tas = NULL;
static size_t g_tas_num = 0;

static br_hmac_drbg_context g_drbg;
static int g_drbg_ready = 0;

/* ------------------------------------------------------------------------ */
/* errors                                                                   */

static void set_err(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_err, sizeof(g_err), fmt, ap);
    va_end(ap);
}

const char *http_last_error(void) {
    return g_err;
}

static const char *tls_err_str(int err) {
    switch (err) {
    case BR_ERR_X509_EXPIRED:
        return "server certificate expired or not yet valid (check the system date/time)";
    case BR_ERR_X509_NOT_TRUSTED:
        return "server certificate is not trusted (unknown CA)";
    case BR_ERR_X509_BAD_SERVER_NAME:
        return "server certificate does not match the host name";
    case BR_ERR_NO_RANDOM:
        return "no entropy source available";
    case BR_ERR_BAD_VERSION:
    case BR_ERR_BAD_CIPHER_SUITE:
    case BR_ERR_UNSUPPORTED_VERSION:
        return "no common TLS version or cipher suite with the server";
    case BR_ERR_IO:
        return "connection closed or timed out";
    default:
        return "TLS failure";
    }
}

/* Report a failed read/write on c, with the TLS error if there is one. */
static int conn_error(HttpConn *c, const char *what) {
    int err = 0;
    if (c->tls) err = br_ssl_engine_last_error(&c->sc.eng);
    if (c->timedOut) {
        set_err("%s: timeout after %d seconds", what, c->timeoutSec);
    } else if (c->peerClosed) {
        set_err("%s: server closed the connection", what);
    } else if (c->sysErr != 0) {
        set_err("%s: %s", what, strerror(c->sysErr));
    } else if (err != BR_ERR_OK) {
        if (err >= BR_ERR_RECV_FATAL_ALERT && err < BR_ERR_SEND_FATAL_ALERT + 256) {
            set_err("%s: TLS alert %d", what, err & 0xFF);
        } else {
            set_err("%s: %s (BearSSL error %d)", what, tls_err_str(err), err);
        }
    } else {
        set_err("%s", what);
    }
    return HTTP_ERROR;
}

/* ------------------------------------------------------------------------ */
/* entropy                                                                  */

static int read_dev_random(unsigned char *buf, size_t len) {
    static const char *devs[] = { "/dev/urandom", "/dev/random" };
    int i;
    for (i = 0; i < 2; i++) {
        size_t got = 0;
        int fd = open(devs[i], O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        while (got < len) {
            ssize_t r = read(fd, buf + got, len - got);
            if (r <= 0) break;
            got += (size_t)r;
        }
        close(fd);
        if (got == len) return 1;
    }
    return 0;
}

static void seed_file_path(char *path, size_t size) {
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') home = "/tmp";
    snprintf(path, size, "%s/%s", home, HTTP_SEED_FILE);
}

static void hash_stat(br_sha256_context *h, const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) br_sha256_update(h, &st, sizeof(st));
}

/*
 * Fallback for systems without /dev/urandom
 */
static void fallback_entropy(unsigned char *out, size_t len) {
    struct timeval tv;

    if (!g_drbg_ready) {
        br_sha256_context h;
        unsigned char seed[32];
        char path[512];
        char hostname[256];
        struct tms tm;
        clock_t ck;
        long v;
        void *p;
        int i, fd;

        if (debug) fprintf(stderr, "WARNING: no /dev/urandom, using fallback entropy for TLS\n");
        br_sha256_init(&h);

        /* timing jitter: count loop iterations until the clock moves */
        for (i = 0; i < 256; i++) {
            struct timeval t0, t1;
            unsigned long n = 0;
            gettimeofday(&t0, NULL);
            do {
                gettimeofday(&t1, NULL);
                n++;
            } while (t1.tv_usec == t0.tv_usec && t1.tv_sec == t0.tv_sec && n < 100000UL);
            br_sha256_update(&h, &t1, sizeof(t1));
            br_sha256_update(&h, &n, sizeof(n));
        }

        v = (long)getpid();  br_sha256_update(&h, &v, sizeof(v));
        v = (long)getppid(); br_sha256_update(&h, &v, sizeof(v));
        v = (long)getuid();  br_sha256_update(&h, &v, sizeof(v));
        v = (long)time(NULL); br_sha256_update(&h, &v, sizeof(v));
        ck = times(&tm);
        br_sha256_update(&h, &ck, sizeof(ck));
        br_sha256_update(&h, &tm, sizeof(tm));
        ck = clock();
        br_sha256_update(&h, &ck, sizeof(ck));
        if (gethostname(hostname, sizeof(hostname)) == 0) br_sha256_update(&h, hostname, strlen(hostname));
        p = &h;
        br_sha256_update(&h, &p, sizeof(p));
        p = malloc(1);
        br_sha256_update(&h, &p, sizeof(p));
        free(p);

        hash_stat(&h, "/tmp");
        hash_stat(&h, "/var/tmp");
        hash_stat(&h, "/var/adm/SYSLOG");
        hash_stat(&h, "/var/adm/wtmpx");
        hash_stat(&h, "/etc/utmpx");
        hash_stat(&h, "/dev/null");

        /* mix in the seed file from the last run */
        seed_file_path(path, sizeof(path));
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            unsigned char old[32];
            ssize_t r = read(fd, old, sizeof(old));
            if (r > 0) br_sha256_update(&h, old, (size_t)r);
            close(fd);
        }

        br_sha256_out(&h, seed);
        br_hmac_drbg_init(&g_drbg, &br_sha256_vtable, seed, sizeof(seed));
        g_drbg_ready = 1;

        /* write a new seed file for the next run */
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            br_hmac_drbg_generate(&g_drbg, seed, sizeof(seed));
            if (write(fd, seed, sizeof(seed)) != (ssize_t)sizeof(seed) && debug) {
                fprintf(stderr, "WARNING: could not write %s\n", path);
            }
            close(fd);
        }
    }
    gettimeofday(&tv, NULL);
    br_hmac_drbg_update(&g_drbg, &tv, sizeof(tv));
    br_hmac_drbg_generate(&g_drbg, out, len);
}

/*
 * Seed the fallback generator before connecting: gathering the entropy takes seconds
 * on slow systems, and servers drop connections that stay idle that long.
 */
static void prepare_entropy(void) {
    unsigned char tmp[32];
    if (g_drbg_ready || read_dev_random(tmp, sizeof(tmp))) return;
    fallback_entropy(tmp, sizeof(tmp));
}

static void inject_entropy(br_ssl_engine_context *eng) {
    unsigned char seed[32];
    if (!read_dev_random(seed, sizeof(seed))) {
        fallback_entropy(seed, sizeof(seed));
    }
    br_ssl_engine_inject_entropy(eng, seed, sizeof(seed));
}

/* ------------------------------------------------------------------------ */
/* trust anchors                                                            */

typedef struct _HttpBlob {
    unsigned char *data;
    size_t len;
    size_t cap;
    int failed;
} HttpBlob;

static void blob_append(void *ctx, const void *data, size_t len) {
    HttpBlob *b = (HttpBlob *)ctx;
    if (b->failed) return;
    if (b->len + len > b->cap) {
        size_t ncap = b->cap ? b->cap * 2 : 1024;
        unsigned char *n;
        while (ncap < b->len + len) ncap *= 2;
        n = realloc(b->data, ncap);
        if (n == NULL) {
            b->failed = 1;
            return;
        }
        b->data = n;
        b->cap = ncap;
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
}

static unsigned char *blobdup(const unsigned char *src, size_t len) {
    unsigned char *d = malloc(len ? len : 1);
    if (d) memcpy(d, src, len);
    return d;
}

static int cert_to_ta(const unsigned char *der, size_t len, br_x509_trust_anchor *ta) {
    br_x509_decoder_context dc;
    HttpBlob dn;
    br_x509_pkey *pk;

    memset(&dn, 0, sizeof(dn));
    br_x509_decoder_init(&dc, blob_append, &dn);
    br_x509_decoder_push(&dc, der, len);
    pk = br_x509_decoder_get_pkey(&dc);
    if (pk == NULL || dn.failed) {
        free(dn.data);
        return HTTP_ERROR;
    }
    ta->dn.data = dn.data;
    ta->dn.len = dn.len;
    ta->flags = br_x509_decoder_isCA(&dc) ? BR_X509_TA_CA : 0;
    switch (pk->key_type) {
    case BR_KEYTYPE_RSA:
        ta->pkey.key_type = BR_KEYTYPE_RSA;
        ta->pkey.key.rsa.n = blobdup(pk->key.rsa.n, pk->key.rsa.nlen);
        ta->pkey.key.rsa.nlen = pk->key.rsa.nlen;
        ta->pkey.key.rsa.e = blobdup(pk->key.rsa.e, pk->key.rsa.elen);
        ta->pkey.key.rsa.elen = pk->key.rsa.elen;
        break;
    case BR_KEYTYPE_EC:
        ta->pkey.key_type = BR_KEYTYPE_EC;
        ta->pkey.key.ec.curve = pk->key.ec.curve;
        ta->pkey.key.ec.q = blobdup(pk->key.ec.q, pk->key.ec.qlen);
        ta->pkey.key.ec.qlen = pk->key.ec.qlen;
        break;
    default:
        free(dn.data);
        return HTTP_ERROR;
    }
    return HTTP_OK;
}

int http_set_ca_bundle(const char *path) {
    FILE *f;
    unsigned char *buf;
    long fsize;
    size_t off, loaded, i;
    br_pem_decoder_context pc;
    HttpBlob obj;
    int inCert = 0;
    br_x509_trust_anchor *tas;
    size_t tasCap;

    f = fopen(path, "rb");
    if (f == NULL) {
        set_err("cannot open CA bundle %s: %s", path, strerror(errno));
        return HTTP_ERROR;
    }
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(f);
        set_err("CA bundle %s is empty", path);
        return HTTP_ERROR;
    }
    buf = malloc((size_t)fsize + 1);
    if (buf == NULL || fread(buf, 1, (size_t)fsize, f) != (size_t)fsize) {
        fclose(f);
        free(buf);
        set_err("cannot read CA bundle %s", path);
        return HTTP_ERROR;
    }
    fclose(f);
    buf[fsize] = '\n'; /* the PEM decoder needs a final newline */

    /* start with the built-in anchors */
    http_builtin_tas_init();
    tasCap = HTTP_BUILTIN_TAS_NUM + 64;
    tas = malloc(tasCap * sizeof(*tas));
    if (tas == NULL) {
        free(buf);
        set_err("out of memory");
        return HTTP_ERROR;
    }
    for (i = 0; i < HTTP_BUILTIN_TAS_NUM; i++) tas[i] = HTTP_BUILTIN_TAS[i];
    loaded = 0;

    memset(&obj, 0, sizeof(obj));
    br_pem_decoder_init(&pc);
    off = 0;
    while (off < (size_t)fsize + 1) {
        off += br_pem_decoder_push(&pc, buf + off, (size_t)fsize + 1 - off);
        switch (br_pem_decoder_event(&pc)) {
        case BR_PEM_BEGIN_OBJ: {
            const char *name = br_pem_decoder_name(&pc);
            inCert = strcmp(name, "CERTIFICATE") == 0 || strcmp(name, "X509 CERTIFICATE") == 0;
            obj.len = 0;
            br_pem_decoder_setdest(&pc, inCert ? blob_append : NULL, &obj);
            break;
        }
        case BR_PEM_END_OBJ:
            if (inCert && !obj.failed) {
                size_t n = HTTP_BUILTIN_TAS_NUM + loaded;
                if (n == tasCap) {
                    br_x509_trust_anchor *nt;
                    tasCap *= 2;
                    nt = realloc(tas, tasCap * sizeof(*tas));
                    if (nt == NULL) break;
                    tas = nt;
                }
                if (cert_to_ta(obj.data, obj.len, &tas[n]) == HTTP_OK) {
                    loaded++;
                } else if (debug) {
                    fprintf(stderr, "Skipping undecodable certificate in %s\n", path);
                }
            }
            inCert = 0;
            break;
        case BR_PEM_ERROR:
            free(obj.data);
            free(buf);
            free(tas);
            set_err("invalid PEM data in CA bundle %s", path);
            return HTTP_ERROR;
        default:
            break;
        }
    }
    free(obj.data);
    free(buf);

    if (loaded == 0) {
        free(tas);
        set_err("no certificates found in CA bundle %s", path);
        return HTTP_ERROR;
    }
    /* previously loaded anchors are kept allocated, this is called once at startup */
    g_tas = tas;
    g_tas_num = HTTP_BUILTIN_TAS_NUM + loaded;
    if (debug) fprintf(stderr, "Loaded %lu CA certificates from %s\n", (unsigned long)loaded, path);
    return HTTP_OK;
}

/* ------------------------------------------------------------------------ */
/* sockets                                                                  */

/* Wait until fd is readable/writable. Returns >0 ready, 0 timeout, <0 error. */
static int wait_fd(int fd, int forWrite, int timeoutSec) {
    fd_set fds;
    struct timeval tv;
    int r;
    for (;;) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = timeoutSec;
        tv.tv_usec = 0;
        r = select(fd + 1, forWrite ? NULL : &fds, forWrite ? &fds : NULL, NULL, &tv);
        if (r < 0 && errno == EINTR) continue;
        return r;
    }
}

/* low-level I/O callbacks, also used by BearSSL */
static int sock_read(void *ctx, unsigned char *buf, size_t len) {
    HttpConn *c = (HttpConn *)ctx;
    for (;;) {
        ssize_t r;
        int w = wait_fd(c->fd, 0, c->timeoutSec);
        if (w == 0) c->timedOut = 1;
        if (w < 0) c->sysErr = errno;
        if (w <= 0) return -1;
        r = read(c->fd, buf, len);
        if (r < 0 && errno == EINTR) continue;
        if (r < 0) c->sysErr = errno;
        if (r == 0) c->peerClosed = 1;
        if (r <= 0) return -1;
        c->bytesIn += (unsigned long)r;
        return (int)r;
    }
}

static int sock_write(void *ctx, const unsigned char *buf, size_t len) {
    HttpConn *c = (HttpConn *)ctx;
    for (;;) {
        ssize_t r;
        int w = wait_fd(c->fd, 1, c->timeoutSec);
        if (w == 0) c->timedOut = 1;
        if (w < 0) c->sysErr = errno;
        if (w <= 0) return -1;
        r = write(c->fd, buf, len);
        if (r < 0 && errno == EINTR) continue;
        if (r < 0) c->sysErr = errno;
        if (r <= 0) return -1;
        c->bytesOut += (unsigned long)r;
        return (int)r;
    }
}

static int tcp_connect(HttpConn *c, const HttpUrl *u) {
    struct hostent *he;
    struct sockaddr_in sa;
    int i, err = 0;
    time_t t0 = time(NULL);

    he = gethostbyname(u->host);
    if (he == NULL || he->h_addrtype != AF_INET || he->h_addr_list[0] == NULL) {
        if (debug) fprintf(stderr, "Resolving %s failed after %ld seconds\n", u->host, (long)(time(NULL) - t0));
        set_err("cannot resolve host %s", u->host);
        return HTTP_ERROR;
    }
    if (debug) {
        fprintf(stderr, "Resolved %s in %ld seconds:", u->host, (long)(time(NULL) - t0));
        for (i = 0; he->h_addr_list[i] != NULL; i++) {
            memcpy(&sa.sin_addr, he->h_addr_list[i], sizeof(sa.sin_addr));
            fprintf(stderr, " %s", inet_ntoa(sa.sin_addr));
        }
        fprintf(stderr, "\n");
    }
    t0 = time(NULL);
    for (i = 0; he->h_addr_list[i] != NULL; i++) {
        int fd, flags, r;

        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            err = errno;
            break;
        }
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(u->port);
        memcpy(&sa.sin_addr, he->h_addr_list[i], sizeof(sa.sin_addr));

        /* non-blocking connect, so we can time out */
        flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        r = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
        if (r < 0 && errno == EINPROGRESS) {
            int w = wait_fd(fd, 1, HTTP_TIMEOUT_SEC);
            if (w > 0) {
                int soerr = 0;
                http_socklen_t sl = sizeof(soerr);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&soerr, &sl) == 0 && soerr == 0) {
                    r = 0;
                } else {
                    errno = soerr ? soerr : ECONNREFUSED;
                }
            } else {
                errno = ETIMEDOUT;
            }
        }
        if (r == 0) {
            fcntl(fd, F_SETFL, flags);
            c->fd = fd;
            if (debug) fprintf(stderr, "Connected to %s (%s) port %u in %ld seconds\n", u->host, inet_ntoa(sa.sin_addr),
                           u->port, (long)(time(NULL) - t0));
            return HTTP_OK;
        }
        err = errno;
        if (debug) fprintf(stderr, "Connecting to %s port %u failed: %s\n", inet_ntoa(sa.sin_addr), u->port, strerror(err));
        close(fd);
    }
    set_err("cannot connect to %s port %u: %s", u->host, u->port, strerror(err));
    return HTTP_ERROR;
}

/* ------------------------------------------------------------------------ */
/* connection I/O (plain or TLS)                                            */

static int tls_start(HttpConn *c, const HttpUrl *u) {
    time_t t0;
    if (g_tas) {
        br_ssl_client_init_full(&c->sc, &c->xc, g_tas, g_tas_num);
    } else {
        http_builtin_tas_init();
        br_ssl_client_init_full(&c->sc, &c->xc, HTTP_BUILTIN_TAS, HTTP_BUILTIN_TAS_NUM);
    }
    br_ssl_engine_set_versions(&c->sc.eng, BR_TLS12, BR_TLS12);
    br_ssl_engine_set_buffer(&c->sc.eng, c->iobuf, sizeof(c->iobuf), 1);
    inject_entropy(&c->sc.eng);
    if (!br_ssl_client_reset(&c->sc, u->host, 0)) {
        return conn_error(c, "TLS init failed");
    }
    br_sslio_init(&c->ioc, &c->sc.eng, sock_read, c, sock_write, c);
    c->tls = 1;
    /* run the handshake now, so its failures are not reported as request failures */
    t0 = time(NULL);
    c->timeoutSec = HTTP_HANDSHAKE_TIMEOUT_SEC;
    if (br_sslio_flush(&c->ioc) < 0) {
        if (debug) fprintf(stderr, "TLS handshake with %s failed after %ld seconds, %lu bytes sent, %lu bytes received\n",
                           u->host, (long)(time(NULL) - t0), c->bytesOut, c->bytesIn);
        return conn_error(c, "TLS handshake failed");
    }
    c->timeoutSec = HTTP_TIMEOUT_SEC;
    if (debug) fprintf(stderr, "TLS handshake with %s took %ld seconds, %lu bytes sent, %lu bytes received\n",
                       u->host, (long)(time(NULL) - t0), c->bytesOut, c->bytesIn);
    return HTTP_OK;
}

static int conn_write(HttpConn *c, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    if (c->tls) {
        if (br_sslio_write_all(&c->ioc, p, len) < 0) return conn_error(c, "request failed");
        return HTTP_OK;
    }
    while (len > 0) {
        int r = sock_write(c, p, len);
        if (r < 0) return conn_error(c, "request failed");
        p += r;
        len -= (size_t)r;
    }
    return HTTP_OK;
}

static int conn_flush(HttpConn *c) {
    if (c->tls && br_sslio_flush(&c->ioc) < 0) return conn_error(c, "request failed");
    return HTTP_OK;
}

/* Read up to len bytes. Returns >0 bytes read, 0 on end of stream. */
static int conn_read(HttpConn *c, unsigned char *buf, size_t len) {
    int r;
    if (c->rpos < c->rlen) {
        size_t n = c->rlen - c->rpos;
        if (n > len) n = len;
        memcpy(buf, c->rbuf + c->rpos, n);
        c->rpos += n;
        return (int)n;
    }
    if (c->tls) {
        r = br_sslio_read(&c->ioc, buf, len);
    } else {
        r = sock_read(c, buf, len);
    }
    return r < 0 ? 0 : r;
}

static int conn_getc(HttpConn *c) {
    if (c->rpos >= c->rlen) {
        int r = c->tls ? br_sslio_read(&c->ioc, c->rbuf, sizeof(c->rbuf))
                       : sock_read(c, c->rbuf, sizeof(c->rbuf));
        if (r <= 0) return -1;
        c->rpos = 0;
        c->rlen = (size_t)r;
    }
    return c->rbuf[c->rpos++];
}

/* Read one CRLF terminated line without the line ending. Returns its length, -1 on EOF/overflow. */
static int read_line(HttpConn *c, char *line, size_t size) {
    size_t n = 0;
    for (;;) {
        int ch = conn_getc(c);
        if (ch < 0) return -1;
        if (ch == '\n') break;
        if (n + 1 >= size) return -1;
        line[n++] = (char)ch;
    }
    if (n > 0 && line[n - 1] == '\r') n--;
    line[n] = '\0';
    return (int)n;
}

/* ------------------------------------------------------------------------ */
/* HTTP                                                                     */

static int parse_url(const char *url, HttpUrl *u) {
    const char *p;
    size_t hlen;
    char *frag;

    memset(u, 0, sizeof(*u));
    if (strncmp(url, "https://", 8) == 0) {
        u->tls = 1;
        u->port = 443;
        p = url + 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        u->tls = 0;
        u->port = 80;
        p = url + 7;
    } else {
        set_err("unsupported URL: %s", url);
        return HTTP_ERROR;
    }
    hlen = strcspn(p, ":/?#");
    if (hlen == 0 || hlen >= sizeof(u->host)) {
        set_err("invalid host in URL: %s", url);
        return HTTP_ERROR;
    }
    memcpy(u->host, p, hlen);
    u->host[hlen] = '\0';
    p += hlen;
    if (*p == ':') {
        char *end;
        long port = strtol(p + 1, &end, 10);
        if (port <= 0 || port > 65535) {
            set_err("invalid port in URL: %s", url);
            return HTTP_ERROR;
        }
        u->port = (unsigned short)port;
        p = end;
    }
    u->path = malloc(strlen(p) + 2);
    if (u->path == NULL) {
        set_err("out of memory");
        return HTTP_ERROR;
    }
    if (*p == '/') {
        strcpy(u->path, p);
    } else {
        u->path[0] = '/';
        strcpy(u->path + 1, p);
    }
    frag = strchr(u->path, '#');
    if (frag) *frag = '\0';
    return HTTP_OK;
}

static int contains_ci(const char *s, const char *word) {
    size_t wl = strlen(word);
    for (; *s; s++) {
        if (strncasecmp(s, word, wl) == 0) return 1;
    }
    return 0;
}

static int copy_body(HttpConn *c, unsigned long len, HttpBodySink sink, void *ctx) {
    unsigned char buf[HTTP_RBUF_SIZE];
    while (len > 0) {
        size_t want = len < sizeof(buf) ? (size_t)len : sizeof(buf);
        int n = conn_read(c, buf, want);
        if (n <= 0) return conn_error(c, "connection closed before end of response");
        if (sink && sink(ctx, buf, (size_t)n) != HTTP_OK) return HTTP_ERROR;
        len -= (unsigned long)n;
    }
    return HTTP_OK;
}

static int read_response(HttpConn *c, int *status, HttpBodySink sink, void *ctx) {
    char line[HTTP_LINE_MAX];
    long clen;
    int chunked, code;

    do {
        clen = -1;
        chunked = 0;
        if (read_line(c, line, sizeof(line)) < 0) return conn_error(c, "no response from server");
        if (strncmp(line, "HTTP/1.", 7) != 0 || strlen(line) < 12) {
            set_err("invalid HTTP status line: %.80s", line);
            return HTTP_ERROR;
        }
        code = atoi(line + 9);
        for (;;) {
            int n = read_line(c, line, sizeof(line));
            if (n < 0) return conn_error(c, "connection closed while reading response headers");
            if (n == 0) break;
            if (strncasecmp(line, "Content-Length:", 15) == 0) {
                clen = strtol(line + 15, NULL, 10);
            } else if (strncasecmp(line, "Transfer-Encoding:", 18) == 0 && contains_ci(line + 18, "chunked")) {
                chunked = 1;
            }
        }
    } while (code >= 100 && code < 200); /* skip interim responses */

    if (debug) fprintf(stderr, "HTTP status %d\n", code);
    if (status) *status = code;
    if (code == 204 || code == 304) return HTTP_OK;

    if (chunked) {
        for (;;) {
            unsigned long size;
            if (read_line(c, line, sizeof(line)) < 0) return conn_error(c, "connection closed in chunked response");
            size = strtoul(line, NULL, 16);
            if (size == 0) break;
            if (copy_body(c, size, sink, ctx) != HTTP_OK) return HTTP_ERROR;
            if (read_line(c, line, sizeof(line)) != 0) {
                set_err("malformed chunked response");
                return HTTP_ERROR;
            }
        }
        /* skip trailers, the server may close right away */
        while (read_line(c, line, sizeof(line)) > 0) {
        }
        return HTTP_OK;
    }
    if (clen >= 0) return copy_body(c, (unsigned long)clen, sink, ctx);

    /* no length: body ends when the server closes the connection */
    for (;;) {
        unsigned char buf[HTTP_RBUF_SIZE];
        int n = conn_read(c, buf, sizeof(buf));
        if (n <= 0) break;
        if (sink && sink(ctx, buf, (size_t)n) != HTTP_OK) return HTTP_ERROR;
    }
    return HTTP_OK;
}

static int http_do(const char *method, const char *url, const char *contentType, long contentLength,
                   HttpBodySource src, void *srcCtx, HttpBodySink sink, void *sinkCtx, int *status) {
    HttpUrl u;
    HttpConn *c = NULL;
    char *head = NULL;
    size_t hsize;
    int off, attempt, rc = HTTP_ERROR;
    void (*oldPipe)(int);

    if (status) *status = 0;
    g_err[0] = '\0';
    if (parse_url(url, &u) != HTTP_OK) return HTTP_ERROR;

    c = calloc(1, sizeof(*c));
    hsize = strlen(u.path) + strlen(u.host) + (contentType ? strlen(contentType) : 0) + 256;
    head = malloc(hsize);
    if (c == NULL || head == NULL) {
        free(c);
        free(head);
        free(u.path);
        set_err("out of memory");
        return HTTP_ERROR;
    }
    c->fd = -1;
    c->timeoutSec = HTTP_TIMEOUT_SEC;

    /* a server closing the connection must not kill us with SIGPIPE */
    oldPipe = signal(SIGPIPE, SIG_IGN);

    off = snprintf(head, hsize, "%s %s HTTP/1.1\r\nHost: %s", method, u.path, u.host);
    if ((u.tls && u.port != 443) || (!u.tls && u.port != 80)) {
        off += snprintf(head + off, hsize - off, ":%u", u.port);
    }
    off += snprintf(head + off, hsize - off, "\r\nUser-Agent: %s\r\nAccept: */*\r\nConnection: close\r\n", HTTP_USER_AGENT);
    if (contentType) off += snprintf(head + off, hsize - off, "Content-Type: %s\r\n", contentType);
    if (contentLength >= 0) off += snprintf(head + off, hsize - off, "Content-Length: %ld\r\n", contentLength);
    snprintf(head + off, hsize - off, "\r\n");

    if (u.tls) prepare_entropy();

    /* nothing of the request is sent before the handshake is done, so retrying it is safe */
    for (attempt = 1; ; attempt++) {
        if (tcp_connect(c, &u) != HTTP_OK) goto done;
        if (!u.tls || tls_start(c, &u) == HTTP_OK) break;
        /* retry only network failures, not e.g. certificate errors */
        if (attempt >= HTTP_HANDSHAKE_ATTEMPTS || !(c->timedOut || c->peerClosed || c->sysErr)) goto done;
        if (debug) fprintf(stderr, "%s, retrying (%d/%d)\n", g_err, attempt + 1, HTTP_HANDSHAKE_ATTEMPTS);
        close(c->fd);
        memset(c, 0, sizeof(*c));
        c->fd = -1;
        c->timeoutSec = HTTP_TIMEOUT_SEC;
    }
    if (conn_write(c, head, strlen(head)) != HTTP_OK) goto done;
    if (src && src(c, srcCtx) != HTTP_OK) goto done;
    if (conn_flush(c) != HTTP_OK) goto done;
    rc = read_response(c, status, sink, sinkCtx);

done:
    if (c->fd >= 0) close(c->fd);
    signal(SIGPIPE, oldPipe);
    free(head);
    free(u.path);
    free(c);
    return rc;
}

/* ------------------------------------------------------------------------ */
/* public API                                                               */

static int mem_sink(void *ctx, const unsigned char *data, size_t len) {
    HttpBuffer *b = (HttpBuffer *)ctx;
    char *n = realloc(b->data, b->size + len + 1);
    if (n == NULL) {
        set_err("out of memory");
        return HTTP_ERROR;
    }
    b->data = n;
    memcpy(b->data + b->size, data, len);
    b->size += len;
    b->data[b->size] = '\0';
    return HTTP_OK;
}

static int file_sink(void *ctx, const unsigned char *data, size_t len) {
    if (fwrite(data, 1, len, (FILE *)ctx) != len) {
        set_err("cannot write to file: %s", strerror(errno));
        return HTTP_ERROR;
    }
    return HTTP_OK;
}

typedef struct _MemSource {
    const char *data;
    size_t len;
} MemSource;

static int mem_source(HttpConn *c, void *ctx) {
    MemSource *m = (MemSource *)ctx;
    return conn_write(c, m->data, m->len);
}

void http_buffer_free(HttpBuffer *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
}

int http_post(const char *url, const char *contentType, const char *body, size_t bodyLen,
              HttpBuffer *resp, int *status) {
    MemSource m;
    int rc;

    m.data = body;
    m.len = bodyLen;
    resp->data = NULL;
    resp->size = 0;
    rc = http_do("POST", url, contentType, (long)bodyLen, mem_source, &m, mem_sink, resp, status);
    if (rc != HTTP_OK) {
        http_buffer_free(resp);
        return rc;
    }
    if (resp->data == NULL) resp->data = calloc(1, 1); /* empty body */
    return HTTP_OK;
}

int http_get_to_file(const char *url, FILE *fp, int *status) {
    return http_do("GET", url, NULL, -1, NULL, NULL, file_sink, fp, status);
}

/* multipart/form-data */

typedef struct _MultipartSource {
    const HttpFormField *fields;
    int nfields;
    const char *boundary;
} MultipartSource;

/* Part header for field i. Returns a malloc'ed string. */
static char *part_header(const HttpFormField *f, const char *boundary) {
    const char *base;
    size_t size;
    char *h;

    base = f->isFile ? strrchr(f->value, '/') : NULL;
    base = base ? base + 1 : f->value;
    size = strlen(boundary) + strlen(f->name) + (f->isFile ? strlen(base) : 0) + 160;
    h = malloc(size);
    if (h == NULL) return NULL;
    if (f->isFile) {
        snprintf(h, size, "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
                 "Content-Type: application/octet-stream\r\n\r\n", boundary, f->name, base);
    } else {
        snprintf(h, size, "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n", boundary, f->name);
    }
    return h;
}

static int send_file(HttpConn *c, const char *path, long expected) {
    unsigned char buf[4096];
    long sent = 0;
    size_t n;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        set_err("cannot open %s: %s", path, strerror(errno));
        return HTTP_ERROR;
    }
    while (sent < expected && (n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if ((long)n > expected - sent) n = (size_t)(expected - sent);
        if (conn_write(c, buf, n) != HTTP_OK) {
            fclose(fp);
            return HTTP_ERROR;
        }
        sent += (long)n;
    }
    fclose(fp);
    if (sent != expected) {
        set_err("file %s changed while uploading", path);
        return HTTP_ERROR;
    }
    return HTTP_OK;
}

static int multipart_source(HttpConn *c, void *ctx) {
    MultipartSource *m = (MultipartSource *)ctx;
    int i;
    for (i = 0; i < m->nfields; i++) {
        const HttpFormField *f = &m->fields[i];
        char *h = part_header(f, m->boundary);
        int rc;
        if (h == NULL) {
            set_err("out of memory");
            return HTTP_ERROR;
        }
        rc = conn_write(c, h, strlen(h));
        free(h);
        if (rc != HTTP_OK) return rc;
        if (f->isFile) {
            struct stat st;
            if (stat(f->value, &st) != 0) {
                set_err("cannot stat %s: %s", f->value, strerror(errno));
                return HTTP_ERROR;
            }
            rc = send_file(c, f->value, (long)st.st_size);
        } else {
            rc = conn_write(c, f->value, strlen(f->value));
        }
        if (rc != HTTP_OK) return rc;
        if (conn_write(c, "\r\n", 2) != HTTP_OK) return HTTP_ERROR;
    }
    if (conn_write(c, "--", 2) != HTTP_OK) return HTTP_ERROR;
    if (conn_write(c, m->boundary, strlen(m->boundary)) != HTTP_OK) return HTTP_ERROR;
    return conn_write(c, "--\r\n", 4);
}

int http_post_multipart(const char *url, const HttpFormField *fields, int nfields, int *status) {
    char boundary[64];
    char ctype[128];
    unsigned char rnd[8];
    long total = 0;
    int i, rc;
    MultipartSource m;
    HttpBuffer resp;

    if (!read_dev_random(rnd, sizeof(rnd))) fallback_entropy(rnd, sizeof(rnd));
    snprintf(boundary, sizeof(boundary), "------------------------SGINet%02x%02x%02x%02x%02x%02x%02x%02x",
             rnd[0], rnd[1], rnd[2], rnd[3], rnd[4], rnd[5], rnd[6], rnd[7]);
    snprintf(ctype, sizeof(ctype), "multipart/form-data; boundary=%s", boundary);

    /* compute Content-Length up front, so the file can be streamed */
    for (i = 0; i < nfields; i++) {
        char *h = part_header(&fields[i], boundary);
        if (h == NULL) {
            set_err("out of memory");
            return HTTP_ERROR;
        }
        total += (long)strlen(h);
        free(h);
        if (fields[i].isFile) {
            struct stat st;
            if (stat(fields[i].value, &st) != 0) {
                set_err("cannot stat %s: %s", fields[i].value, strerror(errno));
                return HTTP_ERROR;
            }
            total += (long)st.st_size;
        } else {
            total += (long)strlen(fields[i].value);
        }
        total += 2; /* CRLF after part data */
    }
    total += 2 + (long)strlen(boundary) + 4; /* "--" boundary "--\r\n" */

    m.fields = fields;
    m.nfields = nfields;
    m.boundary = boundary;
    resp.data = NULL;
    resp.size = 0;
    rc = http_do("POST", url, ctype, total, multipart_source, &m, mem_sink, &resp, status);
    if (debug && resp.data) fprintf(stderr, "Upload response: %s\n", resp.data);
    http_buffer_free(&resp);
    return rc;
}
