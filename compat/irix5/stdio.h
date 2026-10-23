/* stdio.h wrapper for IRIX 5 */
#ifndef SGINET_COMPAT_STDIO_H
#define SGINET_COMPAT_STDIO_H

#include "/usr/include/stdio.h"
#include <stdarg.h>

int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

#endif /* SGINET_COMPAT_STDIO_H */
