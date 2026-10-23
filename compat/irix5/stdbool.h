/* stdbool.h for IRIX 5 */
#ifndef SGINET_COMPAT_STDBOOL_H
#define SGINET_COMPAT_STDBOOL_H

#ifndef bool
#define bool unsigned char
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
#define __bool_true_false_are_defined 1

#endif /* SGINET_COMPAT_STDBOOL_H */
