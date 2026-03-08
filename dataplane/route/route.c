#include "route.h"

#include <string.h>

#include <rte_lpm.h>
#include <rte_malloc.h>

struct route_table {
	struct rte_lpm *lpm;
	struct route_entry *entries;
	uint32_t entry_cap;
	uint32_t entry_count;
};

struct route_table *route_table_create(const char *name, int socket_id, uint32_t max_routes) {
	struct route_table *rt = rte_zmalloc(NULL, sizeof(*rt), 0);
	if (!rt) {
		return NULL;
	}

	struct rte_lpm_config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.max_rules = max_routes ? max_routes : 1024;
	cfg.number_tbl8s = 256;
	cfg.flags = 0;

	rt->lpm = rte_lpm_create(name, socket_id, &cfg);
	if (!rt->lpm) {
		rte_free(rt);
		return NULL;
	}

	rt->entry_cap = cfg.max_rules;
	rt->entries = rte_zmalloc(NULL, sizeof(struct route_entry) * rt->entry_cap, 0);
	if (!rt->entries) {
		rte_lpm_free(rt->lpm);
		rte_free(rt);
		return NULL;
	}
	rt->entry_count = 0;

	return rt;
}

void route_table_free(struct route_table *rt) {
	if (!rt) {
		return;
	}
	if (rt->lpm) {
		rte_lpm_free(rt->lpm);
	}
	if (rt->entries) {
		rte_free(rt->entries);
	}
	rte_free(rt);
}

int route_add(struct route_table *rt, uint32_t dst_ip, uint8_t depth, uint32_t next_hop_ip, uint16_t out_port) {
	if (!rt || !rt->lpm || !rt->entries) {
		return -1;
	}
	if (rt->entry_count >= rt->entry_cap) {
		return -1;
	}
	if (depth > 32) {
		return -1;
	}
	uint32_t idx = rt->entry_count++;
	rt->entries[idx].next_hop_ip = next_hop_ip;
	rt->entries[idx].out_port = out_port;

	if (rte_lpm_add(rt->lpm, dst_ip, depth, idx) < 0) {
		rt->entry_count--;
		return -1;
	}
	return 0;
}

int route_lookup(const struct route_table *rt, uint32_t dst_ip, struct route_entry *out) {
	if (!rt || !rt->lpm || !rt->entries || !out) {
		return -1;
	}
	uint32_t idx = 0;
	if (rte_lpm_lookup(rt->lpm, dst_ip, &idx) < 0) {
		return -1;
	}
	if (idx >= rt->entry_count) {
		return -1;
	}
	*out = rt->entries[idx];
	return 0;
}

uint32_t route_count(const struct route_table *rt) {
	if (!rt) {
		return 0;
	}
	return rt->entry_count;
}

