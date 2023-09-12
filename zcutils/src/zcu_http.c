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

#include "zcu_http.h"

const char *ws_str_responses[WS_HTTP_MAX] = {
	HTTP_PROTO "500 Internal Server Error" HTTP_LINE_END,
	HTTP_PROTO "505 HTTP Version Not Supported" HTTP_LINE_END,
	HTTP_PROTO "400 Bad Request" HTTP_LINE_END,
	HTTP_PROTO "401 Unauthorized" HTTP_LINE_END,
	HTTP_PROTO "404 Not Found" HTTP_LINE_END,
	HTTP_PROTO "405 Method Not Allowed" HTTP_LINE_END,
	HTTP_PROTO "409 Conflict" HTTP_LINE_END,
	HTTP_PROTO "301 Moved Permanently" HTTP_LINE_END,
	HTTP_PROTO "302 Found" HTTP_LINE_END,
	HTTP_PROTO "307 Temporary Redirect" HTTP_LINE_END,
	HTTP_PROTO "200 OK" HTTP_LINE_END,
	HTTP_PROTO "201 Created" HTTP_LINE_END,
	HTTP_PROTO "204 No Content" HTTP_LINE_END,
};

enum ws_responses http_to_ws(int code) {
	switch (code) {
	case 500: return WS_HTTP_500; break;
	case 505: return WS_HTTP_505; break;
	case 400: return WS_HTTP_400; break;
	case 401: return WS_HTTP_401; break;
	case 404: return WS_HTTP_404; break;
	case 405: return WS_HTTP_405; break;
	case 409: return WS_HTTP_409; break;
	case 301: return WS_HTTP_301; break;
	case 302: return WS_HTTP_302; break;
	case 307: return WS_HTTP_307; break;
	case 200: return WS_HTTP_200; break;
	case 201: return WS_HTTP_201; break;
	case 204: return WS_HTTP_204; break;
	default: return WS_HTTP_MAX; break;
	}
}
