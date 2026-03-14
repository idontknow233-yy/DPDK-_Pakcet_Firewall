#ifndef DPDK_PF_SESSION6_IPC_H
#define DPDK_PF_SESSION6_IPC_H

#include <stdint.h>

#include <rte_atomic.h>

#include "session/session6.h"

#define SESSION6_SHARED_NAME "session6_shared_cfg"
#define SESSION6_MAX_EXPORT 4096

struct session6_shared_cfg {
	rte_atomic64_t version;
	uint32_t count;
	struct session6_key keys[SESSION6_MAX_EXPORT];
	struct session_entry entries[SESSION6_MAX_EXPORT];
};

#endif
