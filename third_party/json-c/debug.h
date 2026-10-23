/*
 * $Id: debug.h,v 1.5 2006/01/30 23:07:57 mclark Exp $
 *
 * Copyright (c) 2004, 2005 Metaparadigm Pte. Ltd.
 * Michael Clark <michael@metaparadigm.com>
 * Copyright (c) 2009 Hewlett-Packard Development Company, L.P.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING for details.
 *
 */

/**
 * @file
 * @brief Do not use, json-c internal, may be changed or removed at any time.
 */
#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void mc_set_debug(int debug);
extern int mc_get_debug(void);

extern void mc_set_syslog(int syslog);

extern void mc_debug(const char *msg, ...);
extern void mc_error(const char *msg, ...);
extern void mc_info(const char *msg, ...);

#ifndef __STRING
#define __STRING(x) #x
#endif

#ifndef PARSER_BROKEN_FIXED

#define JASSERT(cond) do {} while(0)

#else

#define JASSERT(cond) do { \
		if (!(cond)) { \
			mc_error("cjson assert failure %s:%d : cond \"" __STRING(cond) "failed\n", __FILE__, __LINE__); \
			*(int *)0 = 1;\
			abort(); \
		}\
	} while(0)

#endif

/* sginet: no variadic macros in C89, the macros expand to the function name */
#define MC_ERROR mc_error

#ifdef MC_MAINTAINER_MODE
#define MC_SET_DEBUG(x) mc_set_debug(x)
#define MC_GET_DEBUG() mc_get_debug()
#define MC_SET_SYSLOG(x) mc_set_syslog(x)
#define MC_DEBUG mc_debug
#define MC_INFO mc_info
#else
#define MC_SET_DEBUG(x) if (0) mc_set_debug(x)
#define MC_GET_DEBUG() (0)
#define MC_SET_SYSLOG(x) if (0) mc_set_syslog(x)
#define MC_DEBUG if (0) mc_debug
#define MC_INFO if (0) mc_info
#endif

#ifdef __cplusplus
}
#endif

#endif
