/* config.h - hand-written for sginet, replaces the autoconf output.
 * Targets IRIX 6.5 / MIPSpro 7.4 (n32) and also builds with gcc on Linux.
 * IRIX5: IRIX 5.x cc (C89), no INFINITY/NAN/isnan/isinf, see math_compat.h. */

#include <limits.h>

#ifndef IRIX5
#define HAVE_DECL_INFINITY 1
#define HAVE_DECL_ISINF 1
#define HAVE_DECL_ISNAN 1
#define HAVE_DECL_NAN 1
#endif
#define HAVE_FCNTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_MEMORY_H 1
#define HAVE_OPEN 1
#define HAVE_REALLOC 1
#define HAVE_SETLOCALE 1
#define HAVE_SNPRINTF 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCASECMP 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRNCASECMP 1
#define HAVE_SYSLOG_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_VPRINTF 1
#define HAVE_VSNPRINTF 1
#ifndef IRIX5
#define HAVE_VSYSLOG 1  /* may be missing in IRIX 5 libc; unused, json-c falls back to vprintf */
#endif
/* IRIX has no vasprintf(); json-c's vasprintf_compat.h provides one there.
 * uselocale()/xlocale.h, __thread and atomic builtins are not used. */
#ifndef __sgi
#define HAVE_VASPRINTF 1
#endif

#define JSON_C_HAVE_INTTYPES_H 1

#define PACKAGE "json-c"
#define PACKAGE_NAME "json-c"
#define PACKAGE_VERSION "0.13.1"
#define PACKAGE_STRING "json-c 0.13.1"
#define VERSION "0.13.1"
#define STDC_HEADERS 1

/* type sizes: n32 is ILP32 with 64-bit long long */
#define SIZEOF_INT 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_INT64_T 8
#if LONG_MAX > 2147483647L
#define SIZEOF_LONG 8
#define SIZEOF_SIZE_T 8
#else
#define SIZEOF_LONG 4
#define SIZEOF_SIZE_T 4
#endif
