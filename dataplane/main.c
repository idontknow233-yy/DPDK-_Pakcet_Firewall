/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
 
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_ip6.h>
#include <rte_ring.h>
#include <rte_memzone.h>
#include <rte_atomic.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#include "acl/acl.h"
#include "acl/acl6.h"
#include "arp/arp.h"
#include "ipc/acl_ipc.h"
#include "ipc/acl_hit_ipc.h"
#include "ipc/acl6_ipc.h"
#include "ipc/acl6_hit_ipc.h"
#include "ipc/session_ipc.h"
#include "ipc/session6_ipc.h"
#include "ipc/stats_ipc.h"
#include "ipc/rlim_ipc.h"
#include "ipc/route_ipc.h"
#include "ipc/route6_ipc.h"
#include "ipc/portcfg_ipc.h"
#include "ipc/attack_ipc.h"
#include "nd/nd.h"
#include "route/route.h"
#include "route/route6.h"
#include "session/session.h"
#include "session/session6.h"
 
static volatile bool force_quit;
 
/* MAC updating enabled by default */
static int mac_updating = 1;
 
/* Ports set in promiscuous mode off by default. Use -P to enable */
static int promiscuous_on = 0;

/* Built-in packet generator (enabled via --pktgen) */
static int pktgen_port = -1;
static uint16_t pktgen_pkt_size = 64;
static struct rte_ether_addr pktgen_dst_mac;
static struct rte_mempool *pktgen_mbuf_pool;
static volatile int pktgen_running;
 
#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1
 
#define MAX_PKT_BURST 256
#define BURST_TX_DRAIN_US 50 /* TX drain every ~50us */
#define MEMPOOL_CACHE_SIZE 256
 
/*
 * Configurable number of RX/TX ring descriptors
 */
#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RX_DESC_DEFAULT;
static uint16_t nb_txd = TX_DESC_DEFAULT;
 
/* ethernet addresses of ports */
static struct rte_ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];
 
/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask = 0;
 
/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];
 
struct __rte_cache_aligned port_pair_params {
#define NUM_PORTS	2
	uint16_t port[NUM_PORTS];
};
 
static struct port_pair_params port_pair_params_array[RTE_MAX_ETHPORTS / 2];
static struct port_pair_params *port_pair_params;
static uint16_t nb_port_pair_params;
 
static unsigned int l2fwd_rx_queue_per_lcore = 1;
 
#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
/* List of queues to be polled for a given lcore. 8< */
struct __rte_cache_aligned lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
};
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];
/* >8 End of list of queues to be polled for a given lcore. */
 
static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];
 
static struct rte_eth_conf port_conf = {
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};
 
struct rte_mempool * l2fwd_pktmbuf_pool = NULL;

static struct session_table session_tables[RTE_MAX_LCORE];
static uint8_t session_table_inited[RTE_MAX_LCORE];
static struct session6_table session6_tables[RTE_MAX_LCORE];
static uint8_t session6_table_inited[RTE_MAX_LCORE];
static pthread_t acl_ctrl_thread;
static pthread_t acl6_ctrl_thread;

struct acl_runtime {
	struct acl_ctx ctx[2];
	rte_atomic32_t active;
	rte_atomic64_t version;
};

static struct acl_runtime acl_rt;

struct acl6_runtime {
	struct acl6_ctx ctx[2];
	rte_atomic32_t active;
	rte_atomic64_t version;
};

static struct acl6_runtime acl6_rt;
static struct rte_ring *acl_cmd_ring;
static struct rte_ring *acl_resp_ring;
static struct acl_shared_cfg *acl_shared_cfg;
static struct acl_hit_shared_cfg *acl_hit_shared_cfg;
static struct rte_ring *acl6_cmd_ring;
static struct rte_ring *acl6_resp_ring;
static struct acl6_shared_cfg *acl6_shared_cfg;
static struct acl6_hit_shared_cfg *acl6_hit_shared_cfg;
static struct session_shared_cfg *session_shared_cfg;
static struct session6_shared_cfg *session6_shared_cfg;
static struct portstats_shared_cfg *portstats_shared_cfg;
static struct denylog_shared_cfg *denylog_shared_cfg;
static struct denylog6_shared_cfg *denylog6_shared_cfg;
static struct rlim_shared_cfg *rlim_shared_cfg;
static struct route_shared_cfg *route_shared_cfg;
static struct route6_shared_cfg *route6_shared_cfg;
static struct portcfg_shared_cfg *portcfg_shared_cfg;
static struct attack_shared_cfg *attack_shared_cfg;
static uint64_t session_timeout_tsc;

static struct route_table *ipv4_rt_tbls[2];
static rte_atomic32_t ipv4_rt_active_idx;
static struct route_table *ipv4_rt_reclaim;
static struct arp_table *arp_tbl;
static struct arp_ifcfg ifcfgs[RTE_MAX_ETHPORTS];
static struct nd_table *nd_tbl;
static struct route6_table *ipv6_rt_tbls[2];
static rte_atomic32_t ipv6_rt_active_idx;
static struct route6_table *ipv6_rt_reclaim;
static struct nd_ifcfg ifcfg6_tbls[2][RTE_MAX_ETHPORTS];
static rte_atomic32_t ifcfg6_active_idx;
static int routing_on;

static inline struct route6_table *ipv6_rt_active(void) {
    uint32_t idx = rte_atomic32_read(&ipv6_rt_active_idx) & 1u;
    return ipv6_rt_tbls[idx];
}

static inline struct route_table *ipv4_rt_active(void) {
    uint32_t idx = rte_atomic32_read(&ipv4_rt_active_idx) & 1u;
    return ipv4_rt_tbls[idx];
}

static inline struct nd_ifcfg *ifcfg6_active(void) {
	uint32_t idx = rte_atomic32_read(&ifcfg6_active_idx) & 1u;
	return ifcfg6_tbls[idx];
}

struct rlim_bucket {
	uint64_t tokens;
	uint64_t last_tsc;
};

struct rlim_table {
	struct rte_hash *h;
	struct rlim_bucket *buckets;
	uint32_t cap;
	uint32_t key_len;
};

static struct rlim_table rlim_syn_tbls[RTE_MAX_LCORE];
static struct rlim_table rlim_udp_tbls[RTE_MAX_LCORE];
static struct rlim_table rlim6_syn_tbls[RTE_MAX_LCORE];
static struct rlim_table rlim6_udp_tbls[RTE_MAX_LCORE];
static uint8_t rlim_inited[RTE_MAX_LCORE];
static uint32_t rlim_syn_pps;
static uint32_t rlim_syn_burst;
static uint32_t rlim_udp_pps;
static uint32_t rlim_udp_burst;

struct scan4_state {
	uint64_t window_start_tsc;
	uint64_t ban_until_tsc;
	uint64_t port_bits;
	uint32_t port_cnt;
};

struct scan6_state {
	uint64_t window_start_tsc;
	uint64_t ban_until_tsc;
	uint64_t port_bits;
	uint32_t port_cnt;
};

struct scan_table {
	struct rte_hash *h;
	void *states;
	uint32_t cap;
	uint32_t key_len;
};

static struct scan_table scan4_tbls[RTE_MAX_LCORE];
static struct scan_table scan6_tbls[RTE_MAX_LCORE];
static rte_atomic64_t attack_syn_cnt[RTE_MAX_LCORE];
static rte_atomic64_t attack_udp_cnt[RTE_MAX_LCORE];
static rte_atomic64_t attack_scan_events_cnt[RTE_MAX_LCORE];
static rte_atomic64_t attack_scan_banned_cnt[RTE_MAX_LCORE];
static uint32_t attack_top_scan4_ports[RTE_MAX_LCORE];
static uint32_t attack_top_scan4_ip[RTE_MAX_LCORE];
static struct rte_ipv6_addr attack_top_scan6_ip[RTE_MAX_LCORE];
static uint32_t attack_top_scan6_ports[RTE_MAX_LCORE];
static uint32_t attack_scan_ports_per_sec;
static uint32_t attack_ban_seconds;
static uint8_t attack_mitigation_enabled;
static rte_atomic64_t acl_deny_pkts[RTE_MAX_LCORE][ACL_MAX_RULES];
static rte_atomic64_t acl_deny_bytes[RTE_MAX_LCORE][ACL_MAX_RULES];
static rte_atomic64_t acl6_deny_pkts[RTE_MAX_LCORE][ACL6_MAX_RULES];
static rte_atomic64_t acl6_deny_bytes[RTE_MAX_LCORE][ACL6_MAX_RULES];

static int rlim_table_init(struct rlim_table *t, const char *name, uint32_t cap, uint32_t key_len, int socket_id) {
	if (!t || !name || cap == 0 || key_len == 0) {
		return -1;
	}
	memset(t, 0, sizeof(*t));
	t->cap = cap;
	t->key_len = key_len;
	t->buckets = rte_zmalloc_socket(NULL, sizeof(struct rlim_bucket) * cap, 0, socket_id);
	if (!t->buckets) {
		return -1;
	}
	struct rte_hash_parameters hp;
	memset(&hp, 0, sizeof(hp));
	hp.name = name;
	hp.entries = cap;
	hp.key_len = key_len;
	hp.hash_func = rte_jhash;
	hp.hash_func_init_val = 0;
	hp.socket_id = socket_id;
	t->h = rte_hash_create(&hp);
	if (!t->h) {
		rte_free(t->buckets);
		memset(t, 0, sizeof(*t));
		return -1;
	}
	return 0;
}

static void rlim_table_free(struct rlim_table *t) {
	if (!t) {
		return;
	}
	if (t->h) {
		rte_hash_free(t->h);
	}
	if (t->buckets) {
		rte_free(t->buckets);
	}
	memset(t, 0, sizeof(*t));
}

static inline int rlim_allow(struct rlim_table *t, const void *key, uint64_t now_tsc, uint64_t hz, uint32_t pps, uint32_t burst) {
	if (!t || !t->h || !t->buckets || pps == 0 || burst == 0 || hz == 0) {
		return 1;
	}
	if (!key) {
		return 1;
	}
	int32_t pos = rte_hash_lookup(t->h, key);
	if (pos < 0) {
		pos = rte_hash_add_key(t->h, key);
		if (pos < 0) {
			return 0;
		}
		struct rlim_bucket *b = &t->buckets[(uint32_t)pos];
		b->tokens = burst;
		b->last_tsc = now_tsc;
	}
	struct rlim_bucket *b = &t->buckets[(uint32_t)pos];
	if (now_tsc > b->last_tsc) {
		uint64_t delta = now_tsc - b->last_tsc;
		uint64_t add = (delta * (uint64_t)pps) / hz;
		if (add) {
			uint64_t nt = b->tokens + add;
			b->tokens = nt > burst ? burst : nt;
			b->last_tsc = now_tsc;
		}
	}
	if (b->tokens == 0) {
		return 0;
	}
	b->tokens--;
	return 1;
}

static int scan_table_init(struct scan_table *t, const char *name, uint32_t cap, uint32_t key_len, size_t state_size, int socket_id) {
	if (!t || !name || cap == 0 || key_len == 0 || state_size == 0) {
		return -1;
	}
	memset(t, 0, sizeof(*t));
	t->cap = cap;
	t->key_len = key_len;
	t->states = rte_zmalloc_socket(NULL, state_size * cap, 0, socket_id);
	if (!t->states) {
		return -1;
	}
	struct rte_hash_parameters hp;
	memset(&hp, 0, sizeof(hp));
	hp.name = name;
	hp.entries = cap;
	hp.key_len = key_len;
	hp.hash_func = rte_jhash;
	hp.hash_func_init_val = 0;
	hp.socket_id = socket_id;
	t->h = rte_hash_create(&hp);
	if (!t->h) {
		rte_free(t->states);
		memset(t, 0, sizeof(*t));
		return -1;
	}
	return 0;
}

static void scan_table_free(struct scan_table *t) {
	if (!t) {
		return;
	}
	if (t->h) {
		rte_hash_free(t->h);
	}
	if (t->states) {
		rte_free(t->states);
	}
	memset(t, 0, sizeof(*t));
}

static inline int scan4_is_banned(unsigned lcore_id, uint32_t src_ip, uint64_t now_tsc) {
	struct scan_table *t = &scan4_tbls[lcore_id];
	if (!t->h || !t->states) {
		return 0;
	}
	int32_t pos = rte_hash_lookup(t->h, &src_ip);
	if (pos < 0) {
		return 0;
	}
	struct scan4_state *st = &((struct scan4_state *)t->states)[(uint32_t)pos];
	return st->ban_until_tsc && now_tsc < st->ban_until_tsc;
}

static inline int scan6_is_banned(unsigned lcore_id, const struct rte_ipv6_addr *src_ip6, uint64_t now_tsc) {
	struct scan_table *t = &scan6_tbls[lcore_id];
	if (!t->h || !t->states || !src_ip6) {
		return 0;
	}
	int32_t pos = rte_hash_lookup(t->h, src_ip6);
	if (pos < 0) {
		return 0;
	}
	struct scan6_state *st = &((struct scan6_state *)t->states)[(uint32_t)pos];
	return st->ban_until_tsc && now_tsc < st->ban_until_tsc;
}

static inline void scan4_track_syn(unsigned lcore_id, uint32_t src_ip, uint16_t dst_port, uint64_t now_tsc, uint64_t hz) {
	struct scan_table *t = &scan4_tbls[lcore_id];
	if (!t->h || !t->states || hz == 0) {
		return;
	}
	int32_t pos = rte_hash_lookup(t->h, &src_ip);
	if (pos < 0) {
		pos = rte_hash_add_key(t->h, &src_ip);
		if (pos < 0) {
			return;
		}
	}
	struct scan4_state *st = &((struct scan4_state *)t->states)[(uint32_t)pos];
	if (st->ban_until_tsc && now_tsc < st->ban_until_tsc) {
		return;
	}
	if (st->window_start_tsc == 0 || now_tsc - st->window_start_tsc >= hz) {
		st->window_start_tsc = now_tsc;
		st->port_bits = 0;
		st->port_cnt = 0;
	}
	uint64_t bit = 1ULL << (dst_port & 63);
	if ((st->port_bits & bit) == 0) {
		st->port_bits |= bit;
		st->port_cnt++;
		if (st->port_cnt > attack_top_scan4_ports[lcore_id]) {
			attack_top_scan4_ports[lcore_id] = st->port_cnt;
			attack_top_scan4_ip[lcore_id] = src_ip;
		}
		if (attack_mitigation_enabled && attack_scan_ports_per_sec && st->port_cnt >= attack_scan_ports_per_sec) {
			st->ban_until_tsc = now_tsc + (uint64_t)attack_ban_seconds * hz;
			rte_atomic64_inc(&attack_scan_events_cnt[lcore_id]);
			rte_atomic64_inc(&attack_scan_banned_cnt[lcore_id]);
		}
	}
}

