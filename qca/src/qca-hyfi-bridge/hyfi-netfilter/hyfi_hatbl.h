/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HYFI_HYFI_HACTIVE_TBL_H_
#define	HYFI_HYFI_HACTIVE_TBL_H_

#include <linux/etherdevice.h>
#include "hyfi_netfilter.h"
#include "hyfi_hdtbl.h"
#include "hyfi_seamless.h"
#include "hyfi_aggr_types.h"

/* ha table defines */
#define HYFI_HACTIVE_TBL_BITS 14
#define HYFI_HACTIVE_TBL_SIZE (1 << HYFI_HACTIVE_TBL_BITS)

#define HYFI_HACTIVE_TBL_EXPIRE_TIME 120000  /* 120 sec */
#define HYFI_HACTIVE_TBL_AGING_TIME  (1 << 14)  /* 16384 msec */

#define HYFI_HACTIVE_TBL_STATIC_ENTRY                       (1 << 0)
#define HYFI_HACTIVE_TBL_SEAMLESS_ENABLED                   (1 << 1)
#define HYFI_HACTIVE_TBL_TRACKED_ENTRY                      (1 << 2)
#define HYFI_HACTIVE_TBL_AGGR_RX_ENTRY                      (1 << 3)
#define HYFI_HACTIVE_TBL_AGGR_TX_ENTRY                      (1 << 4)
#define HYFI_HACTIVE_TBL_ACCL_ENTRY							(1 << 5)

#define HYFI_HACTIVE_TBL_PRIORITY_DSCP_VALID (1 << 31)
#define HYFI_HACTIVE_TBL_PRIORITY_8021_VALID (1 << 30)
#define HYFI_HACTIVE_TBL_PRIORITY_8021_MASK  0x00000007
#define HYFI_HACTIVE_TBL_PRIORITY_8021_SHIFT 0
#define HYFI_HACTIVE_TBL_PRIORITY_DSCP_MASK  0x000001F8
#define HYFI_HACTIVE_TBL_PRIORITY_DSCP_SHIFT 3

/* hybrid active table entry */
struct net_hatbl_entry {
	struct hlist_node hlist;
	struct net_bridge_port *dst;
	struct rcu_head rcu;

	mac_addr sa;
	mac_addr da;
	mac_addr id;
	u_int32_t last_access;
	u_int32_t create_time;
	u_int32_t num_packets;
	u_int32_t num_bytes;
	u_int64_t prev_num_packets;
	u_int64_t prev_num_bytes;
	u_int8_t hash;
	u_int8_t sub_class;
	u_int8_t action; /* drop, throttle ... */
	u_int8_t local; /* not created from HD */
	u_int32_t priority;
	u_int32_t flags;
	u_int32_t ecm_serial;

	struct ha_psw_stm_entry psw_stm_entry;
	struct psw_flow_info psw_info;

	spinlock_t aggr_lock;

	struct hyfi_iface_info iface_info[HYFI_AGGR_MAX_IFACES];
	struct hyfi_aggr_seq_data aggr_seq_data;

	struct hyfi_aggr_rx_entry *aggr_rx_entry;
};

#define HYFI_HACTIVE_TBL_RX_ENTRY \
	( HYFI_HACTIVE_TBL_TRACKED_ENTRY | HYFI_HACTIVE_TBL_AGGR_RX_ENTRY )

static inline void hyfi_ha_set_flag(struct net_hatbl_entry *ha, u_int32_t flag)
{
	ha->flags |= flag;
}

static inline void hyfi_ha_clear_flag(struct net_hatbl_entry *ha,
		u_int32_t flag)
{
	ha->flags &= ~flag;
}

static inline u_int32_t hyfi_ha_has_flag(const struct net_hatbl_entry *ha,
		u_int32_t flag)
{
	return ((ha->flags & flag) != 0);
}

