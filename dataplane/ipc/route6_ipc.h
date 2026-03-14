#ifndef DPDK_PF_ROUTE6_IPC_H
#define DPDK_PF_ROUTE6_IPC_H

#include <stdint.h>

#include <rte_atomic.h>
#include <rte_ip6.h>

#define ROUTE6_SHARED_NAME "route6_shared_cfg"

#define ROUTE6_MAX 1024
#define IFACE6_MAX_PORTS 32

struct ifcfg6_item {
	struct rte_ipv6_addr ip;
	uint8_t depth;
	uint8_t configured;
	uint16_t reserved;
};

struct route6_item {
	struct rte_ipv6_addr dst;
	uint8_t depth;
	uint8_t reserved8[3];
	struct rte_ipv6_addr next_hop;
	uint16_t out_port;
	uint16_t reserved16;
};

struct route6_shared_cfg {
	rte_atomic64_t version;
	uint32_t route_count;
	uint32_t reserved;
	struct ifcfg6_item ifcfg6s[IFACE6_MAX_PORTS];
	struct route6_item routes[ROUTE6_MAX];
};

#endif