static inline void scan6_track_syn(unsigned lcore_id, const struct rte_ipv6_addr *src_ip6, uint16_t dst_port, uint64_t now_tsc, uint64_t hz) {
	struct scan_table *t = &scan6_tbls[lcore_id];
	if (!t->h || !t->states || !src_ip6 || hz == 0) {
		return;
	}
	int32_t pos = rte_hash_lookup(t->h, src_ip6);
	if (pos < 0) {
		pos = rte_hash_add_key(t->h, src_ip6);
		if (pos < 0) {
			return;
		}
	}
	struct scan6_state *st = &((struct scan6_state *)t->states)[(uint32_t)pos];
	if (st->ban_until_tsc && now_tsc < st->ban_until_tsc) {
		return;
	}
	if (st->window_start_tsc == 0 || now_tsc - st->window_start_tsc >= hz) {
		st->window_start_tsc = now_tsc;
		st->port_bits = 0;
		st->port_cnt = 0;
	}
	uint64_t bit = 1ULL << (dst_port & 63);
	if ((st->port_bits & bit) == 0) {
		st->port_bits |= bit;
		st->port_cnt++;
		if (st->port_cnt > attack_top_scan6_ports[lcore_id]) {
			attack_top_scan6_ports[lcore_id] = st->port_cnt;
			attack_top_scan6_ip[lcore_id] = *src_ip6;
		}
		if (attack_mitigation_enabled && attack_scan_ports_per_sec && st->port_cnt >= attack_scan_ports_per_sec) {
			st->ban_until_tsc = now_tsc + (uint64_t)attack_ban_seconds * hz;
			rte_atomic64_inc(&attack_scan_events_cnt[lcore_id]);
			rte_atomic64_inc(&attack_scan_banned_cnt[lcore_id]);
		}
	}
}

static struct session_table *session_table_for_lcore(unsigned lcore_id) {
	if (lcore_id >= RTE_MAX_LCORE || !session_table_inited[lcore_id]) {
		return NULL;
	}
	return &session_tables[lcore_id];
}

static struct session6_table *session6_table_for_lcore(unsigned lcore_id) {
	if (lcore_id >= RTE_MAX_LCORE || !session6_table_inited[lcore_id]) {
		return NULL;
	}
	return &session6_tables[lcore_id];
}

/* Per-port statistics struct */
struct __rte_cache_aligned l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
};
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];

static int portstats_shared_sync(void) {
	if (!portstats_shared_cfg) {
		return -1;
	}
	portstats_shared_cfg->enabled_port_mask = l2fwd_enabled_port_mask;
	for (uint16_t p = 0; p < RTE_MAX_ETHPORTS; p++) {
		portstats_shared_cfg->ports[p].rx = port_statistics[p].rx;
		portstats_shared_cfg->ports[p].tx = port_statistics[p].tx;
		portstats_shared_cfg->ports[p].dropped = port_statistics[p].dropped;
		struct rte_eth_link link;
		memset(&link, 0, sizeof(link));
		rte_eth_link_get_nowait(p, &link);
		portstats_shared_cfg->ports[p].link_up = (uint8_t)link.link_status;
		portstats_shared_cfg->ports[p].link_speed = (uint32_t)link.link_speed;
		portstats_shared_cfg->ports[p].link_duplex = (uint8_t)link.link_duplex;
		memcpy(portstats_shared_cfg->ports[p].mac, &l2fwd_ports_eth_addr[p], RTE_ETHER_ADDR_LEN);
	}
	rte_wmb();
	uint64_t v = rte_atomic64_read(&portstats_shared_cfg->version);
	rte_atomic64_set(&portstats_shared_cfg->version, v + 1);
	return 0;
}

static void send_gratuitous_arp(void) {
	for (uint16_t portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		if (!ifcfgs[portid].configured)
			continue;
		if (!arp_tbl)
			continue;

		struct rte_mbuf *m = arp_build_request(
			l2fwd_pktmbuf_pool,
			&l2fwd_ports_eth_addr[portid],
			ifcfgs[portid].ip,
			ifcfgs[portid].ip
		);
		if (!m)
			continue;

		struct rte_eth_dev_tx_buffer *buffer = tx_buffer[portid];
		int sent = rte_eth_tx_buffer(portid, 0, buffer, m);
		if (sent)
			port_statistics[portid].tx += (uint64_t)sent;
		sent = rte_eth_tx_buffer_flush(portid, 0, buffer);
		if (sent)
			port_statistics[portid].tx += (uint64_t)sent;
	}
}
 
#define MAX_TIMER_PERIOD 86400 /* 1 day max */
/* A tsc-based timer responsible for triggering statistics printout */
static uint64_t timer_period = 10; /* default period is 10 seconds */

struct pending_route {
	uint32_t dst_ip;
	uint8_t depth;
	uint32_t next_hop_ip;
	uint16_t out_port;
};

struct pending_route6 {
	struct rte_ipv6_addr dst_ip;
	uint8_t depth;
	struct rte_ipv6_addr next_hop_ip;
	uint16_t out_port;
	uint16_t reserved;
};

#define MAX_PENDING_ROUTES 1024
static struct pending_route pending_routes[MAX_PENDING_ROUTES];
static uint32_t pending_route_count;
static struct pending_route6 pending_routes6[MAX_PENDING_ROUTES];
static uint32_t pending_route6_count;
 
static int acl_seed_default_rules(struct acl_ctx *ctx) {
	struct acl_rule deny_private = {
		.src_ip = rte_be_to_cpu_32(RTE_IPV4(10, 0, 0, 0)),
		.src_mask = rte_be_to_cpu_32(RTE_IPV4(255, 0, 0, 0)),
		.dst_ip = 0,
		.dst_mask = 0,
		.src_port_min = 0,
		.src_port_max = 0,
		.dst_port_min = 0,
		.dst_port_max = 0,
		.proto = 0,
		.match_ports = 0,
		.allow = 0,
	};
	struct acl_rule allow_all = {
		.src_ip = 0,
		.src_mask = 0,
		.dst_ip = 0,
		.dst_mask = 0,
		.src_port_min = 0,
		.src_port_max = 0,
		.dst_port_min = 0,
		.dst_port_max = 0,
		.proto = 0,
		.match_ports = 0,
		.allow = 1,
	};
	if (acl_add_rule(ctx, &deny_private) != 0) {
		return -1;
	}
	if (acl_add_rule(ctx, &allow_all) != 0) {
		return -1;
	}
	return 0;
}

static int acl6_seed_default_rules(struct acl6_ctx *ctx) {
	struct acl6_rule allow_all;
	memset(&allow_all, 0, sizeof(allow_all));
	allow_all.src_depth = 0;
	allow_all.dst_depth = 0;
	allow_all.match_ports = 0;
	allow_all.proto = 0;
	allow_all.allow = 1;
	return acl6_add_rule(ctx, &allow_all);
}

static inline struct acl_ctx *acl_runtime_active_ctx(void) {
	uint32_t idx = rte_atomic32_read(&acl_rt.active) & 1u;
	return &acl_rt.ctx[idx];
}

static inline struct acl6_ctx *acl6_runtime_active_ctx(void) {
	uint32_t idx = rte_atomic32_read(&acl6_rt.active) & 1u;
	return &acl6_rt.ctx[idx];
}

static int acl_shared_sync(const struct acl_ctx *ctx, uint64_t version) {
	if (!acl_shared_cfg || !ctx) {
		return -1;
	}
	rte_rwlock_read_lock((rte_rwlock_t *)&ctx->lock);
	uint32_t count = ctx->count;
	if (count > ACL_MAX_RULES) {
		count = ACL_MAX_RULES;
	}
	memcpy(acl_shared_cfg->rules, ctx->rules, sizeof(struct acl_rule) * count);
	acl_shared_cfg->count = count;
	rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);
	rte_wmb();
	rte_atomic64_set(&acl_shared_cfg->version, version);
	return 0;
}

