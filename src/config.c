/*
 *   This file is part of nftlb, nftables load balancer.
 *
 *   Copyright (C) RELIANOID
 *   Author: Laura Garcia Liebana <laura@relianoid.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Affero General Public License as
 *   published by the Free Software Foundation, either version 3 of the
 *   License, or any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Affero General Public License for more details.
 *
 *   You should have received a copy of the GNU Affero General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <jansson.h>

#include "config.h"
#include "farms.h"
#include "backends.h"
#include "farmpolicy.h"
#include "policies.h"
#include "elements.h"
#include "sessions.h"
#include "addresses.h"
#include "farmaddress.h"
#include "addresspolicy.h"
#include "u_log.h"

#define CONFIG_MAXBUF			4096
#define CONFIG_OUTBUF_SIZE		1024

static int config_json(json_t *element, int level, int source, int key, int apply_action);

struct config_pair c;
unsigned int continue_obj = 0;
char config_outbuf[CONFIG_OUTBUF_SIZE] = { 0 };

static void init_pair(struct config_pair *c)
{
	c->level = -1;
	c->key = -1;
	c->str_value = NULL;
	c->int_value = -1;
	c->int_value2 = -1;
	c->action = ACTION_START;
}

static void config_dump_int(char *buf, int value)
{
	sprintf(buf, "%d", value);
}

static void config_dump_hex(char *buf, int value)
{
	sprintf(buf, "0x%x", value);
}

static int config_value_family(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_FAMILY_IPV4) == 0)
		return VALUE_FAMILY_IPV4;
	if (strcmp(value, CONFIG_VALUE_FAMILY_IPV6) == 0)
		return VALUE_FAMILY_IPV6;
	if (strcmp(value, CONFIG_VALUE_FAMILY_INET) == 0)
		return VALUE_FAMILY_INET;

	config_set_output(". Parsing unknown value '%s' in '%s', using default '%s'", value, CONFIG_KEY_FAMILY, CONFIG_VALUE_FAMILY_IPV4);
	u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_FAMILY, CONFIG_VALUE_FAMILY_IPV4);
	return VALUE_FAMILY_IPV4;
}

static int config_value_mode(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_MODE_SNAT) == 0)
		return VALUE_MODE_SNAT;
	if (strcmp(value, CONFIG_VALUE_MODE_DNAT) == 0)
		return VALUE_MODE_DNAT;
	if (strcmp(value, CONFIG_VALUE_MODE_DSR) == 0)
		return VALUE_MODE_DSR;
	if (strcmp(value, CONFIG_VALUE_MODE_STLSDNAT) == 0)
		return VALUE_MODE_STLSDNAT;
	if (strcmp(value, CONFIG_VALUE_MODE_LOCAL) == 0)
		return VALUE_MODE_LOCAL;

	config_set_output(". Parsing unknown value '%s' in '%s', using default '%s'", value, CONFIG_KEY_MODE, CONFIG_VALUE_MODE_SNAT);
	u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_MODE, CONFIG_VALUE_MODE_SNAT);
	return VALUE_MODE_SNAT;
}

static int config_value_proto(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_PROTO_TCP) == 0)
		return VALUE_PROTO_TCP;
	if (strcmp(value, CONFIG_VALUE_PROTO_UDP) == 0)
		return VALUE_PROTO_UDP;
	if (strcmp(value, CONFIG_VALUE_PROTO_SCTP) == 0)
		return VALUE_PROTO_SCTP;
	if (strcmp(value, CONFIG_VALUE_PROTO_ALL) == 0)
		return VALUE_PROTO_ALL;

	config_set_output(". Parsing unknown value '%s' in '%s', using default '%s'", value, CONFIG_KEY_PROTO, CONFIG_VALUE_PROTO_TCP);
	u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_PROTO, CONFIG_VALUE_PROTO_TCP);
	return VALUE_PROTO_TCP;
}

static int config_value_sched(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_SCHED_RR) == 0)
		return VALUE_SCHED_RR;
	if (strcmp(value, CONFIG_VALUE_SCHED_WEIGHT) == 0)
		return VALUE_SCHED_WEIGHT;
	if (strcmp(value, CONFIG_VALUE_SCHED_HASH) == 0)
		return VALUE_SCHED_HASH;
	if (strcmp(value, CONFIG_VALUE_SCHED_SYMHASH) == 0)
		return VALUE_SCHED_SYMHASH;

	config_set_output(". Parsing unknown value '%s' in '%s', using default '%s'", value, CONFIG_KEY_SCHED, CONFIG_VALUE_SCHED_RR);
	u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_SCHED, CONFIG_VALUE_SCHED_RR);
	return VALUE_SCHED_RR;
}

static int config_value_meta(const char *value)
{
	int mask = 0;

	if (strstr(value, CONFIG_VALUE_META_NONE) != NULL) {
		mask = VALUE_META_NONE;
		return mask;
	}

	if (strstr(value, CONFIG_VALUE_META_SRCIP) != NULL)
		mask |= VALUE_META_SRCIP;
	if (strstr(value, CONFIG_VALUE_META_DSTIP) != NULL)
		mask |= VALUE_META_DSTIP;
	if (strstr(value, CONFIG_VALUE_META_SRCPORT) != NULL)
		mask |= VALUE_META_SRCPORT;
	if (strstr(value, CONFIG_VALUE_META_DSTPORT) != NULL)
		mask |= VALUE_META_DSTPORT;
	if (strstr(value, CONFIG_VALUE_META_SRCMAC) != NULL)
		mask |= VALUE_META_SRCMAC;
	if (strstr(value, CONFIG_VALUE_META_DSTMAC) != NULL)
		mask |= VALUE_META_DSTMAC;

	if (mask == 0) {
		config_set_output(". Parsing unknown value '%s', using default '%s'", value, CONFIG_VALUE_META_NONE);
		u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_VALUE_META_NONE);
	}

	return mask;
}

static int config_value_helper(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_HELPER_NONE) == 0)
		return VALUE_HELPER_NONE;
	if (strcmp(value, CONFIG_VALUE_HELPER_FTP) == 0)
		return VALUE_HELPER_FTP;
	if (strcmp(value, CONFIG_VALUE_HELPER_PPTP) == 0)
		return VALUE_HELPER_PPTP;
	if (strcmp(value, CONFIG_VALUE_HELPER_SIP) == 0)
		return VALUE_HELPER_SIP;
	if (strcmp(value, CONFIG_VALUE_HELPER_SNMP) == 0)
		return VALUE_HELPER_SNMP;
	if (strcmp(value, CONFIG_VALUE_HELPER_TFTP) == 0)
		return VALUE_HELPER_TFTP;

	config_set_output(". Parsing unknown value '%s' in '%s', using default '%s'", value, CONFIG_KEY_HELPER, CONFIG_VALUE_HELPER_NONE);
	u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_HELPER, CONFIG_VALUE_HELPER_NONE);
	return VALUE_HELPER_NONE;
}

static int config_value_log(const char *value)
{
	int logmask = 0;

	if (strstr(value, CONFIG_VALUE_LOG_NONE) != NULL) {
		logmask = VALUE_LOG_NONE;
		return logmask;
	}

	if (strstr(value, CONFIG_VALUE_LOG_INPUT) != NULL)
		logmask |= VALUE_LOG_INPUT;
	if (strstr(value, CONFIG_VALUE_LOG_FORWARD) != NULL)
		logmask |= VALUE_LOG_FORWARD;
	if (strstr(value, CONFIG_VALUE_LOG_OUTPUT) != NULL)
		logmask |= VALUE_LOG_OUTPUT;

	if (logmask == 0) {
		config_set_output(". Parsing unknown value '%s' in '%s', using default '%s'", value, CONFIG_KEY_LOG, CONFIG_VALUE_LOG_NONE);
		u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_LOG, CONFIG_VALUE_LOG_NONE);
	}

	return logmask;
}

static int config_value_switch(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_SWITCH_ON) == 0)
		return VALUE_SWITCH_ON;
	else
		return VALUE_SWITCH_OFF;

	config_set_output(". Parsing unknown value '%s', using default '%s'", value, CONFIG_VALUE_SWITCH_OFF);
	u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_VALUE_SWITCH_OFF);
	return VALUE_SWITCH_OFF;
}

static int config_value_state(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_STATE_UP) == 0)
		return VALUE_STATE_UP;
	if (strcmp(value, CONFIG_VALUE_STATE_DOWN) == 0)
		return VALUE_STATE_DOWN;
	if (strcmp(value, CONFIG_VALUE_STATE_OFF) == 0)
		return VALUE_STATE_OFF;
	if (strcmp(value, CONFIG_VALUE_STATE_CONFERR) == 0)
		return VALUE_STATE_CONFERR;

	config_set_output(". Parsing unknown value '%s' in '%s', using default '%s'", value, CONFIG_KEY_STATE, CONFIG_VALUE_STATE_UP);
	u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_STATE, CONFIG_VALUE_STATE_UP);
	return VALUE_STATE_UP;
}

static int config_value_action(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_ACTION_STOP) == 0)
		return ACTION_STOP;
	if (strcmp(value, CONFIG_VALUE_ACTION_DELETE) == 0)
		return ACTION_DELETE;
	if (strcmp(value, CONFIG_VALUE_ACTION_START) == 0)
		return ACTION_START;
	if (strcmp(value, CONFIG_VALUE_ACTION_RELOAD) == 0)
		return ACTION_RELOAD;
	if (strcmp(value, CONFIG_VALUE_ACTION_FLUSH) == 0)
		return ACTION_FLUSH;

	return ACTION_NONE;
}

static int config_value_type(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_POLICIES_TYPE_BL) == 0)
		return VALUE_TYPE_DENY;
	if (strcmp(value, CONFIG_VALUE_POLICIES_TYPE_WL) == 0)
		return VALUE_TYPE_ALLOW;

	config_set_output(". Parsing unknown value '%s' in '%s', using default '%s'", value, CONFIG_KEY_TYPE, CONFIG_VALUE_POLICIES_TYPE_BL);
	u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_TYPE, CONFIG_VALUE_POLICIES_TYPE_BL);
	return VALUE_TYPE_DENY;
}

static int config_value_verdict(const char *value)
{
	int verdictmask = VALUE_VERDICT_NONE;

	if (strstr(value, CONFIG_VALUE_VERDICT_LOG) != NULL)
		verdictmask |= VALUE_VERDICT_LOG;
	if (strstr(value, CONFIG_VALUE_VERDICT_DROP) != NULL)
		verdictmask |= VALUE_VERDICT_DROP;
	if (strstr(value, CONFIG_VALUE_VERDICT_ACCEPT) != NULL)
		verdictmask |= VALUE_VERDICT_ACCEPT;

	if (verdictmask == VALUE_VERDICT_NONE) {
		config_set_output(". Parsing unknown value '%s' in '%s'", value, CONFIG_KEY_VERDICT);
		u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_VERDICT);
		return VALUE_VERDICT_NONE;
	}

	return verdictmask;
}

static int config_value_route(const char *value)
{
	if (strcmp(value, CONFIG_VALUE_ROUTE_IN) == 0)
		return VALUE_ROUTE_IN;
	if (strcmp(value, CONFIG_VALUE_ROUTE_OUT) == 0)
		return VALUE_ROUTE_OUT;

	config_set_output(". Parsing unknown value '%s' in '%s', using default '%s'", value, CONFIG_KEY_ROUTE, CONFIG_VALUE_ROUTE_IN);
	u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s', using default '%s'", __FUNCTION__, __LINE__, value, CONFIG_KEY_ROUTE, CONFIG_VALUE_ROUTE_IN);
	return VALUE_ROUTE_IN;
}

static int config_value_ratelimit(int key, int *int_value, int *int_unit, const char *value)
{
	char str_unit[100] = { 0 };

	if (!value[0] || value[0] == '0') {
		*int_value = DEFAULT_NEWRTLIMIT;
		*int_unit = DEFAULT_RTLIMIT_UNIT;
		return PARSER_OK;
	}

	sscanf(value, "%d%*[/]%99[a-zA-Z]", int_value, str_unit);

	if (*int_value < 0) {
		config_set_output(". Invalid value of key '%s' must be >=0", obj_print_key(key));
		u_log_print(LOG_ERR, "%s():%d: invalid value of key '%s' must be >=0", __FUNCTION__, __LINE__, obj_print_key(key));
		return PARSER_VALID_FAILED;
	}

	if (strcmp(str_unit, "") == 0)
		return PARSER_OK;

	if (strcmp(str_unit, CONFIG_VALUE_UNIT_SECOND) == 0)
		*int_unit = VALUE_UNIT_SECOND;
	else if (strcmp(str_unit, CONFIG_VALUE_UNIT_MINUTE) == 0)
		*int_unit = VALUE_UNIT_MINUTE;
	else if (strcmp(str_unit, CONFIG_VALUE_UNIT_HOUR) == 0)
		*int_unit = VALUE_UNIT_HOUR;
	else if (strcmp(str_unit, CONFIG_VALUE_UNIT_DAY) == 0)
		*int_unit = VALUE_UNIT_DAY;
	else if (strcmp(str_unit, CONFIG_VALUE_UNIT_WEEK) == 0)
		*int_unit = VALUE_UNIT_WEEK;
	else {
		config_set_output(". Parsing unknown value '%s' in '%s'", value, obj_print_key(key));
		u_log_print(LOG_ERR, "%s():%d: parsing unknown value '%s' in '%s'", __FUNCTION__, __LINE__, value, obj_print_key(key));
		return PARSER_VALID_FAILED;
	}

	return PARSER_OK;
}

static int config_value(const char *value)
{
	int ret = PARSER_VALID_FAILED;
	int new_int_value;

	switch(c.key) {
	case KEY_FAMILY:
		c.int_value = config_value_family(value);
		ret = PARSER_OK;
		break;
	case KEY_MODE:
		c.int_value = config_value_mode(value);
		ret = PARSER_OK;
		break;
	case KEY_PROTO:
		c.int_value = config_value_proto(value);
		ret = PARSER_OK;
		break;
	case KEY_SCHED:
		c.int_value = config_value_sched(value);
		ret = PARSER_OK;
		break;
	case KEY_SCHEDPARAM:
	case KEY_PERSISTENCE:
		c.int_value = config_value_meta(value);
		ret = PARSER_OK;
		break;
	case KEY_HELPER:
		c.int_value = config_value_helper(value);
		ret = PARSER_OK;
		break;
	case KEY_LOG:
		c.int_value = config_value_log(value);
		ret = PARSER_OK;
		break;
	case KEY_MARK:
		c.int_value = (int)strtol(value, NULL, 16);
		ret = PARSER_OK;
		break;
	case KEY_STATE:
		c.int_value = config_value_state(value);
		ret = PARSER_OK;
		break;
	case KEY_WEIGHT:
	case KEY_PRIORITY:
		new_int_value = atoi(value);
		if (new_int_value >= 1) {
			c.int_value = new_int_value;
			ret = PARSER_OK;
			break;
		}
		config_set_output(". Invalid value of key '%s' must be >=1", obj_print_key(c.key));
		u_log_print(LOG_ERR, "%s():%d: invalid value of key '%s' must be >=1", __FUNCTION__, __LINE__, obj_print_key(c.key));
		break;
	case KEY_RESPONSETTL:
	case KEY_PERSISTTM:
	case KEY_LIMITSTTL:
	case KEY_NEWRTLIMITBURST:
	case KEY_RSTRTLIMITBURST:
	case KEY_ESTCONNLIMIT:
	case KEY_TIMEOUT:
		new_int_value = atoi(value);
		if (new_int_value >= 0) {
			c.int_value = new_int_value;
			ret = PARSER_OK;
			break;
		}
		config_set_output(". Invalid value of key '%s' must be >=0", obj_print_key(c.key));
		u_log_print(LOG_ERR, "%s():%d: invalid value of key '%s' must be >=0", __FUNCTION__, __LINE__, obj_print_key(c.key));
		break;
	case KEY_NEWRTLIMIT:
	case KEY_RSTRTLIMIT:
	case KEY_LOG_RTLIMIT:
		ret = config_value_ratelimit(c.key, &c.int_value, &c.int_value2, (char *)value);
		break;
	case KEY_QUEUE:
		new_int_value = atoi(value);
		if (new_int_value >= -1) {
			c.int_value = new_int_value;
			ret = PARSER_OK;
			break;
		}
		config_set_output(". Invalid value of key '%s' must be >=-1", obj_print_key(c.key));
		u_log_print(LOG_ERR, "%s():%d: invalid value of key '%s' must be >=-1", __FUNCTION__, __LINE__, obj_print_key(c.key));
		break;
	case KEY_ACTION:
		c.int_value = config_value_action(value);
		ret = PARSER_OK;
		break;
	case KEY_TCPSTRICT:
	case KEY_FLOWOFFLOAD:
	case KEY_INTRACONNECT:
		c.int_value = config_value_switch(value);
		ret = PARSER_OK;
		break;
	case KEY_TYPE:
		c.int_value = config_value_type(value);
		ret = PARSER_OK;
		break;
	case KEY_VERDICT:
		c.int_value = config_value_verdict(value);
		ret = PARSER_OK;
		break;
	case KEY_NAME:
	case KEY_NEWNAME:
	case KEY_IPADDR:
	case KEY_CLIENT:
	case KEY_BACKEND:
	case KEY_DATA:
		if (strcmp(value, "") == 0) {
			config_set_output(". Key '%s' cannot be empty", obj_print_key(c.key));
			u_log_print(LOG_ERR, "%s():%d: key '%s' cannot be empty", __FUNCTION__, __LINE__, obj_print_key(c.key));
			break;
		}
		/* fallthrough */
	case KEY_IFACE:
	case KEY_OFACE:
	case KEY_ETHADDR:
	case KEY_VIRTADDR:
	case KEY_VIRTPORTS:
	case KEY_PORTS:
	case KEY_SRCADDR:
	case KEY_PORT:
	case KEY_LOGPREFIX:
	case KEY_NEWRTLIMIT_LOGPREFIX:
	case KEY_RSTRTLIMIT_LOGPREFIX:
	case KEY_ESTCONNLIMIT_LOGPREFIX:
	case KEY_TCPSTRICT_LOGPREFIX:
		c.str_value = (char *)value;
		ret = PARSER_OK;
		break;
	case KEY_USED:
	case KEY_COUNTER_PACKETS:
	case KEY_COUNTER_BYTES:
		ret = PARSER_IGNORE;
		break;
	case KEY_ROUTE:
		c.int_value = config_value_route(value);
		ret = PARSER_OK;
		break;
	default:
		config_set_output(". Unknown parsed key with index '%d'", c.key);
		u_log_print(LOG_ERR, "%s():%d: unknown parsed key with index '%d'", __FUNCTION__, __LINE__, c.key);
		ret = PARSER_OBJ_UNKNOWN;
		break;
	}

	return ret;
}

