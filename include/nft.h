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

#ifndef _NFT_H_
#define _NFT_H_

#include "farms.h"

#define NFTLB_MASQUERADE_MARK_DEFAULT		0x80000000

int nft_reset(void);
int nft_check_tables(void);
int nft_rulerize_farms(struct farm *f);
int nft_rulerize_address(struct address *a);
int nft_rulerize_policies(struct policy *p);
int nft_get_rules_buffer(const char **buf, int key, struct nftst *n);
void nft_del_rules_buffer(const char *buf);

#endif /* _NFT_H_ */
