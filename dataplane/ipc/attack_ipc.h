#ifndef DPDK_PF_ATTACK_IPC_H
#define DPDK_PF_ATTACK_IPC_H

#include <stdint.h>

#include <rte_atomic.h>
#include <rte_ip6.h>

#define ATTACK_SHARED_NAME "attack_shared_cfg"

struct attack_shared_cfg {
	rte_atomic64_t version;
	uint32_t scan_ports_per_sec;
	uint32_t ban_seconds;
	uint8_t mitigation_enabled;
	uint8_t reserved8[3];

	uint32_t syn_pps;
	uint32_t udp_pps;
	uint32_t scan_banned;
	uint32_t scan_events;

	uint32_t top_scan4_ip;
	uint32_t top_scan4_ports;
	struct rte_ipv6_addr top_scan6_ip;
	uint32_t top_scan6_ports;
	uint32_t reserved32;
};

#endif