static int config_key(const char *key)
{
	if (strcmp(key, CONFIG_KEY_FARMS) == 0)
		return KEY_FARMS;
	if (strcmp(key, CONFIG_KEY_NAME) == 0)
		return KEY_NAME;
	if (strcmp(key, CONFIG_KEY_NEWNAME) == 0)
		return KEY_NEWNAME;
	if (strcmp(key, CONFIG_KEY_FQDN) == 0)
		return KEY_FQDN;
	if (strcmp(key, CONFIG_KEY_IFACE) == 0)
		return KEY_IFACE;
	if (strcmp(key, CONFIG_KEY_OFACE) == 0)
		return KEY_OFACE;
	if (strcmp(key, CONFIG_KEY_FAMILY) == 0)
		return KEY_FAMILY;
	if (strcmp(key, CONFIG_KEY_ETHADDR) == 0)
		return KEY_ETHADDR;
	if (strcmp(key, CONFIG_KEY_IETHADDR) == 0)
		return KEY_IETHADDR;
	if (strcmp(key, CONFIG_KEY_OETHADDR) == 0)
		return KEY_OETHADDR;
	if (strcmp(key, CONFIG_KEY_VIRTADDR) == 0)
		return KEY_VIRTADDR;
	if (strcmp(key, CONFIG_KEY_VIRTPORTS) == 0)
		return KEY_VIRTPORTS;
	if (strcmp(key, CONFIG_KEY_IPADDR) == 0)
		return KEY_IPADDR;
	if (strcmp(key, CONFIG_KEY_SRCADDR) == 0)
		return KEY_SRCADDR;
	if (strcmp(key, CONFIG_KEY_PORT) == 0)
		return KEY_PORT;
	if (strcmp(key, CONFIG_KEY_MODE) == 0)
		return KEY_MODE;
	if (strcmp(key, CONFIG_KEY_RESPONSETTL) == 0)
		return KEY_RESPONSETTL;
	if (strcmp(key, CONFIG_KEY_PROTO) == 0)
		return KEY_PROTO;
	if (strcmp(key, CONFIG_KEY_SCHED) == 0)
		return KEY_SCHED;
	if (strcmp(key, CONFIG_KEY_SCHEDPARAM) == 0)
		return KEY_SCHEDPARAM;
	if (strcmp(key, CONFIG_KEY_PERSIST) == 0)
		return KEY_PERSISTENCE;
	if (strcmp(key, CONFIG_KEY_PERSISTTM) == 0)
		return KEY_PERSISTTM;
	if (strcmp(key, CONFIG_KEY_HELPER) == 0)
		return KEY_HELPER;
	if (strcmp(key, CONFIG_KEY_LOG) == 0)
		return KEY_LOG;
	if (strcmp(key, CONFIG_KEY_LOGPREFIX) == 0)
		return KEY_LOGPREFIX;
	if (strcmp(key, CONFIG_KEY_LOG_RTLIMIT) == 0)
		return KEY_LOG_RTLIMIT;
	if (strcmp(key, CONFIG_KEY_MARK) == 0)
		return KEY_MARK;
	if (strcmp(key, CONFIG_KEY_STATE) == 0)
		return KEY_STATE;
	if (strcmp(key, CONFIG_KEY_BCKS) == 0)
		return KEY_BCKS;
	if (strcmp(key, CONFIG_KEY_WEIGHT) == 0)
		return KEY_WEIGHT;
	if (strcmp(key, CONFIG_KEY_PRIORITY) == 0)
		return KEY_PRIORITY;
	if (strcmp(key, CONFIG_KEY_ACTION) == 0)
		return KEY_ACTION;
	if (strcmp(key, CONFIG_KEY_LIMITSTTL) == 0)
		return KEY_LIMITSTTL;
	if (strcmp(key, CONFIG_KEY_NEWRTLIMIT) == 0)
		return KEY_NEWRTLIMIT;
	if (strcmp(key, CONFIG_KEY_NEWRTLIMITBURST) == 0)
		return KEY_NEWRTLIMITBURST;
	if (strcmp(key, CONFIG_KEY_NEWRTLIMIT_LOGPREFIX) == 0)
		return KEY_NEWRTLIMIT_LOGPREFIX;
	if (strcmp(key, CONFIG_KEY_RSTRTLIMIT) == 0)
		return KEY_RSTRTLIMIT;
	if (strcmp(key, CONFIG_KEY_RSTRTLIMITBURST) == 0)
		return KEY_RSTRTLIMITBURST;
	if (strcmp(key, CONFIG_KEY_RSTRTLIMIT_LOGPREFIX) == 0)
		return KEY_RSTRTLIMIT_LOGPREFIX;
	if (strcmp(key, CONFIG_KEY_ESTCONNLIMIT) == 0)
		return KEY_ESTCONNLIMIT;
	if (strcmp(key, CONFIG_KEY_ESTCONNLIMIT_LOGPREFIX) == 0)
		return KEY_ESTCONNLIMIT_LOGPREFIX;
	if (strcmp(key, CONFIG_KEY_TCPSTRICT) == 0)
		return KEY_TCPSTRICT;
	if (strcmp(key, CONFIG_KEY_TCPSTRICT_LOGPREFIX) == 0)
		return KEY_TCPSTRICT_LOGPREFIX;
	if (strcmp(key, CONFIG_KEY_QUEUE) == 0)
		return KEY_QUEUE;
	if (strcmp(key, CONFIG_KEY_FLOWOFFLOAD) == 0)
		return KEY_FLOWOFFLOAD;
	if (strcmp(key, CONFIG_KEY_POLICIES) == 0)
		return KEY_POLICIES;
	if (strcmp(key, CONFIG_KEY_TYPE) == 0)
		return KEY_TYPE;
	if (strcmp(key, CONFIG_KEY_VERDICT) == 0)
		return KEY_VERDICT;
	if (strcmp(key, CONFIG_KEY_TIMEOUT) == 0)
		return KEY_TIMEOUT;
	if (strcmp(key, CONFIG_KEY_ELEMENTS) == 0)
		return KEY_ELEMENTS;
	if (strcmp(key, CONFIG_KEY_DATA) == 0)
		return KEY_DATA;
	if (strcmp(key, CONFIG_KEY_TIME) == 0)
		return KEY_TIME;
	if (strcmp(key, CONFIG_KEY_SESSIONS) == 0)
		return KEY_SESSIONS;
	if (strcmp(key, CONFIG_KEY_CLIENT) == 0)
		return KEY_CLIENT;
	if (strcmp(key, CONFIG_KEY_BACKEND) == 0)
		return KEY_BACKEND;
	if (strcmp(key, CONFIG_KEY_USED) == 0)
		return KEY_USED;
	if (strcmp(key, CONFIG_KEY_INTRACONNECT) == 0)
		return KEY_INTRACONNECT;
	if (strcmp(key, CONFIG_KEY_ADDRESSES) == 0)
		return KEY_ADDRESSES;
	if (strcmp(key, CONFIG_KEY_PORTS) == 0)
		return KEY_PORTS;
	if (strcmp(key, CONFIG_KEY_ROUTE) == 0)
		return KEY_ROUTE;
	if (strcmp(key, CONFIG_KEY_COUNTER_PACKETS) == 0)
		return KEY_COUNTER_PACKETS;
	if (strcmp(key, CONFIG_KEY_COUNTER_BYTES) == 0)
		return KEY_COUNTER_BYTES;

	config_set_output(". Unknown key '%s'", key);
	u_log_print(LOG_ERR, "%s():%d: unknown key '%s'", __FUNCTION__, __LINE__, key);
	return -1;
}

