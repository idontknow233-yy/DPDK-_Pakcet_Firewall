#ifndef DPDK_PF_STATS_IPC_H
#define DPDK_PF_STATS_IPC_H

#include <stdint.h>

#include <rte_atomic.h>
#include <rte_ethdev.h>
#include <rte_ip6.h>
#include <rte_spinlock.h>

#define PORTSTATS_SHARED_NAME "portstats_shared_cfg"
#define DENYLOG_SHARED_NAME "denylog_shared_cfg"
#define DENYLOG6_SHARED_NAME "denylog6_shared_cfg"

#define DENYLOG_MAX 2048
#define DENYLOG6_MAX 2048

struct portstats_item {
	uint64_t rx;
	uint64_t tx;
	uint64_t dropped;
	uint32_t link_speed;
	uint8_t link_up;
	uint8_t link_duplex;
	uint16_t reserved;
	uint8_t mac[RTE_ETHER_ADDR_LEN];
	uint8_t reserved2[2];
};

struct portstats_shared_cfg {
	rte_atomic64_t version;
	uint32_t enabled_port_mask;
	uint16_t nb_ports;
	uint16_t reserved;
	uint64_t tsc_hz;
	struct portstats_item ports[RTE_MAX_ETHPORTS];
};

struct denylog_entry {
	uint64_t tsc;
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
	uint8_t in_port;
	uint16_t reserved;
	uint32_t rule_index;
};

struct denylog_shared_cfg {
	rte_atomic64_t version;
	rte_spinlock_t lock;
	uint32_t head;
	uint32_t count;
	uint64_t tsc_hz;
	struct denylog_entry entries[DENYLOG_MAX];
};

struct denylog6_entry {
	uint64_t tsc;
	struct rte_ipv6_addr src_ip6;
	struct rte_ipv6_addr dst_ip6;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
	uint8_t in_port;
	uint16_t reserved;
	uint32_t rule_index;
};

struct denylog6_shared_cfg {
	rte_atomic64_t version;
	rte_spinlock_t lock;
	uint32_t head;
	uint32_t count;
	uint64_t tsc_hz;
	struct denylog6_entry entries[DENYLOG6_MAX];
};

#endif