static int acl_ipc_init(void) {
	acl_cmd_ring = rte_ring_create(ACL_CMD_RING_NAME, ACL_CMD_RING_SIZE, rte_socket_id(), 0);
	if (!acl_cmd_ring) {
		return -1;
	}
	acl_resp_ring = rte_ring_create(ACL_RESP_RING_NAME, ACL_RESP_RING_SIZE, rte_socket_id(), 0);
	if (!acl_resp_ring) {
		return -1;
	}
	const struct rte_memzone *mz = rte_memzone_reserve(ACL_SHARED_CFG_NAME, sizeof(struct acl_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	acl_shared_cfg = mz->addr;
	memset(acl_shared_cfg, 0, sizeof(*acl_shared_cfg));
	rte_atomic64_init(&acl_shared_cfg->version);
	return acl_shared_sync(acl_runtime_active_ctx(), rte_atomic64_read(&acl_rt.version));
}

static int acl6_shared_sync(const struct acl6_ctx *ctx, uint64_t version) {
	if (!acl6_shared_cfg || !ctx) {
		return -1;
	}
	rte_rwlock_read_lock((rte_rwlock_t *)&ctx->lock);
	uint32_t count = ctx->count;
	if (count > ACL6_MAX_RULES) {
		count = ACL6_MAX_RULES;
	}
	memcpy(acl6_shared_cfg->rules, ctx->rules, sizeof(struct acl6_rule) * count);
	acl6_shared_cfg->count = count;
	rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);
	rte_wmb();
	rte_atomic64_set(&acl6_shared_cfg->version, version);
	return 0;
}

static int acl6_ipc_init(void) {
	acl6_cmd_ring = rte_ring_create(ACL6_CMD_RING_NAME, ACL6_CMD_RING_SIZE, rte_socket_id(), 0);
	if (!acl6_cmd_ring) {
		return -1;
	}
	acl6_resp_ring = rte_ring_create(ACL6_RESP_RING_NAME, ACL6_RESP_RING_SIZE, rte_socket_id(), 0);
	if (!acl6_resp_ring) {
		return -1;
	}
	const struct rte_memzone *mz = rte_memzone_reserve(ACL6_SHARED_CFG_NAME, sizeof(struct acl6_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	acl6_shared_cfg = mz->addr;
	memset(acl6_shared_cfg, 0, sizeof(*acl6_shared_cfg));
	rte_atomic64_init(&acl6_shared_cfg->version);
	return acl6_shared_sync(acl6_runtime_active_ctx(), rte_atomic64_read(&acl6_rt.version));
}

static int session_ipc_init(void) {
	const struct rte_memzone *mz = rte_memzone_reserve(SESSION_SHARED_NAME, sizeof(struct session_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	session_shared_cfg = mz->addr;
	memset(session_shared_cfg, 0, sizeof(*session_shared_cfg));
	rte_atomic64_init(&session_shared_cfg->version);
	return 0;
}

static int session6_ipc_init(void) {
	const struct rte_memzone *mz = rte_memzone_reserve(SESSION6_SHARED_NAME, sizeof(struct session6_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	session6_shared_cfg = mz->addr;
	memset(session6_shared_cfg, 0, sizeof(*session6_shared_cfg));
	rte_atomic64_init(&session6_shared_cfg->version);
	return 0;
}

static int portstats_ipc_init(uint16_t nb_ports) {
	const struct rte_memzone *mz = rte_memzone_reserve(PORTSTATS_SHARED_NAME, sizeof(struct portstats_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	portstats_shared_cfg = mz->addr;
	memset(portstats_shared_cfg, 0, sizeof(*portstats_shared_cfg));
	rte_atomic64_init(&portstats_shared_cfg->version);
	portstats_shared_cfg->enabled_port_mask = l2fwd_enabled_port_mask;
	portstats_shared_cfg->nb_ports = nb_ports;
	portstats_shared_cfg->tsc_hz = rte_get_timer_hz();
	return 0;
}

static int denylog_ipc_init(void) {
	const struct rte_memzone *mz = rte_memzone_reserve(DENYLOG_SHARED_NAME, sizeof(struct denylog_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	denylog_shared_cfg = mz->addr;
	memset(denylog_shared_cfg, 0, sizeof(*denylog_shared_cfg));
	rte_atomic64_init(&denylog_shared_cfg->version);
	rte_spinlock_init(&denylog_shared_cfg->lock);
	denylog_shared_cfg->tsc_hz = rte_get_timer_hz();
	return 0;
}

static int denylog6_ipc_init(void) {
	const struct rte_memzone *mz = rte_memzone_reserve(DENYLOG6_SHARED_NAME, sizeof(struct denylog6_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	denylog6_shared_cfg = mz->addr;
	memset(denylog6_shared_cfg, 0, sizeof(*denylog6_shared_cfg));
	rte_atomic64_init(&denylog6_shared_cfg->version);
	rte_spinlock_init(&denylog6_shared_cfg->lock);
	denylog6_shared_cfg->tsc_hz = rte_get_timer_hz();
	return 0;
}

static int route6_ipc_init(void) {
	const struct rte_memzone *mz = rte_memzone_reserve(ROUTE6_SHARED_NAME, sizeof(struct route6_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	route6_shared_cfg = mz->addr;
	memset(route6_shared_cfg, 0, sizeof(*route6_shared_cfg));
	rte_atomic64_init(&route6_shared_cfg->version);
	return 0;
}

static int portcfg_ipc_init(void) {
    const struct rte_memzone *mz = rte_memzone_reserve(PORTCFG_SHARED_NAME, sizeof(struct portcfg_shared_cfg),
        rte_socket_id(), 0);
    if (!mz) {
        return -1;
    }
    portcfg_shared_cfg = mz->addr;
    memset(portcfg_shared_cfg, 0, sizeof(*portcfg_shared_cfg));
    rte_atomic64_init(&portcfg_shared_cfg->version);
    rte_wmb();
    rte_atomic64_set(&portcfg_shared_cfg->version, 1);
    return 0;
}

static int route_ipc_init(void) {
    const struct rte_memzone *mz = rte_memzone_reserve(ROUTE_SHARED_NAME, sizeof(struct route_shared_cfg),
        rte_socket_id(), 0);
    if (!mz) {
        return -1;
    }
    route_shared_cfg = mz->addr;
    memset(route_shared_cfg, 0, sizeof(*route_shared_cfg));
    rte_atomic64_init(&route_shared_cfg->version);
    return 0;
}

static int attack_ipc_init(void) {
	const struct rte_memzone *mz = rte_memzone_reserve(ATTACK_SHARED_NAME, sizeof(struct attack_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	attack_shared_cfg = mz->addr;
	memset(attack_shared_cfg, 0, sizeof(*attack_shared_cfg));
	rte_atomic64_init(&attack_shared_cfg->version);
	attack_shared_cfg->scan_ports_per_sec = 50;
	attack_shared_cfg->ban_seconds = 60;
	attack_shared_cfg->mitigation_enabled = 0;
	rte_wmb();
	rte_atomic64_set(&attack_shared_cfg->version, 1);
	return 0;
}

static int acl_hit_ipc_init(void) {
	const struct rte_memzone *mz = rte_memzone_reserve(ACL_HIT_SHARED_NAME, sizeof(struct acl_hit_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	acl_hit_shared_cfg = mz->addr;
	memset(acl_hit_shared_cfg, 0, sizeof(*acl_hit_shared_cfg));
	rte_atomic64_init(&acl_hit_shared_cfg->version);
	rte_atomic64_set(&acl_hit_shared_cfg->version, 1);
	return 0;
}

static int acl6_hit_ipc_init(void) {
	const struct rte_memzone *mz = rte_memzone_reserve(ACL6_HIT_SHARED_NAME, sizeof(struct acl6_hit_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	acl6_hit_shared_cfg = mz->addr;
	memset(acl6_hit_shared_cfg, 0, sizeof(*acl6_hit_shared_cfg));
	rte_atomic64_init(&acl6_hit_shared_cfg->version);
	rte_atomic64_set(&acl6_hit_shared_cfg->version, 1);
	return 0;
}

static int rlim_ipc_init(void) {
	const struct rte_memzone *mz = rte_memzone_reserve(RLIM_SHARED_NAME, sizeof(struct rlim_shared_cfg),
		rte_socket_id(), 0);
	if (!mz) {
		return -1;
	}
	rlim_shared_cfg = mz->addr;
	memset(rlim_shared_cfg, 0, sizeof(*rlim_shared_cfg));
	rte_atomic64_init(&rlim_shared_cfg->version);
	rlim_shared_cfg->syn_pps = rlim_syn_pps;
	rlim_shared_cfg->syn_burst = rlim_syn_burst;
	rlim_shared_cfg->udp_pps = rlim_udp_pps;
	rlim_shared_cfg->udp_burst = rlim_udp_burst;
	rte_wmb();
	rte_atomic64_set(&rlim_shared_cfg->version, 1);
	return 0;
}

static void rlim_apply_shared_cfg(void) {
	if (!rlim_shared_cfg) {
		return;
	}
	static uint64_t last_v;
	uint64_t v1 = rte_atomic64_read(&rlim_shared_cfg->version);
	if (v1 == 0 || v1 == last_v) {
		return;
	}
	uint32_t syn_pps = rlim_shared_cfg->syn_pps;
	uint32_t syn_burst = rlim_shared_cfg->syn_burst;
	uint32_t udp_pps = rlim_shared_cfg->udp_pps;
	uint32_t udp_burst = rlim_shared_cfg->udp_burst;
	rte_rmb();
	uint64_t v2 = rte_atomic64_read(&rlim_shared_cfg->version);
	if (v2 != v1) {
		return;
	}
	last_v = v2;
	rlim_syn_pps = syn_pps;
	rlim_syn_burst = syn_burst;
	rlim_udp_pps = udp_pps;
	rlim_udp_burst = udp_burst;
	if (rlim_syn_pps == 0) {
		rlim_syn_burst = 0;
	}
	if (rlim_udp_pps == 0) {
		rlim_udp_burst = 0;
	}
	if ((rlim_syn_pps || rlim_udp_pps)) {
		unsigned lcore_id;
		RTE_LCORE_FOREACH(lcore_id) {
			if (lcore_id >= RTE_MAX_LCORE || rlim_inited[lcore_id]) {
				continue;
			}
			char name[64];
			if (rlim_syn_pps) {
				snprintf(name, sizeof(name), "rlim_syn_%u", lcore_id);
				(void)rlim_table_init(&rlim_syn_tbls[lcore_id], name, 32768, sizeof(uint32_t), rte_socket_id());
				snprintf(name, sizeof(name), "rlim6_syn_%u", lcore_id);
				(void)rlim_table_init(&rlim6_syn_tbls[lcore_id], name, 32768, sizeof(struct rte_ipv6_addr), rte_socket_id());
			}
			if (rlim_udp_pps) {
				snprintf(name, sizeof(name), "rlim_udp_%u", lcore_id);
				(void)rlim_table_init(&rlim_udp_tbls[lcore_id], name, 32768, sizeof(uint32_t), rte_socket_id());
				snprintf(name, sizeof(name), "rlim6_udp_%u", lcore_id);
				(void)rlim_table_init(&rlim6_udp_tbls[lcore_id], name, 32768, sizeof(struct rte_ipv6_addr), rte_socket_id());
			}
			rlim_inited[lcore_id] = 1;
		}
	}
}

static void attack_apply_shared_cfg(void) {
	if (!attack_shared_cfg) {
		return;
	}
	static uint64_t last_v;
	uint64_t v1 = rte_atomic64_read(&attack_shared_cfg->version);
	if (v1 == 0 || v1 == last_v) {
		return;
	}
	uint32_t scan_ports = attack_shared_cfg->scan_ports_per_sec;
	uint32_t ban_sec = attack_shared_cfg->ban_seconds;
	uint8_t mit = attack_shared_cfg->mitigation_enabled;
	rte_rmb();
	uint64_t v2 = rte_atomic64_read(&attack_shared_cfg->version);
	if (v2 != v1) {
		return;
	}
	last_v = v2;
	attack_scan_ports_per_sec = scan_ports ? scan_ports : 50;
	attack_ban_seconds = ban_sec ? ban_sec : 60;
	attack_mitigation_enabled = mit ? 1 : 0;
}

static void attack_stats_sync(uint64_t now_tsc) {
	if (!attack_shared_cfg) {
		return;
	}
	static uint64_t last_tsc;
	uint64_t hz = rte_get_timer_hz();
	if (!hz) {
		return;
	}
	if (last_tsc == 0) {
		last_tsc = now_tsc;
		return;
	}
	if (now_tsc - last_tsc < hz) {
		return;
	}
	uint64_t syn = 0;
	uint64_t udp = 0;
	uint64_t scan_events = 0;
	uint64_t scan_banned = 0;
	uint32_t top4_ports = 0;
	uint32_t top4_ip = 0;
	uint32_t top6_ports = 0;
	struct rte_ipv6_addr top6_ip = RTE_IPV6_ADDR_UNSPEC;

	unsigned lcore_id;
	RTE_LCORE_FOREACH(lcore_id) {
		uint64_t v;
		v = rte_atomic64_read(&attack_syn_cnt[lcore_id]);
		syn += v;
		rte_atomic64_set(&attack_syn_cnt[lcore_id], 0);
		v = rte_atomic64_read(&attack_udp_cnt[lcore_id]);
		udp += v;
		rte_atomic64_set(&attack_udp_cnt[lcore_id], 0);
		v = rte_atomic64_read(&attack_scan_events_cnt[lcore_id]);
		scan_events += v;
		rte_atomic64_set(&attack_scan_events_cnt[lcore_id], 0);
		v = rte_atomic64_read(&attack_scan_banned_cnt[lcore_id]);
		scan_banned += v;
		rte_atomic64_set(&attack_scan_banned_cnt[lcore_id], 0);

		if (attack_top_scan4_ports[lcore_id] > top4_ports) {
			top4_ports = attack_top_scan4_ports[lcore_id];
			top4_ip = attack_top_scan4_ip[lcore_id];
		}
		if (attack_top_scan6_ports[lcore_id] > top6_ports) {
			top6_ports = attack_top_scan6_ports[lcore_id];
			top6_ip = attack_top_scan6_ip[lcore_id];
		}
		attack_top_scan4_ports[lcore_id] = 0;
		attack_top_scan4_ip[lcore_id] = 0;
		attack_top_scan6_ports[lcore_id] = 0;
		memset(&attack_top_scan6_ip[lcore_id], 0, sizeof(attack_top_scan6_ip[lcore_id]));
	}

	attack_shared_cfg->syn_pps = syn > UINT32_MAX ? UINT32_MAX : (uint32_t)syn;
	attack_shared_cfg->udp_pps = udp > UINT32_MAX ? UINT32_MAX : (uint32_t)udp;
	attack_shared_cfg->scan_events = attack_shared_cfg->scan_events + (uint32_t)(scan_events > UINT32_MAX ? UINT32_MAX : scan_events);
	attack_shared_cfg->scan_banned = attack_shared_cfg->scan_banned + (uint32_t)(scan_banned > UINT32_MAX ? UINT32_MAX : scan_banned);
	attack_shared_cfg->top_scan4_ip = top4_ip;
	attack_shared_cfg->top_scan4_ports = top4_ports;
	attack_shared_cfg->top_scan6_ip = top6_ip;
	attack_shared_cfg->top_scan6_ports = top6_ports;
	rte_wmb();
	uint64_t vcur = rte_atomic64_read(&attack_shared_cfg->version);
	rte_atomic64_set(&attack_shared_cfg->version, vcur + 1);
	last_tsc = now_tsc;
}

static void denylog_add(uint64_t tsc, uint16_t in_port, uint32_t src_ip, uint32_t dst_ip,
	uint8_t proto, uint16_t src_port, uint16_t dst_port, uint32_t rule_index) {
	if (!denylog_shared_cfg) {
		return;
	}
	rte_spinlock_lock(&denylog_shared_cfg->lock);
	uint32_t pos = denylog_shared_cfg->head;
	struct denylog_entry *e = &denylog_shared_cfg->entries[pos];
	e->tsc = tsc;
	e->in_port = (uint8_t)in_port;
	e->proto = proto;
	e->src_ip = src_ip;
	e->dst_ip = dst_ip;
	e->src_port = src_port;
	e->dst_port = dst_port;
	e->rule_index = rule_index;
	denylog_shared_cfg->head = (pos + 1) % DENYLOG_MAX;
	if (denylog_shared_cfg->count < DENYLOG_MAX) {
		denylog_shared_cfg->count++;
	}
	uint64_t v = rte_atomic64_read(&denylog_shared_cfg->version);
	rte_atomic64_set(&denylog_shared_cfg->version, v + 1);
	rte_spinlock_unlock(&denylog_shared_cfg->lock);
}

static void denylog6_add(uint64_t tsc, uint16_t in_port, const struct rte_ipv6_addr *src_ip6, const struct rte_ipv6_addr *dst_ip6,
	uint8_t proto, uint16_t src_port, uint16_t dst_port, uint32_t rule_index) {
	if (!denylog6_shared_cfg || !src_ip6 || !dst_ip6) {
		return;
	}
	rte_spinlock_lock(&denylog6_shared_cfg->lock);
	uint32_t pos = denylog6_shared_cfg->head;
	struct denylog6_entry *e = &denylog6_shared_cfg->entries[pos];
	e->tsc = tsc;
	e->in_port = (uint8_t)in_port;
	e->proto = proto;
	e->src_ip6 = *src_ip6;
	e->dst_ip6 = *dst_ip6;
	e->src_port = src_port;
	e->dst_port = dst_port;
	e->rule_index = rule_index;
	denylog6_shared_cfg->head = (pos + 1) % DENYLOG6_MAX;
	if (denylog6_shared_cfg->count < DENYLOG6_MAX) {
		denylog6_shared_cfg->count++;
	}
	uint64_t v = rte_atomic64_read(&denylog6_shared_cfg->version);
	rte_atomic64_set(&denylog6_shared_cfg->version, v + 1);
	rte_spinlock_unlock(&denylog6_shared_cfg->lock);
}

static int session_shared_sync(uint64_t now_tsc) {
	if (!session_shared_cfg) {
		return -1;
	}
	uint32_t out = 0;
	unsigned lcore_id;
	RTE_LCORE_FOREACH(lcore_id) {
		struct session_table *table = session_table_for_lcore(lcore_id);
		if (!table) {
			continue;
		}
		if (out >= SESSION_MAX_EXPORT) {
			break;
		}
		uint32_t left = SESSION_MAX_EXPORT - out;
		out += session_export(table, &session_shared_cfg->keys[out], &session_shared_cfg->entries[out], left,
			now_tsc, session_timeout_tsc);
	}
	session_shared_cfg->count = out;
	rte_wmb();
	uint64_t v = rte_atomic64_read(&session_shared_cfg->version);
	rte_atomic64_set(&session_shared_cfg->version, v + 1);
	return 0;
}

static int session6_shared_sync(uint64_t now_tsc) {
	if (!session6_shared_cfg) {
		return -1;
	}
	uint32_t out = 0;
	unsigned lcore_id;
	RTE_LCORE_FOREACH(lcore_id) {
		struct session6_table *table = session6_table_for_lcore(lcore_id);
		if (!table) {
			continue;
		}
		if (out >= SESSION6_MAX_EXPORT) {
			break;
		}
		uint32_t left = SESSION6_MAX_EXPORT - out;
		out += session6_export(table, &session6_shared_cfg->keys[out], &session6_shared_cfg->entries[out], left,
			now_tsc, session_timeout_tsc);
	}
	session6_shared_cfg->count = out;
	rte_wmb();
	uint64_t v = rte_atomic64_read(&session6_shared_cfg->version);
	rte_atomic64_set(&session6_shared_cfg->version, v + 1);
	return 0;
}

static void acl_hit_reset_all(void) {
	unsigned lcore_id;
	RTE_LCORE_FOREACH(lcore_id) {
		for (uint32_t i = 0; i < ACL_MAX_RULES; i++) {
			rte_atomic64_set(&acl_deny_pkts[lcore_id][i], 0);
			rte_atomic64_set(&acl_deny_bytes[lcore_id][i], 0);
		}
	}
}

static void acl6_hit_reset_all(void) {
	unsigned lcore_id;
	RTE_LCORE_FOREACH(lcore_id) {
		for (uint32_t i = 0; i < ACL6_MAX_RULES; i++) {
			rte_atomic64_set(&acl6_deny_pkts[lcore_id][i], 0);
			rte_atomic64_set(&acl6_deny_bytes[lcore_id][i], 0);
		}
	}
}

static int acl_hit_shared_sync(void) {
	if (!acl_hit_shared_cfg) {
		return -1;
	}
	static uint64_t last_rule_ver;
	uint64_t rv = rte_atomic64_read(&acl_rt.version);
	if (last_rule_ver != 0 && rv != last_rule_ver) {
		acl_hit_reset_all();
	}
	last_rule_ver = rv;
	uint32_t count = acl_count(acl_runtime_active_ctx());
	if (count > ACL_MAX_RULES) {
		count = ACL_MAX_RULES;
	}
	acl_hit_shared_cfg->rule_version = rv;
	acl_hit_shared_cfg->count = count;
	for (uint32_t i = 0; i < count; i++) {
		uint64_t pk = 0;
		uint64_t by = 0;
		unsigned lcore_id;
		RTE_LCORE_FOREACH(lcore_id) {
			pk += rte_atomic64_read(&acl_deny_pkts[lcore_id][i]);
			by += rte_atomic64_read(&acl_deny_bytes[lcore_id][i]);
		}
		acl_hit_shared_cfg->deny_pkts[i] = pk;
		acl_hit_shared_cfg->deny_bytes[i] = by;
	}
	rte_wmb();
	uint64_t v = rte_atomic64_read(&acl_hit_shared_cfg->version);
	rte_atomic64_set(&acl_hit_shared_cfg->version, v + 1);
	return 0;
}

static int acl6_hit_shared_sync(void) {
	if (!acl6_hit_shared_cfg) {
		return -1;
	}
	static uint64_t last_rule_ver;
	uint64_t rv = rte_atomic64_read(&acl6_rt.version);
	if (last_rule_ver != 0 && rv != last_rule_ver) {
		acl6_hit_reset_all();
	}
	last_rule_ver = rv;
	uint32_t count = acl6_count(acl6_runtime_active_ctx());
	if (count > ACL6_MAX_RULES) {
		count = ACL6_MAX_RULES;
	}
	acl6_hit_shared_cfg->rule_version = rv;
	acl6_hit_shared_cfg->count = count;
	for (uint32_t i = 0; i < count; i++) {
		uint64_t pk = 0;
		uint64_t by = 0;
		unsigned lcore_id;
		RTE_LCORE_FOREACH(lcore_id) {
			pk += rte_atomic64_read(&acl6_deny_pkts[lcore_id][i]);
			by += rte_atomic64_read(&acl6_deny_bytes[lcore_id][i]);
		}
		acl6_hit_shared_cfg->deny_pkts[i] = pk;
		acl6_hit_shared_cfg->deny_bytes[i] = by;
	}
	rte_wmb();
	uint64_t v = rte_atomic64_read(&acl6_hit_shared_cfg->version);
	rte_atomic64_set(&acl6_hit_shared_cfg->version, v + 1);
	return 0;
}

static void session_expire_all(uint64_t now_tsc) {
	unsigned lcore_id;
	RTE_LCORE_FOREACH(lcore_id) {
		struct session_table *table = session_table_for_lcore(lcore_id);
		if (!table) {
			continue;
		}
		session_soft_expire(table, now_tsc, session_timeout_tsc);
	}
}

static void session6_expire_all(uint64_t now_tsc) {
	unsigned lcore_id;
	RTE_LCORE_FOREACH(lcore_id) {
		struct session6_table *table = session6_table_for_lcore(lcore_id);
		if (!table) {
			continue;
		}
		session6_soft_expire(table, now_tsc, session_timeout_tsc);
	}
}

static void route6_reclaim(void) {
	if (!ipv6_rt_reclaim) {
		return;
	}
	route6_table_free(ipv6_rt_reclaim);
	ipv6_rt_reclaim = NULL;
}

static void route6_apply_shared_cfg(void) {
	static uint64_t last_version;
	if (!route6_shared_cfg) {
		return;
	}
	uint64_t v = rte_atomic64_read(&route6_shared_cfg->version);
	if (v == 0 || v == last_version) {
		return;
	}

	struct ifcfg6_item ifs[IFACE6_MAX_PORTS];
	struct route6_item routes[ROUTE6_MAX];
	uint32_t count = 0;
	uint64_t v2 = 0;
	for (int i = 0; i < 200; i++) {
		uint64_t v1 = rte_atomic64_read(&route6_shared_cfg->version);
		uint32_t c = route6_shared_cfg->route_count;
		if (c > ROUTE6_MAX) {
			c = ROUTE6_MAX;
		}
		memcpy(ifs, route6_shared_cfg->ifcfg6s, sizeof(ifs));
		memcpy(routes, route6_shared_cfg->routes, sizeof(struct route6_item) * c);
		rte_rmb();
		v2 = rte_atomic64_read(&route6_shared_cfg->version);
		if (v1 == v2) {
			count = c;
			break;
		}
	}
	if (v2 == 0 || v2 == last_version) {
		return;
	}

	uint32_t cur = rte_atomic32_read(&ipv6_rt_active_idx) & 1u;
	uint32_t next = cur ^ 1u;

	struct nd_ifcfg *next_if = ifcfg6_tbls[next];
	memset(next_if, 0, sizeof(ifcfg6_tbls[next]));
	for (uint16_t p = 0; p < RTE_MAX_ETHPORTS && p < IFACE6_MAX_PORTS; p++) {
		next_if[p].ip = ifs[p].ip;
		next_if[p].depth = ifs[p].depth;
		next_if[p].configured = ifs[p].configured;
	}

	int ipv6_on = 0;
	for (uint16_t p = 0; p < RTE_MAX_ETHPORTS && p < IFACE6_MAX_PORTS; p++) {
		if (next_if[p].configured) {
			ipv6_on = 1;
			break;
		}
	}
	if (count) {
		ipv6_on = 1;
	}

	struct route6_table *new_rt = NULL;
	if (ipv6_on) {
		char name[32];
		snprintf(name, sizeof(name), "ipv6_rt_dyn_%u", next);
		new_rt = route6_table_create(name, rte_socket_id(), 2048);
		if (new_rt) {
			const struct rte_ipv6_addr unspec = RTE_IPV6_ADDR_UNSPEC;
			for (uint16_t p = 0; p < RTE_MAX_ETHPORTS && p < IFACE6_MAX_PORTS; p++) {
				if (!next_if[p].configured) {
					continue;
				}
				struct rte_ipv6_addr net = next_if[p].ip;
				rte_ipv6_addr_mask(&net, next_if[p].depth);
				route6_add(new_rt, &net, next_if[p].depth, &unspec, p);
			}
			for (uint32_t i = 0; i < count; i++) {
				route6_add(new_rt, &routes[i].dst, routes[i].depth, &routes[i].next_hop, routes[i].out_port);
			}
		}
	}

	struct route6_table *old_rt = ipv6_rt_tbls[cur];
	ipv6_rt_tbls[next] = new_rt;
	rte_wmb();
	rte_atomic32_set(&ifcfg6_active_idx, next);
	rte_atomic32_set(&ipv6_rt_active_idx, next);
	ipv6_rt_tbls[cur] = NULL;
	ipv6_rt_reclaim = old_rt;

	last_version = v2;
}

static void portcfg_apply_shared_cfg(void) {
	static uint64_t last_version;
	if (!portcfg_shared_cfg) {
		return;
	}
	uint64_t v = rte_atomic64_read(&portcfg_shared_cfg->version);
	if (v == 0 || v == last_version) {
		return;
	}
	uint64_t v1 = v;
	rte_rmb();
	uint64_t v2 = rte_atomic64_read(&portcfg_shared_cfg->version);
	if (v2 != v1) {
		return;
	}

	for (uint16_t p = 0; p < RTE_MAX_ETHPORTS && p < PORTCFG_MAX_PORTS; p++) {
		ifcfgs[p].ip = portcfg_shared_cfg->ports[p].ip;
		ifcfgs[p].mask = portcfg_shared_cfg->ports[p].mask;
		ifcfgs[p].configured = portcfg_shared_cfg->ports[p].configured;
	}

    last_version = v2;
}

static void route_reclaim(void) {
    if (!ipv4_rt_reclaim) {
        return;
    }
    route_table_free(ipv4_rt_reclaim);
    ipv4_rt_reclaim = NULL;
}

static void route_apply_shared_cfg(void) {
    static uint64_t last_version;
    if (!route_shared_cfg) {
        return;
    }
    uint64_t v = rte_atomic64_read(&route_shared_cfg->version);
    if (v == 0 || v == last_version) {
        return;
    }

    struct route4_item routes[ROUTE_MAX];
    uint32_t count = 0;
    uint64_t v2 = 0;
    for (int i = 0; i < 200; i++) {
        uint64_t v1 = rte_atomic64_read(&route_shared_cfg->version);
        uint32_t c = route_shared_cfg->route_count;
        if (c > ROUTE_MAX) {
            c = ROUTE_MAX;
        }
        memcpy(routes, route_shared_cfg->routes, sizeof(struct route4_item) * c);
        rte_rmb();
        v2 = rte_atomic64_read(&route_shared_cfg->version);
        if (v1 == v2) {
            count = c;
            break;
        }
    }
    if (v2 == 0 || v2 == last_version) {
        return;
    }

    uint32_t cur = rte_atomic32_read(&ipv4_rt_active_idx) & 1u;
    uint32_t next = cur ^ 1u;

    int ipv4_on = 0;
    for (uint16_t p = 0; p < RTE_MAX_ETHPORTS; p++) {
        if (ifcfgs[p].configured) {
            ipv4_on = 1;
            break;
        }
    }
    if (count) {
        ipv4_on = 1;
    }

    struct route_table *new_rt = NULL;
    if (ipv4_on) {
        char name[32];
        snprintf(name, sizeof(name), "ipv4_rt_dyn_%u", next);
        new_rt = route_table_create(name, rte_socket_id(), 2048);
        if (new_rt) {
            for (uint16_t p = 0; p < RTE_MAX_ETHPORTS; p++) {
                if (!ifcfgs[p].configured) {
                    continue;
                }
                uint32_t depth = (uint32_t)__builtin_popcount(ifcfgs[p].mask);
                uint32_t net = ifcfgs[p].ip & ifcfgs[p].mask;
                route_add(new_rt, net, (uint8_t)depth, 0, p);
            }
            for (uint32_t i = 0; i < count; i++) {
                route_add(new_rt, routes[i].dst_ip, routes[i].depth,
                    routes[i].next_hop_ip, routes[i].out_port);
            }
        }
    }

    struct route_table *old_rt = ipv4_rt_tbls[cur];
    ipv4_rt_tbls[next] = new_rt;
    rte_wmb();
    rte_atomic32_set(&ipv4_rt_active_idx, next);
    ipv4_rt_tbls[cur] = NULL;
    ipv4_rt_reclaim = old_rt;

    last_version = v2;
}

static int acl_runtime_init(void) {
	memset(&acl_rt, 0, sizeof(acl_rt));
	rte_atomic32_init(&acl_rt.active);
	rte_atomic64_init(&acl_rt.version);
	if (acl_init(&acl_rt.ctx[0], ACL_MAX_RULES) != 0) {
		return -1;
	}
	if (acl_init(&acl_rt.ctx[1], ACL_MAX_RULES) != 0) {
		acl_free(&acl_rt.ctx[0]);
		return -1;
	}
	if (acl_seed_default_rules(&acl_rt.ctx[0]) != 0) {
		return -1;
	}
	if (acl_clone(&acl_rt.ctx[1], &acl_rt.ctx[0]) != 0) {
		return -1;
	}
	rte_atomic32_set(&acl_rt.active, 0);
	rte_atomic64_set(&acl_rt.version, 1);
	return 0;
}

static void acl_runtime_free(void) {
	acl_free(&acl_rt.ctx[0]);
	acl_free(&acl_rt.ctx[1]);
}

static int acl_send_resp(struct acl_cmd_resp *resp) {
	if (!resp) {
		return -1;
	}
	if (!acl_resp_ring) {
		rte_free(resp);
		return -1;
	}
	if (rte_ring_enqueue(acl_resp_ring, resp) != 0) {
		rte_free(resp);
		return -1;
	}
	return 0;
}

static void acl_send_list(uint32_t seq, const struct acl_ctx *ctx, uint64_t version) {
	uint32_t count = acl_count(ctx);
	struct acl_cmd_resp *head = rte_zmalloc(NULL, sizeof(*head), 0);
	if (!head) {
		return;
	}
	head->seq = seq;
	head->status = 0;
	head->count = count;
	head->version = version;
	if (acl_send_resp(head) != 0) {
		return;
	}
}

static int acl_apply_cmd(const struct acl_cmd_msg *cmd) {
	if (!cmd) {
		return -1;
	}
	const struct acl_ctx *active = acl_runtime_active_ctx();
	uint32_t next_idx = (rte_atomic32_read(&acl_rt.active) ^ 1u) & 1u;
	struct acl_ctx *next = &acl_rt.ctx[next_idx];
	if (acl_clone(next, active) != 0) {
		return -1;
	}
	int rc = 0;
	switch (cmd->type) {
	case ACL_CMD_ADD:
		rc = acl_add_rule(next, &cmd->rule);
		break;
	case ACL_CMD_DEL:
		rc = acl_delete_rule(next, cmd->index);
		break;
	case ACL_CMD_CLEAR:
		acl_clear(next);
		rc = 0;
		break;
	default:
		rc = -1;
		break;
	}
	if (rc != 0) {
		return -1;
	}
	uint64_t version = rte_atomic64_add_return(&acl_rt.version, 1);
	rte_wmb();
	rte_atomic32_set(&acl_rt.active, next_idx);
	acl_shared_sync(next, version);
	return 0;
}

static void *acl_ctrl_thread_main(void *arg) {
	(void)arg;
	while (!force_quit) {
		struct acl_cmd_msg *cmd = NULL;
		if (rte_ring_dequeue(acl_cmd_ring, (void **)&cmd) != 0) {
			rte_delay_us_sleep(1000);
			continue;
		}
		if (!cmd) {
			continue;
		}
		if (cmd->type == ACL_CMD_LIST) {
			uint64_t version = rte_atomic64_read(&acl_rt.version);
			acl_send_list(cmd->seq, acl_runtime_active_ctx(), version);
			rte_free(cmd);
			continue;
		}
		int status = acl_apply_cmd(cmd);
		struct acl_cmd_resp *resp = rte_zmalloc(NULL, sizeof(*resp), 0);
		if (resp) {
			resp->seq = cmd->seq;
			resp->status = status;
			resp->count = acl_count(acl_runtime_active_ctx());
			resp->version = rte_atomic64_read(&acl_rt.version);
			acl_send_resp(resp);
		}
		rte_free(cmd);
	}
	return NULL;
}

static int acl6_runtime_init(void) {
	memset(&acl6_rt, 0, sizeof(acl6_rt));
	rte_atomic32_init(&acl6_rt.active);
	rte_atomic64_init(&acl6_rt.version);
	if (acl6_init(&acl6_rt.ctx[0], ACL6_MAX_RULES) != 0) {
		return -1;
	}
	if (acl6_init(&acl6_rt.ctx[1], ACL6_MAX_RULES) != 0) {
		acl6_free(&acl6_rt.ctx[0]);
		return -1;
	}
	if (acl6_seed_default_rules(&acl6_rt.ctx[0]) != 0) {
		return -1;
	}
	if (acl6_clone(&acl6_rt.ctx[1], &acl6_rt.ctx[0]) != 0) {
		return -1;
	}
	rte_atomic32_set(&acl6_rt.active, 0);
	rte_atomic64_set(&acl6_rt.version, 1);
	return 0;
}

static void acl6_runtime_free(void) {
	acl6_free(&acl6_rt.ctx[0]);
	acl6_free(&acl6_rt.ctx[1]);
}

static int acl6_send_resp(struct acl6_cmd_resp *resp) {
	if (!resp) {
		return -1;
	}
	if (!acl6_resp_ring) {
		rte_free(resp);
		return -1;
	}
	if (rte_ring_enqueue(acl6_resp_ring, resp) != 0) {
		rte_free(resp);
		return -1;
	}
	return 0;
}

static void acl6_send_list(uint32_t seq, const struct acl6_ctx *ctx, uint64_t version) {
	uint32_t count = acl6_count(ctx);
	struct acl6_cmd_resp *head = rte_zmalloc(NULL, sizeof(*head), 0);
	if (!head) {
		return;
	}
	head->seq = seq;
	head->status = 0;
	head->count = count;
	head->version = version;
	acl6_send_resp(head);
}

static int acl6_apply_cmd(const struct acl6_cmd_msg *cmd) {
	if (!cmd) {
		return -1;
	}
	const struct acl6_ctx *active = acl6_runtime_active_ctx();
	uint32_t next_idx = (rte_atomic32_read(&acl6_rt.active) ^ 1u) & 1u;
	struct acl6_ctx *next = &acl6_rt.ctx[next_idx];
	if (acl6_clone(next, active) != 0) {
		return -1;
	}
	int rc = 0;
	switch (cmd->type) {
	case ACL6_CMD_ADD:
		rc = acl6_add_rule(next, &cmd->rule);
		break;
	case ACL6_CMD_DEL:
		rc = acl6_delete_rule(next, cmd->index);
		break;
	case ACL6_CMD_CLEAR:
		acl6_clear(next);
		rc = 0;
		break;
	default:
		rc = -1;
		break;
	}
	if (rc != 0) {
		return -1;
	}
	uint64_t version = rte_atomic64_add_return(&acl6_rt.version, 1);
	rte_wmb();
	rte_atomic32_set(&acl6_rt.active, next_idx);
	acl6_shared_sync(next, version);
	return 0;
}

static void *acl6_ctrl_thread_main(void *arg) {
	(void)arg;
	while (!force_quit) {
		struct acl6_cmd_msg *cmd = NULL;
		if (rte_ring_dequeue(acl6_cmd_ring, (void **)&cmd) != 0) {
			rte_delay_us_sleep(1000);
			continue;
		}
		if (!cmd) {
			continue;
		}
		if (cmd->type == ACL6_CMD_LIST) {
			uint64_t version = rte_atomic64_read(&acl6_rt.version);
			acl6_send_list(cmd->seq, acl6_runtime_active_ctx(), version);
			rte_free(cmd);
			continue;
		}
		int status = acl6_apply_cmd(cmd);
		struct acl6_cmd_resp *resp = rte_zmalloc(NULL, sizeof(*resp), 0);
		if (resp) {
			resp->seq = cmd->seq;
			resp->status = status;
			resp->count = acl6_count(acl6_runtime_active_ctx());
			resp->version = rte_atomic64_read(&acl6_rt.version);
			acl6_send_resp(resp);
		}
		rte_free(cmd);
	}
	return NULL;
}

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned portid;
 
	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;
 
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };
 
		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);
 
	printf("\nPort statistics ====================================");
 
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		/* skip disabled ports */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %21"PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   port_statistics[portid].rx,
			   port_statistics[portid].dropped);
 
		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets received: %14"PRIu64
		   "\nTotal packets dropped: %15"PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_packets_dropped);
	printf("\n====================================================\n");
 
	fflush(stdout);
}
 
static void
l2fwd_mac_updating(struct rte_mbuf *m, unsigned dest_portid)
{
	struct rte_ether_hdr *eth;
	void *tmp;
 
	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
 
	/* 02:00:00:00:00:xx */
	tmp = &eth->dst_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dest_portid << 40);
 
	/* src addr */
	rte_ether_addr_copy(&l2fwd_ports_eth_addr[dest_portid], &eth->src_addr);
}
 
/* Simple forward. 8< */
static void
l2fwd_simple_forward(struct rte_mbuf *m, unsigned portid)
{
	unsigned dst_port;
	int sent;
	struct rte_eth_dev_tx_buffer *buffer;
	struct rte_ether_hdr *eth;
	struct rte_ipv4_hdr *ip;
	void *l4_hdr = NULL;
	uint16_t pkt_len;
	uint16_t ip_offset;
	uint8_t ihl;
 
	dst_port = l2fwd_dst_ports[portid];
 
	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	if (routing_on) {
		if (eth->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP)) {
			uint16_t tx_port = 0;
			int send_reply = arp_process_packet(arp_tbl, m, (uint16_t)portid, ifcfgs, l2fwd_ports_eth_addr,
				RTE_MAX_ETHPORTS, &tx_port);
			if (send_reply) {
				buffer = tx_buffer[tx_port];
				sent = rte_eth_tx_buffer(tx_port, 0, buffer, m);
				if (sent)
					port_statistics[tx_port].tx += sent;
				return;
			}
			rte_pktmbuf_free(m);
			port_statistics[portid].dropped++;
			return;
		}
		if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4) &&
			eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6)) {
			rte_pktmbuf_free(m);
			port_statistics[portid].dropped++;
			return;
		}
	}
	if (eth->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
		pkt_len = rte_pktmbuf_data_len(m);
		ip_offset = sizeof(struct rte_ether_hdr);
		if (pkt_len >= ip_offset + sizeof(struct rte_ipv4_hdr)) {
			ip = (struct rte_ipv4_hdr *)((char *)eth + ip_offset);
			ihl = (uint8_t)((ip->version_ihl & 0x0f) * 4);
			if (pkt_len >= ip_offset + ihl) {
				l4_hdr = (char *)ip + ihl;
			}
			unsigned lcore_id = rte_lcore_id();
			uint64_t now = rte_get_timer_cycles();
			uint64_t hz = rte_get_timer_hz();
			uint32_t src_ip = rte_be_to_cpu_32(ip->src_addr);
			if (attack_mitigation_enabled && lcore_id < RTE_MAX_LCORE && scan4_is_banned(lcore_id, src_ip, now)) {
				rte_pktmbuf_free(m);
				port_statistics[portid].dropped++;
				return;
			}
			if (l4_hdr && ip->next_proto_id == IPPROTO_TCP) {
				const struct rte_tcp_hdr *tcp = (const struct rte_tcp_hdr *)l4_hdr;
				uint8_t f = tcp->tcp_flags;
				if ((f & 0x02) && !(f & 0x10)) {
					if (lcore_id < RTE_MAX_LCORE) {
						rte_atomic64_inc(&attack_syn_cnt[lcore_id]);
					}
					if (lcore_id < RTE_MAX_LCORE) {
						scan4_track_syn(lcore_id, src_ip, rte_be_to_cpu_16(tcp->dst_port), now, hz);
					}
				}
			} else if (l4_hdr && ip->next_proto_id == IPPROTO_UDP) {
				if (lcore_id < RTE_MAX_LCORE) {
					rte_atomic64_inc(&attack_udp_cnt[lcore_id]);
				}
			}
			if (rlim_syn_pps || rlim_udp_pps) {
				struct rlim_table *syn_t = NULL;
				struct rlim_table *udp_t = NULL;
				if (lcore_id < RTE_MAX_LCORE && rlim_inited[lcore_id]) {
					syn_t = &rlim_syn_tbls[lcore_id];
					udp_t = &rlim_udp_tbls[lcore_id];
				}
				if (l4_hdr && ip->next_proto_id == IPPROTO_TCP && rlim_syn_pps) {
					const struct rte_tcp_hdr *tcp = (const struct rte_tcp_hdr *)l4_hdr;
					uint8_t f = tcp->tcp_flags;
					if ((f & 0x02) && !(f & 0x10)) {
						if (!rlim_allow(syn_t, &src_ip, now, hz, rlim_syn_pps, rlim_syn_burst)) {
							rte_pktmbuf_free(m);
							port_statistics[portid].dropped++;
							return;
						}
					}
				} else if (l4_hdr && ip->next_proto_id == IPPROTO_UDP && rlim_udp_pps) {
					if (!rlim_allow(udp_t, &src_ip, now, hz, rlim_udp_pps, rlim_udp_burst)) {
						rte_pktmbuf_free(m);
						port_statistics[portid].dropped++;
						return;
					}
				}
			}
			uint32_t deny_rule_index = UINT32_MAX;
			if (!acl_check_ipv4(acl_runtime_active_ctx(), ip, l4_hdr, &deny_rule_index)) {
				if (lcore_id < RTE_MAX_LCORE && deny_rule_index < ACL_MAX_RULES) {
					rte_atomic64_inc(&acl_deny_pkts[lcore_id][deny_rule_index]);
					(void)rte_atomic64_add_return(&acl_deny_bytes[lcore_id][deny_rule_index], rte_pktmbuf_pkt_len(m));
				}
				uint16_t src_port = 0;
				uint16_t dst_port = 0;
				if (l4_hdr && (ip->next_proto_id == IPPROTO_TCP || ip->next_proto_id == IPPROTO_UDP)) {
					if (ip->next_proto_id == IPPROTO_TCP) {
						const struct rte_tcp_hdr *tcp = (const struct rte_tcp_hdr *)l4_hdr;
						src_port = rte_be_to_cpu_16(tcp->src_port);
						dst_port = rte_be_to_cpu_16(tcp->dst_port);
					} else {
						const struct rte_udp_hdr *udp = (const struct rte_udp_hdr *)l4_hdr;
						src_port = rte_be_to_cpu_16(udp->src_port);
						dst_port = rte_be_to_cpu_16(udp->dst_port);
					}
				}
				denylog_add(
					rte_get_timer_cycles(),
					(uint16_t)portid,
					rte_be_to_cpu_32(ip->src_addr),
					rte_be_to_cpu_32(ip->dst_addr),
					ip->next_proto_id,
					src_port,
					dst_port,
					deny_rule_index
				);
				rte_pktmbuf_free(m);
				port_statistics[portid].dropped++;
				return;
			}
			if (l4_hdr && (ip->next_proto_id == IPPROTO_TCP || ip->next_proto_id == IPPROTO_UDP)) {
				struct session_key key;
				if (ip->next_proto_id == IPPROTO_TCP) {
					struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)l4_hdr;
					key.src_port = rte_be_to_cpu_16(tcp->src_port);
					key.dst_port = rte_be_to_cpu_16(tcp->dst_port);
				} else {
					struct rte_udp_hdr *udp = (struct rte_udp_hdr *)l4_hdr;
					key.src_port = rte_be_to_cpu_16(udp->src_port);
					key.dst_port = rte_be_to_cpu_16(udp->dst_port);
				}
				key.src_ip = rte_be_to_cpu_32(ip->src_addr);
				key.dst_ip = rte_be_to_cpu_32(ip->dst_addr);
				key.proto = ip->next_proto_id;
				struct session_table *st = session_table_for_lcore(rte_lcore_id());
				if (st) {
					session_track(st, &key, rte_pktmbuf_pkt_len(m), rte_rdtsc(), NULL);
				}
			}

			if (routing_on) {
				uint32_t dst_ip = rte_be_to_cpu_32(ip->dst_addr);
				for (uint16_t p = 0; p < RTE_MAX_ETHPORTS; p++) {
					if (ifcfgs[p].configured && ifcfgs[p].ip == dst_ip) {
						rte_pktmbuf_free(m);
						port_statistics[portid].dropped++;
						return;
					}
				}
				if (ip->time_to_live <= 1) {
					rte_pktmbuf_free(m);
					port_statistics[portid].dropped++;
					return;
				}
				ip->time_to_live--;
				ip->hdr_checksum = 0;
				ip->hdr_checksum = rte_ipv4_cksum(ip);

				struct route_entry re;
                struct route_table *rt = ipv4_rt_active();
                if (!rt || route_lookup(rt, dst_ip, &re) != 0) {
					rte_pktmbuf_free(m);
					port_statistics[portid].dropped++;
					return;
				}
				dst_port = re.out_port;
				uint32_t next_ip = re.next_hop_ip ? re.next_hop_ip : dst_ip;
				struct rte_ether_addr nh_mac;
				if (arp_tbl && arp_lookup(arp_tbl, next_ip, &nh_mac) == 0) {
					rte_ether_addr_copy(&nh_mac, &eth->dst_addr);
					rte_ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->src_addr);

					buffer = tx_buffer[dst_port];
					sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);
					if (sent)
						port_statistics[dst_port].tx += sent;
					return;
				}

				if (arp_tbl && ifcfgs[dst_port].configured && arp_should_request(arp_tbl, next_ip)) {
					uint64_t now = rte_get_timer_cycles();
					arp_mark_requested(arp_tbl, next_ip, now);
					struct rte_mbuf *req = arp_build_request(l2fwd_pktmbuf_pool,
						&l2fwd_ports_eth_addr[dst_port], ifcfgs[dst_port].ip, next_ip);
					if (req) {
						buffer = tx_buffer[dst_port];
						sent = rte_eth_tx_buffer(dst_port, 0, buffer, req);
						if (sent)
							port_statistics[dst_port].tx += sent;
					}
				}
				rte_pktmbuf_free(m);
				port_statistics[portid].dropped++;
				return;
			}
		}
	}
	if (eth->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6)) {
		uint16_t l3_off = sizeof(struct rte_ether_hdr);
		pkt_len = rte_pktmbuf_data_len(m);
		if (pkt_len >= l3_off + sizeof(struct rte_ipv6_hdr)) {
			struct rte_ipv6_hdr *ip6 = (struct rte_ipv6_hdr *)((char *)eth + l3_off);
			struct route6_table *rt6 = ipv6_rt_active();
			struct nd_ifcfg *if6 = ifcfg6_active();
			uint16_t l4_off = l3_off + sizeof(struct rte_ipv6_hdr);
			int proto = ip6->proto;
			while (l4_off < pkt_len) {
				if (proto == IPPROTO_TCP || proto == IPPROTO_UDP || proto == IPPROTO_ICMPV6) {
					l4_hdr = (char *)eth + l4_off;
					break;
				}
				size_t ext_len = 0;
				int next = rte_ipv6_get_next_ext((const uint8_t *)eth + l4_off, proto, &ext_len);
				if (next < 0 || ext_len == 0) {
					break;
				}
				if (l4_off + (uint16_t)ext_len > pkt_len) {
					break;
				}
				l4_off += (uint16_t)ext_len;
				proto = next;
			}

			unsigned lcore_id = rte_lcore_id();
			uint64_t now = rte_get_timer_cycles();
			uint64_t hz = rte_get_timer_hz();
			if (attack_mitigation_enabled && lcore_id < RTE_MAX_LCORE && scan6_is_banned(lcore_id, &ip6->src_addr, now)) {
				rte_pktmbuf_free(m);
				port_statistics[portid].dropped++;
				return;
			}
			if (l4_hdr && proto == IPPROTO_TCP) {
				const struct rte_tcp_hdr *tcp = (const struct rte_tcp_hdr *)l4_hdr;
				uint8_t f = tcp->tcp_flags;
				if ((f & 0x02) && !(f & 0x10)) {
					if (lcore_id < RTE_MAX_LCORE) {
						rte_atomic64_inc(&attack_syn_cnt[lcore_id]);
						scan6_track_syn(lcore_id, &ip6->src_addr, rte_be_to_cpu_16(tcp->dst_port), now, hz);
					}
				}
			} else if (l4_hdr && proto == IPPROTO_UDP) {
				if (lcore_id < RTE_MAX_LCORE) {
					rte_atomic64_inc(&attack_udp_cnt[lcore_id]);
				}
			}

			if (rt6 && nd_tbl && l4_hdr && proto == IPPROTO_ICMPV6) {
				uint16_t txp = 0;
				int send_reply = nd_process_packet(nd_tbl, m, (uint16_t)portid, if6, l2fwd_ports_eth_addr,
					RTE_MAX_ETHPORTS, &txp);
				if (send_reply) {
					buffer = tx_buffer[txp];
					sent = rte_eth_tx_buffer(txp, 0, buffer, m);
					if (sent)
						port_statistics[txp].tx += sent;
					return;
				}
				rte_pktmbuf_free(m);
				port_statistics[portid].dropped++;
				return;
			}

			if (rt6) {
				uint32_t deny_rule_index = UINT32_MAX;
				if (!acl6_check_ipv6(acl6_runtime_active_ctx(), ip6, l4_hdr, &deny_rule_index)) {
					if (lcore_id < RTE_MAX_LCORE && deny_rule_index < ACL6_MAX_RULES) {
						rte_atomic64_inc(&acl6_deny_pkts[lcore_id][deny_rule_index]);
						(void)rte_atomic64_add_return(&acl6_deny_bytes[lcore_id][deny_rule_index], rte_pktmbuf_pkt_len(m));
					}
					uint16_t src_port = 0;
					uint16_t dst_port = 0;
					if (l4_hdr && (proto == IPPROTO_TCP || proto == IPPROTO_UDP)) {
						if (proto == IPPROTO_TCP) {
							const struct rte_tcp_hdr *tcp = (const struct rte_tcp_hdr *)l4_hdr;
							src_port = rte_be_to_cpu_16(tcp->src_port);
							dst_port = rte_be_to_cpu_16(tcp->dst_port);
						} else {
							const struct rte_udp_hdr *udp = (const struct rte_udp_hdr *)l4_hdr;
							src_port = rte_be_to_cpu_16(udp->src_port);
							dst_port = rte_be_to_cpu_16(udp->dst_port);
						}
					}
					denylog6_add(rte_get_timer_cycles(), (uint16_t)portid, &ip6->src_addr, &ip6->dst_addr,
						(uint8_t)proto, src_port, dst_port, deny_rule_index);
					rte_pktmbuf_free(m);
					port_statistics[portid].dropped++;
					return;
				}
				if (l4_hdr && (proto == IPPROTO_TCP || proto == IPPROTO_UDP)) {
					struct session6_key sk;
					memset(&sk, 0, sizeof(sk));
					sk.src_ip6 = ip6->src_addr;
					sk.dst_ip6 = ip6->dst_addr;
					sk.proto = (uint8_t)proto;
					if (proto == IPPROTO_TCP) {
						const struct rte_tcp_hdr *tcp = (const struct rte_tcp_hdr *)l4_hdr;
						sk.src_port = rte_be_to_cpu_16(tcp->src_port);
						sk.dst_port = rte_be_to_cpu_16(tcp->dst_port);
					} else {
						const struct rte_udp_hdr *udp = (const struct rte_udp_hdr *)l4_hdr;
						sk.src_port = rte_be_to_cpu_16(udp->src_port);
						sk.dst_port = rte_be_to_cpu_16(udp->dst_port);
					}
					uint64_t now = rte_get_timer_cycles();
					session6_track(session6_table_for_lcore(rte_lcore_id()), &sk, pkt_len, now, NULL);
				}
			}

			if (rlim_syn_pps || rlim_udp_pps) {
				struct rlim_table *syn_t = NULL;
				struct rlim_table *udp_t = NULL;
				if (lcore_id < RTE_MAX_LCORE && rlim_inited[lcore_id]) {
					syn_t = &rlim6_syn_tbls[lcore_id];
					udp_t = &rlim6_udp_tbls[lcore_id];
				}
				const void *src_key = &ip6->src_addr;
				if (l4_hdr && proto == IPPROTO_TCP && rlim_syn_pps) {
					const struct rte_tcp_hdr *tcp = (const struct rte_tcp_hdr *)l4_hdr;
					uint8_t f = tcp->tcp_flags;
					if ((f & 0x02) && !(f & 0x10)) {
						if (!rlim_allow(syn_t, src_key, now, hz, rlim_syn_pps, rlim_syn_burst)) {
							rte_pktmbuf_free(m);
							port_statistics[portid].dropped++;
							return;
						}
					}
				} else if (l4_hdr && proto == IPPROTO_UDP && rlim_udp_pps) {
					if (!rlim_allow(udp_t, src_key, now, hz, rlim_udp_pps, rlim_udp_burst)) {
						rte_pktmbuf_free(m);
						port_statistics[portid].dropped++;
						return;
					}
				}
			}

			if (rt6 && nd_tbl) {
				for (uint16_t p = 0; p < RTE_MAX_ETHPORTS; p++) {
					if (if6[p].configured && rte_ipv6_addr_eq(&ip6->dst_addr, &if6[p].ip)) {
						rte_pktmbuf_free(m);
						port_statistics[portid].dropped++;
						return;
					}
				}
				if (ip6->hop_limits <= 1) {
					rte_pktmbuf_free(m);
					port_statistics[portid].dropped++;
					return;
				}
				ip6->hop_limits--;

				struct route6_entry re6;
				if (route6_lookup(rt6, &ip6->dst_addr, &re6) != 0) {
					rte_pktmbuf_free(m);
					port_statistics[portid].dropped++;
					return;
				}
				dst_port = re6.out_port;

				struct rte_ipv6_addr next_ip6 = rte_ipv6_addr_is_unspec(&re6.next_hop) ? ip6->dst_addr : re6.next_hop;
				struct rte_ether_addr nh_mac6;
				if (nd_lookup(nd_tbl, &next_ip6, &nh_mac6) == 0) {
					rte_ether_addr_copy(&nh_mac6, &eth->dst_addr);
					rte_ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->src_addr);
					buffer = tx_buffer[dst_port];
					sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);
					if (sent)
						port_statistics[dst_port].tx += sent;
					return;
				}

				if (if6[dst_port].configured && nd_should_request(nd_tbl, &next_ip6)) {
					uint64_t now = rte_get_timer_cycles();
					nd_mark_requested(nd_tbl, &next_ip6, now);
					struct rte_mbuf *req = nd_build_ns(l2fwd_pktmbuf_pool,
						&l2fwd_ports_eth_addr[dst_port], &if6[dst_port].ip, &next_ip6);
					if (req) {
						buffer = tx_buffer[dst_port];
						sent = rte_eth_tx_buffer(dst_port, 0, buffer, req);
						if (sent)
							port_statistics[dst_port].tx += sent;
					}
				}
				rte_pktmbuf_free(m);
				port_statistics[portid].dropped++;
				return;
			}
		}
	}

	if (mac_updating)
		l2fwd_mac_updating(m, dst_port);
 
	buffer = tx_buffer[dst_port];
	sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);
	if (sent)
		port_statistics[dst_port].tx += sent;
}
/* >8 End of simple forward. */
 
/* main processing loop */
static void
l2fwd_main_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	int sent;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	unsigned i, j, portid, nb_rx;
	struct lcore_queue_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
			BURST_TX_DRAIN_US;
	struct rte_eth_dev_tx_buffer *buffer;
 
	prev_tsc = 0;
	timer_tsc = 0;
 
	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];
 
	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}
 
	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);
 
	for (i = 0; i < qconf->n_rx_port; i++) {
 
		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
 
	}
 
	while (!force_quit) {
 
		/* Drains TX queue in its main loop. 8< */
		cur_tsc = rte_rdtsc();
		if (lcore_id == rte_get_main_lcore()) {
			uint64_t now = rte_get_timer_cycles();
			attack_apply_shared_cfg();
			attack_stats_sync(now);
		}
 
		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {
 
			for (i = 0; i < qconf->n_rx_port; i++) {
 
				portid = l2fwd_dst_ports[qconf->rx_port_list[i]];
				buffer = tx_buffer[portid];
 
				sent = rte_eth_tx_buffer_flush(portid, 0, buffer);
				if (sent)
					port_statistics[portid].tx += sent;
 
			}
 
			/* if timer is enabled */
			if (timer_period > 0) {
 
				/* advance the timer */
				timer_tsc += diff_tsc;
 
				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= timer_period)) {
 
					/* do this only on main core */
					if (lcore_id == rte_get_main_lcore()) {
						uint64_t now = rte_get_timer_cycles();
						rlim_apply_shared_cfg();
                        route_reclaim();
                        route_apply_shared_cfg();
                        route6_reclaim();
                        route6_apply_shared_cfg();
						portcfg_apply_shared_cfg();
						session_expire_all(now);
						session_shared_sync(now);
						session6_expire_all(now);
						session6_shared_sync(now);
						portstats_shared_sync();
						acl_hit_shared_sync();
						acl6_hit_shared_sync();
						print_stats();
						/* reset the timer */
						timer_tsc = 0;
					}
				}
			}
 
			prev_tsc = cur_tsc;
		}
		/* >8 End of draining TX queue. */
 
		/* Read packet from RX queues. 8< */
		for (i = 0; i < qconf->n_rx_port; i++) {
 
			portid = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst(portid, 0,
						 pkts_burst, MAX_PKT_BURST);
 
			if (unlikely(nb_rx == 0))
				continue;
 
			port_statistics[portid].rx += nb_rx;
 
			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				l2fwd_simple_forward(m, portid);
			}
		}
		/* >8 End of read packet from RX queues. */
	}
}

