/*
 *   This file is part of nftlb, nftables load balancer.
 *
 *   Copyright (C) ZEVENET SL.
 *   Author: Laura Garcia <laura.garcia@zevenet.com>
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

#include "nft.h"
#include "objects.h"
#include "farms.h"
#include "backends.h"
#include "sessions.h"
#include "farmpolicy.h"
#include "policies.h"
#include "elements.h"
#include "config.h"
#include "list.h"
#include "sbuffer.h"
#include "tools.h"

#include <stdlib.h>
#include <nftables/libnftables.h>
#include <string.h>
#include <stdarg.h>

#define NFTLB_MAX_CMD				2048
#define NFTLB_MAX_IFACES			100

#define NFTLB_TABLE_NAME			"nftlb"
#define NFTLB_TABLE_PREROUTING		"prerouting"
#define NFTLB_TABLE_POSTROUTING		"postrouting"
#define NFTLB_TABLE_INGRESS			"ingress"
#define NFTLB_TABLE_INGRESS_DNAT	"ingress-dnat"
#define NFTLB_TABLE_FILTER			"filter"
#define NFTLB_TABLE_FORWARD			"forward"
#define NFTLB_TABLE_OUT_FILTER		"output-filter"
#define NFTLB_TABLE_OUT_NAT			"output-nat"

#define NFTLB_TYPE_NONE				""
#define NFTLB_TYPE_NAT				"nat"
#define NFTLB_TYPE_FILTER			"filter"
#define NFTLB_TYPE_NETDEV			"netdev"
#define NFTLB_TYPE_FWD				"forward"

#define NFTLB_HOOK_PREROUTING		"prerouting"
#define NFTLB_HOOK_POSTROUTING		"postrouting"
#define NFTLB_HOOK_INGRESS			"ingress"
#define NFTLB_HOOK_FORWARD			"forward"
#define NFTLB_HOOK_OUTPUT			"output"

#define NFTLB_PREROUTING_PRIO		-100
#define NFTLB_POSTROUTING_PRIO		100
#define NFTLB_FLOWTABLE_BASE_PRIO	50
#define NFTLB_INGRESS_PRIO			101
#define NFTLB_INGRESS_DNAT_PRIO		100
#define NFTLB_FILTER_PRIO			-150
#define NFTLB_RAW_PRIO				-300

#define NFTLB_IPV4_PROTOCOL			"protocol"
#define NFTLB_IPV6_PROTOCOL			"nexthdr"

#define NFTLB_UDP_PROTO				"udp"
#define NFTLB_TCP_PROTO				"tcp"
#define NFTLB_SCTP_PROTO			"sctp"

#define NFTLB_UDP_SERVICES_MAP		"udp-services"
#define NFTLB_TCP_SERVICES_MAP		"tcp-services"
#define NFTLB_SCTP_SERVICES_MAP		"sctp-services"
#define NFTLB_IP_SERVICES_MAP		"services"
#define NFTLB_PROTO_SERVICES_MAP	"proto-services"
#define NFTLB_PORT_SERVICES_MAP		"port-services"

#define NFTLB_UDP_SERVICES6_MAP		"udp-services6"
#define NFTLB_TCP_SERVICES6_MAP		"tcp-services6"
#define NFTLB_SCTP_SERVICES6_MAP	"sctp-services6"
#define NFTLB_IP_SERVICES6_MAP		"services6"
#define NFTLB_PROTO_SERVICES6_MAP	"proto-services6"
#define NFTLB_PORT_SERVICES6_MAP	"port-services6"

#define NFTLB_MAP_KEY_TYPE			0
#define NFTLB_MAP_KEY_RULE			1

#define NFTLB_MAP_TYPE_IPV4			"ipv4_addr"
#define NFTLB_MAP_TYPE_IPV6			"ipv6_addr"
#define NFTLB_MAP_TYPE_INETSRV		"inet_service"
#define NFTLB_MAP_TYPE_MAC			"ether_addr"
#define NFTLB_MAP_TYPE_MARK			"mark"
#define NFTLB_MAP_TYPE_PROTO		"inet_proto"

#define NFTLB_IPV4_FAMILY			0
#define NFTLB_IPV6_FAMILY			1
#define NFTLB_NETDEV_FAMILY			2

#define NFTLB_IPV4_FAMILY_STR		"ip"
#define NFTLB_IPV6_FAMILY_STR		"ip6"
#define NFTLB_NETDEV_FAMILY_STR		"netdev"

#define NFTLB_IP_ACTIVE				(1 << 0)
#define NFTLB_PROTO_IP_PORT_ACTIVE	(1 << 1)
#define NFTLB_PROTO_IP_ACTIVE		(1 << 2)
#define NFTLB_PROTO_PORT_ACTIVE		(1 << 3)
#define NFTLB_MARK_ACTIVE			(1 << 4)
#define NFTLB_TABLE_IP_ACTIVE		(1 << 5)
#define NFTLB_TABLE_IP6_ACTIVE		(1 << 6)
#define NFTLB_TABLE_NETDEV_ACTIVE	(1 << 7)

#define NFTLB_NFT_DADDR				"daddr"
#define NFTLB_NFT_DPORT				"dport"
#define NFTLB_NFT_SADDR				"saddr"
#define NFTLB_NFT_SPORT				"sport"

#define NFTLB_NFT_VERDICT_DROP		"drop"
#define NFTLB_NFT_VERDICT_ACCEPT	"accept"
#define NFTLB_NFT_PREFIX_POLICY_BL	"BL"
#define NFTLB_NFT_PREFIX_POLICY_WL	"WL"

#define NFTLB_NFT_ACTION_ADD		"add"
#define NFTLB_NFT_ACTION_DEL		"delete"
#define NFTLB_NFT_ACTION_FLUSH		"flush"

#define NFTLB_F_CHAIN_ING_FILTER	(1 << 0)
#define NFTLB_F_CHAIN_ING_DNAT		(1 << 1)
#define NFTLB_F_CHAIN_PRE_FILTER	(1 << 2)
#define NFTLB_F_CHAIN_PRE_DNAT		(1 << 3)
#define NFTLB_F_CHAIN_FWD_FILTER	(1 << 4)
#define NFTLB_F_CHAIN_POS_SNAT		(1 << 5)
#define NFTLB_F_CHAIN_OUT_FILTER	(1 << 6)
#define NFTLB_F_CHAIN_OUT_DNAT		(1 << 7)

#define NFTLB_CHECK_AVAIL			0
#define NFTLB_CHECK_USABLE			1

extern unsigned int serialize;
extern int masquerade_mark;
struct nft_ctx *ctx = NULL;

int nftlb_flowtable_prio = NFTLB_FLOWTABLE_BASE_PRIO;

enum chain_counter_position {
	NFTLB_F_CHAIN_ING_FILTER_POS,
	NFTLB_F_CHAIN_ING_DNAT_POS,
	NFTLB_F_CHAIN_PRE_FILTER_POS,
	NFTLB_F_CHAIN_PRE_DNAT_POS,
	NFTLB_F_CHAIN_FWD_FILTER_POS,
	NFTLB_F_CHAIN_POS_SNAT_POS,
	NFTLB_F_CHAIN_OUT_FILTER_POS,
	NFTLB_F_CHAIN_OUT_DNAT_POS,
	NFTLB_F_CHAIN_MAX,
};

enum map_modes {
	BCK_MAP_NONE,
	BCK_MAP_IPADDR,
	BCK_MAP_ETHADDR,
	BCK_MAP_WEIGHT,
	BCK_MAP_MARK,
	BCK_MAP_IPADDR_PORT,
	BCK_MAP_NAME,
	BCK_MAP_SRCIPADDR,
	BCK_MAP_BCK_IPADDR,
	BCK_MAP_BCK_MARK,
	BCK_MAP_BCK_IPADDR_F_PORT,
	BCK_MAP_BCK_BF_SRCIPADDR,
	BCK_MAP_BCK_ID,
	BCK_MAP_OFACE,
	BCK_MAP_PROTO_IPADDR_PORT,
	BCK_MAP_BCK_PROTO_IPADDR_F_PORT,
	BCK_MAP_PROTO_IPADDR,
	BCK_MAP_PROTO_PORT,
	BCK_MAP_PORT,
};

struct if_base_rule {
	char				*ifname;
	unsigned int		rules_v4;
	unsigned int		rules_v6;
};

struct if_base_rule_list {
	struct if_base_rule	*interfaces[NFTLB_MAX_IFACES];
	int					n_interfaces;
};

struct nft_chain_srv_family_counters {
	unsigned int proto_ip_port_cnt;
	unsigned int proto_port_cnt;
	unsigned int proto_ip_cnt;
	unsigned int bckmark_cnt;
};

struct nft_chain_srv_counters {
	struct nft_chain_srv_family_counters ipv4_counters;
	struct nft_chain_srv_family_counters ipv6_counters;
};

struct nft_chain_srv_counters service_counters[NFTLB_F_CHAIN_MAX];

static int get_chain_pos_counter(int type)
{
	if (type & NFTLB_F_CHAIN_ING_FILTER)
		return NFTLB_F_CHAIN_ING_FILTER_POS;
	else if (type & NFTLB_F_CHAIN_PRE_FILTER)
		return NFTLB_F_CHAIN_PRE_FILTER_POS;
	else if (type & NFTLB_F_CHAIN_PRE_DNAT)
		return NFTLB_F_CHAIN_PRE_DNAT_POS;
	else if (type & NFTLB_F_CHAIN_FWD_FILTER)
		return NFTLB_F_CHAIN_FWD_FILTER_POS;
	else if (type & NFTLB_F_CHAIN_POS_SNAT)
		return NFTLB_F_CHAIN_POS_SNAT_POS;
	else if (type & NFTLB_F_CHAIN_ING_DNAT)
		return NFTLB_F_CHAIN_ING_DNAT_POS;
	else if (type & NFTLB_F_CHAIN_OUT_FILTER)
		return NFTLB_F_CHAIN_OUT_FILTER_POS;
	else if (type & NFTLB_F_CHAIN_OUT_DNAT)
		return NFTLB_F_CHAIN_OUT_DNAT_POS;
	else
		return -1;
}

static void get_range_ports(const char *ptr, int *first, int *last)
{
	sscanf(ptr, "%d-%d[^,]", first, last);
}

static int get_array_ports(int *port_list, struct farm *f)
{
	int index = 0;
	char *ptr;
	int i, new;
	int last = 0;

	ptr = f->virtports;
	while (ptr != NULL && *ptr != '\0') {
		last = new = 0;
		get_range_ports(ptr, &new, &last);
		if (last == 0)
			last = new;
		if (new > last)
			goto next;
		for (i = new; i <= last; i++, index++)
			if (port_list)
				port_list[index] = i;
next:
		ptr = strchr(ptr, ',');
		if (ptr != NULL)
			ptr++;
	}

	return index;
}

static unsigned int *get_service_counter(int type, unsigned int structure, int family)
{
	struct nft_chain_srv_family_counters *service_cnt;
	unsigned int *counter;

	if (family == VALUE_FAMILY_IPV6)
		service_cnt = &service_counters[get_chain_pos_counter(type)].ipv6_counters;
	else
		service_cnt = &service_counters[get_chain_pos_counter(type)].ipv4_counters;

	if (structure & NFTLB_PROTO_PORT_ACTIVE)
		counter = &(service_cnt->proto_port_cnt);
	else if (structure & NFTLB_PROTO_IP_ACTIVE)
		counter = &(service_cnt->proto_ip_cnt);
	else
		counter = &(service_cnt->proto_ip_port_cnt);

	return counter;
}

static unsigned int get_rules_needed(struct farm *f)
{
	unsigned int ret = 0;

	if (farm_no_port(f))
		ret |= NFTLB_IP_ACTIVE | NFTLB_PROTO_IP_ACTIVE;
	else if (farm_no_virtaddr(f))
		ret |= NFTLB_IP_ACTIVE | NFTLB_PROTO_PORT_ACTIVE;
	else
		ret |= NFTLB_IP_ACTIVE | NFTLB_PROTO_IP_PORT_ACTIVE;

	return ret;
}

static void update_service_counters(struct farm *f, int type, int family, int qty, int action)
{
	unsigned int *counter = get_service_counter(type, get_rules_needed(f), family);

	if (action == ACTION_START || action == ACTION_RELOAD)
		*counter += qty;
	else if ((action == ACTION_STOP || action == ACTION_DELETE) && ((int)*counter >= qty))
		*counter -= qty;
}

static void print_service_counters(void)
{
	int i;
	for (i = 0; i < NFTLB_F_CHAIN_MAX; i++) {
		tools_printlog(LOG_DEBUG, "%s(): [%u]", __FUNCTION__, i);
		tools_printlog(LOG_DEBUG, "%s():    ipv4_counters.proto_ip_port_cnt = %d", __FUNCTION__, service_counters[i].ipv4_counters.proto_ip_port_cnt);
		tools_printlog(LOG_DEBUG, "%s():    ipv4_counters.proto_port_cnt = %d", __FUNCTION__, service_counters[i].ipv4_counters.proto_port_cnt);
		tools_printlog(LOG_DEBUG, "%s():    ipv4_counters.proto_ip_cnt = %d", __FUNCTION__, service_counters[i].ipv4_counters.proto_ip_cnt);
		tools_printlog(LOG_DEBUG, "%s():    ipv4_counters.bckmark_cnt = %d", __FUNCTION__, service_counters[i].ipv4_counters.bckmark_cnt);
		tools_printlog(LOG_DEBUG, "%s():    ipv6_counters.proto_ip_port_cnt = %d", __FUNCTION__, service_counters[i].ipv6_counters.proto_ip_port_cnt);
		tools_printlog(LOG_DEBUG, "%s():    ipv6_counters.proto_port_cnt = %d", __FUNCTION__, service_counters[i].ipv6_counters.proto_port_cnt);
		tools_printlog(LOG_DEBUG, "%s():    ipv6_counters.proto_ip_cnt = %d", __FUNCTION__, service_counters[i].ipv6_counters.proto_ip_cnt);
		tools_printlog(LOG_DEBUG, "%s():    ipv6_counters.bckmark_cnt = %d", __FUNCTION__, service_counters[i].ipv6_counters.bckmark_cnt);
	}
}

struct nft_base_rules {
	unsigned int tables;
	unsigned int dnat_rules_v4;
	unsigned int dnat_rules_v6;
	unsigned int snat_rules_v4;
	unsigned int snat_rules_v6;
	unsigned int filter_rules_v4;
	unsigned int filter_rules_v6;
	unsigned int fwd_rules_v4;
	unsigned int fwd_rules_v6;
	unsigned int out_filter_rules_v4;
	unsigned int out_filter_rules_v6;
	unsigned int out_nat_rules_v4;
	unsigned int out_nat_rules_v6;
	unsigned int ndv_ingress_policies;
	struct if_base_rule_list ndv_ingress_rules;
	struct if_base_rule_list ndv_ingress_dnat_rules;
};

struct nft_base_rules nft_base_rules;

static void print_nft_base_rules(void)
{
	tools_printlog(LOG_DEBUG, "%s():    table ip = %d", __FUNCTION__, nft_base_rules.tables & NFTLB_TABLE_IP_ACTIVE);
	tools_printlog(LOG_DEBUG, "%s():    table ip6 = %d", __FUNCTION__, nft_base_rules.tables & NFTLB_TABLE_IP6_ACTIVE);
	tools_printlog(LOG_DEBUG, "%s():    table netdev = %d", __FUNCTION__, nft_base_rules.tables & NFTLB_TABLE_NETDEV_ACTIVE);
	tools_printlog(LOG_DEBUG, "%s():    dnat_rules_v4 = %d", __FUNCTION__, nft_base_rules.dnat_rules_v4);
	tools_printlog(LOG_DEBUG, "%s():    snat_rules_v4 = %d", __FUNCTION__, nft_base_rules.snat_rules_v4);
	tools_printlog(LOG_DEBUG, "%s():    filter_rules_v4 = %d", __FUNCTION__, nft_base_rules.filter_rules_v4);
	tools_printlog(LOG_DEBUG, "%s():    fwd_rules_v4 = %d", __FUNCTION__, nft_base_rules.fwd_rules_v4);
	tools_printlog(LOG_DEBUG, "%s():    out_filter_rules_v4 = %d", __FUNCTION__, nft_base_rules.out_filter_rules_v4);
	tools_printlog(LOG_DEBUG, "%s():    out_nat_rules_v4 = %d", __FUNCTION__, nft_base_rules.out_nat_rules_v4);

	tools_printlog(LOG_DEBUG, "%s():    dnat_rules_v6 = %d", __FUNCTION__, nft_base_rules.dnat_rules_v6);
	tools_printlog(LOG_DEBUG, "%s():    snat_rules_v6 = %d", __FUNCTION__, nft_base_rules.snat_rules_v6);
	tools_printlog(LOG_DEBUG, "%s():    filter_rules_v6 = %d", __FUNCTION__, nft_base_rules.filter_rules_v6);
	tools_printlog(LOG_DEBUG, "%s():    fwd_rules_v6 = %d", __FUNCTION__, nft_base_rules.fwd_rules_v6);
	tools_printlog(LOG_DEBUG, "%s():    out_filter_rules_v6 = %d", __FUNCTION__, nft_base_rules.out_filter_rules_v6);
	tools_printlog(LOG_DEBUG, "%s():    out_nat_rules_v6 = %d", __FUNCTION__, nft_base_rules.out_nat_rules_v6);

	tools_printlog(LOG_DEBUG, "%s():    ndv_ingress_policies = %d", __FUNCTION__, nft_base_rules.ndv_ingress_policies);
	tools_printlog(LOG_DEBUG, "%s():    ndv_ingress_rules = %d", __FUNCTION__, nft_base_rules.ndv_ingress_rules.n_interfaces);
	tools_printlog(LOG_DEBUG, "%s():    ndv_ingress_dnat_rules = %d", __FUNCTION__, nft_base_rules.ndv_ingress_dnat_rules.n_interfaces);
}

static int reset_ndv_base(struct if_base_rule_list *ndv_if_rules)
{
	int i;

	for (i = 0; i < ndv_if_rules->n_interfaces; i++) {
		if (!ndv_if_rules->interfaces[i])
			break;
		if (ndv_if_rules->interfaces[i]->ifname)
			free(ndv_if_rules->interfaces[i]->ifname);
		if (ndv_if_rules->interfaces[i])
			free(ndv_if_rules->interfaces[i]);
	}

	ndv_if_rules->n_interfaces = 0;

	return 0;
}

static void clean_rules_counters(void)
{
	reset_ndv_base(&nft_base_rules.ndv_ingress_rules);
	reset_ndv_base(&nft_base_rules.ndv_ingress_dnat_rules);
	nft_base_rules.dnat_rules_v4 = 0;
	nft_base_rules.dnat_rules_v6 = 0;
	nft_base_rules.filter_rules_v4 = 0;
	nft_base_rules.filter_rules_v6 = 0;
	nft_base_rules.fwd_rules_v4 = 0;
	nft_base_rules.fwd_rules_v6 = 0;
	nft_base_rules.snat_rules_v4 = 0;
	nft_base_rules.snat_rules_v6 = 0;
	nft_base_rules.out_filter_rules_v4 = 0;
	nft_base_rules.out_filter_rules_v6 = 0;
	nft_base_rules.out_nat_rules_v4 = 0;
	nft_base_rules.out_nat_rules_v6 = 0;
}

static struct if_base_rule * get_ndv_base(struct if_base_rule_list *ndv_if_rules, char *ifname)
{
	int i;

	for (i = 0; i < ndv_if_rules->n_interfaces; i++) {
		if (strcmp(ndv_if_rules->interfaces[i]->ifname, ifname) == 0)
			return ndv_if_rules->interfaces[i];
	}

	return NULL;
}

static struct if_base_rule * add_ndv_base(struct if_base_rule_list *ndv_if_rules, char *ifname)
{
	struct if_base_rule *ifentry;

	if (ndv_if_rules->n_interfaces == NFTLB_MAX_IFACES) {
		tools_printlog(LOG_ERR, "%s():%d: maximum number of interfaces reached", __FUNCTION__, __LINE__);
		return NULL;
	}

	ifentry = (struct if_base_rule *)malloc(sizeof(struct if_base_rule));
	if (!ifentry) {
		tools_printlog(LOG_ERR, "%s():%d: unable to allocate interface struct for %s", __FUNCTION__, __LINE__, ifname);
		return NULL;
	}

	ndv_if_rules->interfaces[ndv_if_rules->n_interfaces] = ifentry;
	ndv_if_rules->n_interfaces++;

	ifentry->ifname = (char *)malloc(strlen(ifname));
	if (!ifentry->ifname) {
		tools_printlog(LOG_ERR, "%s():%d: unable to allocate interface name for %s", __FUNCTION__, __LINE__, ifname);
		return NULL;
	}

	sprintf(ifentry->ifname, "%s", ifname);
	ifentry->rules_v4 = 0;
	ifentry->rules_v6 = 0;

	return ifentry;
}

static int del_ndv_base(struct if_base_rule_list *ndv_if_rules, char *ifname)
{
	struct if_base_rule *ifentry;

	ifentry = get_ndv_base(ndv_if_rules, ifname);
	if (!ifentry)
		return 1;

	free(ifentry->ifname);
	free(ifentry);

	if (ndv_if_rules->n_interfaces > 0)
		return ndv_if_rules->n_interfaces--;

	return 0;
}

static int exec_cmd_open(char *cmd, const char **out, int error_output)
{
	int error;

	if (strlen(cmd) == 0 || strcmp(cmd, "") == 0)
		return 0;

	tools_printlog(LOG_NOTICE, "nft command exec : %s", cmd);

	ctx = nft_ctx_new(0);
	nft_ctx_buffer_error(ctx);

	if (out != NULL)
		nft_ctx_buffer_output(ctx);

	error = nft_run_cmd_from_buffer(ctx, cmd);

	if (error && error_output)
		tools_printlog(LOG_ERR, "nft command error : %s", nft_ctx_get_error_buffer(ctx));

	if (out != NULL)
		*out = nft_ctx_get_output_buffer(ctx);

	return error;
}

static void exec_cmd_close(const char *out)
{
	if (ctx == NULL)
		return;

	if (out != NULL)
		nft_ctx_unbuffer_output(ctx);

	nft_ctx_unbuffer_error(ctx);
	nft_ctx_free(ctx);
	ctx = NULL;
}

static int exec_cmd(char *cmd)
{
	int error;

	error = exec_cmd_open(cmd, NULL, 1);
	exec_cmd_close(NULL);

	return error;
}

static int exec_cmd_unbuffered(struct sbuffer *buf)
{
	int error;
	error = exec_cmd(get_buf_data(buf));
	reset_buf(buf);

	return error;
}

static void concat_exec_cmd(struct sbuffer *buf, char *fmt, ...)
{
	int len;
	va_list args;

	va_start(args, fmt);
	len = vsnprintf(0, 0, fmt, args);
	va_end(args);

	va_start(args, fmt);
	concat_buf_va(buf, len, fmt, args);
	va_end(args);

	if (serialize)
		exec_cmd_unbuffered(buf);
}

static char * print_nft_service(struct farm *f, int family)
{
	if (family == VALUE_FAMILY_IPV6) {
		if (farm_no_port(f))
			return NFTLB_IP_SERVICES6_MAP;
		else if (farm_no_virtaddr(f))
			return NFTLB_PORT_SERVICES6_MAP;
		else
			return NFTLB_PROTO_SERVICES6_MAP;
	} else {
		if (farm_no_port(f))
			return NFTLB_IP_SERVICES_MAP;
		else if (farm_no_virtaddr(f))
			return NFTLB_PORT_SERVICES_MAP;
		else
			return NFTLB_PROTO_SERVICES_MAP;
	}
}

static char * print_nft_family_type(int family)
{
	switch (family) {
	case VALUE_FAMILY_IPV6:
		return NFTLB_MAP_TYPE_IPV6;
	default:
		return NFTLB_MAP_TYPE_IPV4;
	}
}

static char * print_nft_family(int family)
{
	switch (family) {
	case VALUE_FAMILY_NETDEV:
		return NFTLB_NETDEV_FAMILY_STR;
	case VALUE_FAMILY_IPV6:
		return NFTLB_IPV6_FAMILY_STR;
	default:
		return NFTLB_IPV4_FAMILY_STR;
	}
}

static char * print_nft_family_protocol(int family)
{
	switch (family) {
	case VALUE_FAMILY_IPV6:
		return NFTLB_IPV6_PROTOCOL;
	default:
		return NFTLB_IPV4_PROTOCOL;
	}
}

static char * print_nft_table_family(int family, unsigned int type)
{
	if (family == VALUE_FAMILY_NETDEV || type & NFTLB_F_CHAIN_ING_FILTER || type & NFTLB_F_CHAIN_ING_DNAT)
		return NFTLB_NETDEV_FAMILY_STR;
	else if (family == VALUE_FAMILY_IPV6)
		return NFTLB_IPV6_FAMILY_STR;
	else
		return NFTLB_IPV4_FAMILY_STR;
}

static char * print_nft_protocol(int protocol)
{
	switch (protocol) {
	case VALUE_PROTO_UDP:
		return NFTLB_UDP_PROTO;
	case VALUE_PROTO_SCTP:
		return NFTLB_SCTP_PROTO;
	default:
		return NFTLB_TCP_PROTO;
	}
}

static char * print_nft_verdict(enum type type)
{
	if (type == VALUE_TYPE_WHITE)
		return NFTLB_NFT_VERDICT_ACCEPT;
	else
		return NFTLB_NFT_VERDICT_DROP;
}

static char * print_nft_prefix_policy(enum type type)
{
	if (type == VALUE_TYPE_WHITE)
		return NFTLB_NFT_PREFIX_POLICY_WL;
	else
		return NFTLB_NFT_PREFIX_POLICY_BL;
}

static unsigned int * get_rules_applied(int type, int family, char *iface)
{
	unsigned int * ret = NULL;
	struct if_base_rule *if_base;

	if (type & NFTLB_F_CHAIN_ING_FILTER) {
		if_base = get_ndv_base(&nft_base_rules.ndv_ingress_rules, iface);
		if (!if_base)
			if_base = add_ndv_base(&nft_base_rules.ndv_ingress_rules, iface);
		if (!if_base)
			return ret;
		if (family == VALUE_FAMILY_IPV4)
			ret = &if_base->rules_v4;
		if (family == VALUE_FAMILY_IPV6)
			ret = &if_base->rules_v6;

	} else if (type & NFTLB_F_CHAIN_ING_DNAT) {
		if_base = get_ndv_base(&nft_base_rules.ndv_ingress_dnat_rules, iface);
		if (!if_base)
			if_base = add_ndv_base(&nft_base_rules.ndv_ingress_dnat_rules, iface);
		if (!if_base)
			return ret;
		if (family == VALUE_FAMILY_IPV4)
			ret = &if_base->rules_v4;
		if (family == VALUE_FAMILY_IPV6)
			ret = &if_base->rules_v6;

	} else if (type & NFTLB_F_CHAIN_PRE_DNAT) {
		if (family == VALUE_FAMILY_IPV4)
			ret = &nft_base_rules.dnat_rules_v4;
		if (family == VALUE_FAMILY_IPV6)
			ret = &nft_base_rules.dnat_rules_v6;

	} else if (type & NFTLB_F_CHAIN_PRE_FILTER) {
		if (family == VALUE_FAMILY_IPV4)
			ret = &nft_base_rules.filter_rules_v4;
		if (family == VALUE_FAMILY_IPV6)
			ret = &nft_base_rules.filter_rules_v6;

	} else if (type & NFTLB_F_CHAIN_FWD_FILTER) {
		if (family == VALUE_FAMILY_IPV4)
			ret = &nft_base_rules.fwd_rules_v4;
		if (family == VALUE_FAMILY_IPV6)
			ret = &nft_base_rules.fwd_rules_v6;

	} else if (type & NFTLB_F_CHAIN_POS_SNAT) {
		if (family == VALUE_FAMILY_IPV4)
			ret = &nft_base_rules.snat_rules_v4;
		if (family == VALUE_FAMILY_IPV6)
			ret = &nft_base_rules.snat_rules_v6;

	} else if (type & NFTLB_F_CHAIN_OUT_FILTER) {
		if (family == VALUE_FAMILY_IPV4)
			ret = &nft_base_rules.out_filter_rules_v4;
		if (family == VALUE_FAMILY_IPV6)
			ret = &nft_base_rules.out_filter_rules_v6;

	} else if (type & NFTLB_F_CHAIN_OUT_DNAT) {
		if (family == VALUE_FAMILY_IPV4)
			ret = &nft_base_rules.out_nat_rules_v4;
		if (family == VALUE_FAMILY_IPV6)
			ret = &nft_base_rules.out_nat_rules_v6;
	}

	return ret;
}

static void logprefix_replace(char *buf, char *token, char *value)
{
	char tmp[255] = { 0 };
	char *ptr = buf;
	char *tmpptr = tmp;

	while (*ptr != '\0') {
		if (strncmp(token, ptr, strlen(token)) == 0) {
			strcat(tmpptr, value);
			ptr += strlen(token);
			tmpptr += strlen(value);
		}
		*tmpptr = *ptr;
		ptr++;
		tmpptr++;
	}

	*tmpptr = '\0';
	sprintf(buf, "%s", tmp);
}

static void print_log_format(char *buf, int key, int type, struct farm *f, struct backend *b, struct policy *p)
{
	if (!f) {
		return;
	}

	switch (key) {
	case KEY_LOGPREFIX:
		if (p) {
			sprintf(buf, "%s", p->logprefix);
			logprefix_replace(buf, "KNAME", "policy");
			logprefix_replace(buf, "FNAME", f->name);
			logprefix_replace(buf, "PNAME", p->name);
			logprefix_replace(buf, "TYPE", print_nft_prefix_policy(p->type));
			return;
		}
		sprintf(buf, "%s", f->logprefix);
		logprefix_replace(buf, "KNAME", CONFIG_KEY_LOG);
		logprefix_replace(buf, "FNAME", f->name);
		if ((f->log & VALUE_LOG_INPUT) && ((type & NFTLB_F_CHAIN_ING_FILTER) || (type & NFTLB_F_CHAIN_PRE_FILTER) || (type & NFTLB_F_CHAIN_PRE_DNAT)))
			logprefix_replace(buf, "TYPE", "IN");
		else if ((f->log & VALUE_LOG_FORWARD) && (type & NFTLB_F_CHAIN_FWD_FILTER))
			logprefix_replace(buf, "TYPE", "FWD");
		else if ((f->log & VALUE_LOG_OUTPUT) && ((type & NFTLB_F_CHAIN_POS_SNAT) || (type & NFTLB_F_CHAIN_ING_DNAT)))
			logprefix_replace(buf, "TYPE", "OUT");
		break;
	case KEY_NEWRTLIMIT_LOGPREFIX:
		sprintf(buf, "%s", f->newrtlimit_logprefix);
		logprefix_replace(buf, "KNAME", CONFIG_KEY_NEWRTLIMIT);
		logprefix_replace(buf, "FNAME", f->name);
		break;
	case KEY_RSTRTLIMIT_LOGPREFIX:
		sprintf(buf, "%s", f->rstrtlimit_logprefix);
		logprefix_replace(buf, "KNAME", CONFIG_KEY_RSTRTLIMIT);
		logprefix_replace(buf, "FNAME", f->name);
		break;
	case KEY_ESTCONNLIMIT_LOGPREFIX:
		if (b) {
			sprintf(buf, "%s", b->estconnlimit_logprefix);
			logprefix_replace(buf, "KNAME", CONFIG_KEY_ESTCONNLIMIT);
			logprefix_replace(buf, "FNAME", f->name);
			logprefix_replace(buf, "BNAME", b->name);
			return;
		}
		sprintf(buf, "%s", f->estconnlimit_logprefix);
		logprefix_replace(buf, "KNAME", CONFIG_KEY_ESTCONNLIMIT);
		logprefix_replace(buf, "FNAME", f->name);
		break;
	case KEY_TCPSTRICT_LOGPREFIX:
		sprintf(buf, "%s", f->tcpstrict_logprefix);
		logprefix_replace(buf, "KNAME", CONFIG_KEY_TCPSTRICT);
		logprefix_replace(buf, "FNAME", f->name);
		break;
	default:
		break;
	}
}

static int need_filter(struct farm *f)
{
	return (!farm_is_ingress_mode(f) && f->mode != VALUE_MODE_LOCAL);
}

static int need_forward(struct farm *f)
{
	return ((f->log & VALUE_LOG_FORWARD) || farm_needs_flowtable(f));
}

static int need_output(struct farm *f)
{
	return farm_needs_intraconnect(f);
}

static unsigned int get_stage_by_farm_mode(struct farm *f)
{
	if (f->mode == VALUE_MODE_DSR)
		return NFTLB_F_CHAIN_ING_FILTER;
	else if (f->mode == VALUE_MODE_STLSDNAT)
		return NFTLB_F_CHAIN_ING_DNAT;
	else
		return NFTLB_F_CHAIN_PRE_FILTER;
}

static void get_farm_chain(char *name, struct farm *f, int type)
{
	if (type & NFTLB_F_CHAIN_ING_FILTER)
		sprintf(name, "%s", f->name);
	else if (type & NFTLB_F_CHAIN_PRE_FILTER)
		sprintf(name, "%s-%s", NFTLB_TYPE_FILTER, f->name);
	else if (type & NFTLB_F_CHAIN_PRE_DNAT)
		sprintf(name, "%s-%s", NFTLB_TYPE_NAT, f->name);
	else if (type & NFTLB_F_CHAIN_FWD_FILTER)
		sprintf(name, "%s-%s", NFTLB_TYPE_FWD, f->name);
	else if (type & NFTLB_F_CHAIN_ING_DNAT)
		sprintf(name, "%s-back", f->name);
	else if (type & NFTLB_F_CHAIN_OUT_FILTER)
		sprintf(name, "%s-%s", NFTLB_TYPE_FILTER, f->name);
	else if (type & NFTLB_F_CHAIN_OUT_DNAT)
		sprintf(name, "%s-%s", NFTLB_TYPE_NAT, f->name);
}

static void get_flowtable_name(char *name, struct farm *f)
{
	sprintf(name, "ft-%s", f->name);
}

static void get_farm_service(char *name, struct farm *f, int type, int family, int key_mode)
{
	if (type & NFTLB_F_CHAIN_ING_FILTER)
		sprintf(name, "%s-%s", print_nft_service(f, family), f->iface);
	else if (type & NFTLB_F_CHAIN_PRE_FILTER)
		sprintf(name, "%s-%s", NFTLB_TYPE_FILTER, print_nft_service(f, family));
	else if (type & NFTLB_F_CHAIN_PRE_DNAT)
		sprintf(name, "%s-%s", NFTLB_TYPE_NAT, print_nft_service(f, family));
	else if (type & NFTLB_F_CHAIN_FWD_FILTER)
		sprintf(name, "%s-%s", NFTLB_TYPE_FWD, print_nft_service(f, family));
	else if (type & NFTLB_F_CHAIN_POS_SNAT) {
		sprintf(name, "%s-back", print_nft_service(f, family));
		if (key_mode == BCK_MAP_BCK_ID || key_mode == BCK_MAP_BCK_MARK)
			strcat(name, "-m");
	}
	else if (type & NFTLB_F_CHAIN_ING_DNAT)
		sprintf(name, "%s-dnat-%s", print_nft_service(f, family), f->iface);
	else if (type & NFTLB_F_CHAIN_OUT_FILTER)
		sprintf(name, "%s-%s", NFTLB_TABLE_OUT_FILTER, print_nft_service(f, family));
	else if (type & NFTLB_F_CHAIN_OUT_DNAT)
		sprintf(name, "%s-%s", NFTLB_TABLE_OUT_NAT, print_nft_service(f, family));
}

static int nft_table_handler(struct sbuffer *buf, char *str_family, int action)
{
	switch (action) {
	case ACTION_RELOAD:
		concat_exec_cmd(buf, " ; flush table %s %s", str_family, NFTLB_TABLE_NAME);
		break;
	case ACTION_START:
		concat_exec_cmd(buf, " ; add table %s %s", str_family, NFTLB_TABLE_NAME);
		break;
	case ACTION_STOP:
	case ACTION_DELETE:
		concat_exec_cmd(buf, " ; delete table %s %s", str_family, NFTLB_TABLE_NAME);
		break;
	default:
		break;
	}

	return 0;
}

static int nft_chain_handler(struct sbuffer *buf, char *str_family, char *chain, char *str_type, char *hook, char *str_device, int priority, int action)
{
	switch (action) {
	case ACTION_RELOAD:
		concat_exec_cmd(buf, " ; flush chain %s %s %s", str_family, NFTLB_TABLE_NAME, chain);
		break;
	case ACTION_START:
		concat_buf(buf, " ; add chain %s %s %s", str_family, NFTLB_TABLE_NAME, chain);
		if (!obj_equ_attribute_string(str_type, "") && !obj_equ_attribute_string(hook, "")) {
			concat_buf(buf, " { type %s hook %s", str_type, hook);
			if (!obj_equ_attribute_string(str_device, ""))
				concat_buf(buf, " device %s", str_device);
			concat_buf(buf, " priority %d ;}", priority);
		}
		concat_exec_cmd(buf, "");
		break;
	case ACTION_STOP:
	case ACTION_DELETE:
		concat_exec_cmd(buf, " ; delete chain %s %s %s", str_family, NFTLB_TABLE_NAME, chain);
		break;
	default:
		break;
	}

	return 0;
}

static int run_farm_rules_gen_chain(struct sbuffer *buf, struct farm *f, char *nft_family, int type, int action)
{
	char chain[255] = { 0 };

	get_farm_chain(chain, f, type);

	// output uses the same farm chains than prerouting-filter and prerouting-nat
	if (type == NFTLB_F_CHAIN_OUT_FILTER || type == NFTLB_F_CHAIN_OUT_DNAT)
		return 0;

	nft_chain_handler(buf, nft_family, chain, NULL, NULL, NULL, 0, action);
	return 0;
}

static int run_base_chain_filter_ctmark(struct sbuffer *buf, int type, char *chain_family, char *base_chain)
{
	if (type & NFTLB_F_CHAIN_PRE_FILTER)
		concat_exec_cmd(buf, " ; add rule %s %s %s mark set ct mark", chain_family, NFTLB_TABLE_NAME, base_chain);

	return 0;
}

static int run_base_chain_postrouting_masquerade(struct sbuffer *buf, int type, char *chain_family)
{
	if (type & NFTLB_F_CHAIN_POS_SNAT)
		concat_exec_cmd(buf, " ; add rule %s %s %s ct mark and 0x%x == 0x%x masquerade", chain_family, NFTLB_TABLE_NAME, NFTLB_TABLE_POSTROUTING, masquerade_mark, masquerade_mark);

	return 0;
}

static int run_base_chain_postrouting_bckmark(struct sbuffer *buf, struct farm *f, char *service, int type, int family)
{
	if (~type & NFTLB_F_CHAIN_POS_SNAT)
		return 0;

	concat_exec_cmd(buf, " ; add map %s %s %s { type %s : %s ;}", print_nft_family(family), NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_MARK, print_nft_family_type(family));
	concat_exec_cmd(buf, " ; add rule %s %s %s snat to ct mark map @%s", print_nft_family(family), NFTLB_TABLE_NAME, NFTLB_TABLE_POSTROUTING, service);

	return 1;
}

static int run_farm_rules_gen_meta_param(struct sbuffer *buf, struct farm *f, int family, int param, int type)
{
	int items = 0;

	if ((param & VALUE_META_NONE) ||
		(param & VALUE_META_SRCIP)) {
		(type == NFTLB_MAP_KEY_TYPE) ? concat_buf(buf, " %s", print_nft_family_type(family)) : concat_buf(buf, " %s saddr", print_nft_family(family));
		items++;
	}

	if (param & VALUE_META_DSTIP) {
		if (items)
			concat_buf(buf, " .");
		(type == NFTLB_MAP_KEY_TYPE) ? concat_buf(buf, " %s", print_nft_family_type(family)) : concat_buf(buf, " %s daddr", print_nft_family(family));
		items++;
	}

	if (param & VALUE_META_SRCPORT) {
		if (items)
			concat_buf(buf, " .");
		(type == NFTLB_MAP_KEY_TYPE) ? concat_buf(buf, " inet_service") : concat_buf(buf, " %s sport", print_nft_protocol(f->protocol));
		items++;
	}

	if (param & VALUE_META_DSTPORT) {
		if (items)
			concat_buf(buf, " .");
		(type == NFTLB_MAP_KEY_TYPE) ? concat_buf(buf, " inet_service") : concat_buf(buf, " %s dport", print_nft_protocol(f->protocol));
		items++;
	}

	if (param & VALUE_META_SRCMAC) {
		if (items)
			concat_buf(buf, " .");
		(type == NFTLB_MAP_KEY_TYPE) ? concat_buf(buf, " ether_addr") : concat_buf(buf, " ether saddr");
		items++;
	}

	if (param & VALUE_META_DSTMAC) {
		if (items)
			concat_buf(buf, " .");
		(type == NFTLB_MAP_KEY_TYPE) ? concat_buf(buf, " ether_addr") : concat_buf(buf, " ether daddr");
	}

	if (param & VALUE_META_MARK) {
		if (items)
			concat_buf(buf, " .");
		concat_buf(buf, " mark");
	}

	return 0;
}

static void run_farm_map(struct sbuffer *buf, struct farm *f, int family, unsigned int stage, char *mapname, int key, int data, int timeout, int action)
{
	switch (action) {
	case ACTION_START:
		concat_buf(buf, " ; add map %s %s %s { type ", print_nft_table_family(family, stage), NFTLB_TABLE_NAME, mapname);
		run_farm_rules_gen_meta_param(buf, f, family, key, NFTLB_MAP_KEY_TYPE);
		concat_buf(buf, " :");
		run_farm_rules_gen_meta_param(buf, f, family, data, NFTLB_MAP_KEY_TYPE);
		concat_buf(buf, ";");
		if (timeout != -1)
			concat_buf(buf, " timeout %ds;", timeout);
		concat_exec_cmd(buf, " }");
		break;
	case ACTION_DELETE:
	case ACTION_STOP:
		concat_exec_cmd(buf, " ; delete map %s %s %s", print_nft_table_family(family, stage), NFTLB_TABLE_NAME, mapname);
		break;
	case ACTION_RELOAD:
	default:
		break;
	}
}

static int run_base_table(struct sbuffer *buf, int type, int family, int action)
{
	char *chain_family = print_nft_table_family(family, type);

	if (action == ACTION_STOP || action == ACTION_DELETE) {
		// delete ip and ip6 based nftlb tables
		if ((type & NFTLB_F_CHAIN_PRE_DNAT || type & NFTLB_F_CHAIN_PRE_FILTER) &&
			((nft_base_rules.tables & NFTLB_TABLE_IP_ACTIVE && family == VALUE_FAMILY_IPV4 && (nft_base_rules.dnat_rules_v4 == 0 && nft_base_rules.filter_rules_v4 == 0)) ||
				(nft_base_rules.tables & NFTLB_TABLE_IP6_ACTIVE && family == VALUE_FAMILY_IPV6 && (nft_base_rules.dnat_rules_v6 == 0 && nft_base_rules.filter_rules_v6 == 0)))) {
			if (family == VALUE_FAMILY_IPV4)
				nft_base_rules.tables &= ~NFTLB_TABLE_IP_ACTIVE;
			if (family == VALUE_FAMILY_IPV6)
				nft_base_rules.tables &= ~NFTLB_TABLE_IP6_ACTIVE;
			nft_table_handler(buf, chain_family, ACTION_DELETE);
		}

		if (type & NFTLB_F_CHAIN_ING_FILTER && (nft_base_rules.tables & NFTLB_TABLE_NETDEV_ACTIVE) &&
			nft_base_rules.ndv_ingress_policies == 0 && nft_base_rules.ndv_ingress_rules.n_interfaces == 0 && nft_base_rules.ndv_ingress_dnat_rules.n_interfaces == 0) {
			nft_base_rules.tables &= ~NFTLB_TABLE_NETDEV_ACTIVE;
			nft_table_handler(buf, chain_family, ACTION_DELETE);
		}
	}

	if (action == ACTION_RELOAD || action == ACTION_START) {
		if ((type & NFTLB_F_CHAIN_PRE_DNAT || type & NFTLB_F_CHAIN_PRE_FILTER) &&
			((~nft_base_rules.tables & NFTLB_TABLE_IP_ACTIVE && family == VALUE_FAMILY_IPV4 && (nft_base_rules.dnat_rules_v4 == 0 && nft_base_rules.filter_rules_v4 == 0)) ||
			(~nft_base_rules.tables & NFTLB_TABLE_IP6_ACTIVE && family == VALUE_FAMILY_IPV6 && (nft_base_rules.dnat_rules_v6 == 0 && nft_base_rules.filter_rules_v6 == 0)))) {
			if (family == VALUE_FAMILY_IPV4)
				nft_base_rules.tables |= NFTLB_TABLE_IP_ACTIVE;
			if (family == VALUE_FAMILY_IPV6)
				nft_base_rules.tables |= NFTLB_TABLE_IP6_ACTIVE;
			nft_table_handler(buf, chain_family, ACTION_START);
		}

		if (type & NFTLB_F_CHAIN_ING_FILTER && ~nft_base_rules.tables & NFTLB_TABLE_NETDEV_ACTIVE &&
			nft_base_rules.ndv_ingress_rules.n_interfaces == 0 &&
			nft_base_rules.ndv_ingress_dnat_rules.n_interfaces == 0) {
			nft_base_rules.tables |= NFTLB_TABLE_NETDEV_ACTIVE;
			nft_table_handler(buf, chain_family, ACTION_START);
		}
	}

	return 0;
}

static int run_base_chain(struct sbuffer *buf, struct farm *f, int type, int family, unsigned int rules_needed, int action)
{
	char service[255] = { 0 };
	char servicem[257] = { 0 };
	char base_chain[265] = { 0 };
	char chain_device[10] = { 0 };
	char *chain_type;
	char *chain_hook;
	char *chain_family;
	int chain_prio;
	unsigned int *base_rules;
	struct if_base_rule_list *if_base_list = &nft_base_rules.ndv_ingress_rules;

	get_farm_service(service, f, type, family, BCK_MAP_NONE);
	chain_family = print_nft_table_family(family, type);

	if (type & NFTLB_F_CHAIN_ING_FILTER) {
		base_rules = get_rules_applied(type, family, f->iface);
		if (!base_rules)
			return 1;
		chain_prio = NFTLB_INGRESS_PRIO;
		chain_type = NFTLB_TYPE_FILTER;
		chain_hook = NFTLB_HOOK_INGRESS;
		sprintf(chain_device, "%s", f->iface);
		sprintf(base_chain, "%s-%s", NFTLB_TABLE_INGRESS, chain_device);

	} else if (type & NFTLB_F_CHAIN_ING_DNAT) {
		base_rules = get_rules_applied(type, family, f->oface);
		if (!base_rules)
			return 1;
		if_base_list = &nft_base_rules.ndv_ingress_dnat_rules;
		chain_prio = NFTLB_INGRESS_DNAT_PRIO;
		chain_type = NFTLB_TYPE_FILTER;
		chain_hook = NFTLB_HOOK_INGRESS;
		sprintf(chain_device, "%s", f->oface);
		sprintf(base_chain, "%s-dnat-%s", NFTLB_TABLE_INGRESS, chain_device);

	} else if (type & NFTLB_F_CHAIN_PRE_FILTER) {
		base_rules = get_rules_applied(type, family, "");
		chain_prio = NFTLB_FILTER_PRIO;
		sprintf(base_chain, "%s", NFTLB_TABLE_FILTER);
		chain_type = NFTLB_TYPE_FILTER;
		chain_hook = NFTLB_HOOK_PREROUTING;

	} else if (type & NFTLB_F_CHAIN_PRE_DNAT) {
		base_rules = get_rules_applied(type, family, "");
		chain_prio = NFTLB_PREROUTING_PRIO;
		sprintf(base_chain, "%s", NFTLB_TABLE_PREROUTING);
		chain_type = NFTLB_TYPE_NAT;
		chain_hook = NFTLB_HOOK_PREROUTING;
		get_farm_service(servicem, f, NFTLB_F_CHAIN_POS_SNAT, family, BCK_MAP_BCK_MARK);

	} else if (type & NFTLB_F_CHAIN_POS_SNAT) {
		base_rules = get_rules_applied(type, family, "");
		chain_prio = NFTLB_POSTROUTING_PRIO;
		sprintf(base_chain, "%s", NFTLB_TABLE_POSTROUTING);
		chain_type = NFTLB_TYPE_NAT;
		chain_hook = NFTLB_HOOK_POSTROUTING;
		sprintf(servicem, "%s-m", service);

	} else if (type & NFTLB_F_CHAIN_FWD_FILTER) {
		base_rules = get_rules_applied(type, family, "");
		chain_prio = NFTLB_PREROUTING_PRIO;
		sprintf(base_chain, "%s", NFTLB_TABLE_FORWARD);
		chain_type = NFTLB_TYPE_FILTER;
		chain_hook = NFTLB_HOOK_FORWARD;

	} else if (type & NFTLB_F_CHAIN_OUT_FILTER) {
		base_rules = get_rules_applied(type, family, "");
		chain_prio = NFTLB_FILTER_PRIO;
		sprintf(base_chain, "%s", NFTLB_TABLE_OUT_FILTER);
		chain_type = NFTLB_TYPE_FILTER;
		chain_hook = NFTLB_HOOK_OUTPUT;

	} else if (type & NFTLB_F_CHAIN_OUT_DNAT) {
		base_rules = get_rules_applied(type, family, "");
		chain_prio = NFTLB_PREROUTING_PRIO;
		sprintf(base_chain, "%s", NFTLB_TABLE_OUT_NAT);
		chain_type = NFTLB_TYPE_NAT;
		chain_hook = NFTLB_HOOK_OUTPUT;

	} else
		return 1;

	if (action == ACTION_STOP || action == ACTION_DELETE) {

		if (((*base_rules & NFTLB_PROTO_IP_PORT_ACTIVE) && *get_service_counter(type, NFTLB_PROTO_IP_PORT_ACTIVE, family) == 0) ||
			((*base_rules & NFTLB_PROTO_PORT_ACTIVE) && *get_service_counter(type, NFTLB_PROTO_PORT_ACTIVE, family) == 0) ||
			((*base_rules & NFTLB_PROTO_IP_ACTIVE) && *get_service_counter(type, NFTLB_PROTO_IP_ACTIVE, family) == 0)) {
			nft_chain_handler(buf, chain_family, base_chain, NULL, NULL, NULL, 0, ACTION_RELOAD);
			run_base_chain_filter_ctmark(buf, type, chain_family, base_chain);
			run_base_chain_postrouting_masquerade(buf, type, chain_family);
			run_base_chain_postrouting_bckmark(buf, f, servicem, type, family);
			run_farm_map(buf, f, family, type, service, 0, 0, 0, ACTION_DELETE);
			*base_rules &= ~NFTLB_PROTO_IP_PORT_ACTIVE & ~NFTLB_PROTO_PORT_ACTIVE & ~NFTLB_PROTO_IP_ACTIVE;
		}

		if ((~type & NFTLB_F_CHAIN_POS_SNAT) && (*base_rules & NFTLB_IP_ACTIVE) && *get_service_counter(type, NFTLB_IP_ACTIVE, family) == 0) {
			*base_rules &= ~NFTLB_IP_ACTIVE;
			nft_chain_handler(buf, chain_family, base_chain, NULL, NULL, NULL, 0, ACTION_DELETE);

			if (type & NFTLB_F_CHAIN_ING_FILTER || type & NFTLB_F_CHAIN_ING_DNAT)
				del_ndv_base(if_base_list, f->iface);

			if (type & NFTLB_F_CHAIN_PRE_DNAT) {
				nft_chain_handler(buf, chain_family, NFTLB_TABLE_POSTROUTING, NULL, NULL, NULL, 0, ACTION_DELETE);
				run_farm_map(buf, f, family, type, servicem, 0, 0, 0, ACTION_DELETE);
				base_rules = get_rules_applied(NFTLB_F_CHAIN_POS_SNAT, family, "");
				*base_rules &= ~NFTLB_IP_ACTIVE & ~NFTLB_PROTO_IP_PORT_ACTIVE & ~NFTLB_PROTO_PORT_ACTIVE & ~NFTLB_PROTO_IP_ACTIVE;
			}
		}
	}

	if (action == ACTION_RELOAD || action == ACTION_START) {

		if ((rules_needed & NFTLB_IP_ACTIVE) && !(*base_rules & NFTLB_IP_ACTIVE)) {
			nft_chain_handler(buf, chain_family, base_chain, chain_type, chain_hook, chain_device, chain_prio, ACTION_START);
			run_base_chain_filter_ctmark(buf, type, chain_family, base_chain);
			run_base_chain_postrouting_masquerade(buf, type, chain_family);
			run_base_chain_postrouting_bckmark(buf, f, servicem, type, family);
			*base_rules |= NFTLB_IP_ACTIVE;
		}

		if ((rules_needed & NFTLB_PROTO_IP_PORT_ACTIVE) && !(*base_rules & NFTLB_PROTO_IP_PORT_ACTIVE)) {
			if (type & NFTLB_F_CHAIN_POS_SNAT) {
				if (f->mode == VALUE_MODE_SNAT && !farm_get_masquerade(f)) {
					concat_exec_cmd(buf, " ; add map %s %s %s { type %s . %s . %s : %s ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_PROTO, print_nft_family_type(family), NFTLB_MAP_TYPE_INETSRV, print_nft_family_type(family));
					concat_exec_cmd(buf, " ; add rule %s %s %s snat to %s %s . %s daddr . th dport map @%s", chain_family, NFTLB_TABLE_NAME, NFTLB_TABLE_POSTROUTING, chain_family, print_nft_family_protocol(family), chain_family, service);
					*base_rules |= NFTLB_PROTO_IP_PORT_ACTIVE;
				}
			} else if (type & NFTLB_F_CHAIN_ING_DNAT) {
				concat_exec_cmd(buf, " ; add map %s %s %s { type %s . %s . %s : verdict ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_PROTO, print_nft_family_type(family), NFTLB_MAP_TYPE_INETSRV);
				concat_exec_cmd(buf, " ; add rule %s %s %s %s %s . %s saddr . th sport vmap @%s", chain_family, NFTLB_TABLE_NAME, base_chain, print_nft_family(family), print_nft_family_protocol(family), print_nft_family(family), service);
				*base_rules |= NFTLB_PROTO_IP_PORT_ACTIVE;
			} else if (type & NFTLB_F_CHAIN_FWD_FILTER) {
				concat_exec_cmd(buf, " ; add map %s %s %s { type %s : verdict ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_MARK);
				concat_exec_cmd(buf, " ; add rule %s %s %s ct mark vmap @%s", chain_family, NFTLB_TABLE_NAME, base_chain, service);
				*base_rules |= NFTLB_PROTO_IP_PORT_ACTIVE;
			} else {
				concat_exec_cmd(buf, " ; add map %s %s %s { type %s . %s . %s : verdict ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_PROTO, print_nft_family_type(family), NFTLB_MAP_TYPE_INETSRV);
				concat_exec_cmd(buf, " ; add rule %s %s %s %s %s . %s daddr . th dport vmap @%s", chain_family, NFTLB_TABLE_NAME, base_chain, print_nft_family(family), print_nft_family_protocol(family), print_nft_family(family), service);
				*base_rules |= NFTLB_PROTO_IP_PORT_ACTIVE;
			}
		}

		if ((rules_needed & NFTLB_PROTO_PORT_ACTIVE) && !(*base_rules & NFTLB_PROTO_PORT_ACTIVE)) {
			if (type & NFTLB_F_CHAIN_POS_SNAT) {
				if (f->mode == VALUE_MODE_SNAT && !farm_get_masquerade(f)) {
					concat_exec_cmd(buf, " ; add map %s %s %s { type %s . %s : %s ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_PROTO, NFTLB_MAP_TYPE_INETSRV, print_nft_family_type(family));
					concat_exec_cmd(buf, " ; add rule %s %s %s snat to %s %s . th dport map @%s", chain_family, NFTLB_TABLE_NAME, NFTLB_TABLE_POSTROUTING, chain_family, print_nft_family_protocol(family), service);
					*base_rules |= NFTLB_PROTO_PORT_ACTIVE;
				}
			} else if (type & NFTLB_F_CHAIN_ING_DNAT) {
				concat_exec_cmd(buf, " ; add map %s %s %s { type %s . %s : verdict ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_PROTO, NFTLB_MAP_TYPE_INETSRV);
				concat_exec_cmd(buf, " ; add rule %s %s %s %s %s . th sport vmap @%s", chain_family, NFTLB_TABLE_NAME, base_chain, print_nft_family(family), print_nft_family_protocol(family), service);
				*base_rules |= NFTLB_PROTO_PORT_ACTIVE;
			} else if (type & NFTLB_F_CHAIN_FWD_FILTER) {
				concat_exec_cmd(buf, " ; add map %s %s %s { type %s : verdict ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_MARK);
				concat_exec_cmd(buf, " ; add rule %s %s %s ct mark vmap @%s", chain_family, NFTLB_TABLE_NAME, base_chain, service);
				*base_rules |= NFTLB_PROTO_PORT_ACTIVE;
			} else {
				concat_exec_cmd(buf, " ; add map %s %s %s { type %s . %s : verdict ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_PROTO, NFTLB_MAP_TYPE_INETSRV);
				concat_exec_cmd(buf, " ; add rule %s %s %s %s %s . th dport vmap @%s", chain_family, NFTLB_TABLE_NAME, base_chain, print_nft_family(family), print_nft_family_protocol(family), service);
				*base_rules |= NFTLB_PROTO_PORT_ACTIVE;
			}
		}

		if ((rules_needed & NFTLB_PROTO_IP_ACTIVE) && !(*base_rules & NFTLB_PROTO_IP_ACTIVE)) {
			if (type & NFTLB_F_CHAIN_POS_SNAT) {
				if (f->mode == VALUE_MODE_SNAT && !farm_get_masquerade(f)) {
					concat_exec_cmd(buf, " ; add map %s %s %s { type %s . %s : %s ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_PROTO, print_nft_family_type(family), print_nft_family_type(family));
					concat_exec_cmd(buf, " ; add rule %s %s %s snat to %s %s . %s daddr map @%s", chain_family, NFTLB_TABLE_NAME, NFTLB_TABLE_POSTROUTING, chain_family, print_nft_family_protocol(family), chain_family, service);
					*base_rules |= NFTLB_PROTO_IP_ACTIVE;
				}
			} else if (type & NFTLB_F_CHAIN_ING_DNAT) {
				concat_exec_cmd(buf, " ; add map %s %s %s { type %s . %s : verdict ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_PROTO, print_nft_family_type(family));
				concat_exec_cmd(buf, " ; add rule %s %s %s %s %s . %s saddr vmap @%s", chain_family, NFTLB_TABLE_NAME, base_chain, print_nft_family(family), print_nft_family_protocol(family), print_nft_family(family), service);
				*base_rules |= NFTLB_PROTO_IP_ACTIVE;
			} else if (type & NFTLB_F_CHAIN_FWD_FILTER) {
				concat_exec_cmd(buf, " ; add map %s %s %s { type %s : verdict ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_MARK);
				concat_exec_cmd(buf, " ; add rule %s %s %s ct mark vmap @%s", chain_family, NFTLB_TABLE_NAME, base_chain, service);
				*base_rules |= NFTLB_PROTO_IP_ACTIVE;
			} else {
				concat_exec_cmd(buf, " ; add map %s %s %s { type %s . %s : verdict ;}", chain_family, NFTLB_TABLE_NAME, service, NFTLB_MAP_TYPE_PROTO, print_nft_family_type(family));
				concat_exec_cmd(buf, " ; add rule %s %s %s %s %s . %s daddr vmap @%s", chain_family, NFTLB_TABLE_NAME, base_chain, print_nft_family(family), print_nft_family_protocol(family), print_nft_family(family), service);
				*base_rules |= NFTLB_PROTO_IP_ACTIVE;
			}
		}
	}

	return 0;
}

static int run_farm_rules_gen_srv_data(char **buf, struct farm *f, struct backend *b, char *chain, enum map_modes data_mode)
{
	switch (data_mode) {
	case BCK_MAP_SRCIPADDR:
		if (f != NULL)
			sprintf(*buf, ": %s ", f->srcaddr);
		break;
	case BCK_MAP_NAME:
		sprintf(*buf, ": goto %s ", chain);
		break;
	case BCK_MAP_BCK_BF_SRCIPADDR:
		if (b->srcaddr && b->srcaddr != DEFAULT_SRCADDR && strcmp(b->srcaddr, "") != 0)
			sprintf(*buf, ": %s ", b->srcaddr);
		else if (f != NULL && f->srcaddr != DEFAULT_SRCADDR && strcmp(f->srcaddr, "") != 0)
			sprintf(*buf, ": %s ", f->srcaddr);
		break;
	default:
		break;
	}

	return 0;
}

static int run_farm_rules_gen_srv_map(struct sbuffer *buf, struct farm *f, int family, int type, int proto, int action, enum map_modes key_mode, enum map_modes data_mode)
{
	int port_list[65535] = { 0 };
	char action_str[255] = { 0 };
	char key_str[255] = { 0 };
	char *data_str = NULL;
	char chain[255] = { 0 };
	char service[255] = { 0 };
	char protocol[10] = { 0 };
	char *nft_family = print_nft_table_family(family, type);
	struct backend *b;
	int nports;
	int i;
	int bckmark;
	int output = 0;

	data_str = calloc(1, 255);
	if (!data_str) {
		tools_printlog(LOG_ERR, "%s():%d: memory allocation error", __FUNCTION__, __LINE__);
		return -1;
	}

	switch (action) {
	case ACTION_RELOAD:
	case ACTION_START:
		sprintf(action_str, "add");
		break;
	case ACTION_STOP:
	case ACTION_DELETE:
		sprintf(action_str, "delete");
		break;
	default:
		return -1;
		break;
	}

	get_farm_chain(chain, f, type);
	get_farm_service(service, f, type, f->family, BCK_MAP_NONE);
	sprintf(protocol, "%s", print_nft_protocol(proto));

	switch (key_mode) {
	case BCK_MAP_IPADDR:
		run_farm_rules_gen_srv_data((char **) &data_str, f, NULL, chain, data_mode);
		concat_exec_cmd(buf, " ; %s element %s %s %s { %s %s}", action_str, nft_family, NFTLB_TABLE_NAME, service, f->virtaddr, data_str);
		output++;
		break;
	case BCK_MAP_PROTO_IPADDR:
		run_farm_rules_gen_srv_data((char **) &data_str, f, NULL, chain, data_mode);
		concat_exec_cmd(buf, " ; %s element %s %s %s { %s . %s %s}", action_str, nft_family, NFTLB_TABLE_NAME, service, protocol, f->virtaddr, data_str);
		output++;
		break;
	case BCK_MAP_IPADDR_PORT:
		run_farm_rules_gen_srv_data((char **) &data_str, f, NULL, chain, data_mode);
		nports = get_array_ports(port_list, f);
		for (i = 0; i < nports; i++) {
			if (i)
				concat_buf(buf, ", %s . %d %s", f->virtaddr, port_list[i], data_str);
			else
				concat_buf(buf, " ; %s element %s %s %s { %s . %d %s", action_str, nft_family, NFTLB_TABLE_NAME, service, f->virtaddr, port_list[i], data_str);
			output++;
		}
		if (i)
			concat_exec_cmd(buf, " }");
		break;
	case BCK_MAP_PROTO_IPADDR_PORT:
		run_farm_rules_gen_srv_data((char **) &data_str, f, NULL, chain, data_mode);
		nports = get_array_ports(port_list, f);
		for (i = 0; i < nports; i++) {
			if (i)
				concat_buf(buf, ", %s . %s . %d %s", protocol, f->virtaddr, port_list[i], data_str);
			else
				concat_buf(buf, " ; %s element %s %s %s { %s . %s . %d %s", action_str, nft_family, NFTLB_TABLE_NAME, service, protocol, f->virtaddr, port_list[i], data_str);
			output++;
		}
		if (i)
			concat_exec_cmd(buf, " }");
		break;
	case BCK_MAP_PROTO_PORT:
		run_farm_rules_gen_srv_data((char **) &data_str, f, NULL, chain, data_mode);
		nports = get_array_ports(port_list, f);
		for (i = 0; i < nports; i++) {
			if (i)
				concat_buf(buf, ", %s . %d %s", protocol, port_list[i], data_str);
			else
				concat_buf(buf, " ; %s element %s %s %s { %s . %d %s", action_str, nft_family, NFTLB_TABLE_NAME, service, protocol, port_list[i], data_str);
			output++;
		}
		if (i)
			concat_exec_cmd(buf, " }");
		break;
	default:
		nports = (f->protocol == VALUE_PROTO_ALL) ? 1 : get_array_ports(port_list, f);

		for (i = 0; i < nports; i++) {
			list_for_each_entry(b, &f->backends, list) {
				if (!backend_validate(b))
					continue;

				get_farm_service(service, f, type, f->family, key_mode);

				bckmark = backend_get_mark(b);
				if (type == NFTLB_F_CHAIN_POS_SNAT && bckmark & masquerade_mark)
					continue;

				if ((key_mode == BCK_MAP_BCK_ID || key_mode == BCK_MAP_BCK_MARK) && bckmark != DEFAULT_MARK) {
					if (i > 0) continue;
					sprintf(key_str, "0x%x", bckmark);
				} else if ((key_mode == BCK_MAP_BCK_ID || key_mode == BCK_MAP_BCK_PROTO_IPADDR_F_PORT) && backend_no_port(b)) {
					sprintf(key_str, "%s . %s . %d", protocol, b->ipaddr, port_list[i]);
				} else if ((key_mode == BCK_MAP_BCK_ID || key_mode == BCK_MAP_BCK_PROTO_IPADDR_F_PORT) && !backend_no_port(b)) {
					sprintf(key_str, "%s . %s . %s", protocol, b->ipaddr, b->port);
				} else if ((key_mode == BCK_MAP_BCK_ID || key_mode == BCK_MAP_BCK_IPADDR_F_PORT) && backend_no_port(b)) {
					sprintf(key_str, "%s . %d", b->ipaddr, port_list[i]);
				} else if (key_mode == BCK_MAP_BCK_ID && !backend_no_port(b)) {
					if (i > 0) continue;
					sprintf(key_str, "%s . %s", b->ipaddr, b->port);
				} else if (key_mode == BCK_MAP_BCK_ID || key_mode == BCK_MAP_BCK_IPADDR) {
					if (i > 0) continue;
					sprintf(key_str, "%s", b->ipaddr);
				} else
					continue;

				run_farm_rules_gen_srv_data((char **) &data_str, f, b, chain, data_mode);

				if (b->action == ACTION_STOP || b->action == ACTION_DELETE || b->action == ACTION_RELOAD) {
					if (action == ACTION_START)
						continue;
					concat_exec_cmd(buf, " ; delete element %s %s %s { %s }", nft_family, NFTLB_TABLE_NAME, service, key_str);
					output++;
				}

				if(!backend_is_usable(b))
					continue;

				run_farm_rules_gen_srv_data((char **) &data_str, f, b, chain, data_mode);
				concat_exec_cmd(buf, " ; %s element %s %s %s { %s %s}", action_str, nft_family, NFTLB_TABLE_NAME, service, key_str, data_str);
				output++;
			}
		}
		break;
	}

	if (data_str)
		free(data_str);

	return output;
}

static void run_farm_rules_gen_srv_map_by_type(struct sbuffer *buf, struct farm *f, int type, int family, int protocol, int action)
{
	int elements = 0;

	if (type & NFTLB_F_CHAIN_ING_FILTER)
		elements = run_farm_rules_gen_srv_map(buf, f, family, type, protocol, action, farm_no_port(f) ? BCK_MAP_PROTO_IPADDR : (farm_no_virtaddr(f) ? BCK_MAP_PROTO_PORT : BCK_MAP_PROTO_IPADDR_PORT), BCK_MAP_NAME);
	else if (type & NFTLB_F_CHAIN_PRE_FILTER)
		elements = run_farm_rules_gen_srv_map(buf, f, family, type, protocol, action, farm_no_port(f) ? BCK_MAP_PROTO_IPADDR : (farm_no_virtaddr(f) ? BCK_MAP_PROTO_PORT : BCK_MAP_PROTO_IPADDR_PORT), BCK_MAP_NAME);
	else if (type & NFTLB_F_CHAIN_PRE_DNAT)
		elements = run_farm_rules_gen_srv_map(buf, f, family, type, protocol, action, farm_no_port(f) ? BCK_MAP_PROTO_IPADDR : (farm_no_virtaddr(f) ? BCK_MAP_PROTO_PORT : BCK_MAP_PROTO_IPADDR_PORT), BCK_MAP_NAME);
	else if (type & NFTLB_F_CHAIN_FWD_FILTER)
		elements = run_farm_rules_gen_srv_map(buf, f, family, type, protocol, action, BCK_MAP_BCK_MARK, BCK_MAP_NAME);
	else if (type & NFTLB_F_CHAIN_POS_SNAT)
		elements = run_farm_rules_gen_srv_map(buf, f, family, type, protocol, action, BCK_MAP_BCK_ID, BCK_MAP_BCK_BF_SRCIPADDR);
	else if (type & NFTLB_F_CHAIN_ING_DNAT)
		elements = run_farm_rules_gen_srv_map(buf, f, family, type, protocol, action, farm_no_port(f) ? BCK_MAP_PROTO_IPADDR : (farm_no_virtaddr(f) ? BCK_MAP_PROTO_PORT : BCK_MAP_BCK_PROTO_IPADDR_F_PORT), BCK_MAP_NAME);
	else if (type & NFTLB_F_CHAIN_OUT_FILTER)
		elements = run_farm_rules_gen_srv_map(buf, f, family, type, protocol, action, farm_no_port(f) ? BCK_MAP_PROTO_IPADDR : (farm_no_virtaddr(f) ? BCK_MAP_PROTO_PORT : BCK_MAP_PROTO_IPADDR_PORT), BCK_MAP_NAME);
	else if (type & NFTLB_F_CHAIN_OUT_DNAT)
		elements = run_farm_rules_gen_srv_map(buf, f, family, type, protocol, action, farm_no_port(f) ? BCK_MAP_PROTO_IPADDR : (farm_no_virtaddr(f) ? BCK_MAP_PROTO_PORT : BCK_MAP_PROTO_IPADDR_PORT), BCK_MAP_NAME);
	else
		return;

	if (elements)
		update_service_counters(f, type, family, elements, action);
}

static void run_farm_rules_gen_srv_map_by_protocol(struct sbuffer *buf, struct farm *f, int type, int family, int action)
{
	if (f->protocol != VALUE_PROTO_ALL || (type & NFTLB_F_CHAIN_FWD_FILTER) || (type & NFTLB_F_CHAIN_POS_SNAT)) {
		run_farm_rules_gen_srv_map_by_type(buf, f, type, family, f->protocol, action);
	} else {
		run_farm_rules_gen_srv_map_by_type(buf, f, type, family, VALUE_PROTO_TCP, action);
		run_farm_rules_gen_srv_map_by_type(buf, f, type, family, VALUE_PROTO_UDP, action);
		run_farm_rules_gen_srv_map_by_type(buf, f, type, family, VALUE_PROTO_SCTP, action);
	}
}

static void run_farm_rules_gen_vsrv(struct sbuffer *buf, struct farm *f, int type, int family, int action)
{
	int smart_action = action;

	switch (action) {
	case ACTION_RELOAD:
		if ((f->nft_chains & type) == 0)
			smart_action = ACTION_START;
		break;
	case ACTION_START:
		if (f->nft_chains & type)
			smart_action = ACTION_RELOAD;
		break;
	case ACTION_DELETE:
	case ACTION_STOP:
		if ((f->nft_chains & type) == 0)
			smart_action = ACTION_NONE;
		break;
	default:
		break;
	}

	switch (smart_action) {
	case ACTION_RELOAD:
		run_farm_rules_gen_chain(buf, f, print_nft_table_family(family, type), type, smart_action);
		break;
	case ACTION_START:
		run_farm_rules_gen_chain(buf, f, print_nft_table_family(family, type), type, smart_action);
		run_farm_rules_gen_srv_map_by_protocol(buf, f, type, family, action);
		break;
	case ACTION_DELETE:
	case ACTION_STOP:
		run_farm_rules_gen_srv_map_by_protocol(buf, f, type, family, action);
		run_farm_rules_gen_chain(buf, f, print_nft_table_family(family, type), type, smart_action);
		break;
	default:
		break;
	}

	if (action == ACTION_START || action == ACTION_RELOAD)
		f->nft_chains |= type;
	else if (action == ACTION_STOP || action == ACTION_DELETE)
		f->nft_chains &= ~type;

	return;
}

static int run_farm_snat(struct sbuffer *buf, struct farm *f, int family, int action)
{
	if (f->mode != VALUE_MODE_SNAT)
		return 0;

	run_farm_rules_gen_srv_map_by_protocol(buf, f, NFTLB_F_CHAIN_POS_SNAT, family, action);

	return 0;
}

static int run_farm_stlsnat(struct sbuffer *buf, struct farm *f, int family, int action)
{
	char chain[255] = { 0 };
	char map_str[255] = { 0 };

	if (f->mode != VALUE_MODE_STLSDNAT)
		return 1;

	sprintf(map_str, "map-%s-back", f->name);

	get_farm_chain(chain, f, NFTLB_F_CHAIN_ING_DNAT);

	switch (action) {
	case ACTION_DELETE:
	case ACTION_STOP:
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_ING_DNAT, family, action);
		run_farm_map(buf, f, family, NFTLB_F_CHAIN_ING_DNAT, map_str, VALUE_META_SRCIP, VALUE_META_SRCMAC, f->persistttl, action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_ING_DNAT, family, get_rules_needed(f), action);
		break;
	default:
		run_base_chain(buf, f, NFTLB_F_CHAIN_ING_DNAT, family, get_rules_needed(f), action);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_ING_DNAT, family, action);
		run_farm_map(buf, f, family, NFTLB_F_CHAIN_ING_DNAT, map_str, VALUE_META_SRCIP, VALUE_META_SRCMAC, f->persistttl, action);
		concat_exec_cmd(buf, " ; add rule %s %s %s %s saddr set %s ether saddr set %s ether daddr set %s daddr map @%s fwd to %s", print_nft_table_family(family, NFTLB_F_CHAIN_ING_DNAT), NFTLB_TABLE_NAME, chain, print_nft_family(family), f->virtaddr, f->iethaddr, print_nft_family(family), map_str, f->iface);
		break;
	}

	return 0;
}

static int run_farm_rules_gen_sched(struct sbuffer *buf, struct farm *f, int family)
{
	switch (f->scheduler) {
	case VALUE_SCHED_RR:
		concat_buf(buf, " numgen inc mod %d", f->total_weight);
		break;
	case VALUE_SCHED_WEIGHT:
		concat_buf(buf, " numgen random mod %d", f->total_weight);
		break;
	case VALUE_SCHED_HASH:
		concat_buf(buf, " jhash");
		run_farm_rules_gen_meta_param(buf, f, family, f->schedparam, NFTLB_MAP_KEY_RULE);
		concat_buf(buf, " mod %d", f->total_weight);
		break;
	case VALUE_SCHED_SYMHASH:
		if (f->bcks_available != 1)	// FIXME: Control bug in nftables
			concat_buf(buf, " symhash mod %d", f->total_weight);
		break;
	default:
		return -1;
	}

	return 0;
}

static int run_farm_rules_gen_bck_map(struct sbuffer *buf, struct farm *f, enum map_modes key_mode, enum map_modes data_mode, int usable)
{
	struct backend *b;
	int i = 0;
	int last = 0;
	int new;

	concat_buf(buf, " map {");

	list_for_each_entry(b, &f->backends, list) {
		if (usable == NFTLB_CHECK_USABLE && !backend_is_usable(b))
			continue;
		if (usable == NFTLB_CHECK_AVAIL && !backend_is_available(b))
			continue;
		if (data_mode == BCK_MAP_PORT && backend_no_port(b))
			continue;

		if (i != 0)
			concat_buf(buf, ",");

		switch (key_mode) {
		case BCK_MAP_MARK:
			concat_buf(buf, " 0x%x", backend_get_mark(b));
			break;
		case BCK_MAP_IPADDR:
			concat_buf(buf, " %s", b->ipaddr);
			break;
		case BCK_MAP_WEIGHT:
			new = last + b->weight - 1;
			concat_buf(buf, " %d", last);
			if (new != last)
				concat_buf(buf, "-%d", new);
			last = new + 1;
			break;
		case BCK_MAP_ETHADDR:
			concat_buf(buf, " %s", b->ethaddr);
			break;
		default:
			break;
		}

		concat_buf(buf, ":");

		switch (data_mode) {
		case BCK_MAP_MARK:
			concat_buf(buf, " 0x%x", backend_get_mark(b));
			break;
		case BCK_MAP_ETHADDR:
			concat_buf(buf, " %s", b->ethaddr);
			break;
		case BCK_MAP_IPADDR_PORT:
			if (backend_no_port(b))
				concat_buf(buf, " %s . 0", b->ipaddr);
			else
				concat_buf(buf, " %s . %s", b->ipaddr, b->port);
			break;
		case BCK_MAP_PORT:
			concat_buf(buf, " %s", b->port);
			break;
		case BCK_MAP_IPADDR:
			concat_buf(buf, " %s", b->ipaddr);
			break;
		case BCK_MAP_OFACE:
			if (b->oface)
				concat_buf(buf, " %s", b->oface);
			else
				concat_buf(buf, " %s", f->oface);
			break;
		default:
			break;
		}

		i++;
	}

	concat_buf(buf, " }");

	if (i == 0)
		return -1;

	return 0;
}

static void run_farm_helper(struct sbuffer *buf, struct farm *f, int family, int action, char *protocol)
{
	switch (action) {
	case ACTION_START:
		concat_exec_cmd(buf, " ; add ct helper %s %s %s-%s { type \"%s\" protocol %s ; } ;", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, obj_print_helper(f->helper), protocol, obj_print_helper(f->helper), protocol);
		break;
	case ACTION_DELETE:
	case ACTION_STOP:
		concat_exec_cmd(buf, " ; delete ct helper %s %s %s-%s ; ", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, obj_print_helper(f->helper), protocol);
		break;
	case ACTION_RELOAD:
	default:
		break;
	}
}

static int run_farm_log_prefix(struct sbuffer *buf, struct farm *f, int key, int type, int action)
{
	char logprefix_str[255] = { 0 };

	if (f->log == VALUE_LOG_NONE)
		return 0;

	if (f->log & key) {
		print_log_format(logprefix_str, KEY_LOGPREFIX, type, f, NULL, NULL);
		concat_buf(buf, " log prefix \"%s\"", logprefix_str);
	}

	return 0;
}

static int run_farm_gen_log_rules(struct sbuffer *buf, struct farm *f, int family, char * chain, int key, int type, int action)
{
	if (f->log == VALUE_LOG_NONE)
		return 0;

	if (f->log & key) {
		concat_buf(buf, " ; add rule %s %s %s", print_nft_table_family(family, type), NFTLB_TABLE_NAME, chain);
		run_farm_log_prefix(buf,f, key, type, action);
		concat_exec_cmd(buf, "");
	}

	return 0;
}

static int run_farm_rules_filter_helper(struct sbuffer *buf, struct farm *f, int family, char *chain, int action)
{
	char protocol[255] = {0};

	if (!(f->helper != DEFAULT_HELPER && (f->mode == VALUE_MODE_SNAT || f->mode == VALUE_MODE_DNAT)))
		return 0;

	if (f->protocol == VALUE_PROTO_TCP || f->protocol == VALUE_PROTO_ALL) {
		sprintf(protocol, "tcp");
		run_farm_helper(buf, f, family, action, protocol);
		if (action == ACTION_START || action == ACTION_RELOAD)
			concat_exec_cmd(buf, " ; add rule %s %s %s %s %s %s ct helper set %s-%s", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), print_nft_family_protocol(family), protocol, obj_print_helper(f->helper), protocol);
	}

	if (f->protocol == VALUE_PROTO_UDP || f->protocol == VALUE_PROTO_ALL) {
		sprintf(protocol, "udp");
		run_farm_helper(buf, f, family, action, protocol);
		if (action == ACTION_START || action == ACTION_RELOAD)
			concat_exec_cmd(buf, " ; add rule %s %s %s %s %s %s ct helper set %s-%s", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), print_nft_family_protocol(family), protocol, obj_print_helper(f->helper), protocol);
	}

	return 0;
}

static int run_farm_sessions_map(struct sbuffer *buf, struct farm *f, int stype, int family, int action)
{
	char map_str[255] = { 0 };
	int ttl = 0;

	if (f->persistence == VALUE_META_NONE)
		return 0;

	if (stype == SESSION_TYPE_STATIC)
		sprintf(map_str, "static-sessions-%s", f->name);
	else {
		sprintf(map_str, "persist-%s", f->name);
		ttl = f->persistttl;
	}

	if (f->mode == VALUE_MODE_DSR)
		run_farm_map(buf, f, family, NFTLB_F_CHAIN_ING_FILTER, map_str, f->persistence, VALUE_META_DSTMAC, ttl, action);
	else if (f->mode == VALUE_MODE_STLSDNAT)
		run_farm_map(buf, f, family, NFTLB_F_CHAIN_ING_FILTER, map_str, f->persistence, VALUE_META_DSTIP, ttl, action);
	else
		run_farm_map(buf, f, family, NFTLB_F_CHAIN_PRE_FILTER, map_str, f->persistence, VALUE_META_MARK, ttl, action);

	return 0;
}

static int run_farm_manage_sessions(struct sbuffer *buf, struct farm *f, int stype, int family, int action)
{
	char chain[255] = { 0 };
	char map_str[255] = { 0 };
	char *client;
	struct session *s;
	struct list_head *sessions;

	if (f->persistence == VALUE_META_NONE)
		return 0;

	if (f->bcks_usable == 0)
		return 0;

	if ((action != ACTION_START && action != ACTION_RELOAD))
		return 0;

	get_farm_chain(chain, f, NFTLB_F_CHAIN_ING_FILTER);

	if (stype == SESSION_TYPE_STATIC) {
		sprintf(map_str, "static-sessions-%s", f->name);
		sessions = &f->static_sessions;
	} else {
		sprintf(map_str, "persist-%s", f->name);
		sessions = &f->timed_sessions;
	}

	list_for_each_entry(s, sessions, list) {
		client = (char *) malloc(255);
		if (!client) {
			tools_printlog(LOG_ERR, "%s():%d: unable to allocate parsed client %s for farm %s", __FUNCTION__, __LINE__, s->client, f->name);
			continue;
		}

		if (session_get_client(s, &client)) {

			if (f->mode == VALUE_MODE_DSR) {
				if ((action == ACTION_START || s->action == ACTION_START) && s->bck && s->bck->ethaddr != DEFAULT_ETHADDR)
					concat_exec_cmd(buf, " ; add element %s %s %s { %s : %s }", print_nft_table_family(family, get_stage_by_farm_mode(f)), NFTLB_TABLE_NAME, map_str, client, s->bck->ethaddr);
			} else if(f->mode == VALUE_MODE_STLSDNAT) {
				if ((action == ACTION_START || s->action == ACTION_START) && s->bck && s->bck->ipaddr != DEFAULT_IPADDR)
					concat_exec_cmd(buf, " ; add element %s %s %s { %s : %s }", print_nft_table_family(family, get_stage_by_farm_mode(f)), NFTLB_TABLE_NAME, map_str, client, s->bck->ipaddr);
			} else {
				if ((action == ACTION_START || s->action == ACTION_START) && s->bck && s->bck->mark != DEFAULT_MARK && backend_is_available(s->bck))
					concat_exec_cmd(buf, " ; add element %s %s %s { %s : 0x%x }", print_nft_table_family(family, get_stage_by_farm_mode(f)), NFTLB_TABLE_NAME, map_str, client, backend_get_mark(s->bck));
			}

			if (action == ACTION_RELOAD && (s->action == ACTION_STOP || s->action == ACTION_DELETE))
				concat_exec_cmd(buf, " ; delete element %s %s %s { %s }", print_nft_table_family(family, get_stage_by_farm_mode(f)), NFTLB_TABLE_NAME, map_str, client);
			free(client);
		}
		s->action = ACTION_NONE;
	}

	return 0;
}

static int run_farm_rules_check_sessions(struct sbuffer *buf, struct farm *f, int stype, int family, int type, int action)
{
	char map_str[255] = { 0 };
	char chain[255] = { 0 };

	if (f->persistence == VALUE_META_NONE)
		return 0;

	if (action != ACTION_START && action != ACTION_RELOAD)
		return 0;

	if (stype == SESSION_TYPE_STATIC)
		sprintf(map_str, "static-sessions-%s", f->name);
	else
		sprintf(map_str, "persist-%s", f->name);

	get_farm_chain(chain, f, type);

	switch (f->mode) {
	case VALUE_MODE_DSR:
		get_farm_chain(chain, f, NFTLB_F_CHAIN_ING_FILTER);
		concat_buf(buf, " ; add rule %s %s %s ether daddr set", print_nft_table_family(family, NFTLB_F_CHAIN_ING_FILTER), NFTLB_TABLE_NAME, chain);
		run_farm_rules_gen_meta_param(buf, f, family, f->persistence, NFTLB_MAP_KEY_RULE);
		concat_exec_cmd(buf, " map @%s ether saddr set %s", map_str, f->iethaddr);
		concat_buf(buf, " fwd to");
		if (f->bcks_have_if) {
			concat_buf(buf, " ether daddr");
			run_farm_rules_gen_bck_map(buf, f, BCK_MAP_ETHADDR, BCK_MAP_OFACE, NFTLB_CHECK_AVAIL);
		} else
			concat_buf(buf, " %s", f->oface);
		concat_exec_cmd(buf, "");
		break;

	case VALUE_MODE_STLSDNAT:
		get_farm_chain(chain, f, NFTLB_F_CHAIN_ING_FILTER);
		concat_buf(buf, " ; add rule %s %s %s %s daddr set", print_nft_table_family(family, NFTLB_F_CHAIN_ING_FILTER), NFTLB_TABLE_NAME, chain, print_nft_family(family));
		run_farm_rules_gen_meta_param(buf, f, family, f->persistence, NFTLB_MAP_KEY_RULE);
		concat_exec_cmd(buf, " map @%s ether daddr set %s daddr", map_str, print_nft_family(family));
		run_farm_rules_gen_bck_map(buf, f, BCK_MAP_IPADDR, BCK_MAP_ETHADDR, NFTLB_CHECK_AVAIL);
		concat_buf(buf, " fwd to");
		if (f->bcks_have_if) {
			concat_buf(buf, " ether daddr");
			run_farm_rules_gen_bck_map(buf, f, BCK_MAP_ETHADDR, BCK_MAP_OFACE, NFTLB_CHECK_AVAIL);
		} else
			concat_buf(buf, " %s", f->oface);
		concat_exec_cmd(buf, "");
		break;

	default:
		get_farm_chain(chain, f, NFTLB_F_CHAIN_PRE_FILTER);
		if (stype == SESSION_TYPE_STATIC) {
			concat_buf(buf, " ; add rule %s %s %s ct mark set", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain);
			run_farm_rules_gen_meta_param(buf, f, family, f->persistence, NFTLB_MAP_KEY_RULE);
			concat_exec_cmd(buf, " map @%s accept", map_str);
		} else {
			concat_buf(buf, " ; add rule %s %s %s ct state new ct mark set", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain);
			run_farm_rules_gen_meta_param(buf, f, family, f->persistence, NFTLB_MAP_KEY_RULE);
			concat_exec_cmd(buf, " map @%s", map_str);
		}
		break;
	}

	return 0;
}

static int run_farm_rules_update_sessions(struct sbuffer *buf, struct farm *f, int family, char *chain, int action)
{
	char map_str[255] = { 0 };

	if (f->persistence == VALUE_META_NONE)
		return 0;

	sprintf(map_str, "persist-%s", f->name);

	if (action != ACTION_START && action != ACTION_RELOAD)
		return 0;

	switch (f->mode) {
	case VALUE_MODE_DSR:
		concat_buf(buf, " update @%s { ",  map_str);
		run_farm_rules_gen_meta_param(buf, f, family, f->persistence, NFTLB_MAP_KEY_RULE);
		concat_exec_cmd(buf, " : ");
		run_farm_rules_gen_meta_param(buf, f, family, VALUE_META_DSTMAC, NFTLB_MAP_KEY_RULE);
		concat_exec_cmd(buf, " }");
		break;
	case VALUE_MODE_STLSDNAT:
		concat_buf(buf, " update @%s { ",  map_str);
		run_farm_rules_gen_meta_param(buf, f, family, f->persistence, NFTLB_MAP_KEY_RULE);
		concat_exec_cmd(buf, " : ");
		run_farm_rules_gen_meta_param(buf, f, family, VALUE_META_DSTIP, NFTLB_MAP_KEY_RULE);
		concat_exec_cmd(buf, " }");
		break;
	default:
		concat_buf(buf, " ; add rule %s %s %s ct mark != 0x00000000 update @%s { ", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, map_str);
		run_farm_rules_gen_meta_param(buf, f, family, f->persistence, NFTLB_MAP_KEY_RULE);
		concat_exec_cmd(buf, " : ct mark }");
		break;
	}

	return 0;
}

static void run_farm_meter(struct sbuffer *buf, struct farm *f, int family, char *name, int action)
{
	switch (action) {
	case ACTION_START:
		concat_exec_cmd(buf, " ; add set %s %s %s { type %s ; flags dynamic ; } ;", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, name, print_nft_family_type(family));
		break;
	case ACTION_STOP:
		concat_exec_cmd(buf, " ; delete set %s %s %s ; ", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, name);
		break;
	default:
		break;
	}
}

static int run_farm_rules_filter_policies(struct sbuffer *buf, struct farm *f, int family, char *chain, int action)
{
	char logprefix_str[255] = { 0 };
	char meter_str[255] = { 0 };
	char burst_str[255] = { 0 };

	if ((action == ACTION_START || action == ACTION_RELOAD) && f->tcpstrict == VALUE_SWITCH_ON) {
		print_log_format(logprefix_str, KEY_TCPSTRICT_LOGPREFIX, NFTLB_F_CHAIN_PRE_FILTER, f, NULL, NULL);
		concat_exec_cmd(buf, " ; add rule %s %s %s ct state invalid log prefix \"%s\" drop",
						print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, logprefix_str);
	}

	sprintf(meter_str, "%s-%s", CONFIG_KEY_NEWRTLIMIT, f->name);
	print_log_format(logprefix_str, KEY_NEWRTLIMIT_LOGPREFIX, NFTLB_F_CHAIN_PRE_FILTER, f, NULL, NULL);
	if ((action == ACTION_START && f->newrtlimit != DEFAULT_NEWRTLIMIT) || (f->reload_action & VALUE_RLD_NEWRTLIMIT_START))
		run_farm_meter(buf, f, family, meter_str, ACTION_START);
	if (((action == ACTION_STOP || action == ACTION_DELETE) && f->newrtlimit != DEFAULT_NEWRTLIMIT) || (f->reload_action & VALUE_RLD_NEWRTLIMIT_STOP))
		run_farm_meter(buf, f, family, meter_str, ACTION_STOP);
	if ((action == ACTION_START || action == ACTION_RELOAD) && f->newrtlimit != DEFAULT_NEWRTLIMIT) {
		if (f->newrtlimitbst != DEFAULT_RTLIMITBURST)
			sprintf(burst_str, "burst %d packets ", f->newrtlimitbst);
		concat_exec_cmd(buf, " ; add rule %s %s %s ct state new add @%s { ip saddr limit rate over %d/second %s} log prefix \"%s\" drop",
						print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, meter_str, f->newrtlimit, burst_str, logprefix_str);
	}

	sprintf(meter_str, "%s-%s", CONFIG_KEY_RSTRTLIMIT, f->name);
	print_log_format(logprefix_str, KEY_RSTRTLIMIT_LOGPREFIX, NFTLB_F_CHAIN_PRE_FILTER, f, NULL, NULL);
	if ((action == ACTION_START && f->rstrtlimit != DEFAULT_RSTRTLIMIT) || (f->reload_action & VALUE_RLD_RSTRTLIMIT_START))
		run_farm_meter(buf, f, family, meter_str, ACTION_START);
	if (((action == ACTION_STOP || action == ACTION_DELETE) && f->rstrtlimit != DEFAULT_RSTRTLIMIT) || (f->reload_action & VALUE_RLD_RSTRTLIMIT_STOP))
		run_farm_meter(buf, f, family, meter_str, ACTION_STOP);
	if ((action == ACTION_START || action == ACTION_RELOAD) && f->rstrtlimit != DEFAULT_RSTRTLIMIT) {
		if (f->rstrtlimitbst != DEFAULT_RTLIMITBURST)
			sprintf(burst_str, "burst %d packets ", f->rstrtlimitbst);
		concat_exec_cmd(buf, " ; add rule %s %s %s tcp flags rst add @%s { ip saddr limit rate over %d/second %s} log prefix \"%s\" drop",
						print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, meter_str, f->rstrtlimit, burst_str, logprefix_str);
	}

	sprintf(meter_str, "%s-%s", CONFIG_KEY_ESTCONNLIMIT, f->name);
	print_log_format(logprefix_str, KEY_ESTCONNLIMIT_LOGPREFIX, NFTLB_F_CHAIN_PRE_FILTER, f, NULL, NULL);
	if ((action == ACTION_START && f->estconnlimit != DEFAULT_ESTCONNLIMIT) || (f->reload_action & VALUE_RLD_ESTCONNLIMIT_START))
		run_farm_meter(buf, f, family, meter_str, ACTION_START);
	if (((action == ACTION_STOP || action == ACTION_DELETE) && f->estconnlimit != DEFAULT_ESTCONNLIMIT) || (f->reload_action & VALUE_RLD_ESTCONNLIMIT_STOP))
		run_farm_meter(buf, f, family, meter_str, ACTION_STOP);
	if ((action == ACTION_START || action == ACTION_RELOAD) && f->estconnlimit != DEFAULT_ESTCONNLIMIT)
		concat_exec_cmd(buf, " ; add rule %s %s %s ct state new add @%s { ip saddr ct count over %d } log prefix \"%s\" drop",
						print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, meter_str, f->estconnlimit, logprefix_str);

	if ((action == ACTION_START || action == ACTION_RELOAD) && f->queue != DEFAULT_QUEUE)
		concat_exec_cmd(buf, " ; add rule %s %s %s tcp flags syn queue num %d bypass",
						print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, f->queue);
	return 0;
}

static int run_farm_rules_gen_limits_per_bck(struct sbuffer *buf, struct farm *f, int family, char *chain, int action)
{
	struct backend *b;
	char logprefix_str[255] = { 0 };

	list_for_each_entry(b, &f->backends, list) {
		if (b->estconnlimit == 0)
			continue;

		print_log_format(logprefix_str, KEY_ESTCONNLIMIT_LOGPREFIX, NFTLB_F_CHAIN_PRE_FILTER, f, b, NULL);

		if ((b->action == ACTION_STOP && !backend_is_usable(b)) || (action == ACTION_STOP || action == ACTION_DELETE))
			continue;

		concat_exec_cmd(buf, " ; add rule %s %s %s ct mark 0x%x ct count over %d log prefix \"%s\" drop",
						print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, backend_get_mark(b), b->estconnlimit, logprefix_str);
	}

	return 0;
}

static int run_farm_rules_filter_marks(struct sbuffer *buf, struct farm *f, int family, char *chain, int action)
{
	int mark = farm_get_mark(f);

	if (mark == DEFAULT_MARK)
		return 0;

	if (action == ACTION_START || action == ACTION_RELOAD) {
		if (f->bcks_available != 0) {
			concat_buf(buf, " ; add rule %s %s %s ct state new ct mark 0x0 ct mark set", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain);
			if (run_farm_rules_gen_sched(buf, f, family) == -1)
				return -1;
			run_farm_rules_gen_bck_map(buf, f, BCK_MAP_WEIGHT, BCK_MAP_MARK, NFTLB_CHECK_AVAIL);
			run_farm_rules_gen_limits_per_bck(buf, f, family, chain, action);
		} else if (mark != DEFAULT_MARK) {
			concat_buf(buf, " ; add rule %s %s %s ct state new ct mark 0x0 ct mark set 0x%x", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_FILTER), NFTLB_TABLE_NAME, chain, mark);
		}
	} else if (action == ACTION_STOP || action == ACTION_DELETE || (action == ACTION_RELOAD && f->bcks_usable == 0)) {
		run_farm_rules_gen_limits_per_bck(buf, f, family, chain, action);
	}
	concat_exec_cmd(buf, "");

	return 0;
}

static int run_farm_rules_filter(struct sbuffer *buf, struct farm *f, int family, int action)
{
	char chain[255] = { 0 };
	int need = need_filter(f);

	if (!need && f->reload_action == VALUE_RLD_NONE)
		return 0;

	get_farm_chain(chain, f, NFTLB_F_CHAIN_PRE_FILTER);

	if (action == ACTION_RELOAD && need && (STATEFUL_RLD_START(f->reload_action)))
		action = ACTION_START;

	if (action == ACTION_RELOAD && !need && (STATEFUL_RLD_STOP(f->reload_action)))
		action = ACTION_STOP;

	switch (action) {
	case ACTION_START:
	case ACTION_RELOAD:
		run_base_table(buf, NFTLB_F_CHAIN_PRE_FILTER, family, ACTION_START);
		run_base_chain(buf, f, NFTLB_F_CHAIN_PRE_FILTER, family, get_rules_needed(f), ACTION_START);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_PRE_FILTER, family, action);
		run_farm_rules_filter_policies(buf, f, family, chain, action);
		run_farm_rules_filter_helper(buf, f, family, chain, action);

		run_farm_sessions_map(buf, f, SESSION_TYPE_STATIC, family, action);
		run_farm_sessions_map(buf, f, SESSION_TYPE_TIMED, family, action);
		run_farm_manage_sessions(buf, f, SESSION_TYPE_STATIC, family, action);
		run_farm_manage_sessions(buf, f, SESSION_TYPE_TIMED, family, action);
		run_farm_rules_check_sessions(buf, f, SESSION_TYPE_STATIC, family, NFTLB_F_CHAIN_PRE_FILTER, action);
		run_farm_rules_check_sessions(buf, f, SESSION_TYPE_TIMED, family, NFTLB_F_CHAIN_PRE_FILTER, action);

		run_farm_rules_filter_marks(buf, f, family, chain, action);
		run_farm_rules_update_sessions(buf, f, family, chain, action);
		break;
	case ACTION_DELETE:
	case ACTION_STOP:
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_PRE_FILTER, family, action);
		run_farm_sessions_map(buf, f, SESSION_TYPE_STATIC, family, action);
		run_farm_sessions_map(buf, f, SESSION_TYPE_TIMED, family, action);
		run_farm_rules_filter_marks(buf, f, family, chain, action);
		run_farm_rules_filter_helper(buf, f, family, chain, action);
		run_farm_rules_filter_policies(buf, f, family, chain, action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_PRE_FILTER, family, get_rules_needed(f), action);
		run_base_table(buf, NFTLB_F_CHAIN_PRE_FILTER, family, action);
		break;
	default:
		break;
	}

	return 0;
}

static int get_farm_interfaces(struct farm *f, char *list)
{
	struct backend *b;
	int number = 0;
	char *p = NULL;

	if (f->iface) {
		strcat(list, f->iface);
		number++;
	}

	if (f->oface && !(p = strstr(list, f->oface))) {
		if (number)
			strcat(list, ", ");
		strcat(list, f->oface);
		number++;
	}

	list_for_each_entry(b, &f->backends, list) {
		if (b->oface && !(p = strstr(list, b->oface))) {
			if (number)
				strcat(list, ", ");
			strcat(list, b->oface);
			number++;
		}
	}

	return number;
}

static void run_farm_flowtable(struct sbuffer *buf, struct farm *f, int family, char *name, int action)
{
	char interfaces[255] = { 0 };

	if (!farm_needs_flowtable(f) || !get_farm_interfaces(f, interfaces))
		return;

	switch (action) {
	case ACTION_START:
		concat_exec_cmd(buf, " ; add flowtable %s %s %s { hook %s priority %d ; devices = { %s } ; } ;", print_nft_table_family(family, NFTLB_F_CHAIN_FWD_FILTER), NFTLB_TABLE_NAME, name, NFTLB_HOOK_INGRESS, nftlb_flowtable_prio++, interfaces);
		break;
	case ACTION_STOP:
	case ACTION_DELETE:
		concat_exec_cmd(buf, " ; delete flowtable %s %s %s ; ", print_nft_table_family(family, NFTLB_F_CHAIN_FWD_FILTER), NFTLB_TABLE_NAME, name);
		nftlb_flowtable_prio--;
		break;
	default:
		break;
	}
	return;
}

static void run_farm_gen_flowtable_rules(struct sbuffer *buf, struct farm *f, int family, char *chain, char *name, int action)
{
	if (!farm_needs_flowtable(f) || !f->iface)
		return;

	concat_exec_cmd(buf, " ; add rule %s %s %s flow add @%s", print_nft_table_family(family, NFTLB_F_CHAIN_FWD_FILTER), NFTLB_TABLE_NAME, chain, name);
	return;
}

static int run_farm_rules_forward(struct sbuffer *buf, struct farm *f, int family, int action)
{
	char chain[255] = { 0 };
	char flowtable[255] = { 0 };

	if (!need_forward(f))
		return 0;

	get_farm_chain(chain, f, NFTLB_F_CHAIN_FWD_FILTER);
	get_flowtable_name(flowtable, f);

	switch (action) {
	case ACTION_START:
		run_base_chain(buf, f, NFTLB_F_CHAIN_FWD_FILTER, family, get_rules_needed(f), action);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_FWD_FILTER, family, action);
		run_farm_gen_log_rules(buf,f, family, chain, VALUE_LOG_FORWARD, NFTLB_F_CHAIN_FWD_FILTER, action);
		run_farm_flowtable(buf, f, family, flowtable, action);
		run_farm_gen_flowtable_rules(buf, f, family, chain, flowtable, action);
		break;
	case ACTION_RELOAD:
		run_base_table(buf, NFTLB_F_CHAIN_ING_FILTER, family, action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_FWD_FILTER, family, get_rules_needed(f), action);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_FWD_FILTER, family, action);
		run_farm_gen_log_rules(buf,f, family, chain, VALUE_LOG_FORWARD, NFTLB_F_CHAIN_FWD_FILTER, action);
		run_farm_gen_flowtable_rules(buf, f, family, chain, flowtable, action);
		break;
	case ACTION_DELETE:
	case ACTION_STOP:
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_FWD_FILTER, family, action);
		run_farm_flowtable(buf, f, family, flowtable, action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_FWD_FILTER, family, get_rules_needed(f), action);
		break;
	default:
		break;
	}

	return 0;
}

static int run_farm_rules_output(struct sbuffer *buf, struct farm *f, int family, int action)
{
	if (!need_output(f))
		return 0;

	switch (action) {
	case ACTION_START:
		run_base_chain(buf, f, NFTLB_F_CHAIN_OUT_FILTER, family, get_rules_needed(f), action);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_OUT_FILTER, family, action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_OUT_DNAT, family, get_rules_needed(f), action);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_OUT_DNAT, family, action);
		break;
	case ACTION_RELOAD:
		run_base_chain(buf, f, NFTLB_F_CHAIN_OUT_FILTER, family, get_rules_needed(f), action);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_OUT_FILTER, family, action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_OUT_DNAT, family, get_rules_needed(f), action);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_OUT_DNAT, family, action);
		break;
	case ACTION_DELETE:
	case ACTION_STOP:
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_OUT_FILTER, family, action);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_OUT_DNAT, family, action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_OUT_FILTER, family, get_rules_needed(f), action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_OUT_DNAT, family, get_rules_needed(f), action);
		break;
	default:
		break;
	}

	return 0;
}

static int run_farm_rules_ingress_policies(struct sbuffer *buf, struct farm *f, char *chain)
{
	struct farmpolicy *fp;
	char logprefix_str[255] = { 0 };

	if (f->policies_action != ACTION_START && f->policies_action != ACTION_RELOAD)
		return 0;

	list_for_each_entry(fp, &f->policies, list) {
		if (fp->policy->type != VALUE_TYPE_WHITE)
			continue;

		print_log_format(logprefix_str, KEY_LOGPREFIX, NFTLB_F_CHAIN_ING_FILTER, f, NULL, fp->policy);
		concat_exec_cmd(buf, " ; add rule %s %s %s %s saddr @%s log prefix \"%s\" %s",
						NFTLB_NETDEV_FAMILY_STR, NFTLB_TABLE_NAME, chain, print_nft_family(fp->policy->family), fp->policy->name, logprefix_str, print_nft_verdict(fp->policy->type));
		fp->action = ACTION_NONE;
	}

	list_for_each_entry(fp, &f->policies, list) {
		if (fp->policy->type != VALUE_TYPE_BLACK)
			continue;

		print_log_format(logprefix_str, KEY_LOGPREFIX, NFTLB_F_CHAIN_ING_FILTER, f, NULL, fp->policy);
		concat_exec_cmd(buf, " ; add rule %s %s %s %s saddr @%s log prefix \"%s\" %s",
						NFTLB_NETDEV_FAMILY_STR, NFTLB_TABLE_NAME, chain, print_nft_family(fp->policy->family), fp->policy->name, logprefix_str, print_nft_verdict(fp->policy->type));
		fp->action = ACTION_NONE;
	}

	return 0;
}

static int run_farm_ingress_policies(struct sbuffer *buf, struct farm *f, int family, int action)
{
	char chain[255] = { 0 };

	if (!farm_needs_policies(f))
		return 0;

	get_farm_chain(chain, f, NFTLB_F_CHAIN_ING_FILTER);

	if (f->policies_action == ACTION_START) {
		if (!farm_is_ingress_mode(f)) {
			run_base_table(buf, NFTLB_F_CHAIN_ING_FILTER, family, ACTION_START);
			run_base_chain(buf, f, NFTLB_F_CHAIN_ING_FILTER, family, get_rules_needed(f), ACTION_START);
			run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_ING_FILTER, VALUE_FAMILY_NETDEV, f->policies_action);
		}
		run_farm_rules_ingress_policies(buf, f, chain);
	} else if (f->policies_action == ACTION_STOP && !farm_is_ingress_mode(f)) {
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_ING_FILTER, VALUE_FAMILY_NETDEV, f->policies_action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_ING_FILTER, family, get_rules_needed(f), ACTION_STOP);
		run_base_table(buf, NFTLB_F_CHAIN_ING_FILTER, family, ACTION_STOP);
	} else if (f->policies_action == ACTION_RELOAD) {
		if (!farm_is_ingress_mode(f))
			run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_ING_FILTER, VALUE_FAMILY_NETDEV, f->policies_action);
		run_farm_rules_ingress_policies(buf, f, chain);
	} else {
	}

	f->policies_action = ACTION_NONE;

	return 0;
}

static void get_farm_rules_nat_params(struct sbuffer *buf, struct farm *f, int family)
{
	if (f->bcks_have_port)
		concat_buf(buf, " %s addr . port", print_nft_family(family));
}

static int run_farm_rules_gen_nat(struct sbuffer *buf, struct farm *f, int family, int type, int action)
{
	char chain[255] = { 0 };
	char map_str[255] = { 0 };
	int bck_map_data = BCK_MAP_IPADDR;

	if (f->bcks_usable == 0)
		return 0;

	get_farm_chain(chain, f, type);

	switch (f->mode) {
	case VALUE_MODE_DSR:
		run_farm_rules_check_sessions(buf, f, SESSION_TYPE_STATIC, family, NFTLB_F_CHAIN_ING_FILTER, action);

		concat_buf(buf, " ; add rule %s %s %s", print_nft_table_family(family, NFTLB_F_CHAIN_ING_FILTER), NFTLB_TABLE_NAME, chain);
		run_farm_log_prefix(buf, f, VALUE_LOG_INPUT, NFTLB_F_CHAIN_ING_FILTER, ACTION_START);
		// TODO: support of different output interfaces per backend during saddr
		concat_buf(buf, " ether saddr set %s ether daddr set", f->oethaddr);
		run_farm_rules_gen_sched(buf, f, family);
		run_farm_rules_gen_bck_map(buf, f, BCK_MAP_WEIGHT, BCK_MAP_ETHADDR, NFTLB_CHECK_AVAIL);
		run_farm_rules_update_sessions(buf, f, family, chain, action);
		run_farm_log_prefix(buf, f, VALUE_LOG_OUTPUT, NFTLB_F_CHAIN_ING_DNAT, ACTION_START);
		concat_buf(buf, " fwd to");
		if (f->bcks_have_if) {
			concat_buf(buf, " ether daddr");
			run_farm_rules_gen_bck_map(buf, f, BCK_MAP_ETHADDR, BCK_MAP_OFACE, NFTLB_CHECK_AVAIL);
		} else
			concat_buf(buf, " %s", f->oface);
		concat_exec_cmd(buf, "");
		break;
	case VALUE_MODE_STLSDNAT:
		sprintf(map_str, "map-%s-back", f->name);
		concat_exec_cmd(buf, " ; add rule %s %s %s update @%s { %s saddr : ether saddr }", print_nft_table_family(family, NFTLB_F_CHAIN_ING_FILTER), NFTLB_TABLE_NAME, chain, map_str, print_nft_family(family));

		run_farm_rules_check_sessions(buf, f, SESSION_TYPE_STATIC, family, NFTLB_F_CHAIN_ING_FILTER, action);

		concat_buf(buf, " ; add rule %s %s %s", print_nft_table_family(family, NFTLB_F_CHAIN_ING_FILTER), NFTLB_TABLE_NAME, chain);
		run_farm_log_prefix(buf, f, VALUE_LOG_INPUT, NFTLB_F_CHAIN_ING_FILTER, ACTION_START);
		concat_buf(buf, " %s daddr set", print_nft_family(family));
		run_farm_rules_gen_sched(buf, f, family);
		run_farm_rules_gen_bck_map(buf, f, BCK_MAP_WEIGHT, BCK_MAP_IPADDR, NFTLB_CHECK_AVAIL);
		concat_buf(buf, " ether daddr set ip daddr");
		run_farm_rules_gen_bck_map(buf, f, BCK_MAP_IPADDR, BCK_MAP_ETHADDR, NFTLB_CHECK_AVAIL);

		if (f->bcks_have_port) {
			concat_buf(buf, " th dport set ether daddr");
			run_farm_rules_gen_bck_map(buf, f, BCK_MAP_ETHADDR, BCK_MAP_PORT, NFTLB_CHECK_AVAIL);
		}

		// TODO: support of different output interfaces per backend during saddr
		concat_buf(buf, " ether saddr set %s", f->oethaddr);

		run_farm_rules_update_sessions(buf, f, family, chain, action);

		run_farm_log_prefix(buf, f, VALUE_LOG_OUTPUT, NFTLB_F_CHAIN_ING_DNAT, ACTION_START);
		concat_buf(buf, " fwd to");
		if (f->bcks_have_if) {
			concat_buf(buf, " ether daddr");
			run_farm_rules_gen_bck_map(buf, f, BCK_MAP_ETHADDR, BCK_MAP_OFACE, NFTLB_CHECK_AVAIL);
		} else
			concat_buf(buf, " %s", f->oface);
		concat_exec_cmd(buf, "");
		break;
	default:
		run_farm_gen_log_rules(buf, f, family, chain, VALUE_LOG_INPUT, NFTLB_F_CHAIN_PRE_DNAT, ACTION_START);
		concat_buf(buf, " ; add rule %s %s %s dnat", print_nft_table_family(family, NFTLB_F_CHAIN_PRE_DNAT), NFTLB_TABLE_NAME, chain);

		if (f->bcks_have_port)
			bck_map_data = BCK_MAP_IPADDR_PORT;

		get_farm_rules_nat_params(buf, f, family);
		concat_buf(buf, " to ct mark");
		run_farm_rules_gen_bck_map(buf, f, BCK_MAP_MARK, bck_map_data, NFTLB_CHECK_USABLE);

		concat_exec_cmd(buf, "");
		break;
	}

	return 0;
}

static int run_farm_rules(struct sbuffer *buf, struct farm *f, int family, int action)
{
	switch (f->mode) {
	case VALUE_MODE_STLSDNAT:
	case VALUE_MODE_DSR:
		run_base_table(buf, NFTLB_F_CHAIN_ING_FILTER, family, action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_ING_FILTER, family, get_rules_needed(f), action);
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_ING_FILTER, family, action);
		run_farm_ingress_policies(buf, f, family, action);
		run_farm_stlsnat(buf, f, family, action);

		run_farm_sessions_map(buf, f, SESSION_TYPE_STATIC, family, action);
		run_farm_sessions_map(buf, f, SESSION_TYPE_TIMED, family, action);
		run_farm_manage_sessions(buf, f, SESSION_TYPE_STATIC, family, action);
		run_farm_manage_sessions(buf, f, SESSION_TYPE_TIMED, family, action);

		run_farm_rules_gen_nat(buf, f, family, NFTLB_F_CHAIN_ING_FILTER, action);
		break;
	case VALUE_MODE_LOCAL:
		run_farm_rules_filter(buf, f, family, action);
		run_farm_ingress_policies(buf, f, family, action);
		break;
	default:
		run_base_table(buf, NFTLB_F_CHAIN_PRE_DNAT, family, action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_PRE_DNAT, family, get_rules_needed(f), action);
		run_base_chain(buf, f, NFTLB_F_CHAIN_POS_SNAT, family, get_rules_needed(f), action);

		run_farm_rules_filter(buf, f, family, action);
		run_farm_ingress_policies(buf, f, family, action);

		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_PRE_DNAT, family, action);
		run_farm_rules_gen_nat(buf, f, family, NFTLB_F_CHAIN_PRE_DNAT, action);

		run_farm_rules_forward(buf, f, family, action);
		run_farm_rules_output(buf, f, family, action);

		run_farm_snat(buf, f, family, action);
	}

	return 0;
}

static int run_farm(struct sbuffer *buf, struct farm *f, int action)
{
	if ((f->family == VALUE_FAMILY_IPV4) || (f->family == VALUE_FAMILY_INET)) {
		run_farm_rules(buf, f, VALUE_FAMILY_IPV4, action);
	}

	if ((f->family == VALUE_FAMILY_IPV6) || (f->family == VALUE_FAMILY_INET)) {
		run_farm_rules(buf, f, VALUE_FAMILY_IPV6, action);
	}

	return 0;
}

static int del_farm_rules(struct sbuffer *buf, struct farm *f, int family)
{
	int ret = 0;
	char fchain[255] = { 0 };
	char fservice[255] = { 0 };

	run_farm_ingress_policies(buf, f, family, ACTION_STOP);

	if ((f->nft_chains & NFTLB_F_CHAIN_OUT_FILTER) || (f->nft_chains & NFTLB_F_CHAIN_OUT_DNAT))
		run_farm_rules_output(buf, f, family, ACTION_DELETE);

	if (f->nft_chains & NFTLB_F_CHAIN_PRE_FILTER) {
		sprintf(fchain, "%s-%s", NFTLB_TYPE_FILTER, f->name);
		sprintf(fservice, "%s-%s", NFTLB_TYPE_FILTER, print_nft_service(f, family));
		run_farm_rules_filter(buf, f, family, ACTION_DELETE);
	}

	if (f->nft_chains & NFTLB_F_CHAIN_FWD_FILTER)
		run_farm_rules_forward(buf, f, family, ACTION_DELETE);

	if (farm_is_ingress_mode(f)) {
		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_ING_FILTER, family, ACTION_DELETE);
		run_farm_sessions_map(buf, f, SESSION_TYPE_STATIC, family, ACTION_DELETE);
		run_farm_sessions_map(buf, f, SESSION_TYPE_TIMED, family, ACTION_DELETE);
		run_base_chain(buf, f, NFTLB_F_CHAIN_ING_FILTER, family, get_rules_needed(f), ACTION_DELETE);
		run_farm_stlsnat(buf, f, family, ACTION_DELETE);
		run_base_table(buf, NFTLB_F_CHAIN_ING_FILTER, family, ACTION_DELETE);
	} else {
		run_farm_snat(buf, f, family, ACTION_DELETE);
		run_base_chain(buf, f, NFTLB_F_CHAIN_POS_SNAT, family, get_rules_needed(f), ACTION_DELETE);

		run_farm_rules_gen_vsrv(buf, f, NFTLB_F_CHAIN_PRE_DNAT, family, ACTION_DELETE);
		run_base_chain(buf, f, NFTLB_F_CHAIN_PRE_DNAT, family, get_rules_needed(f), ACTION_DELETE);
		run_base_table(buf, NFTLB_F_CHAIN_PRE_DNAT, family, ACTION_DELETE);
	}

	return ret;
}

static int del_farm(struct sbuffer *buf, struct farm *f)
{
	int ret = 0;

	if ((f->family == VALUE_FAMILY_IPV4) || (f->family == VALUE_FAMILY_INET))
		del_farm_rules(buf, f, VALUE_FAMILY_IPV4);

	if ((f->family == VALUE_FAMILY_IPV6) || (f->family == VALUE_FAMILY_INET))
		del_farm_rules(buf, f, VALUE_FAMILY_IPV6);

	return ret;
}

static int nft_actions_done(struct farm *f)
{
	struct backend *b;

	list_for_each_entry(b, &f->backends, list)
		b->action = ACTION_NONE;

	f->action = ACTION_NONE;
	f->reload_action = VALUE_RLD_NONE;

	return 0;
}

int nft_reset(void)
{
	struct sbuffer buf;
	int ret = 0;

	create_buf(&buf);

	if (nft_base_rules.dnat_rules_v4 ||
	    nft_base_rules.snat_rules_v4 ||
	    nft_base_rules.filter_rules_v4 ||
	    nft_base_rules.fwd_rules_v4 ||
	    nft_base_rules.out_filter_rules_v4 ||
	    nft_base_rules.out_nat_rules_v4)
		nft_table_handler(&buf, print_nft_family(VALUE_FAMILY_IPV4), ACTION_DELETE);

	if (nft_base_rules.dnat_rules_v6 ||
	    nft_base_rules.snat_rules_v6 ||
	    nft_base_rules.filter_rules_v6 ||
	    nft_base_rules.fwd_rules_v6 ||
	    nft_base_rules.out_filter_rules_v6 ||
	    nft_base_rules.out_nat_rules_v6)
		nft_table_handler(&buf, print_nft_family(VALUE_FAMILY_IPV6), ACTION_DELETE);

	if (nft_base_rules.ndv_ingress_rules.n_interfaces ||
		nft_base_rules.ndv_ingress_dnat_rules.n_interfaces ||
		nft_base_rules.ndv_ingress_policies)
		nft_table_handler(&buf, print_nft_family(VALUE_FAMILY_NETDEV), ACTION_DELETE);

	exec_cmd(get_buf_data(&buf));
	clean_buf(&buf);
	clean_rules_counters();

	return ret;
}

int nft_check_tables(void)
{
	char cmd[255] = { 0 };
	const char *buf;

	sprintf(cmd, "list table %s %s", NFTLB_IPV4_FAMILY_STR, NFTLB_TABLE_NAME);
	if (exec_cmd_open(cmd, &buf, 0) == 0)
		nft_base_rules.dnat_rules_v4 = 1;
	exec_cmd_close(buf);

	sprintf(cmd, "list table %s %s", NFTLB_IPV6_FAMILY_STR, NFTLB_TABLE_NAME);
	if (exec_cmd_open(cmd, &buf, 0) == 0)
		nft_base_rules.dnat_rules_v6 = 1;
	exec_cmd_close(buf);

	sprintf(cmd, "list table %s %s", NFTLB_NETDEV_FAMILY_STR, NFTLB_TABLE_NAME);
	if (exec_cmd_open(cmd, &buf, 0) == 0)
		nft_base_rules.ndv_ingress_rules.n_interfaces = 1;
	exec_cmd_close(buf);

	return nft_base_rules.dnat_rules_v4 ||
		   nft_base_rules.dnat_rules_v6 ||
		   nft_base_rules.ndv_ingress_rules.n_interfaces;
}

int nft_rulerize(struct farm *f)
{
	struct sbuffer buf;
	int ret = 0;

	create_buf(&buf);

	switch (f->action) {
	case ACTION_START:
	case ACTION_RELOAD:
		ret = run_farm(&buf, f, f->action);
		break;
	case ACTION_STOP:
	case ACTION_DELETE:
		ret = del_farm(&buf, f);
		break;
	case ACTION_NONE:
	default:
		break;
	}

	exec_cmd(get_buf_data(&buf));
	clean_buf(&buf);
	nft_actions_done(f);

	print_service_counters();
	print_nft_base_rules();

	return ret;
}

static int run_set_elements(struct sbuffer *buf, struct policy *p)
{
	struct element *e;
	int index = 0;

	if (!p->total_elem)
		return 0;

	switch (p->action){
	case ACTION_START:
		list_for_each_entry(e, &p->elements, list) {
			if (index)
				concat_buf(buf, ", %s", e->data);
			else {
				index++;
				concat_buf(buf, " ; add element %s %s %s { %s", NFTLB_NETDEV_FAMILY_STR, NFTLB_TABLE_NAME, p->name, e->data);
			}
			e->action = ACTION_NONE;
		}
		if (index)
			concat_exec_cmd(buf, " }");
		break;
	case ACTION_RELOAD:
		list_for_each_entry(e, &p->elements, list) {
			if (e->action != ACTION_START)
				continue;
			if (index)
				concat_buf(buf, ", %s", e->data);
			else {
				index++;
				concat_buf(buf, " ; add element %s %s %s { %s", NFTLB_NETDEV_FAMILY_STR, NFTLB_TABLE_NAME, p->name, e->data);
			}
			e->action = ACTION_NONE;
		}
		if (index)
			concat_exec_cmd(buf, " }");

		index = 0;
		list_for_each_entry(e, &p->elements, list) {
			if (e->action != ACTION_DELETE && e->action != ACTION_STOP)
				continue;
			if (index)
				concat_buf(buf, ", %s", e->data);
			else {
				index++;
				concat_buf(buf, " ; delete element %s %s %s { %s", NFTLB_NETDEV_FAMILY_STR, NFTLB_TABLE_NAME, p->name, e->data);
			}
			e->action = ACTION_NONE;
		}
		if (index)
			concat_exec_cmd(buf, " }");
		break;
	default:
		break;
	}

	return 0;
}

static int run_policy_set(struct sbuffer *buf, struct policy *p)
{
	switch (p->action) {
	case ACTION_START:
		run_base_table(buf, NFTLB_F_CHAIN_ING_FILTER, VALUE_FAMILY_NETDEV, ACTION_START);
		concat_exec_cmd(buf, " ; add set %s %s %s { type %s ; flags interval ; auto-merge ; }", NFTLB_NETDEV_FAMILY_STR, NFTLB_TABLE_NAME, p->name, print_nft_family_type(p->family));
		nft_base_rules.ndv_ingress_policies++;
		run_set_elements(buf, p);
		break;
	case ACTION_RELOAD:
		run_set_elements(buf, p);
		break;
	case ACTION_FLUSH:
		concat_exec_cmd(buf, " ; flush set %s %s %s", NFTLB_NETDEV_FAMILY_STR, NFTLB_TABLE_NAME, p->name);
		break;
	case ACTION_STOP:
	case ACTION_DELETE:
		concat_exec_cmd(buf, " ; delete set %s %s %s", NFTLB_NETDEV_FAMILY_STR, NFTLB_TABLE_NAME, p->name);
		if (nft_base_rules.ndv_ingress_policies > 0)
			nft_base_rules.ndv_ingress_policies--;
		if (obj_get_total_policies() == 1)
			run_base_table(buf, NFTLB_F_CHAIN_ING_FILTER, VALUE_FAMILY_NETDEV, ACTION_DELETE);
		break;
	case ACTION_NONE:
	default:
		break;
	}

	p->action = ACTION_NONE;

	print_service_counters();
	print_nft_base_rules();

	return 0;
}

int nft_rulerize_policies(struct policy *p)
{
	struct sbuffer buf;
	int ret = 0;

	create_buf(&buf);

	run_policy_set(&buf, p);
	exec_cmd(get_buf_data(&buf));

	clean_buf(&buf);

	return ret;
}

int nft_get_rules_buffer(const char **buf, int key, struct farm *f, struct policy *p)
{
	char cmd[255] = { 0 };
	int error = 0;

	switch (key) {
	case KEY_SESSIONS:
		sprintf(cmd, "list map %s nftlb persist-%s", print_nft_table_family(f->family, get_stage_by_farm_mode(f)), f->name);
		break;
	case KEY_POLICIES:
		sprintf(cmd, "list set netdev nftlb %s", p->name);
		break;
	default:
		return 0;
		break;
	}

	error = exec_cmd_open(cmd, buf, 0);

	return error;
}

void nft_del_rules_buffer(const char *buf)
{
	exec_cmd_close(buf);
}
