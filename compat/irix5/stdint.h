/* stdint.h for IRIX 5 */
#ifndef SGINET_COMPAT_STDINT_H
#define SGINET_COMPAT_STDINT_H

#if defined(__sgi)
#include <sys/types.h>

#define int8_t signed char
#define uint8_t unsigned char
#define int16_t short
#define uint16_t unsigned short
#define int32_t int
#define uint32_t unsigned int
#define int64_t long long
#define uint64_t unsigned long long
#define intmax_t long long
#define uintmax_t unsigned long long
#define intptr_t long
#define uintptr_t unsigned long

#define INT8_MIN (-127-1)
#define INT8_MAX (127)
#define UINT8_MAX (255u)
#define INT16_MIN (-32767-1)
#define INT16_MAX (32767)
#define UINT16_MAX (65535u)
#define INT32_MIN (-2147483647-1)
#define INT32_MAX (2147483647)
#define UINT32_MAX (4294967295u)
#define INT64_MIN (-9223372036854775807LL-1)
#define INT64_MAX (9223372036854775807LL)
#define UINT64_MAX (18446744073709551615uLL)
#define INTMAX_MIN INT64_MIN
#define INTMAX_MAX INT64_MAX
#define UINTMAX_MAX UINT64_MAX
#define INTPTR_MIN (-2147483647L-1)
#define INTPTR_MAX (2147483647L)
#define UINTPTR_MAX (4294967295uL)

#define INT8_C(c) c
#define UINT8_C(c) c ## u
#define INT16_C(c) c
#define UINT16_C(c) c ## u
#define INT32_C(c) c
#define UINT32_C(c) c ## u
#define INT64_C(c) c ## LL
#define UINT64_C(c) c ## uLL
#define INTMAX_C(c) c ## LL
#define UINTMAX_C(c) c ## uLL
#else
/* gcc, for "make c89check" on Linux */
#include_next <stdint.h>
#endif

#ifndef SIZE_MAX
#define SIZE_MAX 4294967295u /* o32 */
#endif

#endif /* SGINET_COMPAT_STDINT_H */