static int
parse_pktgen(const char *arg)
{
	unsigned port, b[6];
	uint16_t sz;
	if (sscanf(arg, "%u,%hu,%02x:%02x:%02x:%02x:%02x:%02x",
	           &port, &sz, &b[0],&b[1],&b[2],&b[3],&b[4],&b[5]) != 8)
		return -1;
	if (port >= RTE_MAX_ETHPORTS || sz < 64)
		return -1;
	pktgen_port = (int)port;
	pktgen_pkt_size = sz;
	for (int i = 0; i < 6; i++)
		pktgen_dst_mac.addr_bytes[i] = (uint8_t)b[i];
	return 0;
}

#define PKTGEN_TX_BURST 64

static void
pktgen_main_loop(void)
{
	unsigned lcore_id = rte_lcore_id();
	uint16_t port = (uint16_t)pktgen_port;

	fprintf(stderr, "PKTGEN: lcore=%u port=%u start\n", lcore_id, port);

	/* test: single alloc */
	struct rte_mbuf *m = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
	fprintf(stderr, "PKTGEN: single alloc=%p\n", (void *)m);
	if (m) {
		rte_pktmbuf_free(m);
		fprintf(stderr, "PKTGEN: single alloc OK, entering loop\n");
	} else {
		fprintf(stderr, "PKTGEN: single alloc FAILED\n");
		return;
	}

	uint64_t total = 0, prev_total = 0;
	uint64_t ts = rte_rdtsc();
	uint64_t hz = rte_get_tsc_hz();

	while (!force_quit) {
		/* alloc one by one */
		struct rte_mbuf *pkts[PKTGEN_TX_BURST];
		int i;
		for (i = 0; i < PKTGEN_TX_BURST; i++) {
			pkts[i] = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
			if (!pkts[i]) break;
		}
		int nb_alloc = i;
		if (nb_alloc == 0) continue;

		for (int i = 0; i < nb_alloc; i++) {
			struct rte_mbuf *m = pkts[i];
			m->data_len = m->pkt_len = pktgen_pkt_size;

			struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m,
				struct rte_ether_hdr *);
			rte_ether_addr_copy(&pktgen_dst_mac, &eth->dst_addr);
			rte_ether_addr_copy(&l2fwd_ports_eth_addr[port],
			                    &eth->src_addr);
			eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

			uint16_t pl = pktgen_pkt_size -
				(uint16_t)(sizeof(*eth) +
				 sizeof(struct rte_ipv4_hdr) +
				 sizeof(struct rte_udp_hdr));
			struct rte_ipv4_hdr *ip4 = (struct rte_ipv4_hdr *)(eth + 1);
			memset(ip4, 0, sizeof(*ip4));
			ip4->version_ihl = 0x45;
			ip4->total_length = rte_cpu_to_be_16(
				(uint16_t)(sizeof(*ip4) +
				 sizeof(struct rte_udp_hdr) + pl));
			ip4->time_to_live = 64;
			ip4->next_proto_id = IPPROTO_UDP;
			ip4->src_addr = rte_cpu_to_be_32(0x0a000001);
			ip4->dst_addr = rte_cpu_to_be_32(0x0a000002);
			ip4->hdr_checksum = rte_ipv4_cksum(ip4);

			struct rte_udp_hdr *udp =
				(struct rte_udp_hdr *)(ip4 + 1);
			udp->src_port = rte_cpu_to_be_16(12345);
			udp->dst_port = rte_cpu_to_be_16(80);
			udp->dgram_len = rte_cpu_to_be_16(
				(uint16_t)(sizeof(*udp) + pl));
			udp->dgram_cksum = 0;
		}

		uint16_t sent = rte_eth_tx_burst(port, 0, pkts,
		                                 (uint16_t)nb_alloc);
		for (uint16_t i = sent; i < (uint16_t)nb_alloc; i++)
			rte_pktmbuf_free(pkts[i]);

		total += sent;

		if (rte_rdtsc() - ts >= hz) {
			uint64_t delta = total - prev_total;
			double sec = (double)hz / rte_get_tsc_hz();
			double pps = (double)delta / sec;
			double mbps = pps * pktgen_pkt_size * 8 / 1e6;
			printf("PKTGEN: %lu total | %.0f pps | %.1f Mbps\n",
			       (unsigned long)total, pps, mbps);
			fflush(stdout);
			prev_total = total;
			ts = rte_rdtsc();
		}
	}
}

