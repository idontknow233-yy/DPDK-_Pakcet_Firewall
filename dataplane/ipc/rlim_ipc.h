#ifndef DPDK_PF_RLIM_IPC_H
#define DPDK_PF_RLIM_IPC_H

#include <stdint.h>

#include <rte_atomic.h>

#define RLIM_SHARED_NAME "rlim_shared_cfg"

struct rlim_shared_cfg {
	rte_atomic64_t version;
	uint32_t syn_pps;
	uint32_t syn_burst;
	uint32_t udp_pps;
	uint32_t udp_burst;
};

#endif
