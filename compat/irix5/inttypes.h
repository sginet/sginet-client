/* inttypes.h for IRIX 5.x replaces the system one (see stdint.h here).
 * Adds the printf/scanf format macros, which IRIX 5 doesn't have. */
#ifndef SGINET_COMPAT_INTTYPES_H
#define SGINET_COMPAT_INTTYPES_H

#if defined(__sgi)
#include <stdint.h>

#define PRId8 "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 "lld"
#define PRIi32 "i"
#define PRIi64 "lli"
#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "llu"
#define PRIx32 "x"
#define PRIx64 "llx"
#define PRIX32 "X"
#define PRIX64 "llX"
#define PRIdMAX "lld"
#define PRIuMAX "llu"
#define PRIdPTR "ld"
#define PRIuPTR "lu"
#define PRIxPTR "lx"

#define SCNd32 "d"
#define SCNd64 "lld"
#define SCNu32 "u"
#define SCNu64 "llu"
#define SCNx32 "x"
#define SCNx64 "llx"
#else
/* gcc, for "make c89check" on Linux */
#include_next <inttypes.h>
#endif

#endif /* SGINET_COMPAT_INTTYPES_H */