static int jump_config_value(int level, int key)
{
	if (
	    (key == KEY_FARMS && level != LEVEL_INIT) ||
	    (key == KEY_BCKS && level != LEVEL_FARMS) ||
	    (key == KEY_POLICIES && level != LEVEL_INIT && level != LEVEL_FARMS && level != LEVEL_ADDRESSES) ||
	    (key == KEY_ADDRESSES && level != LEVEL_FARMS && level != LEVEL_INIT) ||
	    (key == KEY_SESSIONS && level != LEVEL_FARMS) ||
	    (key == KEY_ELEMENTS && level != LEVEL_POLICIES))
			return -1;

	return 0;
}

static int config_json_object(json_t *element, int level, int source, int apply_action)
{
	const char *key;
	json_t *value;
	int ret = PARSER_OK;

	json_object_foreach(element, key, value) {
		c.level = level;
		ret = config_key(key);

		if (ret == -1)
			return ret;

		c.key = ret;
		if (jump_config_value(level, c.key) == 0) {
			ret = config_json(value, level, source, c.key, apply_action);
			if (ret) {
				u_log_print(LOG_ERR, "%s():%d: error parsing object in level %d", __FUNCTION__, __LINE__, c.level);
				return ret;
			}
		}
	}

	return ret;
}

static int config_json_array(json_t *element, int level, int source, int apply_action)
{
	size_t size = json_array_size(element);
	size_t i;
	int ret = PARSER_OK;

	for (i = 0; i < size && ret == PARSER_OK; i++) {
		ret = config_json(json_array_get(element, i), level, source, -1, apply_action);
	}

	return ret;
}

