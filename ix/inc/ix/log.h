/*
 * log.h - the logging service
 */

#pragma once

#include <ix/types.h>

extern bool log_is_early_boot;

extern void logk(int level, const char *fmt, ...);

extern int max_loglevel;

enum {
	LOG_EMERG	= 0, /* system is dead */
	LOG_CRIT	= 1, /* critical */
	LOG_ERR	   	= 2, /* error */
	LOG_WARN	= 3, /* warning */
	LOG_INFO	= 4, /* informational */
	LOG_DEBUG	= 5, /* debug */
};

#define log_emerg(fmt, ...) logk(LOG_EMERG, fmt, ##__VA_ARGS__)
#define log_crit(fmt, ...) logk(LOG_CRIT, fmt, ##__VA_ARGS__)
#define log_err(fmt, ...) logk(LOG_ERR, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) logk(LOG_WARN, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) logk(LOG_INFO, fmt, ##__VA_ARGS__)

#ifdef DEBUG
#define log_debug(fmt, ...) logk(LOG_DEBUG, fmt, ##__VA_ARGS__)
#else
#define log_debug(fmt, ...)
#endif

#define panic(fmt, ...) \
do {logk(LOG_EMERG, fmt, ##__VA_ARGS__); exit(-1);} while (0)