/* No locking or refcounting, assumes caller has no preempt (rcu_read_lock) */
static inline struct net_hatbl_entry* __hyfi_hatbl_get(
		struct hyfi_net_bridge *br, u_int32_t hash, const unsigned char *da,
		u_int32_t sub_class, u_int32_t priority)
{
	struct hlist_node *h;
	struct net_hatbl_entry *ha;

	hlist_for_each_entry_rcu(ha, h, &br->hash_ha[hash], hlist)
	{
		if ((ha->sub_class == sub_class) && (ha->priority == priority)
				&& !compare_ether_addr(ha->da.addr, da)) {
			ha->last_access = jiffies;
			return ha;
		}
	}

	return NULL ;
}

/* No locks are taken, caller must lock ha-lock */
struct net_hatbl_entry *hatbl_find(struct hyfi_net_bridge *br, u_int32_t hash,
		const unsigned char *da, u_int32_t sub_class, u_int32_t priority);
struct net_hatbl_entry *hatbl_find_ecm(struct hyfi_net_bridge *br, u_int32_t hash,
		u_int32_t ecm_serial);
extern int hyfi_hatbl_init(struct hyfi_net_bridge *br);
extern void hyfi_hatbl_fini(struct hyfi_net_bridge *br);
extern void hyfi_hatbl_flush(struct hyfi_net_bridge *br);
extern void hyfi_hatbl_cleanup(unsigned long data);
extern void hyfi_hatbl_delete_by_port(struct hyfi_net_bridge *br,
		const struct net_bridge_port *p);
extern int hyfi_hatbl_fillbuf(struct hyfi_net_bridge *br, void *buf,
		u_int32_t buf_len, u_int32_t offsite, u_int32_t *bytes_written,
		u_int32_t *bytes_needed);

extern struct net_hatbl_entry* hyfi_hatbl_insert(struct hyfi_net_bridge *br,
		u_int32_t hash, u_int32_t sub_class, struct net_hdtbl_entry *hd,
		u_int32_t priority, const u_int8_t* sa);

extern struct net_hatbl_entry* hyfi_hatbl_insert_ecm_classifier(struct hyfi_net_bridge *br,
		u_int32_t hash, u_int32_t sub_class, struct net_hdtbl_entry *hd,
		u_int32_t priority, const u_int8_t* sa, u_int32_t ecm_serial);

extern int hyfi_hatbl_update(struct hyfi_net_bridge *br,
		struct __hatbl_entry *hae, int update_local);

extern int hyfi_hatbl_addentry(struct hyfi_net_bridge *br,
		struct __hatbl_entry *hae);

int hyfi_hatbl_update_local(struct hyfi_net_bridge *br, u_int32_t hash,
		const u_int8_t *sa, const u_int8_t *da, struct net_hdtbl_entry *hd,
		struct net_bridge_port *dst, u_int32_t sub_class, u_int32_t priority,
		u_int32_t flag);

struct net_hatbl_entry * hyfi_hatbl_create_tracked_entry(
		struct hyfi_net_bridge *br, u_int32_t hash, const u_int8_t *sa,
		const u_int8_t *da, u_int32_t sub_class, u_int32_t priority);

struct net_hatbl_entry * hyfi_hatbl_find_tracked_entry(
		struct hyfi_net_bridge *br, u_int32_t hash, const u_int8_t *mac_addr,
		u_int32_t sub_class, u_int32_t priority);

struct net_hatbl_entry * hyfi_hatbl_create_aggr_entry(
		struct hyfi_net_bridge *br, u_int32_t hash, const u_int8_t *sa,
		const u_int8_t *da, u_int32_t sub_class, u_int32_t priority,
		u_int16_t seq);

int hyfi_aggr_init_entry(struct net_hatbl_entry *ha, u_int16_t seq);

struct net_hatbl_entry* hyfi_hatbl_insert_from_fdb(struct hyfi_net_bridge *br,
		u_int32_t hash, struct net_bridge_port *dst, const u_int8_t *sa, const u_int8_t *da,
		const u_int8_t *id, u_int32_t sub_class, u_int32_t priority);

void hyfi_hatbl_update_mcast_stats(struct net_bridge *br, struct sk_buff *skb,
		struct net_bridge_port *dst);

#endif