static int
l2fwd_launch_one_lcore(__rte_unused void *dummy)
{
	unsigned lcore_id = rte_lcore_id();
	unsigned i;
	struct lcore_queue_conf *qconf = &lcore_queue_conf[lcore_id];

	if (pktgen_port >= 0 && qconf->n_rx_port == 1 &&
	    qconf->rx_port_list[0] == (unsigned)pktgen_port) {
		pktgen_main_loop();
		return 0;
	}

	for (i = 0; i < qconf->n_rx_port; i++) {
		if ((unsigned)pktgen_port == qconf->rx_port_list[i])
			return 0;
	}

	l2fwd_main_loop();
	return 0;
}
 
/* display usage */
static void
l2fwd_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-P] [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -P : Enable promiscuous mode (default off)\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
	       "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n"
	       "  --no-mac-updating: Disable MAC addresses updating (enabled by default)\n"
	       "      When enabled:\n"
	       "       - The source MAC address is replaced by the TX port MAC address\n"
	       "       - The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID\n"
	       "  --ifcfg PORT,IP/CIDR: Configure IPv4 address on port (enables routing mode)\n"
	       "  --ifcfg6 PORT,IP6/CIDR: Configure IPv6 address on port (enables routing mode)\n"
	       "  --route DST/CIDR,NEXTHOP,PORT: Add IPv4 route (NEXTHOP can be 0.0.0.0 for direct)\n"
	       "  --route6 DST6/CIDR,NEXTHOP6,PORT: Add IPv6 route (NEXTHOP6 can be :: for direct)\n"
	       "  --session-timeout-sec SEC: Expire sessions after SEC seconds (0 disables)\n"
	       "  --rlim-syn PPS[,BURST]: Per-source TCP SYN rate limit (0 disables)\n"
	       "  --rlim-udp PPS[,BURST]: Per-source UDP rate limit (0 disables)\n"
	       "  --portmap: Configure forwarding port pair mapping\n"
	       "	      Default: alternate port pairs\n"
	       "  --pktgen PORT,SIZE,MAC: Enable built-in pktgen on PORT with SIZE-byte pkts\n"
	       "         Example: --pktgen 2,64,00:0c:29:fb:49:f3\n\n",
	       prgname);
}
 