static int config_json_string(json_t *element, int level, int source, int apply_action)
{
	int ret;

	ret = config_value(json_string_value(element));

	if (ret == PARSER_IGNORE)
		return 0;

	if (ret != PARSER_OK)
		return ret;

	u_log_print(LOG_DEBUG, "%s():%d: %d(level) %d(key) %s(value) %d(value) %d(value2) apply_action %d", __FUNCTION__, __LINE__, c.level, c.key, c.str_value, c.int_value, c.int_value2, apply_action);

	ret = obj_set_attribute(&c, source, apply_action);
	init_pair(&c);

	return ret;
}

static int config_json(json_t *element, int level, int source, int key, int apply_action)
{
	int ret = PARSER_OK;

	u_log_print(LOG_DEBUG, "%s():%d: %d(level) %d(source)", __FUNCTION__, __LINE__, level, source);

	switch (json_typeof(element)) {
	case JSON_OBJECT:
		ret = config_json_object(element, level, source, apply_action);
		break;
	case JSON_ARRAY:
		if (level == LEVEL_INIT && key == KEY_FARMS)
			level = LEVEL_FARMS;
		if (level == LEVEL_INIT && key == KEY_POLICIES)
			level = LEVEL_POLICIES;
		if (level == LEVEL_INIT && key == KEY_ADDRESSES)
			level = LEVEL_ADDRESSES;
		if (level == LEVEL_FARMS && key == KEY_BCKS)
			level = LEVEL_BCKS;
		if (level == LEVEL_FARMS && key == KEY_ADDRESSES)
			level = LEVEL_FARMADDRESS;
		if (level == LEVEL_FARMS && key == KEY_SESSIONS)
			level = LEVEL_SESSIONS;
		if (level == LEVEL_FARMS && key == KEY_POLICIES)
			level = LEVEL_FARMPOLICY;
		if (level == LEVEL_POLICIES && key == KEY_ELEMENTS)
			level = LEVEL_ELEMENTS;
		if (level == LEVEL_ADDRESSES && key == KEY_POLICIES)
			level = LEVEL_ADDRESSPOLICY;

		ret = config_json_array(element, level, source, apply_action);

		if (level == LEVEL_FARMS || level == LEVEL_POLICIES || level == LEVEL_ADDRESSES)
			level = LEVEL_INIT;
		if (level == LEVEL_BCKS || level == LEVEL_FARMPOLICY || level == LEVEL_SESSIONS || level == LEVEL_FARMADDRESS)
			level = LEVEL_FARMS;
		if (level == LEVEL_ELEMENTS)
			level = LEVEL_POLICIES;
		if (level == LEVEL_ADDRESSPOLICY)
			level = LEVEL_ADDRESSES;

		break;
	case JSON_STRING:
		ret = config_json_string(element, level, source, apply_action);

		break;
	default:
		u_log_print(LOG_ERR, "Configuration file unknown element type %d", json_typeof(element));
	}

	return ret;
}

