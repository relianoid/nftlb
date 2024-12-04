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

#include "u_sbuffer.h"
#include "u_log.h"

int u_buf_get_size(struct u_buffer *buf)
{
	return buf->size;
}

char *u_buf_get_next(struct u_buffer *buf)
{
	return buf->data + buf->next;
}

int u_buf_resize(struct u_buffer *buf, int times)
{
	char *pbuf;
	int newsize;

	if (times == 0)
		return 0;

	newsize = buf->size + (times * EXTRA_SIZE) + 1;

	if (!buf->data)
		return 1;

	pbuf = (char *)realloc(buf->data, newsize);
	if (!pbuf)
		return 1;

	buf->data = pbuf;
	buf->size = newsize;
	return 0;
}

int u_buf_create(struct u_buffer *buf)
{
	buf->size = 0;
	buf->next = 0;

	buf->data = (char *)calloc(1, U_DEF_BUFFER_SIZE);
	if (!buf->data) {
		return 1;
	}

	*buf->data = '\0';
	buf->size = U_DEF_BUFFER_SIZE;
	return 0;
}

int u_buf_isempty(struct u_buffer *buf)
{
	return (buf->data[0] == 0);
}

char *u_buf_get_data(struct u_buffer *buf)
{
	return buf->data;
}

int u_buf_clean(struct u_buffer *buf)
{
	if (buf->data)
		free(buf->data);
	buf->size = 0;
	buf->next = 0;
	return 0;
}

int u_buf_reset(struct u_buffer *buf)
{
	buf->data[0] = 0;
	buf->next = 0;
	return 0;
}

int u_buf_concat_va(struct u_buffer *buf, int len, char *fmt, va_list args)
{
	int times = 0;
	char *pnext;

	if (buf->next + len >= buf->size)
		times = ((buf->next + len - buf->size) / EXTRA_SIZE) + 1;

	if (u_buf_resize(buf, times)) {
		u_log_print(
			LOG_ERR,
			"Error resizing the buffer %d times from a size of %d!",
			times, buf->size);
		return 1;
	}

	pnext = u_buf_get_next(buf);
	vsnprintf(pnext, len + 1, fmt, args);
	buf->next += len;

	return 0;
}

int u_buf_concat(struct u_buffer *buf, char *fmt, ...)
{
	int len;
	va_list args;

	va_start(args, fmt);
	len = vsnprintf(0, 0, fmt, args);
	va_end(args);

	va_start(args, fmt);
	u_buf_concat_va(buf, len, fmt, args);
	va_end(args);

	return 0;
}