static int
l2fwd_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;
 
	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
 
	return pm;
}
 
static int
l2fwd_parse_port_pair_config(const char *q_arg)
{
	enum fieldnames {
		FLD_PORT1 = 0,
		FLD_PORT2,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	const char *p, *p0 = q_arg;
	char *str_fld[_NUM_FLD];
	unsigned int size;
	char s[256];
	char *end;
	int i;
 
	nb_port_pair_params = 0;
 
	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		p0 = strchr(p, ')');
		if (p0 == NULL)
			return -1;
 
		size = p0 - p;
		if (size >= sizeof(s))
			return -1;
 
		memcpy(s, p, size);
		s[size] = '\0';
		if (rte_strsplit(s, sizeof(s), str_fld,
				 _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] ||
			    int_fld[i] >= RTE_MAX_ETHPORTS)
				return -1;
		}
		if (nb_port_pair_params >= RTE_MAX_ETHPORTS/2) {
			printf("exceeded max number of port pair params: %hu\n",
				nb_port_pair_params);
			return -1;
		}
		port_pair_params_array[nb_port_pair_params].port[0] =
				(uint16_t)int_fld[FLD_PORT1];
		port_pair_params_array[nb_port_pair_params].port[1] =
				(uint16_t)int_fld[FLD_PORT2];
		++nb_port_pair_params;
	}
	port_pair_params = port_pair_params_array;
	return 0;
}
 
static unsigned int
l2fwd_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;
 
	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;
 
	return n;
}
 
static int parse_ipv4_addr(const char *s, uint32_t *ip_out) {
	struct in_addr a;
	if (!s || !ip_out) {
		return -1;
	}
	if (inet_pton(AF_INET, s, &a) != 1) {
		return -1;
	}
	*ip_out = rte_be_to_cpu_32(a.s_addr);
	return 0;
}

static int parse_ipv4_cidr(const char *s, uint32_t *ip_out, uint8_t *depth_out) {
	if (!s || !ip_out || !depth_out) {
		return -1;
	}
	char buf[64];
	snprintf(buf, sizeof(buf), "%s", s);
	char *slash = strchr(buf, '/');
	if (!slash) {
		return -1;
	}
	*slash = '\0';
	char *depth_str = slash + 1;
	unsigned long depth = strtoul(depth_str, NULL, 10);
	if (depth > 32) {
		return -1;
	}
	uint32_t ip = 0;
	if (parse_ipv4_addr(buf, &ip) != 0) {
		return -1;
	}
	*ip_out = ip;
	*depth_out = (uint8_t)depth;
	return 0;
}

static uint32_t cidr_depth_to_mask(uint8_t depth) {
	if (depth == 0) {
		return 0;
	}
	return 0xFFFFFFFFu << (32 - depth);
}

static int parse_rlim_arg(const char *s, uint32_t *pps_out, uint32_t *burst_out) {
	if (!s || !pps_out || !burst_out) {
		return -1;
	}
	char buf[64];
	snprintf(buf, sizeof(buf), "%s", s);
	char *comma = strchr(buf, ',');
	if (comma) {
		*comma = '\0';
	}
	char *endp = NULL;
	unsigned long pps = strtoul(buf, &endp, 10);
	if (!buf[0] || (endp && *endp)) {
		return -1;
	}
	unsigned long burst = pps;
	if (comma) {
		endp = NULL;
		burst = strtoul(comma + 1, &endp, 10);
		if (!comma[1] || (endp && *endp)) {
			return -1;
		}
	}
	if (pps == 0) {
		*pps_out = 0;
		*burst_out = 0;
		return 0;
	}
	if (burst == 0) {
		return -1;
	}
	*pps_out = (uint32_t)pps;
	*burst_out = (uint32_t)burst;
	return 0;
}