void config_pair_init(struct config_pair *c)
{
	if (!c)
		return;

	init_pair(c);
}

int config_file(const char *file)
{
	FILE		*fd;
	json_error_t	error;
	json_t		*root;
	int		ret = PARSER_OK;

	fd = fopen(file, "r");
	if (fd == NULL) {
		u_log_print(LOG_ERR, "Error open configuration file %s", file);
		return PARSER_FAILED;
	}

	root = json_loadf(fd, JSON_ALLOW_NUL, &error);

	if (root) {
		ret = config_json(root, LEVEL_INIT, CONFIG_SRC_FILE, -1, ACTION_START);
		json_decref(root);
	} else {
		u_log_print(LOG_ERR, "Configuration file error '%s' on line %d: %s", file, error.line, error.text);
		ret = PARSER_STRUCT_FAILED;
	}

	fclose(fd);
	return ret;
}

char *config_get_output(void)
{
	return config_outbuf;
}

void config_delete_output(void)
{
	config_outbuf[0] = '\0';
}

void config_set_output(char *fmt, ...)
{
	int len;
	va_list args;

	len = strlen(config_outbuf);

	va_start(args, fmt);
	len = vsnprintf(config_outbuf + len, CONFIG_OUTBUF_SIZE - len + 1, fmt, args);
	va_end(args);
}

int config_buffer(const char *buf, int apply_action)
{
	json_error_t	error;
	json_t		*root;
	int		ret = PARSER_OK;

	u_log_print(LOG_NOTICE, "%s():%d: payload %d : %s", __FUNCTION__, __LINE__, (int)strlen(buf), buf);

	root = json_loadb(buf, strlen(buf), JSON_ALLOW_NUL, &error);

	if (root) {
		ret = config_json(root, LEVEL_INIT, CONFIG_SRC_BUFFER, -1, apply_action);
		json_decref(root);
	} else {
		u_log_print(LOG_ERR, "Configuration error on line %d: %s", error.line, error.text);
		ret = PARSER_STRUCT_FAILED;
	}

	return ret;
}

static void add_dump_obj(json_t *obj, const char *name, char *value)
{
	if (value == NULL)
		return;

	json_object_set_new(obj, name, json_string(value));
}

static int add_dump_elements(json_t *obj, struct policy *p);

static struct json_t *add_dump_list(json_t *obj, const char *objname, int object,
			  struct list_head *head, char *name)
{
	struct farm *f;
	struct backend *b;
	struct farmpolicy *fp;
	struct policy *p;
	struct element *e;
	struct farmaddress *fa;
	struct address *a = NULL;
	struct addresspolicy *ap;
	struct session *s;
	json_t *jarray;
	json_t *item;
	char value[10];
	char buf[100] = {};

	if (continue_obj)
		jarray = obj;
	else
		jarray = json_array();

