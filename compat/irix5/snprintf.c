/* snprintf()/vsnprintf() for IRIX 5 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(va_copy)
#define COPY_VA(d, s) va_copy(d, s)
#elif defined(__va_copy)
#define COPY_VA(d, s) __va_copy(d, s)
#else
#define COPY_VA(d, s) ((d) = (s)) /* IRIX o32: va_list is a plain pointer */
#endif

static FILE *devnull;

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    va_list ap2;
    int len;
    char *tmp;

    if (devnull == NULL) {
        devnull = fopen("/dev/null", "w");
        if (devnull == NULL) {
            if (size > 0) str[0] = '\0';
            return -1;
        }
    }

    COPY_VA(ap2, ap);
    len = vfprintf(devnull, fmt, ap);
    if (len < 0 || size == 0) {
        if (size > 0) str[0] = '\0';
        va_end(ap2);
        return len;
    }

    if ((size_t)len < size) {
        vsprintf(str, fmt, ap2);
    } else {
        tmp = (char *)malloc((size_t)len + 1);
        if (tmp == NULL) {
            str[0] = '\0';
            va_end(ap2);
            return -1;
        }
        vsprintf(tmp, fmt, ap2);
        memcpy(str, tmp, size - 1);
        str[size - 1] = '\0';
        free(tmp);
    }
    va_end(ap2);
    return len;
}

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return len;
}