static int parse_ifcfg_arg(const char *s) {
	if (!s) {
		return -1;
	}
	char buf[128];
	snprintf(buf, sizeof(buf), "%s", s);
	char *comma = strchr(buf, ',');
	if (!comma) {
		return -1;
	}
	*comma = '\0';
	const char *port_str = buf;
	const char *cidr_str = comma + 1;
	unsigned long port = strtoul(port_str, NULL, 10);
	if (port >= RTE_MAX_ETHPORTS) {
		return -1;
	}
	uint32_t ip = 0;
	uint8_t depth = 0;
	if (parse_ipv4_cidr(cidr_str, &ip, &depth) != 0) {
		return -1;
	}
	ifcfgs[port].ip = ip;
	ifcfgs[port].mask = cidr_depth_to_mask(depth);
	ifcfgs[port].configured = 1;
	routing_on = 1;
	return 0;
}

static int parse_route_arg(const char *s) {
	if (!s) {
		return -1;
	}
	if (pending_route_count >= MAX_PENDING_ROUTES) {
		return -1;
	}
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", s);
	char *c1 = strchr(buf, ',');
	if (!c1) {
		return -1;
	}
	*c1 = '\0';
	char *c2 = strchr(c1 + 1, ',');
	if (!c2) {
		return -1;
	}
	*c2 = '\0';
	const char *dst_cidr = buf;
	const char *nh_str = c1 + 1;
	const char *port_str = c2 + 1;

	uint32_t dst_ip = 0;
	uint8_t depth = 0;
	if (parse_ipv4_cidr(dst_cidr, &dst_ip, &depth) != 0) {
		return -1;
	}
	uint32_t nh = 0;
	if (strcmp(nh_str, "0.0.0.0") != 0) {
		if (parse_ipv4_addr(nh_str, &nh) != 0) {
			return -1;
		}
	}
	unsigned long port = strtoul(port_str, NULL, 10);
	if (port >= RTE_MAX_ETHPORTS) {
		return -1;
	}

	pending_routes[pending_route_count].dst_ip = dst_ip;
	pending_routes[pending_route_count].depth = depth;
	pending_routes[pending_route_count].next_hop_ip = nh;
	pending_routes[pending_route_count].out_port = (uint16_t)port;
	pending_route_count++;
	routing_on = 1;
	return 0;
}

static int parse_ipv6_addr(const char *s, struct rte_ipv6_addr *out) {
	if (!s || !out) {
		return -1;
	}
	struct in6_addr a6;
	if (inet_pton(AF_INET6, s, &a6) != 1) {
		return -1;
	}
	memcpy(out, &a6, sizeof(*out));
	return 0;
}

static int parse_ipv6_cidr(const char *s, struct rte_ipv6_addr *ip, uint8_t *depth) {
	if (!s || !ip || !depth) {
		return -1;
	}
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", s);
	char *slash = strchr(buf, '/');
	if (!slash) {
		return -1;
	}
	*slash = '\0';
	const char *ip_str = buf;
	const char *d_str = slash + 1;
	unsigned long d = strtoul(d_str, NULL, 10);
	if (d > 128) {
		return -1;
	}
	if (parse_ipv6_addr(ip_str, ip) != 0) {
		return -1;
	}
	*depth = (uint8_t)d;
	return 0;
}

static int parse_ifcfg6_arg(const char *s) {
	if (!s) {
		return -1;
	}
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", s);
	char *comma = strchr(buf, ',');
	if (!comma) {
		return -1;
	}
	*comma = '\0';
	const char *port_str = buf;
	const char *cidr_str = comma + 1;
	unsigned long port = strtoul(port_str, NULL, 10);
	if (port >= RTE_MAX_ETHPORTS) {
		return -1;
	}
	struct rte_ipv6_addr ip6;
	uint8_t depth = 0;
	if (parse_ipv6_cidr(cidr_str, &ip6, &depth) != 0) {
		return -1;
	}
	ifcfg6_tbls[0][port].ip = ip6;
	ifcfg6_tbls[0][port].depth = depth;
	ifcfg6_tbls[0][port].configured = 1;
	routing_on = 1;
	return 0;
}

static int parse_route6_arg(const char *s) {
	if (!s) {
		return -1;
	}
	if (pending_route6_count >= MAX_PENDING_ROUTES) {
		return -1;
	}
	char buf[512];
	snprintf(buf, sizeof(buf), "%s", s);
	char *c1 = strchr(buf, ',');
	if (!c1) {
		return -1;
	}
	*c1 = '\0';
	char *c2 = strchr(c1 + 1, ',');
	if (!c2) {
		return -1;
	}
	*c2 = '\0';
	const char *dst_cidr = buf;
	const char *nh_str = c1 + 1;
	const char *port_str = c2 + 1;

	struct rte_ipv6_addr dst;
	uint8_t depth = 0;
	if (parse_ipv6_cidr(dst_cidr, &dst, &depth) != 0) {
		return -1;
	}
	struct rte_ipv6_addr nh = RTE_IPV6_ADDR_UNSPEC;
	if (strcmp(nh_str, "::") != 0) {
		if (parse_ipv6_addr(nh_str, &nh) != 0) {
			return -1;
		}
	}
	unsigned long port = strtoul(port_str, NULL, 10);
	if (port >= RTE_MAX_ETHPORTS) {
		return -1;
	}
	pending_routes6[pending_route6_count].dst_ip = dst;
	pending_routes6[pending_route6_count].depth = depth;
	pending_routes6[pending_route6_count].next_hop_ip = nh;
	pending_routes6[pending_route6_count].out_port = (uint16_t)port;
	pending_route6_count++;
	routing_on = 1;
	return 0;
}

static int
l2fwd_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;
 
	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;
 
	return n;
}
 
static const char short_options[] =
	"p:"  /* portmask */
	"P"   /* promiscuous */
	"q:"  /* number of queues */
	"T:"  /* timer period */
	;
 
#define CMD_LINE_OPT_NO_MAC_UPDATING "no-mac-updating"
#define CMD_LINE_OPT_PORTMAP_CONFIG "portmap"
#define CMD_LINE_OPT_IFCFG "ifcfg"
#define CMD_LINE_OPT_IFCFG6 "ifcfg6"
#define CMD_LINE_OPT_ROUTE "route"
#define CMD_LINE_OPT_ROUTE6 "route6"
#define CMD_LINE_OPT_SESSION_TIMEOUT "session-timeout-sec"
#define CMD_LINE_OPT_RLIM_SYN "rlim-syn"
#define CMD_LINE_OPT_RLIM_UDP "rlim-udp"
#define CMD_LINE_OPT_PKTGEN "pktgen"
 
enum {
	/* long options mapped to a short option */
 
	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_NO_MAC_UPDATING_NUM = 256,
	CMD_LINE_OPT_PORTMAP_NUM,
	CMD_LINE_OPT_IFCFG_NUM,
	CMD_LINE_OPT_IFCFG6_NUM,
	CMD_LINE_OPT_ROUTE_NUM,
	CMD_LINE_OPT_ROUTE6_NUM,
	CMD_LINE_OPT_SESSION_TIMEOUT_NUM,
	CMD_LINE_OPT_RLIM_SYN_NUM,
	CMD_LINE_OPT_RLIM_UDP_NUM,
	CMD_LINE_OPT_PKTGEN_NUM,
};
 
static const struct option lgopts[] = {
	{ CMD_LINE_OPT_NO_MAC_UPDATING, no_argument, 0,
		CMD_LINE_OPT_NO_MAC_UPDATING_NUM},
	{ CMD_LINE_OPT_PORTMAP_CONFIG, 1, 0, CMD_LINE_OPT_PORTMAP_NUM},
	{ CMD_LINE_OPT_IFCFG, 1, 0, CMD_LINE_OPT_IFCFG_NUM},
	{ CMD_LINE_OPT_IFCFG6, 1, 0, CMD_LINE_OPT_IFCFG6_NUM},
	{ CMD_LINE_OPT_ROUTE, 1, 0, CMD_LINE_OPT_ROUTE_NUM},
	{ CMD_LINE_OPT_ROUTE6, 1, 0, CMD_LINE_OPT_ROUTE6_NUM},
	{ CMD_LINE_OPT_SESSION_TIMEOUT, 1, 0, CMD_LINE_OPT_SESSION_TIMEOUT_NUM},
	{ CMD_LINE_OPT_RLIM_SYN, 1, 0, CMD_LINE_OPT_RLIM_SYN_NUM},
	{ CMD_LINE_OPT_RLIM_UDP, 1, 0, CMD_LINE_OPT_RLIM_UDP_NUM},
	{ CMD_LINE_OPT_PKTGEN, 1, 0, CMD_LINE_OPT_PKTGEN_NUM},
	{NULL, 0, 0, 0}
};
 
