#ifndef DPDK_PF_ACL_HIT_IPC_H
#define DPDK_PF_ACL_HIT_IPC_H

#include <stdint.h>

#include <rte_atomic.h>

#include "acl_ipc.h"

#define ACL_HIT_SHARED_NAME "acl_hit_shared_cfg"

struct acl_hit_shared_cfg {
	rte_atomic64_t version;
	uint64_t rule_version;
	uint32_t count;
	uint32_t reserved;
	uint64_t deny_pkts[ACL_MAX_RULES];
	uint64_t deny_bytes[ACL_MAX_RULES];
};

#endif
