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

#include "u_log.h"

char u_log_prefix[LOG_PREFIX_BUFSIZE] = "";
int u_log_level = UTILS_LOG_LEVEL_DEFAULT;
int u_log_output = UTILS_LOG_OUTPUT_DEFAULT;


void u_log_set_prefix(const char *string)
{
    if (strlen(string) >= LOG_PREFIX_BUFSIZE)
        u_log_print(
            LOG_ERR,
            "The farm name is greater than the prefix log: %d >= %d",
            strlen(string), LOG_PREFIX_BUFSIZE);
    else
        memcpy(u_log_prefix, string, strlen(string) + 1);
}

void u_log_set_level(int loglevel)
{
    u_log_level = loglevel;
    setlogmask(LOG_UPTO(loglevel));
}

int u_log_get_level(void)
{
	return u_log_level;
}

void u_log_set_output(int output)
{
    switch (output) {
    case VALUE_LOG_OUTPUT_STDOUT:
        u_log_output = UTILS_LOG_OUTPUT_STDOUT;
        break;
    case VALUE_LOG_OUTPUT_STDERR:
        u_log_output = UTILS_LOG_OUTPUT_STDERR;
        break;
    case VALUE_LOG_OUTPUT_SYSOUT:
        u_log_output =
            UTILS_LOG_OUTPUT_SYSLOG | UTILS_LOG_OUTPUT_STDOUT;
        break;
    case VALUE_LOG_OUTPUT_SYSERR:
        u_log_output =
            UTILS_LOG_OUTPUT_SYSLOG | UTILS_LOG_OUTPUT_STDERR;
        break;
    case VALUE_LOG_OUTPUT_SYSLOG:
    default:
        u_log_output = UTILS_LOG_OUTPUT_SYSLOG;
    }
    return;
}

int u_log_print(int loglevel, const char *fmt, ...)
{
    va_list args;

    if (loglevel > u_log_level)
        return 0;

    if (u_log_output & UTILS_LOG_OUTPUT_STDOUT) {
        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        fprintf(stdout, "\n");
        va_end(args);
    }

    if (u_log_output & UTILS_LOG_OUTPUT_STDERR) {
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
    }

    if (u_log_output & UTILS_LOG_OUTPUT_SYSLOG) {
        va_start(args, fmt);
        vsyslog(loglevel, fmt, args);
        va_end(args);
    }

    return 0;
}
