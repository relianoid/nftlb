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

#ifndef _U_SBUFFER_H_
#define _U_SBUFFER_H_

#include <stdarg.h>
#include "u_common.h"

#define EXTRA_SIZE 1024

struct u_buffer {
	int size;
	int next;
	char *data;
};

#ifdef __cplusplus
extern "C" {
#endif

int u_buf_get_size(struct u_buffer *buf);
char *u_buf_get_next(struct u_buffer *buf);
int u_buf_resize(struct u_buffer *buf, int times);
int u_buf_create(struct u_buffer *buf);
int u_buf_isempty(struct u_buffer *buf);
char *u_buf_get_data(struct u_buffer *buf);
int u_buf_clean(struct u_buffer *buf);
int u_buf_reset(struct u_buffer *buf);
int u_buf_concat_va(struct u_buffer *buf, int len, char *fmt, va_list args);
int u_buf_concat(struct u_buffer *buf, char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* _U_SBUFFER_H_ */
