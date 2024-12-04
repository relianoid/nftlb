/*
 * Copyright (C) RELIANOID
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _U_LOG_H_
#define _U_LOG_H_

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>

#define LOG_PREFIX_BUFSIZE 100

#define UTILS_LOG_OUTPUT_SYSLOG (1 << 0)
#define UTILS_LOG_OUTPUT_STDOUT (1 << 1)
#define UTILS_LOG_OUTPUT_STDERR (1 << 2)

enum u_log_output {
	VALUE_LOG_OUTPUT_SYSLOG,
	VALUE_LOG_OUTPUT_STDOUT,
	VALUE_LOG_OUTPUT_STDERR,
	VALUE_LOG_OUTPUT_SYSOUT,
	VALUE_LOG_OUTPUT_SYSERR,
};

#define UTILS_LOG_LEVEL_DEFAULT LOG_NOTICE
#define UTILS_LOG_OUTPUT_DEFAULT UTILS_LOG_OUTPUT_SYSLOG

extern char u_log_prefix[LOG_PREFIX_BUFSIZE];
extern int u_log_level;
extern int u_log_output;

#ifdef __cplusplus
extern "C" {
#endif

void u_log_set_prefix(const char *string);
void u_log_set_level(int loglevel);
int u_log_get_level(void);
void u_log_set_output(int output);
int u_log_print(int loglevel, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#define u_log_print_th(loglevel, fmt, ...)                                      \
	u_log_print(loglevel, "%s[th:%lx] " fmt, u_log_prefix,        \
	       (unsigned int) (pthread_self()),              \
	       ##__VA_ARGS__)

#endif /* _U_LOG_H_ */