/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(int argc, char **argv)
{
	int opt, ret, timer_secs;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
 
	argvopt = argv;
	port_pair_params = NULL;
 
	while ((opt = getopt_long(argc, argvopt, short_options,
				  lgopts, &option_index)) != EOF) {
 
		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
			if (l2fwd_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;
		case 'P':
			promiscuous_on = 1;
			break;
 
		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
			if (l2fwd_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;
 
		/* timer period */
		case 'T':
			timer_secs = l2fwd_parse_timer_period(optarg);
			if (timer_secs < 0) {
				printf("invalid timer period\n");
				l2fwd_usage(prgname);
				return -1;
			}
			timer_period = timer_secs;
			break;
 
		/* long options */
		case CMD_LINE_OPT_PORTMAP_NUM:
			ret = l2fwd_parse_port_pair_config(optarg);
			if (ret) {
				fprintf(stderr, "Invalid config\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;
 
		case CMD_LINE_OPT_NO_MAC_UPDATING_NUM:
			mac_updating = 0;
			break;
 
		case CMD_LINE_OPT_IFCFG_NUM:
			if (parse_ifcfg_arg(optarg) != 0) {
				fprintf(stderr, "Invalid ifcfg\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;
		case CMD_LINE_OPT_IFCFG6_NUM:
			if (parse_ifcfg6_arg(optarg) != 0) {
				fprintf(stderr, "Invalid ifcfg6\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_ROUTE_NUM:
			if (parse_route_arg(optarg) != 0) {
				fprintf(stderr, "Invalid route\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;
		case CMD_LINE_OPT_ROUTE6_NUM:
			if (parse_route6_arg(optarg) != 0) {
				fprintf(stderr, "Invalid route6\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_SESSION_TIMEOUT_NUM: {
			char *endp = NULL;
			long v = strtol(optarg, &endp, 10);
			if (!optarg[0] || (endp && *endp) || v < 0) {
				fprintf(stderr, "Invalid session timeout\n");
				l2fwd_usage(prgname);
				return -1;
			}
			session_timeout_tsc = (uint64_t)v * rte_get_timer_hz();
			break;
		}
		case CMD_LINE_OPT_RLIM_SYN_NUM:
			if (parse_rlim_arg(optarg, &rlim_syn_pps, &rlim_syn_burst) != 0) {
				fprintf(stderr, "Invalid rlim-syn\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;
		case CMD_LINE_OPT_RLIM_UDP_NUM:
			if (parse_rlim_arg(optarg, &rlim_udp_pps, &rlim_udp_burst) != 0) {
				fprintf(stderr, "Invalid rlim-udp\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_PKTGEN_NUM:
			if (parse_pktgen(optarg) != 0) {
				fprintf(stderr, "Invalid pktgen, use: port,size,xx:xx:xx:xx:xx:xx\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		default:
			l2fwd_usage(prgname);
			return -1;
		}
	}
 
	if (optind >= 0)
		argv[optind-1] = prgname;
 
	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}
 
/*
 * Check port pair config with enabled port mask,
 * and for valid port pair combinations.
 */
static int
check_port_pair_config(void)
{
	uint32_t port_pair_config_mask = 0;
	uint32_t port_pair_mask = 0;
	uint16_t index, i, portid;
 
	for (index = 0; index < nb_port_pair_params; index++) {
		port_pair_mask = 0;
 
		for (i = 0; i < NUM_PORTS; i++)  {
			portid = port_pair_params[index].port[i];
			if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
				printf("port %u is not enabled in port mask\n",
				       portid);
				return -1;
			}
			if (!rte_eth_dev_is_valid_port(portid)) {
				printf("port %u is not present on the board\n",
				       portid);
				return -1;
			}
 
			port_pair_mask |= 1 << portid;
		}
 
		if (port_pair_config_mask & port_pair_mask) {
			printf("port %u is used in other port pairs\n", portid);
			return -1;
		}
		port_pair_config_mask |= port_pair_mask;
	}
 
	l2fwd_enabled_port_mask &= port_pair_config_mask;
 
	return 0;
}
 
/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];
 
	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if (force_quit)
				return;
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(portid, &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				const char *status = link.link_status ? "up" : "down";
				const char *duplex = link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX ? "full-duplex" : "half-duplex";
				snprintf(link_status_text, sizeof(link_status_text),
					"Link %s speed %u Mbps %s", status, link.link_speed, duplex);
				printf("Port %d %s\n", portid, link_status_text);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == RTE_ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;
 
		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}
 
		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}
 
static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}
 
int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	int ret;
	uint16_t nb_ports;
	uint16_t nb_ports_available = 0;
	uint16_t portid, last_port;
	unsigned lcore_id, rx_lcore_id;
	unsigned nb_ports_in_mask = 0;
	unsigned int nb_lcores = 0;
	unsigned int nb_mbufs;
 
	/* Init EAL. 8< */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		rte_exit(EXIT_FAILURE, "Must run as primary process\n");
 
	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

    rte_atomic32_init(&ipv6_rt_active_idx);
    rte_atomic32_init(&ifcfg6_active_idx);
    rte_atomic32_set(&ipv6_rt_active_idx, 0);
    rte_atomic32_set(&ifcfg6_active_idx, 0);
    memset(ifcfg6_tbls, 0, sizeof(ifcfg6_tbls));
    ipv6_rt_tbls[0] = NULL;
    ipv6_rt_tbls[1] = NULL;
    rte_atomic32_init(&ipv4_rt_active_idx);
    rte_atomic32_set(&ipv4_rt_active_idx, 0);
    ipv4_rt_tbls[0] = NULL;
    ipv4_rt_tbls[1] = NULL;
    ipv4_rt_reclaim = NULL;
	ipv6_rt_reclaim = NULL;
	attack_scan_ports_per_sec = 50;
	attack_ban_seconds = 60;
	attack_mitigation_enabled = 0;
	for (unsigned i = 0; i < RTE_MAX_LCORE; i++) {
		memset(&scan4_tbls[i], 0, sizeof(scan4_tbls[i]));
		memset(&scan6_tbls[i], 0, sizeof(scan6_tbls[i]));
		rte_atomic64_init(&attack_syn_cnt[i]);
		rte_atomic64_init(&attack_udp_cnt[i]);
		rte_atomic64_init(&attack_scan_events_cnt[i]);
		rte_atomic64_init(&attack_scan_banned_cnt[i]);
		attack_top_scan4_ports[i] = 0;
		attack_top_scan4_ip[i] = 0;
		attack_top_scan6_ports[i] = 0;
		memset(&attack_top_scan6_ip[i], 0, sizeof(attack_top_scan6_ip[i]));
		for (unsigned j = 0; j < ACL_MAX_RULES; j++) {
			rte_atomic64_init(&acl_deny_pkts[i][j]);
			rte_atomic64_init(&acl_deny_bytes[i][j]);
		}
		for (unsigned j = 0; j < ACL6_MAX_RULES; j++) {
			rte_atomic64_init(&acl6_deny_pkts[i][j]);
			rte_atomic64_init(&acl6_deny_bytes[i][j]);
		}
	}
 
	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");
	/* >8 End of init EAL. */

	if (routing_on)
		mac_updating = 0;
 
	printf("MAC updating %s\n", mac_updating ? "enabled" : "disabled");
	if (routing_on)
		printf("IPv4 routing mode enabled\n");
 
	/* convert to number of cycles */
	timer_period *= rte_get_timer_hz();
 
	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");
 
	if (port_pair_params != NULL) {
		if (check_port_pair_config() < 0)
			rte_exit(EXIT_FAILURE, "Invalid port pair config\n");
	}
 
	/* check port mask to possible port mask */
	if (l2fwd_enabled_port_mask & ~((1 << nb_ports) - 1))
		rte_exit(EXIT_FAILURE, "Invalid portmask; possible (0x%x)\n",
			(1 << nb_ports) - 1);
 
	/* Initialization of the driver. 8< */
 
	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;
 
	/* populate destination port details */
	if (port_pair_params != NULL) {
		uint16_t idx, p;
 
		for (idx = 0; idx < (nb_port_pair_params << 1); idx++) {
			p = idx & 1;
			portid = port_pair_params[idx >> 1].port[p];
			l2fwd_dst_ports[portid] =
				port_pair_params[idx >> 1].port[p ^ 1];
		}
	} else {
		RTE_ETH_FOREACH_DEV(portid) {
			/* skip ports that are not enabled */
			if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
				continue;
 
			if (nb_ports_in_mask % 2) {
				l2fwd_dst_ports[portid] = last_port;
				l2fwd_dst_ports[last_port] = portid;
			} else {
				last_port = portid;
			}
 
			nb_ports_in_mask++;
		}
		if (nb_ports_in_mask % 2) {
			printf("Notice: odd number of ports in portmask.\n");
			l2fwd_dst_ports[last_port] = last_port;
		}
	}
	/* >8 End of initialization of the driver. */
 
	rx_lcore_id = 0;
	qconf = NULL;
 
	/* Initialize the port/queue configuration of each logical core */
	RTE_ETH_FOREACH_DEV(portid) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
 
		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       l2fwd_rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}
 
		if (qconf != &lcore_queue_conf[rx_lcore_id]) {
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];
			nb_lcores++;
		}
 
		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->n_rx_port++;
		printf("Lcore %u: RX port %u TX port %u\n", rx_lcore_id,
		       portid, l2fwd_dst_ports[portid]);
	}
 
	nb_mbufs = RTE_MAX(nb_ports * (nb_rxd + nb_txd + MAX_PKT_BURST +
		nb_lcores * MEMPOOL_CACHE_SIZE), 8192U);
 
	/* Create the mbuf pool. 8< */
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
		MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id());
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	/* >8 End of create the mbuf pool. */

	if (pktgen_port >= 0) {
		unsigned pktgen_nb_mbufs = 16384;
		char pktgen_pool_name[32];
		snprintf(pktgen_pool_name, sizeof(pktgen_pool_name),
		         "pktgen_pool_%d", pktgen_port);
		pktgen_mbuf_pool = rte_pktmbuf_pool_create(
			pktgen_pool_name, pktgen_nb_mbufs,
			256, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
			rte_socket_id());
		if (pktgen_mbuf_pool == NULL)
			rte_exit(EXIT_FAILURE, "Cannot init pktgen mbuf pool\n");
	}

	if (routing_on) {
		arp_tbl = arp_table_create("arp_cache", rte_socket_id(), 2048, 300, 500);
		if (!arp_tbl)
			rte_exit(EXIT_FAILURE, "Cannot init ARP table\n");
        ipv4_rt_tbls[0] = route_table_create("ipv4_rt", rte_socket_id(), 2048);
        if (!ipv4_rt_tbls[0])
            rte_exit(EXIT_FAILURE, "Cannot init IPv4 route table\n");

        for (uint16_t p = 0; p < RTE_MAX_ETHPORTS; p++) {
            if (!ifcfgs[p].configured)
                continue;
            uint32_t depth = (uint32_t)__builtin_popcount(ifcfgs[p].mask);
            uint32_t net = ifcfgs[p].ip & ifcfgs[p].mask;
            route_add(ipv4_rt_tbls[0], net, (uint8_t)depth, 0, p);
        }
        for (uint32_t i = 0; i < pending_route_count; i++) {
            route_add(ipv4_rt_tbls[0], pending_routes[i].dst_ip, pending_routes[i].depth,
                pending_routes[i].next_hop_ip, pending_routes[i].out_port);
        }

		int ipv6_on = 0;
		for (uint16_t p = 0; p < RTE_MAX_ETHPORTS; p++) {
			if (ifcfg6_tbls[0][p].configured) {
				ipv6_on = 1;
				break;
			}
		}
		if (pending_route6_count) {
			ipv6_on = 1;
		}
		if (ipv6_on) {
			nd_tbl = nd_table_create("nd_cache", rte_socket_id(), 2048, 300, 500);
			if (!nd_tbl)
				rte_exit(EXIT_FAILURE, "Cannot init ND table\n");
			ipv6_rt_tbls[0] = route6_table_create("ipv6_rt", rte_socket_id(), 2048);
			if (!ipv6_rt_tbls[0])
				rte_exit(EXIT_FAILURE, "Cannot init IPv6 route table\n");

			const struct rte_ipv6_addr unspec = RTE_IPV6_ADDR_UNSPEC;
			for (uint16_t p = 0; p < RTE_MAX_ETHPORTS; p++) {
				if (!ifcfg6_tbls[0][p].configured)
					continue;
				struct rte_ipv6_addr net = ifcfg6_tbls[0][p].ip;
				rte_ipv6_addr_mask(&net, ifcfg6_tbls[0][p].depth);
				route6_add(ipv6_rt_tbls[0], &net, ifcfg6_tbls[0][p].depth, &unspec, p);
			}
			for (uint32_t i = 0; i < pending_route6_count; i++) {
				route6_add(ipv6_rt_tbls[0], &pending_routes6[i].dst_ip, pending_routes6[i].depth,
					&pending_routes6[i].next_hop_ip, pending_routes6[i].out_port);
			}
		}
	}

	if (acl_runtime_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init ACL runtime\n");
	if (acl_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init ACL IPC\n");
	if (acl6_runtime_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init ACL6 runtime\n");
	if (acl6_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init ACL6 IPC\n");
	RTE_LCORE_FOREACH(lcore_id) {
		char name[64];
		snprintf(name, sizeof(name), "session_table_%u", lcore_id);
		if (session_table_init(&session_tables[lcore_id], name, 65536, rte_socket_id()) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init session table for lcore %u\n", lcore_id);
		session_table_inited[lcore_id] = 1;
		snprintf(name, sizeof(name), "session6_table_%u", lcore_id);
		if (session6_table_init(&session6_tables[lcore_id], name, 65536, rte_socket_id()) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init session6 table for lcore %u\n", lcore_id);
		session6_table_inited[lcore_id] = 1;
		snprintf(name, sizeof(name), "scan4_%u", lcore_id);
		if (scan_table_init(&scan4_tbls[lcore_id], name, 32768, sizeof(uint32_t), sizeof(struct scan4_state), rte_socket_id()) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init scan4 table for lcore %u\n", lcore_id);
		snprintf(name, sizeof(name), "scan6_%u", lcore_id);
		if (scan_table_init(&scan6_tbls[lcore_id], name, 32768, sizeof(struct rte_ipv6_addr), sizeof(struct scan6_state), rte_socket_id()) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init scan6 table for lcore %u\n", lcore_id);
	}
	if (rlim_syn_pps || rlim_udp_pps) {
		RTE_LCORE_FOREACH(lcore_id) {
			char name[64];
			if (rlim_syn_pps) {
				snprintf(name, sizeof(name), "rlim_syn_%u", lcore_id);
				if (rlim_table_init(&rlim_syn_tbls[lcore_id], name, 32768, sizeof(uint32_t), rte_socket_id()) != 0)
					rte_exit(EXIT_FAILURE, "Cannot init rlim syn table for lcore %u\n", lcore_id);
				snprintf(name, sizeof(name), "rlim6_syn_%u", lcore_id);
				if (rlim_table_init(&rlim6_syn_tbls[lcore_id], name, 32768, sizeof(struct rte_ipv6_addr), rte_socket_id()) != 0)
					rte_exit(EXIT_FAILURE, "Cannot init rlim6 syn table for lcore %u\n", lcore_id);
			}
			if (rlim_udp_pps) {
				snprintf(name, sizeof(name), "rlim_udp_%u", lcore_id);
				if (rlim_table_init(&rlim_udp_tbls[lcore_id], name, 32768, sizeof(uint32_t), rte_socket_id()) != 0)
					rte_exit(EXIT_FAILURE, "Cannot init rlim udp table for lcore %u\n", lcore_id);
				snprintf(name, sizeof(name), "rlim6_udp_%u", lcore_id);
				if (rlim_table_init(&rlim6_udp_tbls[lcore_id], name, 32768, sizeof(struct rte_ipv6_addr), rte_socket_id()) != 0)
					rte_exit(EXIT_FAILURE, "Cannot init rlim6 udp table for lcore %u\n", lcore_id);
			}
			rlim_inited[lcore_id] = 1;
		}
	}
	if (session_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init session IPC\n");
	if (session6_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init session6 IPC\n");
	if (portstats_ipc_init(RTE_MAX_ETHPORTS) != 0)
		rte_exit(EXIT_FAILURE, "Cannot init portstats IPC\n");
	if (denylog_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init denylog IPC\n");
	if (denylog6_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init denylog6 IPC\n");
	if (route6_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init route6 IPC\n");
    if (portcfg_ipc_init() != 0)
        rte_exit(EXIT_FAILURE, "Cannot init portcfg IPC\n");
	if (route_ipc_init() != 0) {
		rte_exit(EXIT_FAILURE, "Cannot init route IPC\n");
	}
	if (attack_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init attack IPC\n");
	if (acl_hit_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init acl hit IPC\n");
	if (acl6_hit_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init acl6 hit IPC\n");
	if (rlim_ipc_init() != 0)
		rte_exit(EXIT_FAILURE, "Cannot init rlim IPC\n");
	if (pthread_create(&acl_ctrl_thread, NULL, acl_ctrl_thread_main, NULL) != 0)
		rte_exit(EXIT_FAILURE, "Cannot start ACL control thread\n");
	if (pthread_create(&acl6_ctrl_thread, NULL, acl6_ctrl_thread_main, NULL) != 0)
		rte_exit(EXIT_FAILURE, "Cannot start ACL6 control thread\n");
 
	/* Initialise each port */
	RTE_ETH_FOREACH_DEV(portid) {
		struct rte_eth_rxconf rxq_conf;
		struct rte_eth_txconf txq_conf;
		struct rte_eth_conf local_port_conf = port_conf;
		struct rte_eth_dev_info dev_info;
 
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", portid);
			continue;
		}
		nb_ports_available++;
 
		/* init port */
		printf("Initializing port %u... ", portid);
		fflush(stdout);
 
		ret = rte_eth_dev_info_get(portid, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Error during getting device (port %u) info: %s\n",
				portid, strerror(-ret));
 
		if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
		/* Configure the number of queues for a port. */
		ret = rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, portid);
		/* >8 End of configuration of the number of queues for a port. */
 
		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, port=%u\n",
				 ret, portid);
 
		ret = rte_eth_macaddr_get(portid,
					  &l2fwd_ports_eth_addr[portid]);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot get MAC address: err=%d, port=%u\n",
				 ret, portid);
 
		/* init one RX queue */
		fflush(stdout);
		rxq_conf = dev_info.default_rxconf;
		rxq_conf.offloads = local_port_conf.rxmode.offloads;
		/* RX queue setup. 8< */
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     &rxq_conf,
					     l2fwd_pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, portid);
		/* >8 End of RX queue setup. */
 
		/* Init one TX queue on each port. 8< */
		fflush(stdout);
		txq_conf = dev_info.default_txconf;
		txq_conf.offloads = local_port_conf.txmode.offloads;
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				&txq_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, portid);
		/* >8 End of init one TX queue on each port. */
 
		/* Initialize TX buffers */
		tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
				RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
				rte_eth_dev_socket_id(portid));
		if (tx_buffer[portid] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
					portid);
 
		rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);
 
		ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
				rte_eth_tx_buffer_count_callback,
				&port_statistics[portid].dropped);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
				 portid);
 
		ret = rte_eth_dev_set_ptypes(portid, RTE_PTYPE_UNKNOWN, NULL,
					     0);
		if (ret < 0)
			printf("Port %u, Failed to disable Ptype parsing\n",
					portid);
		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, portid);
 
		printf("done: \n");
		if (promiscuous_on) {
			ret = rte_eth_promiscuous_enable(portid);
			if (ret != 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_promiscuous_enable:err=%s, port=%u\n",
					rte_strerror(-ret), portid);
		}
 
		printf("Port %u, MAC address: " RTE_ETHER_ADDR_PRT_FMT "\n\n",
			portid,
			RTE_ETHER_ADDR_BYTES(&l2fwd_ports_eth_addr[portid]));
 
		/* initialize port stats */
		memset(&port_statistics, 0, sizeof(port_statistics));
	}
 
	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}
 
	check_all_ports_link_status(l2fwd_enabled_port_mask);

	if (routing_on) {
		send_gratuitous_arp();
	}
 
	ret = 0;
	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MAIN);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0) {
			ret = -1;
			break;
		}
	}
 
	RTE_ETH_FOREACH_DEV(portid) {
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("Closing port %d...", portid);
		ret = rte_eth_dev_stop(portid);
		if (ret != 0)
			printf("rte_eth_dev_stop: err=%d, port=%d\n",
			       ret, portid);
		rte_eth_dev_close(portid);
		printf(" Done\n");
	}
 
	force_quit = true;
	pthread_join(acl_ctrl_thread, NULL);
	acl_runtime_free();
	pthread_join(acl6_ctrl_thread, NULL);
	acl6_runtime_free();
    arp_table_free(arp_tbl);
    route_reclaim();
    if (ipv4_rt_tbls[0]) {
        route_table_free(ipv4_rt_tbls[0]);
        ipv4_rt_tbls[0] = NULL;
    }
    if (ipv4_rt_tbls[1]) {
        route_table_free(ipv4_rt_tbls[1]);
        ipv4_rt_tbls[1] = NULL;
    }
	nd_table_free(nd_tbl);
	route6_reclaim();
	if (ipv6_rt_tbls[0]) {
		route6_table_free(ipv6_rt_tbls[0]);
		ipv6_rt_tbls[0] = NULL;
	}
	if (ipv6_rt_tbls[1]) {
		route6_table_free(ipv6_rt_tbls[1]);
		ipv6_rt_tbls[1] = NULL;
	}
	RTE_LCORE_FOREACH(lcore_id) {
		if (session_table_inited[lcore_id]) {
			session_table_free(&session_tables[lcore_id]);
			session_table_inited[lcore_id] = 0;
		}
		if (session6_table_inited[lcore_id]) {
			session6_table_free(&session6_tables[lcore_id]);
			session6_table_inited[lcore_id] = 0;
		}
		scan_table_free(&scan4_tbls[lcore_id]);
		scan_table_free(&scan6_tbls[lcore_id]);
		if (rlim_inited[lcore_id]) {
			rlim_table_free(&rlim_syn_tbls[lcore_id]);
			rlim_table_free(&rlim_udp_tbls[lcore_id]);
			rlim_table_free(&rlim6_syn_tbls[lcore_id]);
			rlim_table_free(&rlim6_udp_tbls[lcore_id]);
			rlim_inited[lcore_id] = 0;
		}
	}
	rte_eal_cleanup();
	printf("Bye...\n");
 
	return ret;
}