	switch (object) {
	case LEVEL_FARMS:
		list_for_each_entry(f, head, list) {

			if (name != NULL && (strcmp(name, "") != 0) && (strcmp(f->name, name) != 0))
				continue;

			fa = farmaddress_get_first(f);
			if (fa)
				a = fa->address;

			item = json_object();
			add_dump_obj(item, CONFIG_KEY_NAME, f->name);

			if (a) {
				add_dump_obj(item, CONFIG_KEY_FAMILY, obj_print_family(a->family));
				add_dump_obj(item, CONFIG_KEY_VIRTADDR, a->ipaddr);
				add_dump_obj(item, CONFIG_KEY_VIRTPORTS, a->ports);
			}

			if (f->srcaddr)
				add_dump_obj(item, CONFIG_KEY_SRCADDR, f->srcaddr);
			else
				add_dump_obj(item, CONFIG_KEY_SRCADDR, "");

			add_dump_obj(item, CONFIG_KEY_MODE, obj_print_mode(f->mode));
			if (f->mode == VALUE_MODE_STLSDNAT) {
				config_dump_int(value, f->responsettl);
				add_dump_obj(item, CONFIG_KEY_RESPONSETTL, value);
			}

			if (a)
				add_dump_obj(item, CONFIG_KEY_PROTO, obj_print_proto(a->protocol));
			add_dump_obj(item, CONFIG_KEY_SCHED, obj_print_sched(f->scheduler));

			obj_print_meta(f->schedparam, (char *)buf);
			add_dump_obj(item, CONFIG_KEY_SCHEDPARAM, buf);

			obj_print_meta(f->persistence, (char *)buf);
			add_dump_obj(item, CONFIG_KEY_PERSIST, buf);

			config_dump_int(value, f->persistttl);
			add_dump_obj(item, CONFIG_KEY_PERSISTTM, value);

			add_dump_obj(item, CONFIG_KEY_HELPER, obj_print_helper(f->helper));

			obj_print_log(f->log, (char *)buf);
			add_dump_obj(item, CONFIG_KEY_LOG, buf);
			if (f->logprefix && strcmp(f->logprefix, DEFAULT_LOG_LOGPREFIX) != 0)
				add_dump_obj(item, CONFIG_KEY_LOGPREFIX, f->logprefix);
			obj_print_rtlimit(buf, f->logrtlimit, f->logrtlimit_unit);
			add_dump_obj(item, CONFIG_KEY_LOG_RTLIMIT, buf);

			config_dump_hex(value, f->mark);
			add_dump_obj(item, CONFIG_KEY_MARK, value);
			config_dump_int(value, f->priority);
			add_dump_obj(item, CONFIG_KEY_PRIORITY, value);
			add_dump_obj(item, CONFIG_KEY_STATE, obj_print_state(f->state));

			config_dump_int(value, f->limitsttl);
			add_dump_obj(item, CONFIG_KEY_LIMITSTTL, value);
			obj_print_rtlimit(buf, f->newrtlimit, f->newrtlimit_unit);
			add_dump_obj(item, CONFIG_KEY_NEWRTLIMIT, buf);
			config_dump_int(value, f->newrtlimitbst);
			add_dump_obj(item, CONFIG_KEY_NEWRTLIMITBURST, value);
			if (f->newrtlimit_logprefix && strcmp(f->newrtlimit_logprefix, DEFAULT_LOGPREFIX) != 0)
				add_dump_obj(item, CONFIG_KEY_NEWRTLIMIT_LOGPREFIX, f->newrtlimit_logprefix);

			obj_print_rtlimit(buf, f->rstrtlimit, f->newrtlimit_unit);
			add_dump_obj(item, CONFIG_KEY_RSTRTLIMIT, buf);
			config_dump_int(value, f->rstrtlimitbst);
			add_dump_obj(item, CONFIG_KEY_RSTRTLIMITBURST, value);
			if (f->rstrtlimit_logprefix && strcmp(f->rstrtlimit_logprefix, DEFAULT_LOGPREFIX) != 0)
				add_dump_obj(item, CONFIG_KEY_RSTRTLIMIT_LOGPREFIX, f->rstrtlimit_logprefix);

			config_dump_int(value, f->estconnlimit);
			add_dump_obj(item, CONFIG_KEY_ESTCONNLIMIT, value);
			if (f->estconnlimit_logprefix && strcmp(f->estconnlimit_logprefix, DEFAULT_LOGPREFIX) != 0)
				add_dump_obj(item, CONFIG_KEY_ESTCONNLIMIT_LOGPREFIX, f->estconnlimit_logprefix);

			add_dump_obj(item, CONFIG_KEY_TCPSTRICT, obj_print_switch(f->tcpstrict));
			if (f->tcpstrict_logprefix && strcmp(f->tcpstrict_logprefix, DEFAULT_LOGPREFIX) != 0)
				add_dump_obj(item, CONFIG_KEY_TCPSTRICT_LOGPREFIX, f->tcpstrict_logprefix);

			config_dump_int(value, f->queue);
			add_dump_obj(item, CONFIG_KEY_QUEUE, value);

			obj_print_verdict(f->verdict, (char *)buf);
			add_dump_obj(item, CONFIG_KEY_VERDICT, buf);

			if (f->flow_offload)
				add_dump_obj(item, CONFIG_KEY_FLOWOFFLOAD, obj_print_switch(f->flow_offload));

			if (f->intra_connect)
				add_dump_obj(item, CONFIG_KEY_INTRACONNECT, obj_print_switch(f->intra_connect));

			add_dump_list(item, CONFIG_KEY_ADDRESSES, LEVEL_FARMADDRESS, &f->addresses, NULL);
			add_dump_list(item, CONFIG_KEY_BCKS, LEVEL_BCKS, &f->backends, NULL);

			add_dump_list(item, CONFIG_KEY_POLICIES, LEVEL_FARMPOLICY, &f->policies, NULL);

			if (f->total_static_sessions != 0)
				add_dump_list(item, CONFIG_KEY_SESSIONS, LEVEL_SESSIONS, &f->static_sessions, NULL);

			json_array_append_new(jarray, item);
			a = NULL;
		}
		break;
	case LEVEL_BCKS:
		list_for_each_entry(b, head, list) {
			item = json_object();
			add_dump_obj(item, CONFIG_KEY_NAME, b->name);
			add_dump_obj(item, CONFIG_KEY_IPADDR, b->ipaddr);
			add_dump_obj(item, CONFIG_KEY_PORT, b->port);
			add_dump_obj(item, CONFIG_KEY_SRCADDR, b->srcaddr);
			config_dump_int(value, b->weight);
			add_dump_obj(item, CONFIG_KEY_WEIGHT, value);
			config_dump_int(value, b->priority);
			add_dump_obj(item, CONFIG_KEY_PRIORITY, value);
			config_dump_hex(value, b->mark);
			add_dump_obj(item, CONFIG_KEY_MARK, value);

			config_dump_int(value, b->estconnlimit);
			add_dump_obj(item, CONFIG_KEY_ESTCONNLIMIT, value);
			if (b->estconnlimit_logprefix && strcmp(b->estconnlimit_logprefix, DEFAULT_B_ESTCONNLIMIT_LOGPREFIX) != 0)
				add_dump_obj(item, CONFIG_KEY_ESTCONNLIMIT_LOGPREFIX, b->estconnlimit_logprefix);

			add_dump_obj(item, CONFIG_KEY_STATE, obj_print_state(b->state));
			json_array_append_new(jarray, item);
		}
		break;
	case LEVEL_FARMPOLICY:
		list_for_each_entry(fp, head, list) {
			item = json_object();
			add_dump_obj(item, CONFIG_KEY_NAME, fp->policy->name);
			json_array_append_new(jarray, item);
		}
		break;
	case LEVEL_POLICIES:
		list_for_each_entry(p, head, list) {
			if (name != NULL && (strcmp(name, "") != 0) && (strcmp(p->name, name) != 0))
				continue;

			item = json_object();
			add_dump_obj(item, CONFIG_KEY_NAME, p->name);
			add_dump_obj(item, CONFIG_KEY_FAMILY, obj_print_family(p->family));
			add_dump_obj(item, CONFIG_KEY_TYPE, obj_print_policy_type(p->type));

			add_dump_obj(item, CONFIG_KEY_ROUTE, obj_print_policy_route(p->route));
			config_dump_int(value, p->timeout);
			add_dump_obj(item, CONFIG_KEY_TIMEOUT, value);
			if (p->logprefix && strcmp(p->logprefix, DEFAULT_POLICY_LOGPREFIX) != 0)
				add_dump_obj(item, CONFIG_KEY_LOGPREFIX, p->logprefix);

			config_dump_int(value, p->used);
			add_dump_obj(item, CONFIG_KEY_USED, value);
			add_dump_elements(item, p);
			json_array_append_new(jarray, item);
		}
		break;
	case LEVEL_FARMADDRESS:
		list_for_each_entry(fa, head, list) {
			item = json_object();
			add_dump_obj(item, CONFIG_KEY_NAME, fa->address->name);
			add_dump_obj(item, CONFIG_KEY_FAMILY, obj_print_family(fa->address->family));
			add_dump_obj(item, CONFIG_KEY_IPADDR, fa->address->ipaddr);
			add_dump_obj(item, CONFIG_KEY_PORTS, fa->address->ports);
			add_dump_obj(item, CONFIG_KEY_PROTO, obj_print_proto(fa->address->protocol));

			config_dump_int(value, fa->address->used);
			add_dump_obj(item, CONFIG_KEY_USED, value);

			json_array_append_new(jarray, item);
		}
		break;
	case LEVEL_ADDRESSES:
		list_for_each_entry(a, head, list) {
			if (name != NULL && (strcmp(name, "") != 0) && (strcmp(a->name, name) != 0))
				continue;

			item = json_object();
			add_dump_obj(item, CONFIG_KEY_NAME, a->name);
			add_dump_obj(item, CONFIG_KEY_FAMILY, obj_print_family(a->family));
			add_dump_obj(item, CONFIG_KEY_IPADDR, a->ipaddr);
			add_dump_obj(item, CONFIG_KEY_PORTS, a->ports);
			add_dump_obj(item, CONFIG_KEY_PROTO, obj_print_proto(a->protocol));

			obj_print_verdict(a->verdict, (char *)buf);
			add_dump_obj(item, CONFIG_KEY_VERDICT, buf);

			if (a->logprefix && strcmp(a->logprefix, DEFAULT_LOG_LOGPREFIX_ADDRESS) != 0)
				add_dump_obj(item, CONFIG_KEY_LOGPREFIX, a->logprefix);
			obj_print_rtlimit(buf, a->logrtlimit, a->logrtlimit_unit);
			add_dump_obj(item, CONFIG_KEY_LOG_RTLIMIT, buf);

			config_dump_int(value, a->used);
			add_dump_obj(item, CONFIG_KEY_USED, value);

			add_dump_list(item, CONFIG_KEY_POLICIES, LEVEL_ADDRESSPOLICY, &a->policies, NULL);

			json_array_append_new(jarray, item);
		}
		break;
	case LEVEL_ADDRESSPOLICY:
		list_for_each_entry(ap, head, list) {
			item = json_object();
			add_dump_obj(item, CONFIG_KEY_NAME, ap->policy->name);
			json_array_append_new(jarray, item);
		}
		break;
	case LEVEL_ELEMENTS:
		list_for_each_entry(e, head, list) {
			item = json_object();
			add_dump_obj(item, CONFIG_KEY_DATA, e->data);
			if (e->time)
				add_dump_obj(item, CONFIG_KEY_TIME, e->time);

			add_dump_obj(item, CONFIG_KEY_COUNTER_PACKETS, e->counter_pkts);
			add_dump_obj(item, CONFIG_KEY_COUNTER_BYTES, e->counter_bytes);
			json_array_append_new(jarray, item);
		}
		break;
	case LEVEL_SESSIONS:
		list_for_each_entry(s, head, list) {
			item = json_object();
			add_dump_obj(item, CONFIG_KEY_CLIENT, s->client);

			if (!s->bck)
				add_dump_obj(item, CONFIG_KEY_BACKEND, UNDEFINED_VALUE);
			else
				add_dump_obj(item, CONFIG_KEY_BACKEND, s->bck->name);

			if (s->expiration)
				add_dump_obj(item, CONFIG_KEY_EXPIRATION, s->expiration);

			json_array_append_new(jarray, item);
		}
		break;
	default:
		return NULL;
	}

