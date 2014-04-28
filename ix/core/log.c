/*
 * log.c - the logging system
 *
 * FIXME: Should we direct logs to a file?
 */

#include <ix/stddef.h>
#include <ix/log.h>
#include <ix/cpu.h>

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#define MAX_LOG_LEN	1024

__thread bool log_is_early_boot = true;

int max_loglevel = LOG_DEBUG;

void logk(int level, const char *fmt, ...)
{
	va_list ptr;
	char buf[MAX_LOG_LEN];
	time_t ts;
	off_t off = 0;

	if (level > max_loglevel)
		return;

	if (!log_is_early_boot) {
		snprintf(buf, 9, "CPU %02d| ", percpu_get(cpu_id));
		off = strlen(buf);
	}

	time(&ts);
	off += strftime(buf + off, 32, "%H:%M:%S ", localtime(&ts));

	snprintf(buf + off, 6, "<%d>: ", level);
	off = strlen(buf);

	va_start(ptr, fmt);
	vsnprintf(buf + off, MAX_LOG_LEN - off, fmt, ptr);
	va_end(ptr);

	printf("%s", buf);
}

