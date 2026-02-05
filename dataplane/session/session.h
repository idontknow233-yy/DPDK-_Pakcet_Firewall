#ifndef DPDK_PF_SESSION_H
#define DPDK_PF_SESSION_H

#include <stdbool.h>
#include <stdint.h>

struct rte_hash;

struct session_key {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
};

struct session_entry {
	uint64_t last_seen_tsc;
	uint64_t packets;
	uint64_t bytes;
};

struct session_table {
	struct rte_hash *hash;
	struct session_entry *entries;
	uint32_t capacity;
};

int session_table_init(struct session_table *table, const char *name, uint32_t capacity, int socket_id);
void session_table_free(struct session_table *table);
int session_track(struct session_table *table, const struct session_key *key, uint32_t pkt_len, uint64_t now_tsc, bool *is_new);

#endif
