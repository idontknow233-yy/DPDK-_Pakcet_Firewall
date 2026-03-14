#ifndef DPDK_PF_ROUTE6_H
#define DPDK_PF_ROUTE6_H

#include <stdint.h>

#include <rte_ip6.h>

struct route6_entry {
	struct rte_ipv6_addr next_hop;
	uint16_t out_port;
	uint16_t reserved;
};

struct route6_table;

struct route6_table *route6_table_create(const char *name, int socket_id, uint32_t max_routes);
void route6_table_free(struct route6_table *rt);

int route6_add(struct route6_table *rt, const struct rte_ipv6_addr *dst, uint8_t depth, const struct rte_ipv6_addr *next_hop, uint16_t out_port);
int route6_lookup(const struct route6_table *rt, const struct rte_ipv6_addr *dst, struct route6_entry *out);

uint32_t route6_count(const struct route6_table *rt);

#endif