	if (!continue_obj) {
		json_object_set_new(obj, objname, jarray);
		return jarray;
	}

	return NULL;
}

static int add_dump_elements(json_t *jdata, struct policy *p)
{
	element_get_list(p);
	add_dump_list(jdata, CONFIG_KEY_ELEMENTS, LEVEL_ELEMENTS, &p->elements, NULL);
	element_s_delete(p);

	return 0;
}

int config_print_farms(char **buf, char *name)
{
	struct list_head *farms = obj_get_farms();
	json_t *jdata;
	struct farm *f;

	if (name && strcmp(name, "") != 0) {
		f = farm_lookup_by_name(name);
		if (!f)
			return -1;
	}

	jdata = json_object();
	add_dump_list(jdata, CONFIG_KEY_FARMS, LEVEL_FARMS, farms, name);

	free(*buf);
	*buf = json_dumps(jdata, JSON_INDENT(8));
	json_decref(jdata);

	if (*buf == NULL)
		return -1;

	return 0;
}

int config_print_farm_sessions(char **buf, char *name)
{
	json_t *jdata = json_object();
	json_t *jdata_cont;
	struct farm *f;

	if (!name || strcmp(name, "") == 0)
		return PARSER_STRUCT_FAILED;

	f = farm_lookup_by_name(name);
	if (!f)
		return PARSER_OBJ_UNKNOWN;

	jdata_cont = add_dump_list(jdata, CONFIG_KEY_SESSIONS, LEVEL_SESSIONS, &f->static_sessions, name);
	continue_obj = 1;
	session_get_timed(f);
	add_dump_list(jdata_cont, CONFIG_KEY_SESSIONS, LEVEL_SESSIONS, &f->timed_sessions, name);
	continue_obj = 0;
	free(*buf);
	*buf = json_dumps(jdata, JSON_INDENT(8));

	json_decref(jdata);
	session_s_delete(f, SESSION_TYPE_TIMED);

	if (*buf == NULL)
		return PARSER_FAILED;

	return PARSER_OK;
}

int config_print_policies(char **buf, char *name)
{
	struct list_head *policies = obj_get_policies();
	json_t* jdata = json_object();

	add_dump_list(jdata, CONFIG_KEY_POLICIES, LEVEL_POLICIES, policies, name);

	free(*buf);
	*buf = json_dumps(jdata, JSON_INDENT(8));
	json_decref(jdata);

	if (*buf == NULL)
		return PARSER_FAILED;

	return PARSER_OK;
}

int config_set_farm_action(const char *name, const char *value)
{
	struct farm *f;

	if (!name || strcmp(name, "") == 0)
		return (farm_s_set_action(config_value_action(value)) >= 0) ? PARSER_OK : PARSER_FAILED;

	f = farm_lookup_by_name(name);
	if (!f) {
		config_set_output(". Unknown farm '%s'", name);
		return PARSER_OBJ_UNKNOWN;
	}

	return (farm_set_action(f, config_value_action(value)) >= 0) ? PARSER_OK : PARSER_FAILED;
}

int config_set_session_backend_action(const char *fname, const char *bname, const char *value)
{
	struct farm *f;
	struct backend *b;
	int ret = 0;

	if (!fname || strcmp(fname, "") == 0)
		return PARSER_STRUCT_FAILED;

	if (!bname || strcmp(bname, "") == 0)
		return PARSER_STRUCT_FAILED;

	f = farm_lookup_by_name(fname);
	if (!f) {
		config_set_output(". Unknown farm '%s'", fname);
		return PARSER_OBJ_UNKNOWN;
	}

	if (f->persistence == VALUE_META_NONE) {
		config_set_output(". Farm '%s' without session persistence", fname);
		return PARSER_VALID_FAILED;
	}

	b = backend_lookup_by_key(f, KEY_NAME, bname, 0);
	if (!b) {
		config_set_output(". Unknown backend '%s' in farm '%s'", bname, fname);
		return PARSER_OBJ_UNKNOWN;
	}

	session_get_timed(f);

	if (config_value_action(value) == ACTION_DELETE) {
		ret = session_backend_action(f, b, ACTION_STOP);
		farm_set_action(f, ACTION_RELOAD);
		obj_rulerize(OBJ_START);
	}
	ret = session_backend_action(f, b, config_value_action(value));

	session_s_delete(f, SESSION_TYPE_TIMED);
	return ret;
}

int config_set_backend_action(const char *fname, const char *bname, const char *value)
{
	struct farm *f;
	struct backend *b;
	int ret = 0;

	if (!fname || strcmp(fname, "") == 0)
		return PARSER_STRUCT_FAILED;

	f = farm_lookup_by_name(fname);
	if (!f) {
		config_set_output(". Unknown farm '%s'", fname);
		return PARSER_OBJ_UNKNOWN;
	}

	session_get_timed(f);

	if (!bname || strcmp(bname, "") == 0) {
		ret = backend_s_set_action(f, config_value_action(value));
		goto out;
	}

	b = backend_lookup_by_key(f, KEY_NAME, bname, 0);
	if (!b) {
		config_set_output(". Unknown backend '%s' in farm '%s'", bname, fname);
		session_s_delete(f, SESSION_TYPE_TIMED);
		return PARSER_OBJ_UNKNOWN;
	}

	ret = backend_set_action(b, config_value_action(value));

out:
	session_s_delete(f, SESSION_TYPE_TIMED);
	return (ret >= 0) ? PARSER_OK : PARSER_FAILED;
}

