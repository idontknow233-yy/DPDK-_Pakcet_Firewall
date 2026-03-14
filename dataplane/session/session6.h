#ifndef DPDK_PF_SESSION6_H
#define DPDK_PF_SESSION6_H

#include <stdbool.h>
#include <stdint.h>

#include <rte_ip6.h>
#include <rte_spinlock.h>

struct rte_hash;

struct session6_key {
	struct rte_ipv6_addr src_ip6;
	struct rte_ipv6_addr dst_ip6;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
	uint8_t reserved[3];
};

struct session_entry;

struct session6_table {
	struct rte_hash *hash;
	struct session_entry *entries;
	uint32_t capacity;
	rte_spinlock_t lock;
};

int session6_table_init(struct session6_table *table, const char *name, uint32_t capacity, int socket_id);
void session6_table_free(struct session6_table *table);
int session6_track(struct session6_table *table, const struct session6_key *key, uint32_t pkt_len, uint64_t now_tsc, bool *is_new);
uint32_t session6_soft_expire(struct session6_table *table, uint64_t now_tsc, uint64_t timeout_tsc);
uint32_t session6_export(struct session6_table *table, struct session6_key *keys, struct session_entry *entries, uint32_t max_entries,
	uint64_t now_tsc, uint64_t timeout_tsc);

#endif
