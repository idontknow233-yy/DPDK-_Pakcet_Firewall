#ifndef DPDK_PF_ROUTE_H
#define DPDK_PF_ROUTE_H

#include <stdint.h>

struct route_entry {
	uint32_t next_hop_ip;
	uint16_t out_port;
};

struct route_table;

struct route_table *route_table_create(const char *name, int socket_id, uint32_t max_routes);
void route_table_free(struct route_table *rt);

int route_add(struct route_table *rt, uint32_t dst_ip, uint8_t depth, uint32_t next_hop_ip, uint16_t out_port);
int route_lookup(const struct route_table *rt, uint32_t dst_ip, struct route_entry *out);

uint32_t route_count(const struct route_table *rt);

#endif
