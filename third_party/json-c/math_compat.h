#ifndef __math_compat_h
#define __math_compat_h

/**
 * @file
 * @brief Do not use, json-c internal, may be changed or removed at any time.
 */

/* Define isnan, isinf, infinity and nan on Windows/MSVC */

#ifdef IRIX5
/* sginet: IRIX 5.x math.h is C89 */
#ifndef isnan
#define isnan(x) ((x) != (x))
#endif
#ifndef isinf
#define isinf(x) (!isnan(x) && isnan((x) - (x)))
#endif
#ifndef INFINITY
#define INFINITY HUGE_VAL
#endif
#ifndef NAN
static double json_c_nan(void)
{
	volatile double zero = 0.0;
	return zero / zero;
}
#define NAN json_c_nan()
#endif
#define HAVE_DECL_ISNAN
#define HAVE_DECL_ISINF
#define HAVE_DECL_INFINITY
#define HAVE_DECL_NAN
#endif /* IRIX5 */

#ifndef HAVE_DECL_ISNAN
# ifdef HAVE_DECL__ISNAN
#include <float.h>
#define isnan(x) _isnan(x)
# endif
#endif

#ifndef HAVE_DECL_ISINF
# ifdef HAVE_DECL__FINITE
#include <float.h>
#define isinf(x) (!_finite(x))
# endif
#endif

#ifndef HAVE_DECL_INFINITY
#include <float.h>
#define INFINITY (DBL_MAX + DBL_MAX)
#define HAVE_DECL_INFINITY
#endif

#ifndef HAVE_DECL_NAN
#define NAN (INFINITY - INFINITY)
#define HAVE_DECL_NAN
#endif

#endif
