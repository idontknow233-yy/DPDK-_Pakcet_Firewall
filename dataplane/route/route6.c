#include "route6.h"

#include <string.h>

#include <rte_lpm6.h>
#include <rte_malloc.h>

struct route6_table {
	struct rte_lpm6 *lpm;
	struct route6_entry *entries;
	uint32_t entry_cap;
	uint32_t entry_count;
};

struct route6_table *route6_table_create(const char *name, int socket_id, uint32_t max_routes) {
	struct route6_table *rt = rte_zmalloc(NULL, sizeof(*rt), 0);
	if (!rt) {
		return NULL;
	}

	struct rte_lpm6_config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.max_rules = max_routes ? max_routes : 1024;
	cfg.number_tbl8s = 1024;
	cfg.flags = 0;

	rt->lpm = rte_lpm6_create(name, socket_id, &cfg);
	if (!rt->lpm) {
		rte_free(rt);
		return NULL;
	}

	rt->entry_cap = cfg.max_rules;
	rt->entries = rte_zmalloc(NULL, sizeof(struct route6_entry) * rt->entry_cap, 0);
	if (!rt->entries) {
		rte_lpm6_free(rt->lpm);
		rte_free(rt);
		return NULL;
	}
	rt->entry_count = 0;

	return rt;
}

void route6_table_free(struct route6_table *rt) {
	if (!rt) {
		return;
	}
	if (rt->lpm) {
		rte_lpm6_free(rt->lpm);
	}
	if (rt->entries) {
		rte_free(rt->entries);
	}
	rte_free(rt);
}

int route6_add(struct route6_table *rt, const struct rte_ipv6_addr *dst, uint8_t depth, const struct rte_ipv6_addr *next_hop, uint16_t out_port) {
	if (!rt || !rt->lpm || !rt->entries || !dst) {
		return -1;
	}
	if (rt->entry_count >= rt->entry_cap) {
		return -1;
	}
	if (depth > RTE_IPV6_MAX_DEPTH) {
		return -1;
	}
	uint32_t idx = rt->entry_count++;
	memset(&rt->entries[idx], 0, sizeof(rt->entries[idx]));
	if (next_hop) {
		rt->entries[idx].next_hop = *next_hop;
	}
	rt->entries[idx].out_port = out_port;

	if (rte_lpm6_add(rt->lpm, dst, depth, idx) < 0) {
		rt->entry_count--;
		return -1;
	}
	return 0;
}

int route6_lookup(const struct route6_table *rt, const struct rte_ipv6_addr *dst, struct route6_entry *out) {
	if (!rt || !rt->lpm || !rt->entries || !dst || !out) {
		return -1;
	}
	uint32_t idx = 0;
	if (rte_lpm6_lookup(rt->lpm, dst, &idx) < 0) {
		return -1;
	}
	if (idx >= rt->entry_count) {
		return -1;
	}
	*out = rt->entries[idx];
	return 0;
}

uint32_t route6_count(const struct route6_table *rt) {
	if (!rt) {
		return 0;
	}
	return rt->entry_count;
}