int config_set_session_action(const char *fname, const char *sname, const char *value)
{
	struct farm *f;
	struct session *s = NULL;
	char name[255] = { 0 };
	char *c;
	int ret = -1;
	int timed = 0;
	int action = config_value_action(value);

	if (!fname || strcmp(fname, "") == 0) {
		config_set_output(". Please select a valid farm");
		return PARSER_STRUCT_FAILED;
	}

	f = farm_lookup_by_name(fname);
	if (!f) {
		config_set_output(". Unknown farm '%s'", fname);
		return PARSER_OBJ_UNKNOWN;
	}

	if (!sname || strcmp(sname, "") == 0) {
		ret = session_s_set_action(f, NULL, action);
		goto apply;
	}

	/* Traduce URL to plain text */
	sprintf(name, "%s", sname);
	for (c = name; (c = strchr(c, '_')); ++c)
		*c = ' ';

	s = session_lookup_by_key(f, SESSION_TYPE_STATIC, KEY_CLIENT, name);
	if (s) {
		ret = session_set_action(s, SESSION_TYPE_STATIC, action);
		goto apply;
	}

	if (action == ACTION_STOP) {
		timed = 1;
		session_get_timed(f);
		s = session_lookup_by_key(f, SESSION_TYPE_TIMED, KEY_CLIENT, name);
		if (s) {
			ret = session_set_action(s, SESSION_TYPE_TIMED, action);
			goto apply;
		}
		return PARSER_OK;
	}

	config_set_output(". Unknown session '%s' in farm '%s'", sname, fname);
	return PARSER_OBJ_UNKNOWN;

apply:
	if (ret > 0) {
		config_set_farm_action(fname, CONFIG_VALUE_ACTION_RELOAD);
		obj_rulerize(OBJ_START);
	}

	if (timed) {
		session_s_delete(f, SESSION_TYPE_TIMED);
		return PARSER_OK;
	}

	if (action != ACTION_STOP)
		return PARSER_OK;

	if (!s)
		return (session_s_set_action(f, NULL, ACTION_DELETE) >= 0) ? PARSER_OK : PARSER_FAILED;

	return (session_set_action(s, SESSION_TYPE_STATIC, ACTION_DELETE) >= 0) ? PARSER_OK : PARSER_FAILED;
}

int config_set_fpolicy_action(const char *fname, const char *fpname, const char *value)
{
	struct farm *f;
	struct farmpolicy *fp;

	if (!fname || strcmp(fname, "") == 0) {
		config_set_output(". Please select a valid farm");
		return PARSER_STRUCT_FAILED;
	}

	f = farm_lookup_by_name(fname);
	if (!f) {
		config_set_output(". Unknown farm '%s'", fname);
		return PARSER_OBJ_UNKNOWN;
	}

	if (!fpname || strcmp(fpname, "") == 0)
		return farmpolicy_s_set_action(f, config_value_action(value));

	fp = farmpolicy_lookup_by_name(f, fpname);
	if (!fp) {
		config_set_output(". Unknown farm policy '%s' in farm '%s'", fpname, fname);
		return PARSER_OBJ_UNKNOWN;
	}

	farmpolicy_set_action(fp, config_value_action(value));

	return PARSER_OK;
}

int config_check_farm(const char *name)
{
	struct farm *f;

	if (!name || strcmp(name, "") == 0)
		return PARSER_OBJ_UNKNOWN;

	f = farm_lookup_by_name(name);
	if (!f) {
		config_set_output(". Unknown farm '%s'", name);
		return PARSER_OBJ_UNKNOWN;
	}

	return PARSER_OK;
}

int config_check_policy(const char *name)
{
	struct policy *p;

	if (!name || strcmp(name, "") == 0)
		return PARSER_OBJ_UNKNOWN;

	p = policy_lookup_by_name(name);
	if (!p) {
		config_set_output(". Unknown policy '%s'", name);
		return PARSER_OBJ_UNKNOWN;
	}

	return PARSER_OK;
}

int config_set_policy_action(const char *name, const char *value)
{
	struct policy *p;

	if (!name || strcmp(name, "") == 0)
		return (policy_s_set_action(config_value_action(value)) >= 0) ? PARSER_OK : PARSER_FAILED;

	p = policy_lookup_by_name(name);
	if (!p) {
		config_set_output(". Unknown policy '%s'", name);
		return PARSER_OBJ_UNKNOWN;
	}

	return (policy_set_action(p, config_value_action(value)) >= 0) ? PARSER_OK : PARSER_FAILED;
}

int config_set_element_action(const char *pname, const char *edata, const char *value)
{
	struct policy *p;
	struct element *e;

	if (!pname || strcmp(pname, "") == 0) {
		config_set_output(". Please select a valid policy");
		return PARSER_STRUCT_FAILED;
	}

	p = policy_lookup_by_name(pname);
	if (!p) {
		config_set_output(". Unknown policy '%s'", pname);
		return PARSER_OBJ_UNKNOWN;
	}

	if (!edata || strcmp(edata, "") == 0)
		return (element_s_set_action(p, config_value_action(value)) >= 0) ? PARSER_OK : PARSER_FAILED;

	e = element_lookup_by_name(p, edata);
	if (!e) {
		config_set_output(". Unknown element '%s' in policy '%s'", edata, pname);
		return PARSER_OBJ_UNKNOWN;
	}

	return (element_set_action(e, config_value_action(value)) >= 0) ? PARSER_OK : PARSER_FAILED;
}

int config_get_elements(const char *pname)
{
	struct policy *p;

	p = policy_lookup_by_name(pname);
	if (!p) {
		config_set_output(". Unknown policy '%s'", pname);
		return PARSER_OBJ_UNKNOWN;
	}

	return element_get_list(p);
}

int config_delete_elements(const char *pname)
{
	struct policy *p;

	p = policy_lookup_by_name(pname);
	if (!p) {
		config_set_output(". Unknown policy '%s'", pname);
		return PARSER_OBJ_UNKNOWN;
	}

	return element_s_delete(p);
}

void config_print_response(char **buf, char *fmt, ...)
{
	int len = 0;
	va_list args;

	len = sprintf(*buf, "{\"response\": \"");

	va_start(args, fmt);
	len += vsprintf(*buf + len, fmt, args);
	va_end(args);

	sprintf(*buf + len, "\"}");
}

int config_set_address_action(const char *name, const char *value)
{
	struct address *a;

	if (!name || strcmp(name, "") == 0)
		return (address_s_set_action(config_value_action(value)) >= 0) ? PARSER_OK : PARSER_FAILED;

	a = address_lookup_by_name(name);
	if (!a) {
		config_set_output(". Unknown address '%s'", name);
		return PARSER_OBJ_UNKNOWN;
	}

	return (address_set_action(a, config_value_action(value)) >= 0) ? PARSER_OK : PARSER_FAILED;
}

int config_set_farmaddress_action(const char *fname, const char *faname, const char *value)
{
	struct farm *f;
	struct farmaddress *fa;

	if (!fname || strcmp(fname, "") == 0) {
		config_set_output(". Please select a valid farm");
		return PARSER_STRUCT_FAILED;
	}

	f = farm_lookup_by_name(fname);
	if (!f) {
		config_set_output(". Unknown farm '%s'", fname);
		return PARSER_OBJ_UNKNOWN;
	}

	if (!faname || strcmp(faname, "") == 0)
		return farmaddress_s_set_action(f, config_value_action(value));

	fa = farmaddress_lookup_by_name(f, faname);
	if (!fa) {
		config_set_output(". Unknown farm address '%s' in farm '%s'", faname, fname);
		return PARSER_OBJ_UNKNOWN;
	}

	farmaddress_set_action(fa, config_value_action(value));

	return 0;
}

int config_print_addresses(char **buf, char *name)
{
	struct list_head *addresses = obj_get_addresses();
	json_t* jdata = json_object();

	add_dump_list(jdata, CONFIG_KEY_ADDRESSES, LEVEL_ADDRESSES, addresses, name);

	*buf = json_dumps(jdata, JSON_INDENT(8));
	json_decref(jdata);

	if (*buf == NULL)
		return -1;

	return 0;
}
